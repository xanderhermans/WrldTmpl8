void _perPixelSpatialResampling(const uint x, const uint y,
	__global struct DebugInfo* debugInfo,
	__constant struct RenderParams* params,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__global struct Light* lights,
	struct Reservoir* res, struct Reservoir* prevReservoirs);

void generateLightCandidate(const uint numberOfLights, float* probability, uint* index, uint* seedptr)
{
	*index = getRandomIndex(numberOfLights, seedptr);
	*probability = 1.0 / numberOfLights;
}

float evaluatePHat(
	__global struct Light* lights, uint lightIndex, float3 N,
	float3 brdf, float3 shadingPoint)
{
	struct Light* light = &lights[lightIndex];
	// take middle of voxel
	uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);

	const uint emitterVoxel = light->voxel;
	const float3 lightIntensity = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel);

	const float3 R = emitterVoxelCoordsf - shadingPoint;
	const float3 L = normalize(R);
	const float NdotL = max(dot(N, L), 0.0);
	const float distsquared = dot(R, R);

	const float pHat = length((brdf * lightIntensity * NdotL) / distsquared);
	return pHat;
}

void rayToCenterOfVoxelsAtCoord(const uint3 emitterVoxelCoords, const float3 shadingPoint,
	float3* L, float3* R, float* dist)
{
	float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);

	*R = emitterVoxelCoordsf - shadingPoint;
	*L = normalize(*R);
	*dist = length(*R);
}

bool isLightOccluded(const uint3 emitterVoxelCoords, const float3 shadingPoint,
	const float3 L, const float3 R, const float dist,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	float traceddistance;
	uint tracedside;
	const uint voxel2 = TraceRay((float4)(shadingPoint, 0), (float4)(L, 1.0), &traceddistance, &tracedside, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
	uint3 hitVoxelCoords = GridCoordinatesFromHit(tracedside, L, traceddistance, shadingPoint);

	// if we hit the emitter we know nothing occluded it
	return !Uint3equals(emitterVoxelCoords, hitVoxelCoords);
}

// candidate sampling + temporal sampling
__kernel void perPixelInitialSampling(__global struct DebugInfo* debugInfo,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo, 
	__constant struct RenderParams* params,
	__global struct Light* lights,
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	const int x = get_global_id(0), y = get_global_id(1);
	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	const float dist = ray->distance;
	const uint voxelValue = ray->voxelValue;

	struct Reservoir res = { 0.0, 0, 0, 0.0, 0 };

	if (voxelValue != 0)
	{
		uint* seedptr = &ray->seed;

		const float3 D = ray->rayDirection;
		const uint side = ray->side;
		const float3 N = VoxelNormal(side, D);
		const float3 brdf = ToFloatRGB(voxelValue) * INVPI;
		const float3 shadingPoint = D * dist + params->E;

		const uint numberOfLights = params->numberOfLights;
		const uint numberOfCandidates = min(numberOfLights, params->numberOfCandidates);

		// BEGIN CANDIDATE SAMPLING
		for (int i = 0; i < numberOfCandidates; i++)
		{
			uint lightIndex;
			float p;
			generateLightCandidate(numberOfLights, &p, &lightIndex, seedptr);

			float pHat = evaluatePHat(lights, lightIndex, N, brdf, shadingPoint);
			UpdateReservoir(&res, pHat / p, lightIndex, RandomFloat(seedptr));
		}

		float pHat = evaluatePHat(lights, res.lightIndex, N, brdf, shadingPoint);
		AdjustWeight(&res, pHat);

		uint lightIndex = res.lightIndex;
		struct Light* light = &lights[lightIndex];

		uint3 emitterVoxelCoords = indexToCoordinates(light->position);

		float3 L, R; float dist2;
		const float3 shadingPointOffset = shadingPoint + 0.1 * N;
		rayToCenterOfVoxelsAtCoord(emitterVoxelCoords, shadingPointOffset, &L, &R, &dist2);
		if (isLightOccluded(emitterVoxelCoords, shadingPointOffset, L, R, dist2, grid, uberGrid, brick0))
		{
			// discarded
			res.adjustedWeight = 0;
			res.sumOfWeights = 0;
		}
		// END CANDIDATE SAMPLING

		// BEGIN TEMPORAL SAMPLING
		if (params->restirtemporalframe > 0 && params->temporal)
		{
			int2 prevxy = ReprojectWorldPoint(params, shadingPoint);
			if (IsInBounds(prevxy, (int2)(0, 0), (int2)(SCRWIDTH, SCRHEIGHT)))
			{
				struct CLRay* prevRay = &prevAlbedo[prevxy.x + prevxy.y * SCRWIDTH];
				const uint prevSide = prevRay->side;
				const float prevDist = prevRay->distance;
				if (prevSide == side && distance(dist, prevDist) <= 0.15 * dist)
				{
					struct Reservoir prevRes = prevReservoirs[prevxy.x + prevxy.y * SCRWIDTH];
					const uint prevStreamLength = min(params->numberOfMaxTemporalImportance * numberOfCandidates, prevRes.streamLength);
					float prev_pHat = evaluatePHat(lights, prevRes.lightIndex, N, brdf, shadingPoint);

					ReWeighSumOfWeights(&prevRes, prev_pHat, prevStreamLength);

					CombineReservoir(&res, &prevRes, RandomFloat(seedptr));

					// already calculated potential contribution. re-use.
					float _pHat = res.lightIndex == lightIndex ? pHat : prev_pHat;

					AdjustWeight(&res, _pHat);
				}
			}
		}
		// END TEMPORAL SAMPLING

		//spatiotemporal instead of a spatial pass?
		//if (params->spatial)
		//{
		//	_perPixelSpatialResampling(x, y, debugInfo, params, albedo, lights, &res, prevReservoirs);
		//}

	}
	reservoirs[x + y * SCRWIDTH] = res;
}

// spatial resampling
void _perPixelSpatialResampling(const uint x, const uint y,
	__global struct DebugInfo* debugInfo,
	__constant struct RenderParams* params, 
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__global struct Light* lights,
	struct Reservoir* res, struct Reservoir* prevReservoirs)
{
	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	uint* seedptr = &ray->seed;
	const float dist = ray->distance;
	const uint voxelValue = ray->voxelValue;
	const float3 D = ray->rayDirection;
	const uint side = ray->side;
	const float3 N = VoxelNormal(side, D);
	const float3 brdf = ToFloatRGB(voxelValue) * INVPI;
	const float3 shadingPoint = D * dist + params->E;

	for (int i = 0; i < params->spatialTaps; i++)
	{
		float angle = RandomFloat(seedptr) * TWOPI;
		// radius = (r - 1) * [0..1] + 1 so we minimze the probability of picking a radius that ends up with the current pixel
		float radius = convert_float(params->spatialRadius - 1) * sqrt(RandomFloat(seedptr)) + 1.0;
		int xOffset = convert_int(cos(angle) * radius);
		int yOffset = convert_int(sin(angle) * radius);
		int neighbourX = x + xOffset;
		int neighbourY = y + yOffset;

		// if neighbour is out of bounds or the same as our current reservoir we reject it.				
		if ((neighbourX == x && neighbourY == y)
			|| neighbourX < 0 || neighbourY < 0
			|| neighbourX >= SCRWIDTH || neighbourY >= SCRHEIGHT)
		{
			continue;
		}

		uint neighbourIndex = neighbourX + neighbourY * SCRWIDTH;
		struct Reservoir neighbourRes = prevReservoirs[neighbourIndex];

		if (neighbourRes.streamLength != 0)
		{
			const struct CLRay* neigbourRay = &albedo[neighbourIndex];
			const uint neighbourSide = neigbourRay->side;
			const float neighbourDist = neigbourRay->distance;
			if (neighbourSide != side || distance(dist, neighbourDist) > 0.15 * dist)
			{
				continue;
			}

			// use the world coordinate?
			//const float3 neigbourShadingPoint = ShadingPoint(neigbourRay, params->E);

			//re-evaluate sum of weights with new phat
			float new_pHat = evaluatePHat(lights, neighbourRes.lightIndex, N, brdf, shadingPoint);
			ReWeighSumOfWeights(&neighbourRes, new_pHat, neighbourRes.streamLength);
			//ReWeighSumOfWeights(&neighbourRes, new_pHat, 32);

			CombineReservoir(res, &neighbourRes, RandomFloat(seedptr));
		}
	}

	float pHat = evaluatePHat(lights, res->lightIndex, N, brdf, shadingPoint);
	AdjustWeight(res, pHat);
}

__kernel void perPixelSpatialResampling(__global struct DebugInfo* debugInfo,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__constant struct RenderParams* params,
	__global struct Light* lights,
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
)
{
	const uint x = get_global_id(0), y = get_global_id(1);

	struct Reservoir res = prevReservoirs[x + y * SCRWIDTH];

	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	const uint voxelValue = ray->voxelValue;
	if (voxelValue != 0)
	{
		if (params->spatial)
		{
			_perPixelSpatialResampling(x, y, debugInfo, params, prevAlbedo, albedo, lights, &res, prevReservoirs);
		}
	}

	reservoirs[x + y * SCRWIDTH] = res;
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
	int x = screenPos.x, y = screenPos.y;
	// trace primary ray
	float dist = hit->distance;
	float3 D = hit->rayDirection;
	uint side = hit->side;
	float3 N = VoxelNormal(side, D);
	uint* seedptr = &hit->seed;
	uint voxel = hit->voxelValue;
	uint numberOfLights = params->numberOfLights;

	struct Reservoir res = prevReservoirs[x + y * SCRWIDTH];

	float3 color = (float3)(0.0, 0.0, 0.0);
	// hit the background/exit the scene
	if (voxel == 0)
	{
		dist = 1e20f;
	}
	else
	{
		// hit emitter
		if (IsEmitter(voxel))
		{
			float3 emitterColor = ToFloatRGB(voxel) * EmitStrength(voxel);
#if GFXEXP == 1
			emitterColor *= INVPI;
#endif
			color += emitterColor;
		}
		// hit other voxel and we have a light to sample from the reservoir sampling
#if GFXEXP == 0
		else
#endif
			if (res.adjustedWeight > 0 && res.streamLength > 0)
			{
				const float3 BRDF1 = INVPI * ToFloatRGB(voxel);

				const float3 shadingPoint = D * dist + params->E;

				uint lightIndex = res.lightIndex;
				struct Light light = lights[lightIndex];
				uint3 emitterVoxelCoords = indexToCoordinates(light.position);

				float3 L, R; float dist2;
				const float3 shadingPointOffset = shadingPoint + 0.1 * N;
				rayToCenterOfVoxelsAtCoord(emitterVoxelCoords, shadingPointOffset, &L, &R, &dist2);
				// if we hit the emitter we know nothing occluded it
				if (!isLightOccluded(emitterVoxelCoords, shadingPointOffset, L, R, dist2, grid, uberGrid, brick0))
				{
					const float NdotL = max(dot(N, L), 0.0);
					uint emitterVoxel = light.voxel;
					float3 emitterColor = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel);
#if GFXEXP == 1
					emitterColor *= INVPI;
#endif

					float distancesquared = max(dist2 * dist2, 10e-6);
					const float3 directContribution = ((emitterColor * NdotL) / distancesquared) * res.adjustedWeight;
					const float3 incoming = BRDF1 * directContribution;

					color += incoming;
				}
				else
				{
					// discarded
					res.adjustedWeight = 0;
					res.sumOfWeights = 0;
					prevReservoirs[x + y * SCRWIDTH] = res;
				}
			}
	}

	return (float4)(color, dist);
}

__kernel void renderAlbedo(__global struct DebugInfo* debugInfo,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__constant struct RenderParams* params,
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
	//uint3 hitVoxelCoords = GridCoordinatesFromHit(side, D, dist, params->E);
	//uint voxel1 = Get(hitVoxelCoords.x, hitVoxelCoords.y, hitVoxelCoords.z, grid, BRICKPARAMS);

	// no need to copy since we swap the current and previous albedo buffer every frame
	//prevAlbedo[x + y * SCRWIDTH] = albedo[x + y * SCRWIDTH];
	struct CLRay* hit = &albedo[x + y * SCRWIDTH];
	hit->voxelValue = voxel;
	hit->side = side;
	hit->seed = seed;
	hit->distance = dist;
	hit->rayDirection = D;
}

__kernel void renderRIS(__global struct DebugInfo* debugInfo,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__global float4* frame, __constant struct RenderParams* params,
	__global struct Light* lights,
	__global struct Reservoir* reservoirs, __global struct Reservoir* prevReservoirs,
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
	float4 pixel = render_di_ris(debugInfo, hit, (int2)(x, y), params, lights, reservoirs, prevReservoirs, grid, BRICKPARAMS, sky, blueNoise, uberGrid);

	// store pixel in linear color space, to be processed by finalize kernel for TAA
	frame[x + y * SCRWIDTH] = pixel; // depth in w
	reservoirs[x + y * SCRWIDTH] = prevReservoirs[x + y * SCRWIDTH];
	reservoirs[x + y * SCRWIDTH].distance = hit->distance;
	reservoirs[x + y * SCRWIDTH].side = hit->side;
}