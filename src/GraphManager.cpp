#include "GraphManager.h"

#include "RE/B/BIPED_OBJECT.h"
#include "RE/B/BipedAnim.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/W/WeaponAnimationGraphManagerHolder.h"

namespace SmokingGuns
{
	bool GraphManager::TryReadFrameworkVersion(
		RE::PlayerCharacter* a_player,
		bool a_firstPerson,
		std::int32_t& a_version)
	{
		if (!a_player) {
			return false;
		}

		const auto& biped =
			a_player->GetBiped(a_firstPerson);

		if (!biped) {
			return false;
		}

		const auto* weaponObject =
			biped->GetBipObject(
				RE::BIPED_OBJECT::kWeaponGun);

		if (!weaponObject) {
			return false;
		}

		auto* holder =
			weaponObject->objectGraphManager.get();

		if (!holder) {
			return false;
		}

		const RE::BSFixedString variableName{
			"SG_FrameworkVersion"
		};

		std::int32_t version = 0;

		if (!holder->GetGraphVariableImplInt(
			variableName,
			version)) {
			return false;
		}

		a_version = version;
		return true;
	}


	FrameworkGraphStatus GraphManager::DetectFramework(
		RE::PlayerCharacter* a_player)
	{
		FrameworkGraphStatus status{};

		status.firstPersonFound =
			TryReadFrameworkVersion(
				a_player,
				true,
				status.firstPersonVersion);

		status.thirdPersonFound =
			TryReadFrameworkVersion(
				a_player,
				false,
				status.thirdPersonVersion);

		return status;
	}


	bool GraphManager::TrySetFloatVariable(
		RE::PlayerCharacter* a_player,
		bool a_firstPerson,
		const char* a_variableName,
		float a_value)
	{
		if (!a_player || !a_variableName) {
			return false;
		}

		const auto& biped =
			a_player->GetBiped(a_firstPerson);

		if (!biped) {
			return false;
		}

		const auto* weaponObject =
			biped->GetBipObject(
				RE::BIPED_OBJECT::kWeaponGun);

		if (!weaponObject) {
			return false;
		}

		auto* holder =
			weaponObject->objectGraphManager.get();

		if (!holder) {
			return false;
		}

		const RE::BSFixedString variableName{
			a_variableName
		};

		return holder->SetGraphVariableFloat(
			variableName,
			a_value);
	}


	GraphWriteStatus GraphManager::SetFloatVariable(
		RE::PlayerCharacter* a_player,
		const char* a_variableName,
		float a_value)
	{
		GraphWriteStatus status{};

		status.firstPersonWritten =
			TrySetFloatVariable(
				a_player,
				true,
				a_variableName,
				a_value);

		status.thirdPersonWritten =
			TrySetFloatVariable(
				a_player,
				false,
				a_variableName,
				a_value);

		return status;
	}

	bool GraphManager::TrySetIntVariable(
		RE::PlayerCharacter* a_player,
		bool a_firstPerson,
		const char* a_variableName,
		std::int32_t a_value)
	{
		if (!a_player || !a_variableName) {
			return false;
		}

		const auto& biped = a_player->GetBiped(a_firstPerson);
		if (!biped) {
			return false;
		}

		const auto* weaponObject = biped->GetBipObject(
			RE::BIPED_OBJECT::kWeaponGun);
		if (!weaponObject || !weaponObject->objectGraphManager) {
			return false;
		}

		return weaponObject->objectGraphManager->SetGraphVariableInt(
			RE::BSFixedString{ a_variableName }, a_value);
	}

	GraphWriteStatus GraphManager::SetIntVariable(
		RE::PlayerCharacter* a_player,
		const char* a_variableName,
		std::int32_t a_value)
	{
		return {
			TrySetIntVariable(a_player, true, a_variableName, a_value),
			TrySetIntVariable(a_player, false, a_variableName, a_value)
		};
	}

	bool GraphManager::TryReadFloatVariable(
		RE::PlayerCharacter* a_player,
		bool a_firstPerson,
		const char* a_variableName,
		float& a_value)
	{
		if (!a_player || !a_variableName) {
			return false;
		}

		const auto& biped =
			a_player->GetBiped(a_firstPerson);

		if (!biped) {
			return false;
		}

		const auto* weaponObject =
			biped->GetBipObject(
				RE::BIPED_OBJECT::kWeaponGun);

		if (!weaponObject) {
			return false;
		}

		auto* holder =
			weaponObject->objectGraphManager.get();

		if (!holder) {
			return false;
		}

		const RE::BSFixedString variableName{
			a_variableName
		};

		return holder->GetGraphVariableImplFloat(
			variableName,
			a_value);
	}

	GraphWriteStatus GraphManager::ReadFloatVariable(
		RE::PlayerCharacter* a_player,
		const char* a_variableName,
		float& a_firstPersonValue,
		float& a_thirdPersonValue)
	{
		GraphWriteStatus status{};

		status.firstPersonWritten =
			TryReadFloatVariable(
				a_player,
				true,
				a_variableName,
				a_firstPersonValue);

		status.thirdPersonWritten =
			TryReadFloatVariable(
				a_player,
				false,
				a_variableName,
				a_thirdPersonValue);

		return status;
	}
}
