float4 render_di_ris(const float2 screenPos, __constant struct RenderParams* params,
	__global struct Light* lights, __global struct Reservoir* reservoirs, __global struct Reservoir* initialSampling,
	__read_only image3d_t grid,
	__global const PAYLOAD* brick0,
#if ONEBRICKBUFFER == 0
	__global const PAYLOAD* brick1, __global const PAYLOAD* brick2, __global const PAYLOAD* brick3,
#endif
	__global float4* sky, __global const uint* blueNoise,
	__global const unsigned char* uberGrid
);

__kernel void initiate_per_pixel_samples(__constant struct RenderParams* params, __global struct Light* lights, __global struct Reservoir* reservoirs)
{
	const int x = get_global_id(0), y = get_global_id(1);
	struct Reservoir* res = &reservoirs[x + y * SCRWIDTH];
	
	res->type = 0;
	res->sumOfWeights = 0;
	res->streamLength = 0;
	res->light_index = 0;

	uint seed = WangHash(x * 171 + y * 1773 + params->R0);

	const int numberOfLights = params->numberOfLights;
	for (int i = 0; i < NUMBEROFLIGHTCANDIDATES; i++)
	{
		const int random_light_index = getRandomLightIndex(numberOfLights, &seed);
		struct Light *light = &lights[random_light_index];
		light->index = random_light_index;
		UpdateReservoir(res, light, RandomFloat(&seed));
	}
}

__kernel void renderRIS(__global float4* frame, __constant struct RenderParams* params,
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

	float4 pixel = render_di_ris((float2)(x, y), params, lights, reservoirs, initialSampling, grid, BRICKPARAMS, sky, blueNoise, uberGrid);

	// store pixel in linear color space, to be processed by finalize kernel for TAA
	frame[x + y * SCRWIDTH] = pixel; // depth in w
}

float4 render_di_ris(const float2 screenPos, __constant struct RenderParams* params,
	__global struct Light* lights, __global struct Reservoir* reservoirs, __global struct Reservoir* initialSampling,
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
	float dist;
	uint side = 0;
	const int x = (int)screenPos.x, y = (int)screenPos.y;
	uint seed = WangHash(x * 171 + y * 1773 + params->R0);
	float screenx = screenPos.x;
	float screeny = screenPos.y;
	const float3 D = GenerateCameraRay((float2)(screenx, screeny), params);
	const uint voxel = TraceRay((float4)(params->E, 0), (float4)(D, 1), &dist, &side, grid, uberGrid, BRICKPARAMS, 999999 /* no cap needed */);
	const float skyLightScale = params->skyLightScale;

	if (voxel == 0) // exit the scene
	{
		return (float4)(0, 0, 0, 1e20f);
	}
	if (IsEmitter(voxel))
	{
		return (float4)(ToFloatRGB(voxel) * EmitStrength(voxel), 1e20f);
	}

	const float3 BRDF1 = INVPI * ToFloatRGB(voxel);

	const float4 primaryHitPoint = (float4)(params->E + D * dist, 0);
	const float3 N = VoxelNormal(side, D);

	struct Reservoir* res = &reservoirs[x + y * SCRWIDTH];
	const float weight = params->numberOfLights;
	//uint3 emitterVoxelCoords = indexToCoordinates(res->position_selected);
	//uint emitterVoxel = Get(emitterVoxelCoords.x, emitterVoxelCoords.y, emitterVoxelCoords.z, grid, brick0);
	struct Light* light = &lights[res->light_index];
	uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);
	//uint emitterVoxel = Get(emitterVoxelCoords.x, emitterVoxelCoords.y, emitterVoxelCoords.z, grid, brick0);
	uint emitterVoxel = light->voxel;

	const float3 R = (float3)(emitterVoxelCoordsf - primaryHitPoint.xyz);
	const float3 L = normalize(R);
	uint side2;
	float dist2;
	const uint voxel2 = TraceRay(primaryHitPoint + 0.1f * (float4)(N, 0), (float4)(L, 1.0), &dist2, &side2, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
	if (distance(length(R), dist2) > 0.8)
	{
		return (float4)(0.0, 0.0, 0.0, dist);
	}

	const float NdotL = max(dot(N, L), 0.0);

	const float3 directContribution = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel) * NdotL / (dist2 * dist2) * weight;
	const float3 incoming = BRDF1 * directContribution;
	return (float4)(incoming, dist);
}