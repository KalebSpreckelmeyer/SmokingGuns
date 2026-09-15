#pragma once

#include <cstdint>

namespace RE
{
	class PlayerCharacter;
}

namespace SmokingGuns
{
	struct FrameworkGraphStatus
	{
		bool firstPersonFound{ false };
		std::int32_t firstPersonVersion{ 0 };

		bool thirdPersonFound{ false };
		std::int32_t thirdPersonVersion{ 0 };

		[[nodiscard]] bool IsCompatible() const
		{
			return
				(firstPersonFound && firstPersonVersion == 1) ||
				(thirdPersonFound && thirdPersonVersion == 1);
		}
	};

	struct GraphWriteStatus
	{
		bool firstPersonWritten{ false };
		bool thirdPersonWritten{ false };

		[[nodiscard]] bool AnyWritten() const
		{
			return firstPersonWritten || thirdPersonWritten;
		}
	};

	class GraphManager
	{
	public:
		static FrameworkGraphStatus DetectFramework(
			RE::PlayerCharacter* a_player);

		static GraphWriteStatus SetFloatVariable(
			RE::PlayerCharacter* a_player,
			const char* a_variableName,
			float a_value);

		static GraphWriteStatus ReadFloatVariable(
			RE::PlayerCharacter* a_player,
			const char* a_variableName,
			float& a_firstPersonValue,
			float& a_thirdPersonValue);

	private:
		static bool TryReadFrameworkVersion(
			RE::PlayerCharacter* a_player,
			bool a_firstPerson,
			std::int32_t& a_version);

		static bool TrySetFloatVariable(
			RE::PlayerCharacter* a_player,
			bool a_firstPerson,
			const char* a_variableName,
			float a_value);

		static bool TryReadFloatVariable(
			RE::PlayerCharacter* a_player,
			bool a_firstPerson,
			const char* a_variableName,
			float& a_value);
	};
}