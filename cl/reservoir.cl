void UpdateReservoir(struct Reservoir* _this, const float weight, const uint index, const float r0)
{
	_this->streamLength += 1;
	_this->sumOfWeights += weight;
	if (r0 < weight / _this->sumOfWeights)
	{
		_this->lightIndex = index;
	}
}

//void Resize(struct Reservoir* _this, const int length)
//{
//	float avgWeight = _this->sumOfWeights / max(1.0, convert_float(_this->streamLength));
//	_this->streamLength = length;
//	_this->sumOfWeights = avgWeight * length;
//}

void ReWeighSumOfWeights(struct Reservoir* _this, const float newPHat, const uint streamLength)
{
	float sumOfWeights = _this->adjustedWeight * streamLength * newPHat;
	_this->sumOfWeights = sumOfWeights;
	_this->streamLength = streamLength;
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