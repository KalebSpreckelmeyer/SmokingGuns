#include "RuntimeManager.h"

#include "ConfigManager.h"
#include "RuntimeAttachment.h"
#include "SmokeFollowTest.h"

#include <memory>

namespace SmokingGuns
{
	namespace
	{
		constexpr std::uint64_t kPeriodicReconcileFrames = 15;

		struct PlayerUpdateHook
		{
			static void thunk(
				RE::PlayerCharacter* a_player,
				float a_delta)
			{
				func(a_player, a_delta);

				RuntimeManager::GetSingleton().Update();
				SmokeFollowTest::OnPlayerUpdate();
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	RuntimeManager& RuntimeManager::GetSingleton()
	{
		static RuntimeManager singleton;
		return singleton;
	}

	void RuntimeManager::InstallUpdateHook()
	{
		if (hookInstalled) {
			return;
		}

		REL::Relocation<std::uintptr_t> vtable{
			RE::PlayerCharacter::VTABLE[0]
		};

		PlayerUpdateHook::func = vtable.write_vfunc(
			0xCF,
			PlayerUpdateHook::thunk);

		hookInstalled = true;

		REX::INFO(
			"[Smoking Guns][RuntimeManager] "
			"Player update hook installed");
	}

	bool RuntimeManager::OnWeaponEquipped(
		RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return false;
		}

		forceReconcile = true;

		const auto* profile =
			ConfigManager::GetSingleton().GetWeaponProfile(a_weapon);

		REX::INFO(
			"[Smoking Guns][RuntimeManager] "
			"Equip hint for {:08X}; profile={}",
			a_weapon->GetFormID(),
			profile ? "FOUND" : "NOT FOUND");

		return profile != nullptr;
	}

	RE::TESObjectWEAP* RuntimeManager::ResolveEquippedWeapon(
		RE::PlayerCharacter* a_player) const
	{
		if (!a_player) {
			return nullptr;
		}

		RE::BGSEquipIndex equipIndex{};
		equipIndex.index = 0;

		RE::BGSObjectInstance equipped{ nullptr, nullptr };

		const auto* result = a_player->GetEquippedItem(
			std::addressof(equipped),
			equipIndex);

		if (!result || !equipped.object) {
			return nullptr;
		}

		return equipped.object->As<RE::TESObjectWEAP>();
	}

	void RuntimeManager::Update()
	{
		++updateCount;

		if (!forceReconcile &&
			(updateCount % kPeriodicReconcileFrames) != 0) {

			return;
		}

		forceReconcile = false;

		auto* player = RE::PlayerCharacter::GetSingleton();

		if (!player) {
			return;
		}

		auto* weapon = ResolveEquippedWeapon(player);

		if (!weapon) {
			observedWeaponFormID = 0;
			observedFirstPersonRoot = nullptr;
			observedThirdPersonRoot = nullptr;
			return;
		}

		const auto* profile =
			ConfigManager::GetSingleton().GetWeaponProfile(weapon);

		if (!profile || profile->effects.empty()) {
			observedWeaponFormID = weapon->GetFormID();
			observedFirstPersonRoot = player->Get3D(true);
			observedThirdPersonRoot = player->Get3D(false);
			return;
		}

		auto* firstPersonRoot = player->Get3D(true);
		auto* thirdPersonRoot = player->Get3D(false);

		const bool weaponChanged =
			observedWeaponFormID != weapon->GetFormID();

		const bool firstPersonRootChanged =
			observedFirstPersonRoot != firstPersonRoot;

		const bool thirdPersonRootChanged =
			observedThirdPersonRoot != thirdPersonRoot;

		const bool firstPersonMissing =
			firstPersonRoot &&
			!RuntimeAttachment::HasRequiredNodes(
				firstPersonRoot,
				*profile);

		const bool thirdPersonMissing =
			thirdPersonRoot &&
			!RuntimeAttachment::HasRequiredNodes(
				thirdPersonRoot,
				*profile);

		if (weaponChanged ||
			firstPersonRootChanged ||
			thirdPersonRootChanged ||
			firstPersonMissing ||
			thirdPersonMissing) {

			REX::DEBUG(
				"[Smoking Guns][RuntimeManager] "
				"Reconciling {:08X}: weaponChanged={}, "
				"rootsChanged=({}, {}), missing=({}, {})",
				weapon->GetFormID(),
				weaponChanged,
				firstPersonRootChanged,
				thirdPersonRootChanged,
				firstPersonMissing,
				thirdPersonMissing);

			if (firstPersonRoot &&
				(firstPersonRootChanged || firstPersonMissing || weaponChanged)) {

				(void)RuntimeAttachment::ReconcileTree(
					firstPersonRoot,
					*profile,
					"1P");
			}

			if (thirdPersonRoot &&
				(thirdPersonRootChanged || thirdPersonMissing || weaponChanged)) {

				(void)RuntimeAttachment::ReconcileTree(
					thirdPersonRoot,
					*profile,
					"3P");
			}
		}

		observedWeaponFormID = weapon->GetFormID();
		observedFirstPersonRoot = firstPersonRoot;
		observedThirdPersonRoot = thirdPersonRoot;
	}
}
