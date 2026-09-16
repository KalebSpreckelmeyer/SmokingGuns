#include "AttachmentManager.h"
#include "ConfigManager.h"
#include "RE/S/Setting.h"
#include "RE/I/ITEM_REMOVE_REASON.h"
#include "RE/N/NiPoint3.h"
#include "RE/T/TESObjectREFR.h"
#include <vector>
#include "RE/B/BGSInventoryItem.h"
#include "RE/B/BGSInventoryList.h"
#include "RE/B/BGSObjectInstanceExtra.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/B/BGSDefaultObject.h"
#include "RE/B/BGSKeyword.h"
#include <cstddef>

namespace SmokingGuns
{
	namespace
	{
		namespace
		{
			struct ScopedSilentPickup
			{
				RE::TESForm** slot{ nullptr };
				RE::TESForm* original{ nullptr };

				ScopedSilentPickup()
				{
					auto* defaultForm =
						RE::TESForm::GetFormByID(
							0x001B3FAD);

					auto* replacement =
						RE::TESForm::GetFormByID(
							0x0023F1CA);

					auto* defaultObject =
						defaultForm ?
						defaultForm->As<
						RE::BGSDefaultObject>() :
						nullptr;

					auto* replacementKeyword =
						replacement ?
						replacement->As<
						RE::BGSKeyword>() :
						nullptr;

					if (!defaultObject ||
						!replacementKeyword) {

						return;
					}

					slot =
						reinterpret_cast<RE::TESForm**>(
							reinterpret_cast<std::byte*>(
								defaultObject) +
							0x20);

					original = *slot;
					*slot = replacementKeyword;
				}

				~ScopedSilentPickup()
				{
					if (slot) {
						*slot = original;
					}
				}
			};
		}
		struct ScopedHUDMessageSuppression
		{
			RE::Setting* setting{ nullptr };
			bool originalValue{ true };

			ScopedHUDMessageSuppression()
			{
				setting =
					RE::GetINISetting(
						"bShowHUDMessages:Interface");

				if (!setting) {
					return;
				}

				originalValue =
					setting->GetBinary();

				setting->SetBinary(false);
			}

			~ScopedHUDMessageSuppression()
			{
				if (setting) {
					setting->SetBinary(
						originalValue);
				}
			}
		};
	}

	RE::BGSObjectInstanceExtra*
		AttachmentManager::FindEquippedInstanceExtra(
			RE::PlayerCharacter* a_player,
			RE::TESObjectWEAP* a_weapon)
	{
		if (!a_player || !a_weapon) {
			return nullptr;
		}

		auto* inventory =
			a_player->inventoryList;

		if (!inventory) {
			return nullptr;
		}

		RE::BSAutoReadLock lock{
			inventory->rwLock
		};

		for (auto& item : inventory->data) {

			if (item.object != a_weapon) {
				continue;
			}

			for (auto* stack = item.stackData.get();
				stack;
				stack = stack->nextStack.get()) {

				if (!stack->IsEquipped()) {
					continue;
				}

				if (!stack->extra) {
					continue;
				}

				auto* objectInstanceExtra =
					stack->extra
					->GetByType<
					RE::BGSObjectInstanceExtra>();

				if (objectInstanceExtra) {
					return objectInstanceExtra;
				}
			}
		}

		return nullptr;
	}

	std::optional<std::uint8_t>
		AttachmentManager::FindAttachIndex(
			const RE::TESObjectWEAP* a_weapon,
			const RE::BGSMod::Attachment::Mod* a_mod)
	{
		if (!a_weapon || !a_mod) {
			return std::nullopt;
		}

		const auto modAttachPoint =
			a_mod->attachPoint.keywordIndex;

		const auto& parents =
			a_weapon->attachParents;

		for (std::uint32_t i = 0;
			i < parents.size;
			++i) {

			if (parents.array[i].keywordIndex !=
				modAttachPoint) {
				continue;
			}

			// AddMod takes an 8-bit attach index.
			if (i > 0xFF) {
				return std::nullopt;
			}

			return static_cast<std::uint8_t>(i);
		}

		return std::nullopt;
	}

	std::optional<AttachmentIndexInfo>
		AttachmentManager::FindInstalledModIndexInfo(
			RE::BGSObjectInstanceExtra* a_instanceExtra,
			const RE::BGSMod::Attachment::Mod* a_mod)
	{
		if (!a_instanceExtra || !a_mod) {
			return std::nullopt;
		}

		const auto indexData =
			a_instanceExtra->GetIndexData();

		for (const auto& entry : indexData) {

			if (entry.objectID !=
				a_mod->GetFormID()) {
				continue;
			}

			AttachmentIndexInfo result{};

			result.attachIndex =
				entry.index;

			result.rank =
				entry.rank;

			return result;
		}

		return std::nullopt;
	}

	bool AttachmentManager::AddModToInstance(
		RE::BGSObjectInstanceExtra* a_instanceExtra,
		RE::BGSMod::Attachment::Mod* a_mod,
		std::uint8_t a_attachIndex,
		std::uint8_t a_rank)
	{
		if (!a_instanceExtra || !a_mod) {
			return false;
		}

		if (a_instanceExtra->HasMod(*a_mod)) {
			return true;
		}

		a_instanceExtra->AddMod(
			*a_mod,
			a_attachIndex,
			a_rank,
			false);

		return a_instanceExtra->HasMod(*a_mod);
	}

	bool AttachmentManager::RefreshModifiedPlayerStack(
		RE::PlayerCharacter* a_player,
		RE::TESObjectWEAP* a_weapon,
		std::uint32_t a_stackIndex)
	{
		if (!a_player || !a_weapon) {
			return false;
		}

		RE::TESObjectREFR::RemoveItemData removeData{
			a_weapon,
			1
		};

		//
		// Target only the exact stack we just modified.
		//
		removeData.stackData.push_back(
			a_stackIndex);

		constexpr auto kDroppingReason =
			static_cast<RE::ITEM_REMOVE_REASON>(3);

		removeData.reason = kDroppingReason;

		//
		// Garden of Eden drops the temporary reference
		// below the player before immediately picking it
		// back up.
		//
		RE::NiPoint3 dropLocation =
			a_player->GetPosition();

		dropLocation.z -= 200.0f;

		removeData.dropLoc =
			&dropLocation;

		REX::INFO(
			"[Smoking Guns] Refreshing modified 10mm "
			"through drop/re-add: stack={}",
			a_stackIndex);

		ScopedHUDMessageSuppression suppressHUD;

		const auto droppedHandle =
			a_player->RemoveItem(
				removeData);

		if (!droppedHandle) {
			REX::WARN(
				"[Smoking Guns] RemoveItem did not "
				"return a dropped reference");

			return false;
		}

		auto droppedRef =
			droppedHandle.get();

		if (!droppedRef) {
			REX::WARN(
				"[Smoking Guns] Could not resolve "
				"dropped item reference");

			return false;
		}

		REX::INFO(
			"[Smoking Guns] Temporary dropped reference: {:08X}",
			droppedRef->GetFormID());

		bool pickedUp = false;

		{
			ScopedSilentPickup silentPickup;

			pickedUp =
				droppedRef->ActivateRef(
					a_player,
					nullptr,
					1,
					false,
					true,
					false);
		}

		REX::INFO(
			"[Smoking Guns] Re-add activation result: {}",
			pickedUp ?
			"SUCCESS" :
			"FAILED");

		return pickedUp;
	}

	bool AttachmentManager::PrepareUnequippedWeaponStacks(
		RE::PlayerCharacter* a_player,
		RE::TESObjectWEAP* a_weapon,
		const WeaponProfile& a_profile)
	{
		if (!a_player || !a_weapon) {
			return false;
		}

		auto* inventory =
			a_player->inventoryList;

		if (!inventory) {
			REX::WARN(
				"[Smoking Guns] Player inventory list was unavailable");

			return false;
		}

		//
		// TEMPORARY TEST:
		// Only test the vanilla 10mm until the Garden-style
		// AddMod + inventory refresh path is proven.
		//
		if (a_weapon->GetFormID() != 0x00004822) {
			REX::DEBUG(
				"[Smoking Guns] Pre-equip attachment test currently "
				"limited to the vanilla 10mm");

			return false;
		}

		struct PendingTarget
		{
			RE::BGSObjectInstanceExtra* instanceExtra{ nullptr };
			std::uint32_t stackIndex{ 0 };
			RE::BGSMod::Attachment::Mod* attachment{ nullptr };
		};

		std::vector<PendingTarget> targets;

		//
		// Locate ONE suitable unequipped stack while holding
		// the inventory read lock.
		//
		// We only inspect here. No mutation occurs until after
		// the lock has been released.
		//
		{
			RE::BSAutoReadLock lock{
				inventory->rwLock
			};

			for (auto& item : inventory->data) {

				if (item.object != a_weapon) {
					continue;
				}

				std::uint32_t stackIndex = 0;

				for (auto* stack = item.stackData.get();
					stack;
					stack = stack->nextStack.get(), ++stackIndex) {

					if (stack->IsEquipped()) {
						REX::DEBUG(
							"[Smoking Guns] Skipping already-equipped "
							"10mm stack during pre-equip preparation");

						continue;
					}

					if (!stack->extra) {
						REX::WARN(
							"[Smoking Guns] Unequipped 10mm stack "
							"had no ExtraDataList");

						continue;
					}

					auto* instanceExtra =
						stack->extra->GetByType<
						RE::BGSObjectInstanceExtra>();

					if (!instanceExtra) {
						REX::WARN(
							"[Smoking Guns] Unequipped 10mm stack "
							"had no BGSObjectInstanceExtra");

						continue;
					}

					for (auto* attachment :
						a_profile.requiredAttachments) {

						if (!attachment) {
							continue;
						}

						if (instanceExtra->HasMod(*attachment)) {
							REX::DEBUG(
								"[Smoking Guns] Pre-equip attachment "
								"{:08X} already present on stack {}",
								attachment->GetFormID(),
								stackIndex);

							continue;
						}

						targets.push_back(
							PendingTarget{
								instanceExtra,
								stackIndex,
								attachment
							});

						//
						// This experiment intentionally handles
						// only ONE target per invocation.
						//
						break;
					}

					if (!targets.empty()) {
						break;
					}
				}

				if (!targets.empty()) {
					break;
				}
			}
		}

		//
		// Nothing needed modification.
		//
		if (targets.empty()) {
			return false;
		}

		//
		// There is deliberately only one target for this test.
		//
		const auto& target =
			targets.front();

		if (!target.instanceExtra ||
			!target.attachment) {

			return false;
		}

		constexpr std::uint8_t testAttachIndex = 0;
		constexpr std::uint8_t testRank = 0;

		REX::INFO(
			"[Smoking Guns] Direct AddMod installing "
			"{:08X} on stack {}",
			target.attachment->GetFormID(),
			target.stackIndex);

		//
		// Step 1:
		// Modify the exact weapon instance.
		//
		target.instanceExtra->AddMod(
			*target.attachment,
			testAttachIndex,
			testRank,
			true);

		const bool installed =
			target.instanceExtra->HasMod(
				*target.attachment);

		REX::INFO(
			"[Smoking Guns] Direct AddMod result for {:08X}: {}",
			target.attachment->GetFormID(),
			installed ?
			"PRESENT" :
			"FAILED");

		if (!installed) {
			return false;
		}

		//
		// Step 2:
		// Garden-style refresh.
		//
		// This removes this exact item and immediately lets
		// Fallout add it back, forcing its instance data to
		// be reconstructed.
		//
		const bool refreshed =
			RefreshModifiedPlayerStack(
				a_player,
				a_weapon,
				target.stackIndex);

		REX::INFO(
			"[Smoking Guns] Post-mod item refresh: {}",
			refreshed ?
			"SUCCESS" :
			"FAILED");

		return refreshed;
	}

	void AttachmentManager::LogAllMods(
		RE::BGSObjectInstanceExtra* a_instanceExtra)
	{
		if (!a_instanceExtra) {
			return;
		}

		const auto indexData =
			a_instanceExtra->GetIndexData();

		REX::INFO(
			"[Smoking Guns] ObjectIndexData contains {} entries",
			indexData.size());

		for (std::size_t i = 0;
			i < indexData.size();
			++i) {

			const auto& entry =
				indexData[i];

			REX::INFO(
				"[Smoking Guns] ModEntry[{}]: "
				"form={:08X}, index={}, rank={}, disabled={}",
				i,
				entry.objectID,
				entry.index,
				entry.rank,
				entry.disabled);
		}
	}
	bool AttachmentManager::AttachModThroughInventoryWorker(
		RE::BGSInventoryList* a_inventory,
		RE::TESObjectWEAP* a_weapon,
		std::uint32_t a_stackIndex,
		RE::BGSMod::Attachment::Mod* a_mod)
	{
		if (!a_inventory || !a_weapon || !a_mod) {
			return false;
		}

		REX::INFO(
			"[Smoking Guns] Inventory worker entered: "
			"weapon={:08X}, OMOD={:08X}, stackIndex={}",
			a_weapon->GetFormID(),
			a_mod->GetFormID(),
			a_stackIndex);

		RE::BGSInventoryItem::CheckStackIDFunctor compare{
			a_stackIndex
		};

		bool workerSuccess = false;

		REX::DEBUG(
			"[Smoking Guns] Constructing ModifyModDataFunctor");

		RE::BGSInventoryItem::ModifyModDataFunctor writer{
			a_mod,
			0,
			true,
			&workerSuccess
		};

		REX::DEBUG(
			"[Smoking Guns] Calling FindAndWriteStackDataForItem");

		a_inventory->FindAndWriteStackDataForItem(
			a_weapon,
			compare,
			writer);

		REX::DEBUG(
			"[Smoking Guns] FindAndWriteStackDataForItem returned; "
			"workerSuccess={}",
			workerSuccess);

		return workerSuccess;
	}
}