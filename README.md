# Smoking Guns F4SE

**Smoking Guns F4SE** is the native runtime component for **Smoking Guns**, a weapon-smoke framework for Fallout 4.

The plugin identifies the equipped weapon, reads authored Smoking Guns Parent Attach Points from the live assembled weapon tree, and injects the configured effect NIFs directly into the first- and third-person scene graphs.

## Purpose

Smoking Guns uses modular NIF effects and weapon behavior graphs to add persistent, state-based smoke effects to firearms.

The runtime injector does not require per-emitter OMOD, STAT, MISC, ACTI, Object Template, or attach-keyword records. Weapon authors provide real `BSConnectPoint::Parents` entries named `P-SG_*`; the plugin creates its own `SG_Runtime_*` nodes beneath those locators.

## Features

* Resolves the equipped weapon to a section-based INI profile.
* Traverses modular first- and third-person weapon trees for `P-SG_*` locators.
* Loads effect NIFs directly through Fallout's model database.
* Reconciles missing runtime nodes idempotently after weapon-tree rebuilds.
* Provides native functionality to the accompanying Papyrus scripts.
* Keeps smoke calculations and behavior-graph variable writes separate from scene injection.
* Built with multi-runtime Fallout 4 compatibility in mind.

## Weapon Profiles

Weapon profiles live in `Data/F4SE/Plugins/SmokingGuns/Weapons/*.ini`.

```ini
# .500 S&W Magnum
[SomeWeapon.esp|001234]

P-SG_EjectionPort = SmokingGuns\Effects\EjectionPortTest.nif
P-SG_MuzzleCenter = SmokingGuns\Effects\MuzzleSmoke.nif  # inline comments work
```

The section identifies the weapon by plugin and local FormID. Each entry maps an authored Parent Attach Point to an effect NIF path relative to `Data\Meshes`. Repeating a locator for the same weapon replaces its earlier mapping.

## Effect NIF Authoring

Effect NIFs are loaded as model-database templates and cloned into independent first- and third-person instances. The runtime wrapper supplies the Parent Attach Point transform, so the effect should be authored around local origin and does not need its own matching Child Attach Point or Creation Kit record.

Controller sequences must have unique names across every effect NIF active on the same weapon. Copying or renaming a NIF file does not rename its internal `NiControllerSequence` blocks. If two attached NIFs expose the same sequence name, a behavior-graph `pSequence` request may activate only one of them.

For example, use location-specific names such as:

```text
SG_10mm_EjectionPort_Fire_A
SG_10mm_EjectionPort_Fire_B
SG_10mm_MuzzleCenter_Fire_A
SG_10mm_MuzzleCenter_Fire_B
```

The weapon behavior graph must broadcast each location-specific sequence that should play. Controller palettes and controlled targets should remain self-contained within their effect NIF.

## Runtime Support

The project is being developed around CommonLibF4 with support planned for the mainstream Fallout 4 runtimes, including:

* Fallout 4 1.10.163 / F4SE 0.6.23
* Fallout 4 1.10.980
* Fallout 4 1.10.984
* Current 1.11.x runtimes

Fallout 4 VR is not currently targeted.

## Status

**Work in progress.**

The plugin is being developed alongside the main Smoking Guns framework, and its API and implementation may change substantially before release.

## Related Project

Smoking Guns is a modular visual-effects project that adds accumulating and dissipating weapon smoke to Fallout 4 firearms, with effects tied to individual weapon components and animation events.
