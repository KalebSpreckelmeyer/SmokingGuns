#pragma once

namespace SmokingGuns
{
	struct SmokeCalculationResult
	{
		float weightImpulse{ 0.0f };
		float ammoComponent{ 0.0f };
		float weightComponent{ 0.0f };
		float smokeImpulse{ 0.0f };
	};

	class SmokeCalculation
	{
	public:
		static SmokeCalculationResult Calculate(
			float a_ammoImpulse,
			float a_weaponWeight,
			float a_ammoMult,
			float a_weightMult,
			float a_overallMult);
	};
}