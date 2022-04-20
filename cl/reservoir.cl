void UpdateReservoir(struct Reservoir* _this, const float weight, const uint index, float r0)
{
	
}

void SpatialResampling(struct Reservoir* res, const int x, const int y, const int frame, uint* seedptr, __global struct Reservoir* reservoirs)
{
	const int radius = SPATIALRADIUS;
	for (int k = 0; k < SPATIALTAPS; k++)
	{
		for (int i = y - radius; i < y + radius; i++)
		{
			for (int j = x; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j--)
			{
				if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
				{
					//struct Reservoir* res1 = &reservoirs[j + i * SCRWIDTH + frame * SCRWIDTH * SCRHEIGHT];
					//float r0 = RandomFloat(seedptr);
					//UpdateReservoirWithReservoir(res, res1, r0);
				}
			}
			for (int j = x + 1; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j++)
			{
				if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
				{
					//struct Reservoir* res1 = &reservoirs[j + i * SCRWIDTH + frame * SCRWIDTH * SCRHEIGHT];
					//float r0 = RandomFloat(seedptr);
					//UpdateReservoirWithReservoir(res, res1, r0);
				}
			}
		}
	}
}