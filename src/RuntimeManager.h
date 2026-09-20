#pragma once

#include <RE/Fallout.h>

#include <atomic>
#include <cstdint>

namespace SmokingGuns
{
	class RuntimeManager
	{
	public:
		static RuntimeManager& GetSingleton();

		void InstallUpdateHook();
		void Update();
		void OnSaveLoaded();

		bool OnWeaponEquipped(RE::TESObjectWEAP* a_weapon);

	private:
		RuntimeManager() = default;

		RE::TESObjectWEAP* ResolveEquippedWeapon(
			RE::PlayerCharacter* a_player) const;

		enum class GraphRefreshStage
		{
			kIdle,
			kClear,
			kSet
		};

		std::atomic<std::uint64_t> saveLoadGeneration{ 0 };
		std::uint64_t processedLoadGeneration{ 0 };
		GraphRefreshStage firstPersonRefresh{ GraphRefreshStage::kIdle };
		GraphRefreshStage thirdPersonRefresh{ GraphRefreshStage::kIdle };

		std::uint32_t observedWeaponFormID{ 0 };
		const RE::NiAVObject* observedFirstPersonRoot{ nullptr };
		const RE::NiAVObject* observedThirdPersonRoot{ nullptr };

		std::uint64_t updateCount{ 0 };
		bool forceReconcile{ false };
		bool hookInstalled{ false };
	};
}
