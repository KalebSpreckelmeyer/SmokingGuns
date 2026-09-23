#include "ConfigManager.h"
#include "GraphManager.h"
#include "RuntimeManager.h"
#include "SmokeCalculation.h"
#include <variant>

namespace Papyrus
{
	bool ReportEquipped(std::monostate, RE::TESForm* a_form)
	{
		if (!a_form) {
			REX::WARN("[Smoking Guns] ReportEquipped received a null form");
			return false;
		}

		//
		// Step 1:
		// Ignore equip events that aren't weapons.
		//
		auto* eventWeapon =
			a_form->As<RE::TESObjectWEAP>();

		if (!eventWeapon) {
			REX::DEBUG(
				"[Smoking Guns] Ignoring non-weapon equip event: {:08X}",
				a_form->GetFormID());

			return false;
		}

		//
		// Step 2:
		// Acquire the player.
		//
		auto* player =
			RE::PlayerCharacter::GetSingleton();

		if (!player) {
			REX::ERROR(
				"[Smoking Guns] Could not acquire PlayerCharacter");

			return false;
		}

		//
		// Step 3:
		// Resolve the weapon instance that is actually equipped.
		//
		RE::BGSEquipIndex equipIndex{};
		equipIndex.index = 0;

		RE::BGSObjectInstance equipped{
			nullptr,
			nullptr
		};

		auto* result =
			player->GetEquippedItem(
				&equipped,
				equipIndex);

		if (!result || !equipped.object) {
			REX::WARN(
				"[Smoking Guns] Equip event {:08X}, "
				"but GetEquippedItem returned nothing",
				eventWeapon->GetFormID());

			return false;
		}

		auto* equippedWeapon =
			equipped.object->As<RE::TESObjectWEAP>();

		if (!equippedWeapon) {
			REX::WARN(
				"[Smoking Guns] Equipped item was not a weapon");

			return false;
		}

		//
		// This is useful for spotting equip-timing problems later.
		//
		if (equippedWeapon->GetFormID() !=
			eventWeapon->GetFormID()) {

			REX::WARN(
				"[Smoking Guns] Equip event/current weapon mismatch: "
				"event={:08X}, current={:08X}",
				eventWeapon->GetFormID(),
				equippedWeapon->GetFormID());

			return false;
		}

		SmokingGuns::RuntimeManager::GetSingleton()
			.OnWeaponEquipped(equippedWeapon);

		//
		// Step 4:
		// Check framework compatibility BEFORE doing ammo,
		// weight, configuration or smoke calculation work.
		//
		const auto graphStatus =
			SmokingGuns::GraphManager::DetectFramework(
				player);

		if (graphStatus.firstPersonFound) {
			REX::INFO(
				"[Smoking Guns] 1P SG graph detected: "
				"SG_FrameworkVersion={}",
				graphStatus.firstPersonVersion);
		}
		else {
			REX::DEBUG(
				"[Smoking Guns] No SG framework variable found "
				"on 1P weapon graph");
		}

		if (graphStatus.thirdPersonFound) {
			REX::INFO(
				"[Smoking Guns] 3P SG graph detected: "
				"SG_FrameworkVersion={}",
				graphStatus.thirdPersonVersion);
		}
		else {
			REX::DEBUG(
				"[Smoking Guns] No SG framework variable found "
				"on 3P weapon graph");
		}

		if (!graphStatus.IsCompatible()) {
			REX::INFO(
				"[Smoking Guns] Weapon {:08X} is not currently "
				"recognized as Smoking Guns compatible",
				equippedWeapon->GetFormID());

			return false;
		}

		REX::INFO(
			"[Smoking Guns] Weapon {:08X} is compatible with "
			"Smoking Guns framework v1",
			equippedWeapon->GetFormID());

		//
		// Step 5:
		// Look up the Smoking Guns weapon profile.
		//
		const auto& config =
			SmokingGuns::ConfigManager::GetSingleton();

		const auto* weaponProfile =
			config.GetWeaponProfile(
				equippedWeapon);

		if (!weaponProfile) {
			REX::WARN(
				"[Smoking Guns] Compatible weapon {:08X} "
				"has no weapon profile",
				equippedWeapon->GetFormID());
		}
		else {
			REX::INFO(
				"[Smoking Guns] Weapon profile found: {} runtime effects",
				weaponProfile->effects.size());

			for (const auto& effect : weaponProfile->effects) {
				REX::DEBUG(
					"[Smoking Guns] Runtime effect: '{}' -> '{}'",
					effect.attachPoint +
						(effect.instance.empty() ? std::string{} :
							"." + effect.instance),
					effect.nifPath);
			}
		}

		//
		// Step 6:
		// Instance Data
		// 

		auto* instanceData =
			equipped.instanceData.get();

		if (!instanceData) {
			REX::WARN(
				"[Smoking Guns] Equipped weapon {:08X} "
				"had no instance data",
				equippedWeapon->GetFormID());

			return false;
		}

		auto* weaponData =
			static_cast<
			RE::TESObjectWEAP::InstanceData*>(
				instanceData);

		//
		// Step 8:
		// Read the resolved ammo and look up its configured impulse.
		//
		auto* instanceAmmo =
			weaponData->ammo;

		const auto instanceAmmoFormID =
			instanceAmmo ?
			instanceAmmo->GetFormID() :
			0;

		const float ammoImpulse =
			config.GetAmmoImpulse(
				instanceAmmo);

		REX::INFO(
			"[Smoking Guns] Equipped weapon data: "
			"base={:08X}, "
			"weight={:.3f}, "
			"ammo={:08X}, "
			"ammoImpulse={:.3f}, "
			"capacity={}",
			equippedWeapon->GetFormID(),
			weaponData->weight,
			instanceAmmoFormID,
			ammoImpulse,
			weaponData->ammoCapacity);

		//
		// Step 9:
		// Calculate SmokeImpulse and write it to the weapon graphs.
		//
		const auto smokeCalculation =
			SmokingGuns::SmokeCalculation::Calculate(
				ammoImpulse,
				weaponData->weight,
				config.GetAmmoMult(),
				config.GetWeightMult(),
				config.GetReferenceWeight(),
				config.GetOverallMult());

		REX::INFO(
			"[Smoking Guns] Smoke calculation: "
			"AmmoImpulse={:.3f}, "
			"Weight={:.3f}, "
			"ReferenceWeight={:.3f}, "
			"WeightImpulse={:.3f}, "
			"AmmoComponent={:.3f}, "
			"WeightComponent={:.3f}, "
			"OverallMult={:.3f}, "
			"SmokeImpulse={:.3f}",
			ammoImpulse,
			weaponData->weight,
			config.GetReferenceWeight(),
			smokeCalculation.weightImpulse,
			smokeCalculation.ammoComponent,
			smokeCalculation.weightComponent,
			config.GetOverallMult(),
			smokeCalculation.smokeImpulse);

		const auto smokeWrite =
			SmokingGuns::GraphManager::SetFloatVariable(
				player,
				"SmokeImpulse",
				smokeCalculation.smokeImpulse);

		REX::INFO(
			"[Smoking Guns] SmokeImpulse={:.3f} "
			"write result: 1P={}, 3P={}",
			smokeCalculation.smokeImpulse,
			smokeWrite.firstPersonWritten,
			smokeWrite.thirdPersonWritten);

		//
		// Step 10:
		// SmokeDecayImpulse already has settled semantics,
		// so write it directly from the general config.
		//
		const float smokeDecayImpulse =
			config.GetSmokeDecayImpulse();

		const auto decayWrite =
			SmokingGuns::GraphManager::SetFloatVariable(
				player,
				"SmokeDecayImpulse",
				smokeDecayImpulse);

		REX::INFO(
			"[Smoking Guns] SmokeDecayImpulse={:.3f} "
			"write result: 1P={}, 3P={}",
			smokeDecayImpulse,
			decayWrite.firstPersonWritten,
			decayWrite.thirdPersonWritten);

		//
		// Temporary diagnostic readback.
		//
		float decay1P = 0.0f;
		float decay3P = 0.0f;

		const auto decayRead =
			SmokingGuns::GraphManager::ReadFloatVariable(
				player,
				"SmokeDecayImpulse",
				decay1P,
				decay3P);

		REX::INFO(
			"[Smoking Guns] SmokeDecayImpulse readback: "
			"1P={} value={:.3f}, "
			"3P={} value={:.3f}",
			decayRead.firstPersonWritten,
			decay1P,
			decayRead.thirdPersonWritten,
			decay3P);

		return true;
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			REX::ERROR("[Smoking Guns] Papyrus VM was null");
			return false;
		}

		REX::INFO("[Smoking Guns] Binding ReportEquipped");

		a_vm->BindNativeMethod(
			"SGNative",
			"ReportEquipped",
			ReportEquipped);

		REX::INFO("[Smoking Guns] Papyrus functions registered");
		return true;
	}
}

namespace
{
	void F4SEAPI MessageHandler(
		F4SE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}

		switch (a_message->type) {
		case F4SE::MessagingInterface::kPostLoadGame:
			SmokingGuns::RuntimeManager::GetSingleton()
				.OnSaveLoaded();
			break;

		case F4SE::MessagingInterface::kGameDataReady:
			REX::INFO("[Smoking Guns] Received GameDataReady");

			SmokingGuns::ConfigManager::GetSingleton()
				.LoadGameData();

			break;

		default:
			break;
		}
	}
}

//
// Classic F4SE query export.
// Required so old-gen F4SE 0.6.23 recognizes the DLL.
//
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(
	const F4SE::QueryInterface* a_f4se,
	F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "SmokingGuns";
	a_info->version = 1;

	// Smoking Guns is a game-runtime plugin, not a CK plugin.
	if (a_f4se->IsEditor()) {
		return false;
	}

	return true;
}


//
// Shared load entry point.
//
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(
	const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	REX::INFO("[Smoking Guns] Plugin loaded");

	SmokingGuns::ConfigManager::GetSingleton().Load();

	SmokingGuns::RuntimeManager::GetSingleton()
		.InstallUpdateHook();

	const auto messaging = F4SE::GetMessagingInterface();

	if (!messaging) {
		REX::ERROR("[Smoking Guns] Failed to acquire messaging interface");
		return false;
	}

	if (!messaging->RegisterListener(MessageHandler)) {
		REX::ERROR("[Smoking Guns] Failed to register messaging listener");
		return false;
	}

	REX::INFO("[Smoking Guns] Messaging listener registered");

	const auto papyrus = F4SE::GetPapyrusInterface();

	if (!papyrus) {
		REX::ERROR("[Smoking Guns] Failed to acquire Papyrus interface");
		return false;
	}

	if (!papyrus->Register(Papyrus::RegisterFunctions)) {
		REX::ERROR("[Smoking Guns] Failed to register Papyrus callback");
		return false;
	}


	REX::INFO("[Smoking Guns] Papyrus callback registered");

	return true;
}
