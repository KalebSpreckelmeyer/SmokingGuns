#include "ConfigManager.h"
#include <variant>

namespace Papyrus
{
	bool Ping(std::monostate)
	{
		REX::INFO("[Smoking Guns] Ping received from Papyrus");
		return true;
	}

	bool ReportEquipped(std::monostate, RE::TESForm* a_form)
	{
		if (!a_form) {
			REX::WARN("[Smoking Guns] ReportEquipped received a null form");
			return false;
		}

		// The Papyrus event can report non-weapons, so reject them here too.
		auto* eventWeapon = a_form->As<RE::TESObjectWEAP>();

		if (!eventWeapon) {
			REX::DEBUG(
				"[Smoking Guns] Ignoring non-weapon equip event: {:08X}",
				a_form->GetFormID());

			return false;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();

		if (!player) {
			REX::ERROR("[Smoking Guns] Could not acquire PlayerCharacter");
			return false;
		}

		// Fallout's first/current weapon equip index.
		RE::BGSEquipIndex equipIndex{};
		equipIndex.index = 0;

		// This is an output object. GetEquippedItem will fill it with
		// the object and instance data that are actually equipped.
		RE::BGSObjectInstance equipped{ nullptr, nullptr };

		auto* result = player->GetEquippedItem(
			&equipped,
			equipIndex);

		if (!result || !equipped.object) {
			REX::WARN(
				"[Smoking Guns] Equip event {:08X}, but GetEquippedItem returned nothing",
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

		auto* instanceData = equipped.instanceData.get();

		if (!instanceData) {
			REX::WARN(
				"[Smoking Guns] Equipped weapon {:08X} had no instance data",
				equippedWeapon->GetFormID());

			return false;
		}

		// We have already confirmed that equipped.object is a TESObjectWEAP,
		// so its associated instance data should be TESObjectWEAP::InstanceData.
		auto* weaponData =
			static_cast<RE::TESObjectWEAP::InstanceData*>(instanceData);

		auto* instanceAmmo = weaponData->ammo;

		const float ammoImpulse =
			SmokingGuns::ConfigManager::GetSingleton()
			.GetAmmoImpulse(instanceAmmo);

		const auto instanceAmmoFormID =
			instanceAmmo ? instanceAmmo->GetFormID() : 0;

		REX::INFO(
			"[Smoking Guns] Equipped weapon: "
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

		return true;
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			REX::ERROR("[Smoking Guns] Papyrus VM was null");
			return false;
		}

		REX::INFO("[Smoking Guns] Binding Ping");

		a_vm->BindNativeMethod(
			"SGNative",
			"Ping",
			Ping);

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