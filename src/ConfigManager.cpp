#include "ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>

namespace
{
	std::string Trim(std::string a_value)
	{
		const auto notSpace = [](unsigned char a_char) {
			return !std::isspace(a_char);
			};

		a_value.erase(
			a_value.begin(),
			std::find_if(
				a_value.begin(),
				a_value.end(),
				notSpace));

		a_value.erase(
			std::find_if(
				a_value.rbegin(),
				a_value.rend(),
				notSpace)
			.base(),
			a_value.end());

		return a_value;
	}

	std::string StripInlineComment(std::string a_value)
	{
		bool inQuotes = false;

		for (std::size_t index = 0;
			index < a_value.size();
			++index) {

			const char character = a_value[index];

			if (character == '"') {
				inQuotes = !inQuotes;
				continue;
			}

			if (!inQuotes &&
				(character == '#' || character == ';')) {

				a_value.erase(index);
				break;
			}
		}

		return Trim(std::move(a_value));
	}

	std::string Unquote(std::string a_value)
	{
		a_value = Trim(std::move(a_value));

		if (a_value.size() >= 2 &&
			a_value.front() == '"' &&
			a_value.back() == '"') {

			return a_value.substr(1, a_value.size() - 2);
		}

		return a_value;
	}

	bool TryParseFormReference(
		const std::string& a_text,
		std::string& a_pluginName,
		std::uint32_t& a_localFormID)
	{
		const auto separator = a_text.find('|');

		if (separator == std::string::npos) {
			return false;
		}

		a_pluginName = Trim(a_text.substr(0, separator));
		const std::string formIDText =
			Trim(a_text.substr(separator + 1));

		if (a_pluginName.empty() || formIDText.empty()) {
			return false;
		}

		try {
			std::size_t parsed = 0;
			a_localFormID = static_cast<std::uint32_t>(
				std::stoul(formIDText, &parsed, 16));

			return parsed == formIDText.size();
		}
		catch (...) {
			return false;
		}
	}

	bool TryParseFloat(
		const std::string& a_text,
		float& a_result)
	{
		try {
			std::size_t parsedCharacters = 0;

			a_result = std::stof(
				a_text,
				&parsedCharacters);

			// Reject things such as:
			// 1.0garbage
			if (parsedCharacters != a_text.size()) {
				return false;
			}

			return true;
		}
		catch (...) {
			return false;
		}
	}
}

namespace SmokingGuns
{
	ConfigManager& ConfigManager::GetSingleton()
	{
		static ConfigManager singleton;
		return singleton;
	}

	float ConfigManager::GetAmmoMult() const
	{
		return ammoMult;
	}

	float ConfigManager::GetWeightMult() const
	{
		return weightMult;
	}

	float ConfigManager::GetReferenceWeight() const
	{
		return referenceWeight;
	}

	float ConfigManager::GetOverallMult() const
	{
		return overallMult;
	}

	float ConfigManager::GetSmokeDecayImpulse() const
	{
		return smokeDecayImpulse;
	}

	float ConfigManager::GetAmmoImpulse(const RE::TESAmmo* a_ammo) const
	{
		if (!a_ammo) {
			return 0.0f;
		}

		const auto it = ammoImpulses.find(a_ammo->GetFormID());

		if (it == ammoImpulses.end()) {
			return 0.0f;
		}

		return it->second;
	}

	const WeaponProfile* ConfigManager::GetWeaponProfile(
		const RE::TESObjectWEAP* a_weapon) const
	{
		if (!a_weapon) {
			return nullptr;
		}

		const auto it =
			weaponProfiles.find(
				a_weapon->GetFormID());

		if (it == weaponProfiles.end()) {
			return nullptr;
		}

		return &it->second;
	}

	void ConfigManager::Load()
	{
		REX::INFO("[Smoking Guns] Loading configuration");

		LoadGeneralConfig();
	}

	void ConfigManager::LoadGameData()
	{
		REX::INFO(
			"[Smoking Guns] Game data ready - loading form-based configuration");

		LoadAmmoConfigs();
		LoadWeaponConfigs();
	}

	void ConfigManager::LoadGeneralConfig()
	{
		const std::filesystem::path configPath =
			"Data/F4SE/Plugins/SmokingGuns/SmokingGuns.ini";

		std::ifstream file(configPath);

		if (!file.is_open()) {
			REX::WARN(
				"[Smoking Guns] General config not found: {}",
				configPath.string());

			REX::INFO(
				"[Smoking Guns] Using general defaults: "
				"AmmoMult={:.3f}, "
				"WeightMult={:.3f}, "
				"ReferenceWeight={:.3f}, "
				"OverallMult={:.3f}, "
				"SmokeDecayImpulse={:.3f}",
				ammoMult,
				weightMult,
				referenceWeight,
				overallMult,
				smokeDecayImpulse);

			return;
		}

		std::string line;
		std::size_t lineNumber = 0;

		while (std::getline(file, line)) {
			++lineNumber;

			line = StripInlineComment(std::move(line));

			// Ignore blank lines and comments.
			if (line.empty() ||
				line.starts_with('#') ||
				line.starts_with(';')) {
				continue;
			}

			const auto equalsPosition = line.find('=');

			if (equalsPosition == std::string::npos) {
				REX::WARN(
					"[Smoking Guns] Ignoring malformed general config line {}: {}",
					lineNumber,
					line);

				continue;
			}

			const std::string key =
				Trim(line.substr(0, equalsPosition));

			const std::string valueText =
				Trim(line.substr(equalsPosition + 1));

			float value = 0.0f;

			if (!TryParseFloat(valueText, value)) {
				REX::WARN(
					"[Smoking Guns] Invalid number on general config line {}: {}",
					lineNumber,
					valueText);

				continue;
			}

			if (key == "AmmoMult") {
				ammoMult = value;
			}
			else if (key == "WeightMult") {
				weightMult = value;
			}
			else if (key == "ReferenceWeight") {
				if (value > 0.0f) {
					referenceWeight = value;
				}
				else {
					REX::WARN(
						"[Smoking Guns] ReferenceWeight must be greater than zero "
						"on general config line {}: {}",
						lineNumber,
						valueText);
				}
			}
			else if (key == "OverallMult") {
				overallMult = value;
			}
			else if (key == "SmokeDecayImpulse") {
				smokeDecayImpulse = value;
			}
			else {
				REX::WARN(
					"[Smoking Guns] Unknown general config key on line {}: {}",
					lineNumber,
					key);
			}
		}

		REX::INFO(
			"[Smoking Guns] General config loaded: "
			"AmmoMult={:.3f}, "
			"WeightMult={:.3f}, "
			"ReferenceWeight={:.3f}, "
			"OverallMult={:.3f}, "
			"SmokeDecayImpulse={:.3f}",
			ammoMult,
			weightMult,
			referenceWeight,
			overallMult,
			smokeDecayImpulse);
	}

	void ConfigManager::LoadAmmoConfigs()
	{
		const std::filesystem::path ammoDirectory =
			"Data/F4SE/Plugins/SmokingGuns/Ammo";

		ammoImpulses.clear();

		if (!std::filesystem::exists(ammoDirectory)) {
			REX::WARN(
				"[Smoking Guns] Ammo config directory not found: {}",
				ammoDirectory.string());

			return;
		}

		auto* dataHandler = RE::TESDataHandler::GetSingleton();

		if (!dataHandler) {
			REX::ERROR(
				"[Smoking Guns] TESDataHandler was unavailable while loading ammo configs");

			return;
		}

		std::size_t loadedEntries = 0;

		for (const auto& directoryEntry :
			std::filesystem::directory_iterator(ammoDirectory)) {

			if (!directoryEntry.is_regular_file()) {
				continue;
			}

			const auto& path = directoryEntry.path();

			if (path.extension() != ".ini") {
				continue;
			}

			REX::INFO(
				"[Smoking Guns] Loading ammo config: {}",
				path.filename().string());

			std::ifstream file(path);

			if (!file.is_open()) {
				REX::WARN(
					"[Smoking Guns] Could not open ammo config: {}",
					path.string());

				continue;
			}

			std::string line;
			std::size_t lineNumber = 0;

			while (std::getline(file, line)) {
				++lineNumber;

				line = StripInlineComment(std::move(line));

				if (line.empty() ||
					line.starts_with('#') ||
					line.starts_with(';')) {
					continue;
				}

				const auto equalsPosition = line.find('=');

				if (equalsPosition == std::string::npos) {
					REX::WARN(
						"[Smoking Guns] Ignoring malformed ammo config line {} in {}: {}",
						lineNumber,
						path.filename().string(),
						line);

					continue;
				}

				const std::string leftSide =
					Trim(line.substr(0, equalsPosition));

				const std::string valueText =
					Trim(line.substr(equalsPosition + 1));

				float impulse = 0.0f;

				if (!TryParseFloat(valueText, impulse)) {
					REX::WARN(
						"[Smoking Guns] Invalid ammo impulse on line {} in {}: {}",
						lineNumber,
						path.filename().string(),
						valueText);

					continue;
				}

				// Everything after ':' is just a human-readable label.
				const auto colonPosition = leftSide.find(':');

				const std::string formReference =
					Trim(leftSide.substr(0, colonPosition));

				const auto separatorPosition =
					formReference.find('|');

				if (separatorPosition == std::string::npos) {
					REX::WARN(
						"[Smoking Guns] Missing plugin/FormID separator on line {} in {}",
						lineNumber,
						path.filename().string());

					continue;
				}

				const std::string pluginName =
					Trim(formReference.substr(0, separatorPosition));

				const std::string formIDText =
					Trim(formReference.substr(separatorPosition + 1));

				std::uint32_t localFormID = 0;

				try {
					std::size_t parsedCharacters = 0;

					localFormID = static_cast<std::uint32_t>(
						std::stoul(
							formIDText,
							&parsedCharacters,
							16));

					if (parsedCharacters != formIDText.size()) {
						throw std::invalid_argument("Trailing characters");
					}
				}
				catch (...) {
					REX::WARN(
						"[Smoking Guns] Invalid FormID on line {} in {}: {}",
						lineNumber,
						path.filename().string(),
						formIDText);

					continue;
				}

				auto* form =
					dataHandler->LookupForm(
						localFormID,
						pluginName);

				auto* ammo =
					form ? form->As<RE::TESAmmo>() : nullptr;

				if (!ammo) {
					REX::WARN(
						"[Smoking Guns] Could not resolve ammo {}|{:06X} from {}",
						pluginName,
						localFormID,
						path.filename().string());

					continue;
				}

				ammoImpulses[ammo->GetFormID()] = impulse;

				++loadedEntries;

				REX::DEBUG(
					"[Smoking Guns] Ammo registered: "
					"{}|{:06X} -> runtime {:08X}, impulse={:.3f}",
					pluginName,
					localFormID,
					ammo->GetFormID(),
					impulse);
			}
		}

		REX::INFO(
			"[Smoking Guns] Loaded {} ammo impulse entries",
			loadedEntries);
	}

	void ConfigManager::LoadWeaponConfigs()
	{
		const std::filesystem::path weaponDirectory =
			"Data/F4SE/Plugins/SmokingGuns/Weapons";

		weaponProfiles.clear();

		if (!std::filesystem::exists(weaponDirectory)) {
			REX::WARN(
				"[Smoking Guns] Weapon config directory not found: {}",
				weaponDirectory.string());

			return;
		}

		auto* dataHandler = RE::TESDataHandler::GetSingleton();

		if (!dataHandler) {
			REX::ERROR(
				"[Smoking Guns] TESDataHandler was unavailable "
				"while loading weapon configs");

			return;
		}

		std::size_t loadedRequirements = 0;

		for (const auto& directoryEntry :
			std::filesystem::directory_iterator(weaponDirectory)) {

			if (!directoryEntry.is_regular_file()) {
				continue;
			}

			const auto& path = directoryEntry.path();

			if (path.extension() != ".ini") {
				continue;
			}

			REX::INFO(
				"[Smoking Guns] Loading weapon config: {}",
				path.filename().string());

			std::ifstream file(path);

			if (!file.is_open()) {
				REX::WARN(
					"[Smoking Guns] Could not open weapon config: {}",
					path.string());

				continue;
			}

			std::optional<std::uint32_t> currentWeaponFormID;
			std::string line;
			std::size_t lineNumber = 0;

			while (std::getline(file, line)) {
				++lineNumber;
				line = StripInlineComment(std::move(line));

				if (line.empty()) {
					continue;
				}

				if (line.front() == '[' && line.back() == ']') {
					const std::string weaponReference =
						Trim(line.substr(1, line.size() - 2));

					std::string pluginName;
					std::uint32_t localFormID = 0;

					if (!TryParseFormReference(
						weaponReference,
						pluginName,
						localFormID)) {

						REX::WARN(
							"[Smoking Guns] Invalid weapon section on line "
							"{} in {}: {}",
							lineNumber,
							path.filename().string(),
							line);

						currentWeaponFormID.reset();
						continue;
					}

					auto* form = dataHandler->LookupForm(
						localFormID,
						pluginName);

					auto* weapon =
						form ? form->As<RE::TESObjectWEAP>() : nullptr;

					if (!weapon) {
						REX::WARN(
							"[Smoking Guns] Could not resolve weapon "
							"{}|{:06X} from {}",
							pluginName,
							localFormID,
							path.filename().string());

						currentWeaponFormID.reset();
						continue;
					}

					currentWeaponFormID = weapon->GetFormID();
					weaponProfiles.try_emplace(*currentWeaponFormID);

					REX::DEBUG(
						"[Smoking Guns] Weapon profile section: "
						"{}|{:06X} -> runtime {:08X}",
						pluginName,
						localFormID,
						*currentWeaponFormID);

					continue;
				}

				const auto equalsPosition = line.find('=');

				if (equalsPosition == std::string::npos) {
					REX::WARN(
						"[Smoking Guns] Malformed weapon requirement "
						"on line {} in {}: {}",
						lineNumber,
						path.filename().string(),
						line);

					continue;
				}

				if (!currentWeaponFormID) {
					REX::WARN(
						"[Smoking Guns] Effect requirement outside a valid "
						"weapon section on line {} in {}",
						lineNumber,
						path.filename().string());

					continue;
				}

				const std::string profileKey =
					Trim(line.substr(0, equalsPosition));

				const std::string nifPath =
					Unquote(line.substr(equalsPosition + 1));

				if (profileKey == "ReloadMode") {
					auto& mode = weaponProfiles[*currentWeaponFormID].reloadMode;
					if (nifPath == "Explicit") {
						mode = ReloadMode::kExplicit;
					}
					else if (nifPath == "Timed") {
						mode = ReloadMode::kTimed;
					}
					else if (nifPath == "None") {
						mode = ReloadMode::kNone;
					}
					else {
						REX::WARN(
							"[Smoking Guns] Invalid ReloadMode '{}' on line "
							"{} in {} (expected None, Explicit or Timed)",
							nifPath, lineNumber, path.filename().string());
					}
					continue;
				}

				if (!profileKey.starts_with("P-SG_") ||
					nifPath.empty()) {

					REX::WARN(
						"[Smoking Guns] Invalid effect requirement on line "
						"{} in {}: {}",
						lineNumber,
						path.filename().string(),
						line);

					continue;
				}

				const auto separator = profileKey.find('.', 4);
				const std::string attachPoint = separator == std::string::npos ?
					profileKey : profileKey.substr(0, separator);
				const std::string instance = separator == std::string::npos ?
					std::string{} : Trim(profileKey.substr(separator + 1));
				if (attachPoint.size() <= 4 ||
					(separator != std::string::npos && instance.empty())) {
					REX::WARN(
						"[Smoking Guns] Invalid effect key on line {} in {}: {}",
						lineNumber, path.filename().string(), line);
					continue;
				}

				auto& requirements =
					weaponProfiles[*currentWeaponFormID].effects;

				const auto existing = std::find_if(
					requirements.begin(),
					requirements.end(),
					[&attachPoint, &instance](const EffectRequirement& a_entry) {
						return a_entry.attachPoint == attachPoint &&
							a_entry.instance == instance;
					});

				if (existing != requirements.end()) {
					REX::WARN(
						"[Smoking Guns] Replacing duplicate requirement '{}' "
						"for weapon {:08X}",
						profileKey,
						*currentWeaponFormID);

					existing->nifPath = nifPath;
				}
				else {
					requirements.push_back({ attachPoint, instance, nifPath });
					++loadedRequirements;
				}
			}
		}

		REX::INFO(
			"[Smoking Guns] Loaded {} weapon profiles with {} "
			"runtime effect requirements",
			weaponProfiles.size(),
			loadedRequirements);
	}
}
