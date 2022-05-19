// Direct light contribution calculation
float3 evaluatePHatFull(
	const float3 pointOnLight, const float3 Nlight,
	const float3 shadingPoint, const float3 N, 
	const float3 brdf, const uint voxelEmitterValue, const uint emitterSize)
{
	const float size = convert_float(emitterSize);
	float3 emission = ToFloatRGB(voxelEmitterValue) * EmitStrength(voxelEmitterValue);

#if GFXEXP == 1
	emission *= INVPI;
#endif
	
	const float3 R = pointOnLight - shadingPoint;
	const float3 L = normalize(R);
	const float NdotL = max(dot(N, L), 0.0);
	const float distsquared = max(10e-6, dot(R, R));

	const float area = size * size;
#if VOXELSAREPOINTLIGHTS == 0
	const float NlightDotL = max(dot(Nlight, -L), 0.0);
#else
	const float NlightDotL = 1.0;
#endif

	const float solidAngle = (NlightDotL * area) / distsquared;
	const float3 directLightContribution = brdf * emission * NdotL * solidAngle;
	return directLightContribution;
}

// Direct light contribution scalar given a point on a light and the shading point.
float evaluatePHat(
	const float3 pointOnLight, const float3 Nlight,
	const float3 shadingPoint, const float3 N,
	const float3 brdf, const uint voxelEmitterValue, const uint emitterSize)
{
	const float3 pHat = evaluatePHatFull(pointOnLight, Nlight,
		shadingPoint, N, brdf, voxelEmitterValue, emitterSize);
	const float result = length(pHat);
	if (isnan(result) > 0.0 || isinf(result) > 0.0) return 0.0;
	return result;
}