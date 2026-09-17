#pragma once

#include "ConfigManager.h"

#include <RE/Fallout.h>

#include <cstddef>
#include <string>

namespace SmokingGuns::RuntimeAttachment
{
	struct ReconcileResult
	{
		std::size_t alreadyPresent{ 0 };
		std::size_t attached{ 0 };
		std::size_t locatorMissing{ 0 };
		std::size_t loadFailed{ 0 };

		[[nodiscard]] bool IsSatisfied() const noexcept
		{
			return locatorMissing == 0 && loadFailed == 0;
		}
	};

	[[nodiscard]] std::string MakeRuntimeNodeName(
		const std::string& a_attachPoint);

	[[nodiscard]] bool HasRequiredNodes(
		RE::NiAVObject* a_treeRoot,
		const WeaponProfile& a_profile);

	void ReleaseRetainedAttachments();

	[[nodiscard]] ReconcileResult ReconcileTree(
		RE::NiAVObject* a_treeRoot,
		const WeaponProfile& a_profile,
		const char* a_treeLabel);
}
