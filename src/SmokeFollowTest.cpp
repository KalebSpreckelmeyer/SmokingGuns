#include "SmokeFollowTest.h"

#include <cstdint>
#include <string>
#include <vector>
#include <cmath>

#include <RE/Fallout.h>

namespace SmokingGuns::SmokeFollowTest
{
	namespace
	{
		constexpr char kWeaponEmitterNode[] =
			"SG_Runtime_EjectionPort";

		struct State
		{
			bool running{ false };

			RE::ObjectRefHandle hostHandle{};

			RE::NiPoint3 frozenHostPosition{};

			std::uint64_t tickCount{ 0 };

			bool rayMappingLogged{ false };

		};

		State g_state{};


		RE::NiAVObject* FindNode(
			RE::NiAVObject* a_root,
			const char* a_name)
		{
			if (!a_root ||
				!a_name ||
				!*a_name) {

				return nullptr;
			}

			RE::BSFixedString name{
				a_name
			};

			return a_root->GetObjectByName(
				name);
		}


		RE::NiAVObject*
			FindFirstPersonWeaponEmitter(
				RE::PlayerCharacter* a_player)
		{
			if (!a_player) {
				return nullptr;
			}

			//
			// For PlayerCharacter,
			// Get3D(true) = first-person tree.
			//
			auto* firstPersonRoot =
				a_player->Get3D(true);

			if (!firstPersonRoot) {
				return nullptr;
			}

			return FindNode(
				firstPersonRoot,
				kWeaponEmitterNode);
		}


		void LogRayRoundTrip(
			RE::NiCamera* a_camera,
			float a_inputX,
			float a_inputY,
			const char* a_label)
		{
			if (!a_camera) {
				return;
			}

			RE::NiPoint3 rayOrigin{};
			RE::NiPoint3 rayDirection{};

			a_camera->ViewPointToRay(
				a_inputX,
				a_inputY,
				&rayOrigin,
				&rayDirection);

			const float rayLength =
				std::sqrt(
					rayDirection.x * rayDirection.x +
					rayDirection.y * rayDirection.y +
					rayDirection.z * rayDirection.z);

			if (rayLength <= 0.0001F) {
				REX::WARN(
					"[Smoking Guns][SmokeFollowTest] "
					"RAYMAP {} input=({:.3f},{:.3f}) zero-length ray",
					a_label,
					a_inputX,
					a_inputY);

				return;
			}

			rayDirection.x /= rayLength;
			rayDirection.y /= rayLength;
			rayDirection.z /= rayLength;

			RE::NiPoint3 testPoint{
				rayOrigin.x + rayDirection.x * 100.0F,
				rayOrigin.y + rayDirection.y * 100.0F,
				rayOrigin.z + rayDirection.z * 100.0F
			};

			float outputX = 0.0F;
			float outputY = 0.0F;
			float outputZ = 0.0F;

			const bool projected =
				a_camera->WorldPtToScreenPt3(
					testPoint,
					outputX,
					outputY,
					outputZ,
					0.00001F);

			REX::INFO(
				"[Smoking Guns][SmokeFollowTest] "
				"RAYMAP {} "
				"input=({:.3f},{:.3f}) "
				"projected={} "
				"roundtrip=({:.3f},{:.3f},{:.3f})",
				a_label,
				a_inputX,
				a_inputY,
				projected,
				outputX,
				outputY,
				outputZ);
		}


		void Tick()
		{
			if (!g_state.running) {
				return;
			}

			auto* player =
				RE::PlayerCharacter::GetSingleton();

			if (!player) {
				return;
			}

			auto* playerCamera =
				RE::PlayerCamera::GetSingleton();

			if (!playerCamera) {
				return;
			}

			const bool isFirstPerson =
				playerCamera->QCameraEquals(
					RE::CameraState::kFirstPerson);

			auto host =
				g_state.hostHandle.get();

			if (!host) {
				REX::WARN(
					"[Smoking Guns][SmokeFollowTest] "
					"Host reference disappeared");

				g_state.running = false;
				return;
			}

			auto* weaponEmitter =
				FindFirstPersonWeaponEmitter(
					player);

			if (!weaponEmitter) {

				//
				// Don't spam this every frame.
				//
				if ((g_state.tickCount % 120) == 0) {
					REX::WARN(
						"[Smoking Guns][SmokeFollowTest] "
						"Could not find first-person "
						"weapon node '{}'",
						kWeaponEmitterNode);
				}

				++g_state.tickCount;
				return;
			}

			if (!isFirstPerson) {

				//
				// THIRD PERSON:
				// This path is already proven correct.
				//
				auto* thirdPersonRoot =
					player->Get3D(false);

				if (!thirdPersonRoot) {
					++g_state.tickCount;
					return;
				}

				auto* thirdPersonEmitter =
					FindNode(
						thirdPersonRoot,
						kWeaponEmitterNode);

				if (!thirdPersonEmitter) {
					++g_state.tickCount;
					return;
				}

				RE::NiPoint3 desiredHostPosition{
					thirdPersonEmitter->world.translate.x,
					thirdPersonEmitter->world.translate.y,
					thirdPersonEmitter->world.translate.z
				};

				host->SetLocationOnReference(
					desiredHostPosition);

				host->Update3DPosition(true);
			}
			else {

				auto* firstPersonRoot =
					player->Get3D(true);

				if (!firstPersonRoot) {
					++g_state.tickCount;
					return;
				}

				auto* firstPersonEmitter =
					FindNode(
						firstPersonRoot,
						kWeaponEmitterNode);

				if (!firstPersonEmitter) {
					++g_state.tickCount;
					return;
				}

				auto* cameraNode =
					FindNode(
						firstPersonRoot,
						"Camera");

				if (!cameraNode) {

					if ((g_state.tickCount % 120) == 0) {
						REX::WARN(
							"[Smoking Guns][SmokeFollowTest] "
							"Could not find first-person Camera node");
					}

					++g_state.tickCount;
					return;
				}

				//
				// Build a camera-relative vector from the 1P Camera
				// anchor to SG_EjectionPort_Runtime.
				//
				RE::NiPoint3 rawViewVector{
					firstPersonEmitter->world.translate.x -
						cameraNode->world.translate.x,

					firstPersonEmitter->world.translate.y -
						cameraNode->world.translate.y,

					firstPersonEmitter->world.translate.z -
						cameraNode->world.translate.z
				};

				//
				// Player angles are already in radians.
				//
				const auto& playerAngle =
					player->data.angle;

				const float pitch =
					playerAngle.x;

				const float yaw =
					playerAngle.z;

				//
				// Undo the view yaw first.
				//
				const float cosYaw =
					std::cos(yaw);

				const float sinYaw =
					std::sin(yaw);

				const float yawX =
					cosYaw * rawViewVector.x -
					sinYaw * rawViewVector.y;

				const float yawY =
					sinYaw * rawViewVector.x +
					cosYaw * rawViewVector.y;

				const float yawZ =
					rawViewVector.z;

				//
				// Then undo pitch around X.
				//
				const float cosPitch =
					std::cos(pitch);

				const float sinPitch =
					std::sin(pitch);

				RE::NiPoint3 stabilizedViewVector{
					yawX,

					cosPitch * yawY -
						sinPitch * yawZ,

					sinPitch * yawY +
						cosPitch * yawZ
				};

				auto* worldCamera =
					RE::Main::WorldRootCamera();

				if (!worldCamera) {
					++g_state.tickCount;
					return;
				}

				//
				// One-time diagnostic: determine the actual coordinate
				// convention used by NiCamera::ViewPointToRay by sending
				// known inputs through it and projecting them back.
				//
				if (!g_state.rayMappingLogged) {
					LogRayRoundTrip(
						worldCamera,
						0.0F,
						0.0F,
						"ZERO");

					LogRayRoundTrip(
						worldCamera,
						0.5F,
						0.5F,
						"HALF");

					LogRayRoundTrip(
						worldCamera,
						1.0F,
						1.0F,
						"ONE");

					LogRayRoundTrip(
						worldCamera,
						-0.5F,
						-0.5F,
						"NEG_HALF");

					g_state.rayMappingLogged =
						true;
				}

				//
				// Project the stabilized 1P muzzle directly into
				// normalized viewport coordinates.
				//
				// stabilizedViewVector axes:
				//   X = horizontal
				//   Y = forward/depth
				//   Z = vertical
				//
				if (stabilizedViewVector.y <= 0.01F) {
					++g_state.tickCount;
					return;
				}

				constexpr float kPi =
					3.14159265358979323846F;

				const float halfHorizontalFOV =
					playerCamera->firstPersonFOV *
					0.5F *
					(kPi / 180.0F);

				const float tanHalfHorizontalFOV =
					std::tan(
						halfHorizontalFOV);

				const float frustumWidth =
					worldCamera->viewFrustum.right -
					worldCamera->viewFrustum.left;

				const float frustumHeight =
					worldCamera->viewFrustum.top -
					worldCamera->viewFrustum.bottom;

				if (std::abs(frustumHeight) <= 0.0001F ||
					std::abs(tanHalfHorizontalFOV) <= 0.0001F) {

					++g_state.tickCount;
					return;
				}

				const float aspect =
					frustumWidth /
					frustumHeight;

				if (std::abs(aspect) <= 0.0001F) {
					++g_state.tickCount;
					return;
				}

				//
				// The RAYMAP diagnostic proved that ViewPointToRay()
				// does NOT take WorldPtToScreenPt3()'s normalized
				// 0..1 viewport coordinates.
				//
				// Measured mapping:
				//
				//   screenX = 0.5 + 0.5 * rayInputX
				//   screenY = 0.5 + 0.5 * aspect * rayInputY
				//
				// Inverting that mapping and combining it with the
				// first-person perspective projection simplifies to
				// using the same horizontal-FOV scale on both axes.
				//
				const float rayInputX =
					stabilizedViewVector.x /
					(
						stabilizedViewVector.y *
						tanHalfHorizontalFOV
						);

				const float rayInputY =
					stabilizedViewVector.z /
					(
						stabilizedViewVector.y *
						tanHalfHorizontalFOV
						);

				//
				// These are only for readable diagnostics. They show
				// where that ray should project in normalized 0..1
				// screen coordinates.
				//
				const float projectedScreenX =
					0.5F +
					0.5F *
					rayInputX;

				const float projectedScreenY =
					0.5F +
					0.5F *
					aspect *
					rayInputY;

				RE::NiPoint3 rayOrigin{};
				RE::NiPoint3 rayDirection{};

				worldCamera->ViewPointToRay(
					rayInputX,
					rayInputY,
					&rayOrigin,
					&rayDirection);

				const float rayLength =
					std::sqrt(
						rayDirection.x *
						rayDirection.x +
						rayDirection.y *
						rayDirection.y +
						rayDirection.z *
						rayDirection.z);

				if (rayLength <= 0.0001F) {
					++g_state.tickCount;
					return;
				}

				rayDirection.x /= rayLength;
				rayDirection.y /= rayLength;
				rayDirection.z /= rayLength;

				//
				// Preserve the viewmodel camera-to-muzzle distance.
				// This also keeps forward/back weapon animation instead
				// of flattening everything to one arbitrary depth.
				//
				const float muzzleDistance =
					std::sqrt(
						stabilizedViewVector.x *
						stabilizedViewVector.x +
						stabilizedViewVector.y *
						stabilizedViewVector.y +
						stabilizedViewVector.z *
						stabilizedViewVector.z);

				RE::NiPoint3 desiredHostPosition{
					rayOrigin.x +
						rayDirection.x *
							muzzleDistance,

					rayOrigin.y +
						rayDirection.y *
							muzzleDistance,

					rayOrigin.z +
						rayDirection.z *
							muzzleDistance
				};

				host->SetLocationOnReference(
					desiredHostPosition);

				host->Update3DPosition(
					true);

				if ((g_state.tickCount % 60) == 0) {
					const auto actualHostPosition =
						host->GetPosition();

					REX::INFO(
						"[Smoking Guns][SmokeFollowTest] "
						"1P-DYNAMIC "
						"stable=({:.2f},{:.2f},{:.2f}) "
						"ray=({:.4f},{:.4f}) "
						"projectedScreen=({:.4f},{:.4f}) "
						"fov={:.2f} aspect={:.4f} "
						"distance={:.2f} "
						"desired=({:.2f},{:.2f},{:.2f}) "
						"actual=({:.2f},{:.2f},{:.2f})",

						stabilizedViewVector.x,
						stabilizedViewVector.y,
						stabilizedViewVector.z,

						rayInputX,
						rayInputY,

						projectedScreenX,
						projectedScreenY,

						playerCamera->firstPersonFOV,
						aspect,

						muzzleDistance,

						desiredHostPosition.x,
						desiredHostPosition.y,
						desiredHostPosition.z,

						actualHostPosition.x,
						actualHostPosition.y,
						actualHostPosition.z);
				}
			}

			++g_state.tickCount;
		}

	}

	void OnPlayerUpdate()
	{
		if (g_state.running) {
			Tick();
		}
	}


	void DumpNodeTree(
		RE::NiAVObject* a_root)
	{
		if (!a_root) {
			return;
		}

		struct PendingNode
		{
			RE::NiAVObject* object{
				nullptr
			};

			std::uint32_t depth{
				0
			};
		};

		std::vector<PendingNode>
			pending;

		pending.push_back({
			a_root,
			0
			});

		while (!pending.empty()) {

			const auto current =
				pending.back();

			pending.pop_back();

			if (!current.object ||
				current.depth > 20) {

				continue;
			}

			std::string indent(
				current.depth * 2,
				' ');

			REX::INFO(
				"[Smoking Guns][SmokeFollowTest] "
				"{}{}",
				indent,
				current.object->name.c_str());

			auto* node =
				current.object->IsNode();

			if (!node) {
				continue;
			}

			for (auto& child :
				node->children) {

				if (!child) {
					continue;
				}

				pending.push_back({
					child.get(),
					current.depth + 1
					});
			}
		}
	}


	bool Start(
		RE::TESObjectREFR* a_hostRef)
	{
		if (g_state.running) {
			REX::WARN(
				"[Smoking Guns][SmokeFollowTest] "
				"Test already running");

			return false;
		}

		if (!a_hostRef) {
			REX::ERROR(
				"[Smoking Guns][SmokeFollowTest] "
				"Null host reference");

			return false;
		}

		auto* player =
			RE::PlayerCharacter::GetSingleton();

		if (!player) {
			REX::ERROR(
				"[Smoking Guns][SmokeFollowTest] "
				"Player unavailable");

			return false;
		}

		auto* weaponEmitter =
			FindFirstPersonWeaponEmitter(
				player);

		if (!weaponEmitter) {
			REX::ERROR(
				"[Smoking Guns][SmokeFollowTest] "
				"Could not find first-person "
				"weapon node '{}'",
				kWeaponEmitterNode);

			return false;
		}

		//
		// Log the initial first-person transforms once.
		//
		auto* firstPersonRoot =
			player->Get3D(true);

		if (firstPersonRoot) {

			auto* cameraNode =
				FindNode(
					firstPersonRoot,
					"Camera");

			if (cameraNode) {
				REX::INFO(
					"[Smoking Guns][SmokeFollowTest] "
					"1P root=({:.2f},{:.2f},{:.2f}) "
					"camera=({:.2f},{:.2f},{:.2f}) "
					"weapon=({:.2f},{:.2f},{:.2f})",

					firstPersonRoot->world.translate.x,
					firstPersonRoot->world.translate.y,
					firstPersonRoot->world.translate.z,

					cameraNode->world.translate.x,
					cameraNode->world.translate.y,
					cameraNode->world.translate.z,

					weaponEmitter->world.translate.x,
					weaponEmitter->world.translate.y,
					weaponEmitter->world.translate.z);

				if (cameraNode) {
					const auto* rtti =
						cameraNode->GetRTTI();

					REX::INFO(
						"[Smoking Guns][SmokeFollowTest] "
						"1P Camera object RTTI = '{}'",
						rtti && rtti->name ?
						rtti->name :
						"<null>");
				}
			}
		}

		auto handle =
			a_hostRef->GetHandle();

		if (!handle) {
			REX::ERROR(
				"[Smoking Guns][SmokeFollowTest] "
				"Could not acquire host handle");

			return false;
		}

		g_state.hostHandle =
			handle;

		g_state.frozenHostPosition =
			a_hostRef->GetPosition();

		g_state.tickCount = 0;

		g_state.rayMappingLogged =
			false;

		g_state.running = true;

		REX::INFO(
			"[Smoking Guns][SmokeFollowTest] "
			"STARTED existing host={:08X} "
			"frozenPos=({:.2f},{:.2f},{:.2f})",

			a_hostRef->GetFormID(),

			g_state.frozenHostPosition.x,
			g_state.frozenHostPosition.y,
			g_state.frozenHostPosition.z);


		return true;
	}


	bool Update()
	{
		//
		// Kept only because SGNative still exposes
		// UpdateSmokeFollowTest to Papyrus.
		//
		// It no longer drives Tick().
		//
		return g_state.running;
	}


	bool Stop()
	{
		if (!g_state.running) {
			return false;
		}

		g_state.running = false;

		auto host =
			g_state.hostHandle.get();

		REX::INFO(
			"[Smoking Guns][SmokeFollowTest] "
			"STOPPED host={:08X}",
			host ?
			host->GetFormID() :
			0);

		//
		// Intentionally don't delete or disable
		// the host during the prototype.
		//
		g_state.hostHandle.reset();

		return true;
	}


	bool IsRunning()
	{
		return g_state.running;
	}
}
