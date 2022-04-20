void generateLightCandidate(const uint numberOfLights, float* probability, uint* index, uint* seedptr)
{
	*index = getRandomIndex(numberOfLights, seedptr);
	*probability = 1.0 / numberOfLights;
}

float evaluatePHat(
	__global struct Light* lights, const uint lightIndex, const float3 N, 
	const float3 brdf, const float3 shadingPoint)
{
	struct Light* light = &lights[lightIndex];
	const uint3 emitterVoxelCoords = indexToCoordinates(light->index);
	// take middle of voxel
	const float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);

	const uint emitterVoxel = light->voxel;
	const float3 lightIntensity = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel);

	const float3 R = emitterVoxelCoordsf - shadingPoint;
	const float3 L = normalize(R);
	const float NdotL = max(dot(N, L), 0.0);
	const float distsquared = dot(R, R);

	const float pHat = length((brdf * lightIntensity * NdotL) / distsquared);
	return pHat;
}

// candidate sampling + temporal sampling
__kernel void perPixelInitialSampling(__global struct DebugInfo* debugInfo, 
	__global struct CLRay* albedo, __constant struct RenderParams* params, 
	__global struct Light* lights, 
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs, 
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	const int x = get_global_id(0), y = get_global_id(1);
	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	uint* seedptr = &ray->seed;
	const float3 D = ray->rayDirection;
	const float3 N = VoxelNormal(ray->side, D);
	const float3 brdf = ToFloatRGB(ray->voxelValue) * INVPI;
	const float3 shadingPoint = D * ray->distance + params->E;

	const uint numberOfLights = params->numberOfLights;
	const uint numberOfCandidates = min(numberOfLights, params->numberOfCandidates);

	struct Reservoir res = {0.0, 0, 0, 0.0};
	
	//float pphat = 0;
	for (int i = 0; i < numberOfCandidates; i++)
	{
		uint lightIndex;
		float p;
		generateLightCandidate(numberOfLights, &p, &lightIndex, seedptr);

		float pHat = evaluatePHat(lights, lightIndex, N, brdf, shadingPoint);
		UpdateReservoir(&res, pHat / p, lightIndex, RandomFloat(seedptr));

		//if (i == 10) pphat = pHat;
	}

	float pHat = evaluatePHat(lights, res.lightIndex, N, brdf, shadingPoint);
	AdjustWeight(&res, pHat);

	reservoirs[x + y * SCRWIDTH] = res;

	if (x == 500 && y == 500)
	{
		debugInfo->res = res;
		//debugInfo->phat = pphat;
	}
}

// spatial resampling
__kernel void perPixelSpatialResampling(__global struct DebugInfo* debugInfo,
	__global struct CLRay* albedo, __constant struct RenderParams* params,
	__global struct Light* lights,
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	const int x = get_global_id(0), y = get_global_id(1);
	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	uint* seedptr = &ray->seed;
	float3 shadingPoint = ray->rayDirection * ray->distance + params->E;
}

// Shading step
float4 render_di_ris(__global struct DebugInfo* debugInfo, const struct CLRay* hit, 
	const int2 screenPos, __constant struct RenderParams* params,
	__global struct Light* lights, 
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs,
	__read_only image3d_t grid,
	__global const PAYLOAD* brick0,
#if ONEBRICKBUFFER == 0
	__global const PAYLOAD* brick1, __global const PAYLOAD* brick2, __global const PAYLOAD* brick3,
#endif
	__global float4* sky, __global const uint* blueNoise,
	__global const unsigned char* uberGrid
)
{
	// trace primary ray
	const float dist = hit->distance;
	const float3 D = hit->rayDirection;
	const uint side = hit->side;
	const float3 N = VoxelNormal(side, D);
	const int x = screenPos.x, y = screenPos.y;
	uint* seedptr = &hit->seed;
	const uint voxel = hit->voxelValue; 
	
	struct Reservoir* res = &reservoirs[x + y * SCRWIDTH];

	float3 color = (float3)(0.0, 0.0, 0.0);
	// hit the background/exit the scene
	if (voxel == 0)
	{
		dist = 1e20f;
		res->sumOfWeights = 0.0;
		res->streamLength = 0;
		res->lightIndex = 0;
		res->adjustedWeight = 0.0;
	}
	// hit emitter
	else if (IsEmitter(voxel))
	{
		color = ToFloatRGB(voxel) * EmitStrength(voxel);
	}
	// hit other voxel
	else
	{
		const float3 BRDF1 = INVPI * ToFloatRGB(voxel);

		const float3 primaryHitPoint = D * dist + params->E;

		uint lightIndex = getRandomIndex(params->numberOfLights, seedptr);
		struct Light* light = &lights[lightIndex];
		uint3 emitterVoxelCoords = indexToCoordinates(light->index);
		float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);
		uint emitterVoxel = light->voxel;

		const float3 R = emitterVoxelCoordsf - primaryHitPoint;
		const float3 L = normalize(R);
		uint side2;
		float dist2 = 0;
		const uint voxel2 = TraceRay((float4)(primaryHitPoint, 0) + 0.1f * (float4)(N, 0), (float4)(L, 1.0), &dist2, &side2, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
		//if (distance(length(R), dist2) < 0.8)
		{
			const float NdotL = max(dot(N, L), 0.0);

			const float3 directContribution = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel) * NdotL / (length(R) * length(R)) * (float)params->numberOfLights;
			const float3 incoming = BRDF1 * directContribution;

			color = incoming;
			//if (NdotL < 0.0001) color = DEBUGCOLOR;
			//else color = DEBUGCOLOR2;
		}

	}

	prevReservoirs[x + y * SCRWIDTH] = *res;

	return (float4)(color, dist);
}

__kernel void renderAlbedo(__global struct DebugInfo* debugInfo, 
	__global struct CLRay* albedo, __constant struct RenderParams* params,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	const int x = get_global_id(0), y = get_global_id(1);
	float screenx = x;
	float screeny = y;
	uint seed = WangHash(x * 171 + y * 1773 + params->R0);
	const float3 D = GenerateCameraRay((float2)(screenx, screeny), params);

	uint side = 0;
	float dist = 0;
	const uint voxel = TraceRay((float4)(params->E, 0), (float4)(D, 1), &dist, &side, grid, uberGrid, BRICKPARAMS, 999999 /* no cap needed */);
	//float3 hit_position = D * dist + params->E;

	struct CLRay* hit = &albedo[x + y * SCRWIDTH];
	hit->voxelValue = voxel;
	hit->side = side;
	hit->seed = seed;
	hit->distance = dist;
	hit->rayDirection = D;
}

__kernel void renderRIS(__global struct DebugInfo* debugInfo, __global struct CLRay* albedo, __global float4* frame, __constant struct RenderParams* params,
	__global struct Light* lights, __global struct Reservoir* reservoirs, __global struct Reservoir* initialSampling,
	__read_only image3d_t grid, __global float4* sky, __global const uint* blueNoise,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
#if ONEBRICKBUFFER == 0
	, __global const PAYLOAD* brick1, __global const PAYLOAD* brick2, __global const PAYLOAD* brick3
#endif
)
{
	// produce primary ray for pixel
	const int x = get_global_id(0), y = get_global_id(1);
	struct CLRay* hit = &albedo[x + y * SCRWIDTH];

	float4 pixel = render_di_ris(debugInfo, hit, (int2)(x, y), params, lights, reservoirs, initialSampling, grid, BRICKPARAMS, sky, blueNoise, uberGrid);

	// store pixel in linear color space, to be processed by finalize kernel for TAA
	frame[x + y * SCRWIDTH] = pixel; // depth in w
}