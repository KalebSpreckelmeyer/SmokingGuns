#pragma once

#include <RE/Fallout.h>

#include <cstdint>

namespace SmokingGuns
{
	class RuntimeManager
	{
	public:
		static RuntimeManager& GetSingleton();

		void InstallUpdateHook();
		void Update();

		bool OnWeaponEquipped(RE::TESObjectWEAP* a_weapon);

	private:
		RuntimeManager() = default;

		RE::TESObjectWEAP* ResolveEquippedWeapon(
			RE::PlayerCharacter* a_player) const;

		std::uint32_t observedWeaponFormID{ 0 };
		const RE::NiAVObject* observedFirstPersonRoot{ nullptr };
		const RE::NiAVObject* observedThirdPersonRoot{ nullptr };

		std::uint64_t updateCount{ 0 };
		bool forceReconcile{ false };
		bool hookInstalled{ false };
	};
}
