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

	const float3 contribution = ToFloatRGB(voxel) * EmitStrength(voxel);
	const float3 BRDF1 = INVPI * ToFloatRGB(voxel);

	struct Reservoir res;
	res.sumOfWeights = 0;
	res.streamLength = 0;
	res.position_selected = 0;
	const int numberOfCandidates = 16;
	for (int i = 0; i < numberOfCandidates; i++)
	{
		int random_index = RandomFloat(&seed) * (params->numberOfLights - 1);
		const struct Light randomlight = lights[random_index];
		UpdateReservoir(&res, &randomlight, RandomFloat(&seed));
	}




	const float3 directContribution = BRDF1 * 0;
	const float3 incoming = directContribution + contribution;
	return (float4)(incoming, dist);
}