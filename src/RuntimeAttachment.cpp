#include "RuntimeAttachment.h"

#include "ConnectPoint.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

namespace SmokingGuns::RuntimeAttachment
{
	namespace
	{
		constexpr std::string_view kLocatorPrefix{ "P-SG_" };
		constexpr std::string_view kRuntimePrefix{ "SG_Runtime_" };

		struct LocatedConnectPoint
		{
			RE::NiNode* parent{ nullptr };
			const RE::BSConnectPoint::Parents::ConnectPoint* point{ nullptr };
		};

		RE::NiMatrix3 QuaternionToMatrix(
			const RE::NiQuaternion& a_rotation)
		{
			float w = a_rotation.w;
			float x = a_rotation.x;
			float y = a_rotation.y;
			float z = a_rotation.z;

			const float lengthSquared =
				w * w + x * x + y * y + z * z;

			if (lengthSquared > 0.000001F) {
				const float inverseLength =
					1.0F / std::sqrt(lengthSquared);

				w *= inverseLength;
				x *= inverseLength;
				y *= inverseLength;
				z *= inverseLength;
			}
			else {
				return RE::NiMatrix3{
					1.0F, 0.0F, 0.0F, 0.0F,
					0.0F, 1.0F, 0.0F, 0.0F,
					0.0F, 0.0F, 1.0F, 0.0F
				};
			}

			const float xx = x * x;
			const float yy = y * y;
			const float zz = z * z;
			const float xy = x * y;
			const float xz = x * z;
			const float yz = y * z;
			const float wx = w * x;
			const float wy = w * y;
			const float wz = w * z;

			return RE::NiMatrix3{
				1.0F - 2.0F * (yy + zz),
				2.0F * (xy - wz),
				2.0F * (xz + wy),
				0.0F,
				2.0F * (xy + wz),
				1.0F - 2.0F * (xx + zz),
				2.0F * (yz - wx),
				0.0F,
				2.0F * (xz - wy),
				2.0F * (yz + wx),
				1.0F - 2.0F * (xx + yy),
				0.0F
			};
		}

		RE::NiAVObject* FindObject(
			RE::NiAVObject* a_root,
			const std::string& a_name)
		{
			if (!a_root || a_name.empty()) {
				return nullptr;
			}

			return a_root->GetObjectByName(
				RE::BSFixedString{ a_name.c_str() });
		}

		LocatedConnectPoint FindConnectPoint(
			RE::NiAVObject* a_treeRoot,
			const std::string& a_attachPoint)
		{
			if (!a_treeRoot || a_attachPoint.empty()) {
				return {};
			}

			std::vector<RE::NiAVObject*> pending;
			pending.push_back(a_treeRoot);

			const RE::BSFixedString cpaName{ "CPA" };

			while (!pending.empty()) {
				auto* owner = pending.back();
				pending.pop_back();

				if (!owner) {
					continue;
				}

				auto* extra = owner->GetExtraData(cpaName);
				auto* parents =
					netimmerse_cast<
						RE::BSConnectPoint::Parents*>(extra);

				if (parents) {
					for (auto* point : parents->points) {
						if (!point ||
							point->name != a_attachPoint.c_str()) {

							continue;
						}

						RE::NiAVObject* parentObject = owner;

						if (!point->parent.empty()) {
							parentObject = owner->GetObjectByName(
								point->parent);
						}

						auto* parentNode =
							parentObject ?
							parentObject->IsNode() :
							nullptr;

						if (!parentNode) {
							REX::WARN(
								"[Smoking Guns][RuntimeAttachment] "
								"Locator '{}' named parent '{}' was not "
								"a node in component '{}'",
								a_attachPoint,
								point->parent.c_str(),
								owner->name.c_str());

							continue;
						}

						return { parentNode, point };
					}
				}

				auto* node = owner->IsNode();

				if (!node) {
					continue;
				}

				for (auto& child : node->children) {
					if (child) {
						pending.push_back(child.get());
					}
				}
			}

			return {};
		}

		RE::NiPointer<RE::NiNode> LoadEffect(
			const std::string& a_nifPath)
		{
			std::string normalizedPath = a_nifPath;

			std::replace(
				normalizedPath.begin(),
				normalizedPath.end(),
				'/',
				'\\');

			RE::BSModelDB::DBTraits::ArgsType args{};
			args.prepareAfterLoad = true;
			args.performProcess = true;
			args.loadTextures = true;

			RE::NiPointer<RE::NiNode> effectRoot;

			const auto result = RE::BSModelDB::Demand(
				normalizedPath.c_str(),
				std::addressof(effectRoot),
				args);

			if (result != RE::BSResource::ErrorCode::kNone ||
				!effectRoot) {

				REX::ERROR(
					"[Smoking Guns][RuntimeAttachment] "
					"Could not load effect NIF '{}' (error={})",
					normalizedPath,
					static_cast<std::uint32_t>(result));

				return nullptr;
			}

			return effectRoot;
		}
	}

	std::string MakeRuntimeNodeName(
		const std::string& a_attachPoint)
	{
		if (a_attachPoint.starts_with(kLocatorPrefix)) {
			return std::string{ kRuntimePrefix } +
				a_attachPoint.substr(kLocatorPrefix.size());
		}

		std::string sanitized = a_attachPoint;

		for (auto& character : sanitized) {
			if (!std::isalnum(
				static_cast<unsigned char>(character)) &&
				character != '_') {

				character = '_';
			}
		}

		return std::string{ kRuntimePrefix } + sanitized;
	}

	bool HasRequiredNodes(
		RE::NiAVObject* a_treeRoot,
		const WeaponProfile& a_profile)
	{
		if (!a_treeRoot) {
			return false;
		}

		for (const auto& requirement : a_profile.effects) {
			if (!FindObject(
				a_treeRoot,
				MakeRuntimeNodeName(
					requirement.attachPoint))) {

				return false;
			}
		}

		return true;
	}

	ReconcileResult ReconcileTree(
		RE::NiAVObject* a_treeRoot,
		const WeaponProfile& a_profile,
		const char* a_treeLabel)
	{
		ReconcileResult result{};

		if (!a_treeRoot) {
			result.locatorMissing = a_profile.effects.size();
			return result;
		}

		for (const auto& requirement : a_profile.effects) {
			const auto runtimeName =
				MakeRuntimeNodeName(
					requirement.attachPoint);

			if (FindObject(a_treeRoot, runtimeName)) {
				++result.alreadyPresent;
				continue;
			}

			const auto located = FindConnectPoint(
				a_treeRoot,
				requirement.attachPoint);

			if (!located.parent || !located.point) {
				++result.locatorMissing;

				REX::WARN(
					"[Smoking Guns][RuntimeAttachment] "
					"{} locator '{}' was not found in the live tree",
					a_treeLabel,
					requirement.attachPoint);

				continue;
			}

			auto effectRoot = LoadEffect(requirement.nifPath);

			if (!effectRoot) {
				++result.loadFailed;
				continue;
			}

			auto runtimeNode = RE::make_nismart<RE::NiNode>(1);
			runtimeNode->name = RE::BSFixedString{ runtimeName.c_str() };
			runtimeNode->local.translate = located.point->position;
			runtimeNode->local.rotate =
				QuaternionToMatrix(located.point->rotation);
			runtimeNode->local.scale = located.point->scale;

			runtimeNode->AttachChild(effectRoot.get(), true);
			located.parent->AttachChild(runtimeNode.get(), true);

			++result.attached;

			REX::INFO(
				"[Smoking Guns][RuntimeAttachment] "
				"{} attached '{}' as '{}' beneath '{}'",
				a_treeLabel,
				requirement.nifPath,
				runtimeName,
				located.parent->name.c_str());
		}

		return result;
	}
}
