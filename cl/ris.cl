bool isLightOccluded(const struct Light* light, const float3 shadingPoint,
	const float3 L, float* dist, uint* side,
	__read_only image3d_t grid,
	__global const unsigned char* uberGrid, __global const PAYLOAD* brick0
	, __global struct DebugInfo* debugInfo, const uint x, const uint y
)
{
	const uint3 emitterVoxelCoords = indexToCoordinates(light->position);
	const uint voxel2 = TraceRay((float4)(shadingPoint, 0), (float4)(L, 1.0),
		dist, side, grid, uberGrid, BRICKPARAMS, GRIDWIDTH);
	uint3 hitVoxelCoords = GridCoordinatesFromHit(*side, L, *dist, shadingPoint);

	const uint sizeOfLight = light->size;

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

	struct Reservoir res = { };

	if (voxelValue != 0)
	{
		uint* seedptr = &ray->seed;

		const float3 D = ray->rayDirection;
		const uint side = ray->side;
		const float3 N = VoxelNormal(side, D);
		const float3 brdf = ToFloatRGB(voxelValue) * INVPI;
		const float3 shadingPoint = D * dist + params->E;
		const float3 shadingPointOffset = shadingPoint + 0.1 * N;

		const uint numberOfLights = params->numberOfLights;
		const uint numberOfCandidates = min(numberOfLights, params->numberOfCandidates);

		// BEGIN CANDIDATE SAMPLING
		for (int i = 0; i < numberOfCandidates; i++)
		{
			uint lightIndex;
			float p;
			GenerateLightCandidate(numberOfLights, &p, &lightIndex, seedptr);

			float3 positionOnVoxel; float invVoxelSideProbability; float3 Nlight;
			struct Light* light = &lights[lightIndex];
			PointOnVoxelLight(light, shadingPointOffset, seedptr, &positionOnVoxel,
				&invVoxelSideProbability, &Nlight);

			float pHat = evaluatePHat(positionOnVoxel, Nlight,
				shadingPoint, N, brdf, light->voxel, light->size) * invVoxelSideProbability;
			UpdateReservoir(&res, pHat / p, pHat, lightIndex, RandomFloat(seedptr),
				positionOnVoxel, invVoxelSideProbability, Nlight);
		}

		AdjustWeight(&res, res.pHat);

		// Discard reservoir if occluded
		const struct Light light = lights[res.lightIndex];
		const float3 R = res.positionOnVoxel - shadingPointOffset;
		const float3 L = normalize(R);
		float dist2; uint side2;
		if (isLightOccluded(&light, shadingPointOffset, L, &dist2, &side2, grid, uberGrid,
			brick0, debugInfo, 0, 0))
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
				if (prevSide == side && distance(dist, prevDist) <= 0.1 * dist)
				{
					struct Reservoir prevRes = prevReservoirs[prevxy.x + prevxy.y * SCRWIDTH];
					const uint prevStreamLength = min(params->numberOfMaxTemporalImportance * numberOfCandidates, prevRes.streamLength);
					if (prevStreamLength > 0)
					{
						uint prevLightIndex = prevRes.lightIndex;
						struct Light* prevLight = &lights[prevLightIndex];

						// technically the reprojection will not result in the same shadingPoint but it is close so we could
						// re-use prevRes.pHat to save performance if desired.
						float prev_pHat = evaluatePHat(prevRes.positionOnVoxel, prevRes.Nlight,
							shadingPoint, N, brdf, prevLight->voxel, prevLight->size) * prevRes.invPositionProbability;

						ReWeighSumOfWeights(&prevRes, prev_pHat, prevStreamLength);
						CombineReservoir(&res, &prevRes, RandomFloat(seedptr));

						// already calculated potential contribution. re-use.
						struct Light* light = &lights[res.lightIndex];
						float _pHat = evaluatePHat(res.positionOnVoxel, res.Nlight,
							shadingPoint, N, brdf, light->voxel, light->size) * res.invPositionProbability;

						AdjustWeight(&res, _pHat);
					}
				}
			}
		}
		// END TEMPORAL SAMPLING
	}

	reservoirs[x + y * SCRWIDTH] = res;
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
			_perPixelSpatialResampling(x, y, debugInfo, params, prevAlbedo, albedo, lights,
				&res, prevReservoirs);
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
		if (params->skyDomeSampling) color = SampleSky((float3)(D.x, D.z, D.y), sky, params->skyWidth, params->skyHeight);
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
				const float3 shadingPoint = D * dist + params->E;

				const float3 shadingPointOffset = shadingPoint + 0.1 * N;
				const struct Light light = lights[res.lightIndex];
				const float3 R = res.positionOnVoxel - shadingPointOffset;
				const float3 L = normalize(R);
				float dist2; uint side2;
				// if we hit the emitter we know nothing occluded it
				if (!isLightOccluded(&light, shadingPointOffset, L, &dist2, &side2, grid, uberGrid,
					brick0, debugInfo, x, y))
				{
					const float3 brdf = ToFloatRGB(voxel) * INVPI;
					const float3 incoming0 = evaluatePHatFull(res.positionOnVoxel, res.Nlight,
						shadingPoint, N, brdf, light.voxel, light.size);
					const float3 incoming = incoming0 * res.adjustedWeight * res.invPositionProbability;
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

		//color = ToFloatRGB(voxel);
	}

	//sample sky
	if (params->skyDomeSampling)
	{
		float3 incoming = (float3)(0.0, 0.0, 0.0);
		const float skyLightScale = params->skyLightScale;
		const float r0 = blueNoiseSampler(blueNoise, x, y, params->frame, 0);
		const float r1 = blueNoiseSampler(blueNoise, x, y, params->frame, 1);
		const float4 R = (float4)(DiffuseReflectionCosWeighted(r0, r1, N), 1);
		uint side2;
		float dist2;
		const float3 shadingPoint = D * dist + params->E;
		const float3 shadingPointOffset = shadingPoint + 0.1 * N;
		const uint voxel2 = TraceRay((float4)(shadingPointOffset, 0), R, &dist2, &side2, grid, uberGrid, BRICKPARAMS, GRIDWIDTH / 12);
		const float3 N2 = VoxelNormal(side2, R.xyz);
		float3 toAdd = (float3)skyLightScale, M = N;
		if (voxel2 != 0) toAdd *= INVPI * ToFloatRGB(voxel2), M = N2;
		float4 sky;
		if (M.x < -0.9f) sky = params->skyLight[0];
		if (M.x > 0.9f) sky = params->skyLight[1];
		if (M.y < -0.9f) sky = params->skyLight[2];
		if (M.y > 0.9f) sky = params->skyLight[3];
		if (M.z < -0.9f) sky = params->skyLight[4];
		if (M.z > 0.9f) sky = params->skyLight[5];
		incoming += toAdd * sky.xyz;

		const float3 brdf = ToFloatRGB(voxel) * INVPI;
		color += incoming * brdf;
	}

	//if (x == DBX && y == DBY) color = DEBUGCOLOR;
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
	const uint voxel = TraceRay((float4)(params->E, 0), (float4)(D, 1), &dist, &side, grid,
		uberGrid, BRICKPARAMS, 999999 /* no cap needed */);

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
	float4 pixel = render_di_ris(debugInfo, hit, (int2)(x, y), params, lights, reservoirs,
		prevReservoirs, grid, BRICKPARAMS, sky, blueNoise, uberGrid);

	// store pixel in linear color space, to be processed by finalize kernel for TAA
	frame[x + y * SCRWIDTH] = pixel; // depth in w
	reservoirs[x + y * SCRWIDTH] = prevReservoirs[x + y * SCRWIDTH];
}