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

		struct RetainedAttachment
		{
			RE::NiAVObject* treeRoot{ nullptr };
			std::string runtimeName;
			RE::NiPointer<RE::NiNode> runtimeNode;
		};

		std::vector<RetainedAttachment> retainedAttachments;

		RetainedAttachment* FindRetainedAttachment(
			RE::NiAVObject* a_treeRoot,
			const std::string& a_runtimeName)
		{
			const auto found = std::ranges::find_if(
				retainedAttachments,
				[&](const RetainedAttachment& a_entry) {
					return a_entry.treeRoot == a_treeRoot &&
						a_entry.runtimeName == a_runtimeName;
				});

			return found != retainedAttachments.end() ?
				std::addressof(*found) :
				nullptr;
		}

		void RetainAttachment(
			RE::NiAVObject* a_treeRoot,
			const std::string& a_runtimeName,
			const RE::NiPointer<RE::NiNode>& a_runtimeNode)
		{
			if (auto* retained = FindRetainedAttachment(
				a_treeRoot,
				a_runtimeName)) {

				retained->runtimeNode = a_runtimeNode;
				return;
			}

			retainedAttachments.push_back({
				a_treeRoot,
				a_runtimeName,
				a_runtimeNode
			});
		}

		bool ParentContainsChild(
			const RE::NiNode* a_parent,
			const RE::NiAVObject* a_child)
		{
			if (!a_parent || !a_child) {
				return false;
			}

			for (const auto& candidate : a_parent->children) {
				if (candidate.get() == a_child) {
					return true;
				}
			}

			return false;
		}

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

		RE::NiPointer<RE::NiNode> LoadEffectTemplate(
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

		RE::NiPointer<RE::NiNode> CloneEffect(
			RE::NiNode* a_templateRoot,
			const std::string& a_nifPath)
		{
			if (!a_templateRoot) {
				return nullptr;
			}

			// BSModelDB owns and may reuse the demanded root.  Build a unique
			// scene instance before assigning a parent in either weapon tree.
			RE::NiCloningProcess cloning{};
			cloning.copyType =
				RE::NiCloningProcess::CopyType::kCopyExact;
			cloning.scale = RE::NiPoint3{ 1.0F, 1.0F, 1.0F };

			RE::NiPointer<RE::NiObject> clonedObject{
				a_templateRoot->CreateClone(cloning)
			};

			if (!clonedObject) {
				REX::ERROR(
					"[Smoking Guns][RuntimeAttachment] "
					"Could not clone effect NIF template '{}'",
					a_nifPath);

				return nullptr;
			}

			a_templateRoot->ProcessClone(cloning);

			auto* clonedRoot = clonedObject->IsNode();

			if (!clonedRoot) {
				REX::ERROR(
					"[Smoking Guns][RuntimeAttachment] "
					"Clone of effect NIF '{}' was not a node",
					a_nifPath);

				return nullptr;
			}

			RE::NiPointer<RE::NiNode> effectRoot{ clonedRoot };

			if (auto* resourceManager =
				RE::BSShaderResourceManager::GetSingleton()) {

				resourceManager->ApplyMaterials(effectRoot.get());
			}
			else {
				REX::WARN(
					"[Smoking Guns][RuntimeAttachment] "
					"Shader resource manager was unavailable while "
					"preparing '{}'",
					a_nifPath);
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

	void ReleaseRetainedAttachments()
	{
		if (!retainedAttachments.empty()) {
			REX::DEBUG(
				"[Smoking Guns][RuntimeAttachment] "
				"Releasing {} retained runtime attachment(s)",
				retainedAttachments.size());
		}

		retainedAttachments.clear();
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

			if (const auto* retained = FindRetainedAttachment(
				a_treeRoot,
				runtimeName);
				retained && retained->runtimeNode) {
				REX::DEBUG(
					"[Smoking Guns][RuntimeAttachment] "
					"{} replacing detached runtime node '{}'",
					a_treeLabel,
					runtimeName);
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

			auto effectTemplate =
				LoadEffectTemplate(requirement.nifPath);

			if (!effectTemplate) {
				++result.loadFailed;
				continue;
			}

			auto effectRoot = CloneEffect(
				effectTemplate.get(),
				requirement.nifPath);

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

			RE::NiUpdateData updateData{};

			effectRoot->PreAttachUpdate(runtimeNode.get(), updateData);
			runtimeNode->AttachChild(effectRoot.get(), true);
			effectRoot->PostAttachUpdate();

			runtimeNode->PreAttachUpdate(located.parent, updateData);
			located.parent->AttachChild(runtimeNode.get(), true);
			runtimeNode->PostAttachUpdate();
			runtimeNode->UpdateTransformAndBounds(updateData);
			located.parent->UpdateUpwardPass(updateData);

			RetainAttachment(a_treeRoot, runtimeName, runtimeNode);

			const bool parentSet =
				runtimeNode->parent == located.parent;
			const bool parentHasChild = ParentContainsChild(
				located.parent,
				runtimeNode.get());
			const bool reachable =
				FindObject(a_treeRoot, runtimeName) == runtimeNode.get();
			const bool effectParentSet =
				effectRoot->parent == runtimeNode.get();
			const bool runtimeHasEffect = ParentContainsChild(
				runtimeNode.get(),
				effectRoot.get());
			if (parentSet && parentHasChild && reachable &&
				effectParentSet && runtimeHasEffect) {

				++result.attached;
			}
			else {
				++result.loadFailed;
			}

			if (parentSet && parentHasChild && reachable &&
				effectParentSet && runtimeHasEffect) {

				REX::INFO(
					"[Smoking Guns][RuntimeAttachment] "
					"{} attached '{}' as '{}' beneath '{}'",
					a_treeLabel,
					requirement.nifPath,
					runtimeName,
					located.parent->name.c_str());
			}
			else {
				REX::ERROR(
					"[Smoking Guns][RuntimeAttachment] "
					"{} attach verification for '{}' as '{}': "
					"parent='{}', parentSet={}, "
					"parentHasChild={}, reachable={}, "
					"effectParentSet={}, runtimeHasEffect={}",
					a_treeLabel,
					requirement.nifPath,
					runtimeName,
					located.parent->name.c_str(),
					parentSet,
					parentHasChild,
					reachable,
					effectParentSet,
					runtimeHasEffect);
			}
		}

		return result;
	}
}
