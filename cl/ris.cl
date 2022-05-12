void _perPixelSpatialResampling(const uint x, const uint y,
	__global struct DebugInfo* debugInfo,
	__constant struct RenderParams* params,
	__global struct CLRay* prevAlbedo, __global struct CLRay* albedo,
	__global struct Light* lights,
	struct Reservoir* res, struct Reservoir* prevReservoirs);

void generateLightCandidate(const uint numberOfLights, float* probability, uint* index, uint* seedptr)
{
	*index = getRandomIndex(numberOfLights, seedptr);
	// pdf of the light when uniformly sample is 1.0 / number of lights.
	*probability = 1.0 / numberOfLights;
}

void pointOnVoxelLight(struct Light* light, 
	float3 shadingPoint, uint* seedptr,
	float3* positionOnVoxel, float* invPositionProbability)
{
	const int sizeOfLight = light->size;
	// take middle of voxel
	uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	const float3 center = convert_float3(emitterVoxelCoords) + (float3)(0.5 * sizeOfLight, 0.5 * sizeOfLight, 0.5 * sizeOfLight);

#if VOXELSAREPOINTLIGHTS == 1
	int innerSizeOfLight = sizeOfLight - 2;
	int numberOfOutsideVoxels = sizeOfLight * sizeOfLight * sizeOfLight - innerSizeOfLight * innerSizeOfLight * innerSizeOfLight;
	// the lights are all cube shaped so voxels that are not on the outside will never contribute and therefore have pdf = 0
	*invPositionProbability = max(0.0, convert_float(numberOfOutsideVoxels));
	//*invPositionProbability = 1;
	*positionOnVoxel = center;
#else
	const float3 L = normalize(center - shadingPoint);
	*positionOnVoxel = RandomPointOnVoxel(center, L, sizeOfLight, seedptr, invPositionProbability);
#endif
}

// Direct light contribution scalar given a point on a light and the shading point.
float evaluatePHat(const struct Light* light, const float3 pointOnLight, 
	const float invPositionProbability, 
	float3 N, float3 brdf, float3 shadingPoint)
{
	const float3 R = pointOnLight - shadingPoint;
	const float3 L = normalize(R);

	const float NdotL = max(dot(N, L), 0.0);
	const float distsquared = max(0.0, dot(R, R));

	const uint emitterVoxel = light->voxel;
	const float3 lightIntensity = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel);

	const float pHat = length((brdf * lightIntensity * NdotL) / distsquared * invPositionProbability);
	//const float pHat = length((brdf * lightIntensity * NdotL) / distsquared);
	return pHat;
}

bool isLightOccluded(const struct Light* light, const float3 shadingPoint,
	const float3 L, float* dist, uint* side,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
	, __global struct DebugInfo* debugInfo, const uint x, const uint y
)
{
	const uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	const uint voxel2 = TraceRay((float4)(shadingPoint, 0), (float4)(L, 1.0), dist, side, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
	uint3 hitVoxelCoords = GridCoordinatesFromHit(*side, L, *dist, shadingPoint);
	
	const uint sizeOfLight = light->size;
	if (x == 500 && y == 500)
	{
		debugInfo->f1.w = sizeOfLight;
		debugInfo->f1.xyz = convert_float3(hitVoxelCoords);
		debugInfo->f2.xyz = convert_float3(emitterVoxelCoords);
		debugInfo->f2.w = convert_float(Uint3CubeEquals(emitterVoxelCoords, hitVoxelCoords, sizeOfLight));
	}

	// if we hit the emitter we know nothing occluded it
	return !Uint3CubeEquals(emitterVoxelCoords, hitVoxelCoords, sizeOfLight);
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

			float3 positionOnVoxel; float invPositionProbability;
			struct Light* light = &lights[lightIndex];
			pointOnVoxelLight(light, shadingPoint, seedptr, &positionOnVoxel, &invPositionProbability);

			float pHat = evaluatePHat(light, positionOnVoxel, invPositionProbability, N, brdf, shadingPoint);
			UpdateReservoir(&res, pHat / p, lightIndex, RandomFloat(seedptr), positionOnVoxel, invPositionProbability);
		}

		const uint lightIndex = res.lightIndex;
		struct Light* light = &lights[lightIndex];
		const uint sizeOfLight = light->size;
		float pHat = evaluatePHat(light, res.positionOnVoxel, res.invPositionProbability, N, brdf, shadingPoint);
		AdjustWeight(&res, pHat);

		if (x == 500 && y == 500)
		{
			float3 positionOnVoxel1; float invPositionProbability1;
			float3 positionOnVoxel2; float invPositionProbability2;
			struct Light light1 = { light->position, light->voxel, 1 };
			struct Light light2 = { light->position, light->voxel, 8 };
			uint _seed = *seedptr;
			uint seed = _seed;
			pointOnVoxelLight(&light1, shadingPoint, &seed, &positionOnVoxel1, &invPositionProbability1);
			float pHat1 = evaluatePHat(&light1, positionOnVoxel1, invPositionProbability1, N, brdf, shadingPoint);
			seed = _seed;
			pointOnVoxelLight(&light2, shadingPoint, &seed, &positionOnVoxel2, &invPositionProbability2);
			float pHat2 = evaluatePHat(&light2, positionOnVoxel2, invPositionProbability2, N, brdf, shadingPoint);

			float aw1 = res.sumOfWeights / max(10e-6, pHat1 * res.streamLength);
			float aw2 = res.sumOfWeights / max(10e-6, pHat2 * res.streamLength);
			debugInfo->f3.x = pHat1;
			debugInfo->f3.y = aw1;
			debugInfo->f3.w = invPositionProbability1;
			debugInfo->f4.x = pHat2;
			debugInfo->f4.y = aw2;
			debugInfo->f4.w = invPositionProbability2;
			debugInfo->res = res;
		}

		const float3 shadingPointOffset = shadingPoint + 0.1 * N;

		const float3 R = res.positionOnVoxel - shadingPointOffset;
		const float3 L = normalize(R);
		float dist2; uint side2;
		if (isLightOccluded(light, shadingPointOffset, L, &dist2, &side2, grid, uberGrid, brick0, debugInfo, 0, 0))
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
					uint prevLightIndex = prevRes.lightIndex;
					struct Light* prevLight = &lights[prevLightIndex];

					float prev_pHat = evaluatePHat(prevLight, prevRes.positionOnVoxel, prevRes.invPositionProbability, N, brdf, shadingPoint);
					ReWeighSumOfWeights(&prevRes, prev_pHat, prevStreamLength);

					CombineReservoir(&res, &prevRes, RandomFloat(seedptr));

					// already calculated potential contribution. re-use.
					float _pHat = res.lightIndex == lightIndex ? pHat : prev_pHat;

					AdjustWeight(&res, _pHat);

					//struct Reservoir prevRes = prevReservoirs[prevxy.x + prevxy.y * SCRWIDTH];
					//const uint prevStreamLength = min(params->numberOfMaxTemporalImportance * numberOfCandidates, prevRes.streamLength);
					//uint prevLightIndex = prevRes.lightIndex;
					//struct Light* prevLight = &lights[prevLightIndex];

					//float3 positionOnVoxel; float invPositionProbability;
					//const uint sizeOfLight = light->size;
					//pointOnVoxelLight(prevLight, shadingPoint, seedptr, &positionOnVoxel, &invPositionProbability);

					//float prev_pHat = evaluatePHat(prevLight, positionOnVoxel, invPositionProbability, N, brdf, shadingPoint);//ReWeighSumOfWeights(&prevRes, prev_pHat, prevStreamLength);
					//ReWeighSumOfWeights(&prevRes, prev_pHat, prevStreamLength);
					//CombineReservoir(&res, &prevRes, RandomFloat(seedptr));

					//// already calculated potential contribution. re-use.
					//float _pHat = res.lightIndex == lightIndex ? pHat : prev_pHat;

					//AdjustWeight(&res, _pHat);
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
			uint neighbourLightIndex = neighbourRes.lightIndex;
			struct Light* neighbourLight = &lights[neighbourLightIndex];
			float new_pHat = evaluatePHat(neighbourLight, neighbourRes.positionOnVoxel, neighbourRes.invPositionProbability, N, brdf, shadingPoint);

			ReWeighSumOfWeights(&neighbourRes, new_pHat, neighbourRes.streamLength);
			//ReWeighSumOfWeights(&neighbourRes, new_pHat, 32);

			CombineReservoir(res, &neighbourRes, RandomFloat(seedptr));
		}
	}

	uint lightIndex = res->lightIndex;
	struct Light* light = &lights[lightIndex];
	float pHat = evaluatePHat(light, res->positionOnVoxel, res->invPositionProbability, N, brdf, shadingPoint);
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

				const float3 shadingPointOffset = shadingPoint + 0.1 * N;
				float weight = res.adjustedWeight * res.invPositionProbability;

				const float3 R = res.positionOnVoxel - shadingPointOffset;
				const float3 L = normalize(R);
				float dist = dot(res.positionOnVoxel - shadingPoint, res.positionOnVoxel - shadingPoint);
				float dist2; uint side2;

				// if we hit the emitter we know nothing occluded it
				if (!isLightOccluded(&light, shadingPointOffset, L, &dist2, &side2, grid, uberGrid, brick0, debugInfo, x, y))
				{
					const float NdotL = max(dot(N, L), 0.0);
					uint emitterVoxel = light.voxel;
					float3 emitterColor = ToFloatRGB(emitterVoxel) * EmitStrength(emitterVoxel);
#if GFXEXP == 1
					emitterColor *= INVPI;
#endif

					float distancesquared = max(dist, 10e-6);
					const float3 directContribution = ((emitterColor * NdotL) / distancesquared) * weight;
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

	if (x == 500 && y == 500) color = (float3)(1.0, 0.0, 0.0);
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
}