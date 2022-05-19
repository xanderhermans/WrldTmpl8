

void UpdateReservoir(struct Reservoir* _this, const float weight, const float pHat, const uint index, const float r0, const float3 positionOnVoxel, const float invPositionProbability, const float3 Nlight)
{
	_this->streamLength += 1;
	_this->sumOfWeights += weight;
	if (r0 < weight / max(10e-6, _this->sumOfWeights))
	{
		_this->lightIndex = index;
		_this->positionOnVoxel = positionOnVoxel;
		_this->invPositionProbability = invPositionProbability;
		_this->Nlight = Nlight;
		_this->pHat = pHat;
	}
}

void UpdateReservoirSimple(struct Reservoir* _this, const float weight, const float pHat, const uint index, const float r0)
{
	UpdateReservoir(_this, weight, pHat, index, r0, (float3)0, 0, (float3)0);
}

void ReWeighSumOfWeights(struct Reservoir* _this, const float newPHat, const uint streamLength)
{
	float sumOfWeights = _this->adjustedWeight * streamLength * newPHat;
	_this->sumOfWeights = sumOfWeights;
	_this->streamLength = streamLength;
	_this->pHat = newPHat;
}

void CombineReservoir(struct Reservoir* _this, struct Reservoir* _that, const float r0)
{
	uint streamLength1 = _this->streamLength;
	uint streamLength2 = _that->streamLength;

	UpdateReservoir(_this, _that->sumOfWeights, _that->pHat, _that->lightIndex, r0, _that->positionOnVoxel, _that->invPositionProbability, _that->Nlight);
	_this->streamLength = streamLength1 + streamLength2;
	// Note: needs to be followed by AdjustWeight call.
}

void AdjustWeight(struct Reservoir* _this, const float pHat)
{
	_this->adjustedWeight = _this->sumOfWeights / max(10e-6, pHat * _this->streamLength);
}