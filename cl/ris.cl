__kernel void initiate_per_pixel_samples(__global struct Test* test, __global struct CLRay* albedo, __constant struct RenderParams* params, __global struct Light* lights, __global struct Reservoir* reservoirs)
{
	const int x = get_global_id(0), y = get_global_id(1);
	struct Reservoir res;
	struct CLRay* ray = &albedo[x + y * SCRWIDTH];
	float3 shadingPoint = ray->ray_direction * ray->distance + params->E;

	res.visible = 1;
	res.sumOfWeights = 0;
	res.streamLength = 0;
	res.light_index = 0;

	uint seed = WangHash(x * 171 + y * 1773 + params->R0);

	const int numberOfLights = params->numberOfLights;
	const int random_light_index = getRandomLightIndex(numberOfLights, &seed);
	struct Light* light = &lights[random_light_index];
	light->index = random_light_index;
	uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5); // assume middle of voxel
	float distanceToShadingPoint = length(emitterVoxelCoordsf - shadingPoint);
	float visibility = 1; //assume visible for now;
	float weight = visibility / distanceToShadingPoint;

	UpdateReservoir(&res, weight, random_light_index, RandomFloat(&seed));

	int restirframecount = modulo(params->restirframecount, NUMBEROFTEMPORALFRAMES);
	int prevframe = modulo(restirframecount - 1, NUMBEROFTEMPORALFRAMES);
	if (params->restirframecount > 0 && params->spatial)
	{
		SpatialResampling(&res, x, y, prevframe, &seed, reservoirs);
	}

	//NormalizeReservoir(&res);
	//res.light_index = 0;
	reservoirs[x + y * SCRWIDTH + restirframecount * SCRWIDTH * SCRHEIGHT] = res;

	if (x == 500 && y == 500 && params->restirframecount > 0)
	{
		struct Reservoir res2 = reservoirs[x + y * SCRWIDTH + prevframe * SCRWIDTH * SCRHEIGHT];
		test->frame = res2.sumOfWeights;
		test->prevframe = res2.streamLength;
	}
}

float4 render_di_ris(const struct CLRay* hit, const int2 screenPos, __constant struct RenderParams* params,
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
	float dist = hit->distance;
	uint side = hit->side;
	const int x = screenPos.x, y = screenPos.y;
	uint seed = hit->seed;
	const uint voxel = hit->voxel; 
	
	int restirframecount = modulo(params->restirframecount, NUMBEROFTEMPORALFRAMES);
	struct Reservoir* res = &reservoirs[x + y * SCRWIDTH + restirframecount * SCRWIDTH * SCRHEIGHT];

	if (voxel == 0) // exit the scene
	{
		NormalizeReservoir(res);
		return (float4)(0, 0, 0, 1e20f);
	}
	if (IsEmitter(voxel))
	{
		NormalizeReservoir(res);
		return (float4)(ToFloatRGB(voxel) * EmitStrength(voxel), 1e20f);
	}

	const float3 BRDF1 = INVPI * ToFloatRGB(voxel);

	const float3 primaryHitPoint = hit->ray_direction * hit->distance + params->E;
	const float3 D = hit->ray_direction;
	const float3 N = VoxelNormal(side, D);

	const float weight = params->numberOfLights;
	const bool checkVisibility = false;

#if SPATIALDEBUG == 0
	struct Light* light = &lights[res->light_index];
	uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	float3 emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);
	uint emitterVoxel = light->voxel;
#else
	float3 emitterVoxelCoordsf;
	uint emitterVoxel;
	if (res->light_index == 0xffffff)
	{
		return (float4)(1.0, 0.0, 1.0, dist);
	}
	else if (res->light_index == 0xfffff)
	{
		return (float4)(0.0, 1.0, 1.0, dist);
	}
	else
	{
		struct Light* light = &lights[res->light_index];
		uint3 emitterVoxelCoords = indexToCoordinates(light->position);
		emitterVoxelCoordsf = convert_float3(emitterVoxelCoords) + (float3)(0.5, 0.5, 0.5);
		emitterVoxel = light->voxel;
	}
#endif

	const float3 R = emitterVoxelCoordsf - primaryHitPoint;
	const float3 L = normalize(R);
	uint side2;
	float dist2 = 0;
	const uint voxel2 = TraceRay((float4)(primaryHitPoint, 0) + 0.1f * (float4)(N, 0), (float4)(L, 1.0), &dist2, &side2, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
	if (checkVisibility && distance(length(R), dist2) > 0.8)
	{
		res->visible = 0;
		NormalizeReservoir(res);
		return (float4)(0.0, 0.0, 0.0, dist);
	}
	res->visible = 1;

	const float NdotL = max(dot(N, L), 0.0);

	const float3 directContribution = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel) * NdotL / (length(R) * length(R)) * weight;
	const float3 incoming = BRDF1 * directContribution;

	NormalizeReservoir(res);
	return (float4)(incoming, dist);
}

__kernel void renderAlbedo(__global struct CLRay* albedo, __constant struct RenderParams* params,
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
	hit->voxel = voxel;
	hit->voxel_position = 0;
	hit->side = side;
	hit->seed = seed;
	hit->distance = dist;
	hit->ray_direction = D;
}

__kernel void renderRIS(__global struct CLRay* albedo, __global float4* frame, __constant struct RenderParams* params,
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

	float4 pixel = render_di_ris(hit, (int2)(x, y), params, lights, reservoirs, initialSampling, grid, BRICKPARAMS, sky, blueNoise, uberGrid);

	// store pixel in linear color space, to be processed by finalize kernel for TAA
	frame[x + y * SCRWIDTH] = pixel; // depth in w
}