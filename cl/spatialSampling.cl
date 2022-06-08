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
	const float3 shadingPointOffset = shadingPoint + 0.1 * N;

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

		if (neighbourRes.streamLength > 0)
		{
			const struct CLRay* neigbourRay = &albedo[neighbourIndex];
			const uint neighbourSide = neigbourRay->side;
			const float neighbourDist = neigbourRay->distance;
			if (neighbourSide == side && distance(dist, neighbourDist) <= 0.1 * dist)
			{
				// use the world coordinate?
				//const float3 neigbourShadingPoint = ShadingPoint(neigbourRay, params->E);

				//re-evaluate sum of weights with new phat because the shading point is different.
				uint neighbourLightIndex = neighbourRes.lightIndex;
				struct Light* neighbourLight = &lights[neighbourLightIndex];

				float pHat = evaluatePHat(neighbourRes.positionOnVoxel, neighbourRes.Nlight,
					shadingPoint, N, brdf, neighbourLight->voxel, neighbourLight->size) * neighbourRes.invPositionProbability;
				//float pHat = evaluatePHat(neighbourRes.positionOnVoxel, neighbourRes.Nlight,
				//	shadingPoint, N, brdf, neighbourLight->voxel, neighbourLight->size);
				
				ReWeighSumOfWeights(&neighbourRes, pHat, neighbourRes.streamLength);
				CombineReservoir(res, &neighbourRes, RandomFloat(seedptr));
			}
		}
	}

	AdjustWeight(res, res->pHat);
}