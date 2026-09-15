#include "ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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

	void ConfigManager::Load()
	{
		REX::INFO("[Smoking Guns] Loading configuration");

		LoadGeneralConfig();
	}

	void ConfigManager::LoadGameData()
	{
		REX::INFO("[Smoking Guns] Game data ready - loading form-based configuration");

		LoadAmmoConfigs();
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
				"OverallMult={:.3f}, "
				"SmokeDecayImpulse={:.3f}",
				ammoMult,
				weightMult,
				overallMult,
				smokeDecayImpulse);

			return;
		}

		std::string line;
		std::size_t lineNumber = 0;

		while (std::getline(file, line)) {
			++lineNumber;

			line = Trim(line);

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
			"OverallMult={:.3f}, "
			"SmokeDecayImpulse={:.3f}",
			ammoMult,
			weightMult,
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

				line = Trim(line);

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
}