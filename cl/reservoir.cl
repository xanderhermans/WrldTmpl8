void UpdateReservoir(struct Reservoir* _this, const float weight, const uint index, float r0)
{
	_this->sumOfWeights += weight;
	_this->streamLength += 1;
	if (r0 < weight / _this->sumOfWeights)
	{
		_this->light_index = index;
	}
}

void NormalizeReservoir(struct Reservoir* _this)
{
	_this->sumOfWeights = _this->sumOfWeights / convert_float(_this->streamLength);
	_this->streamLength = 1;
}

void UpdateReservoirWithReservoir(struct Reservoir* _this, const struct Reservoir* other, float r0)
{
	if (other->visible == 1)
	{
		if (_this->visible == 0)
		{
			_this->sumOfWeights = other->sumOfWeights;
			_this->streamLength = other->streamLength;
			_this->light_index = other->light_index;
#if SPATIALDEBUG == 1
			_this->light_index = 0xffffff;
#endif
		}
		else
		{
			_this->sumOfWeights += other->sumOfWeights;
			_this->streamLength += other->streamLength;
			if (r0 < other->sumOfWeights / _this->sumOfWeights)
				//if (r0 < 1.0 / 1000.0)
			{
				_this->light_index = other->light_index;
#if SPATIALDEBUG == 1
				_this->light_index = 0xfffff;
#endif
			}
		}
	}
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
					struct Reservoir* res1 = &reservoirs[j + i * SCRWIDTH + frame * SCRWIDTH * SCRHEIGHT];
					float r0 = RandomFloat(seedptr);
					UpdateReservoirWithReservoir(res, res1, r0);
				}
			}
			for (int j = x + 1; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j++)
			{
				if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
				{
					struct Reservoir* res1 = &reservoirs[j + i * SCRWIDTH + frame * SCRWIDTH * SCRHEIGHT];
					float r0 = RandomFloat(seedptr);
					UpdateReservoirWithReservoir(res, res1, r0);
				}
			}
		}
	}
}