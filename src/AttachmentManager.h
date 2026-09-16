#include <optional>

namespace RE
{
	class BGSObjectInstanceExtra;
	class PlayerCharacter;
	class TESObjectWEAP;

	namespace BGSMod::Attachment
	{
		class Mod;
	}
}

namespace SmokingGuns
{
	struct WeaponProfile;

	struct AttachmentIndexInfo
	{
		std::uint8_t attachIndex{ 0 };
		std::uint8_t rank{ 0 };
	};

	class AttachmentManager
	{
	public:
		static RE::BGSObjectInstanceExtra* FindEquippedInstanceExtra(
			RE::PlayerCharacter* a_player,
			RE::TESObjectWEAP* a_weapon);

		static std::optional<std::uint8_t> FindAttachIndex(
			const RE::TESObjectWEAP* a_weapon,
			const RE::BGSMod::Attachment::Mod* a_mod);

		static std::optional<AttachmentIndexInfo> FindInstalledModIndexInfo(
			RE::BGSObjectInstanceExtra* a_instanceExtra,
			const RE::BGSMod::Attachment::Mod* a_mod);

		static bool AddModToInstance(
			RE::BGSObjectInstanceExtra* a_instanceExtra,
			RE::BGSMod::Attachment::Mod* a_mod,
			std::uint8_t a_attachIndex,
			std::uint8_t a_rank);

		static bool PrepareUnequippedWeaponStacks(
			RE::PlayerCharacter* a_player,
			RE::TESObjectWEAP* a_weapon,
			const WeaponProfile& a_profile);

		static void LogAllMods(
			RE::BGSObjectInstanceExtra* a_instanceExtra);

		static bool AttachModThroughInventoryWorker(
			RE::BGSInventoryList* a_inventory,
			RE::TESObjectWEAP* a_weapon,
			std::uint32_t a_stackIndex,
			RE::BGSMod::Attachment::Mod* a_mod);

		static bool RefreshModifiedPlayerStack(
			RE::PlayerCharacter* a_player,
			RE::TESObjectWEAP* a_weapon,
			std::uint32_t a_stackIndex);
	};
}