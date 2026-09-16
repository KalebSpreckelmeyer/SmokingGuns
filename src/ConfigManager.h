#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "RE/B/BGSMod.h"

namespace SmokingGuns
{
	struct WeaponProfile
	{
		std::vector<RE::BGSMod::Attachment::Mod*> requiredAttachments;
	};

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

		[[nodiscard]] const WeaponProfile* GetWeaponProfile(
			const RE::TESObjectWEAP* a_weapon) const;

	private:
		ConfigManager() = default;

		void LoadGeneralConfig();
		void LoadAmmoConfigs();
		void LoadWeaponConfigs();

		float ammoMult{ 1.0f };
		float weightMult{ 1.0f };
		float overallMult{ 1.0f };
		float smokeDecayImpulse{ 3.0f };

		std::unordered_map<std::uint32_t, float> ammoImpulses;

		std::unordered_map<std::uint32_t, WeaponProfile> weaponProfiles;
	};
}