#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace SmokingGuns
{
	class ConfigManager
	{
	public:
		static ConfigManager& GetSingleton();

		void Load();
		void LoadGameData();

		[[nodiscard]] float GetAmmoMult() const;
		[[nodiscard]] float GetWeightMult() const;
		[[nodiscard]] float GetOverallMult() const;
		[[nodiscard]] float GetSmokeDecayImpulse() const;

		[[nodiscard]] float GetAmmoImpulse(const RE::TESAmmo* a_ammo) const;

	private:
		ConfigManager() = default;

		void LoadGeneralConfig();
		void LoadAmmoConfigs();

		float ammoMult{ 1.0f };
		float weightMult{ 1.0f };
		float overallMult{ 1.0f };
		float smokeDecayImpulse{ 3.0f };

		std::unordered_map<std::uint32_t, float> ammoImpulses;
	};
}