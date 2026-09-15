#include "SmokeCalculation.h"

#include <algorithm>

namespace SmokingGuns
{
	SmokeCalculationResult SmokeCalculation::Calculate(
		float a_ammoImpulse,
		float a_weaponWeight,
		float a_ammoMult,
		float a_weightMult,
		float a_overallMult)
	{
		SmokeCalculationResult result{};

		// Treat a 5 lb weapon as a weight impulse of 1.0.
		//
		// This keeps raw Fallout weapon weight from overwhelming
		// the ammo component.
		constexpr float referenceWeight = 5.0f;

		result.weightImpulse =
			std::max(a_weaponWeight, 0.0f) /
			referenceWeight;

		result.ammoComponent =
			std::max(a_ammoImpulse, 0.0f) *
			a_ammoMult;

		result.weightComponent =
			result.weightImpulse *
			a_weightMult;

		result.smokeImpulse =
			(result.ammoComponent +
				result.weightComponent) *
			a_overallMult;

		result.smokeImpulse =
			std::max(result.smokeImpulse, 0.0f);

		return result;
	}
}