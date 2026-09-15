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

		REX::INFO(
			"[Smoking Guns] Equipped form received: {:08X}",
			a_form->GetFormID());

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