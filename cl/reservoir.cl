void UpdateReservoir(struct Reservoir* _this, const float weight, const uint index, const float r0)
{
	_this->streamLength += 1;
	_this->sumOfWeights += weight;
	if (r0 < weight / _this->sumOfWeights)
	{
		_this->lightIndex = index;
	}
}

void CombineReservoir(struct Reservoir* _this, struct Reservoir* _that, const float r0)
{
	uint streamLength1 = _this->streamLength;
	uint streamLength2 = _that->streamLength;

	UpdateReservoir(_this, _that->sumOfWeights, _that->lightIndex, r0);
	_this->streamLength = streamLength1 + streamLength2;
	// Note: needs to be followed by AdjustWeight call.
}

void AdjustWeight(struct Reservoir* _this, const float pHat)
{
	_this->adjustedWeight = _this->sumOfWeights / max(10e-6, pHat * _this->streamLength);
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