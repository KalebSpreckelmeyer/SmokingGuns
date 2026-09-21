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

The runtime checks for rebuilt or missing effect nodes on an equip hint and roughly every 0.5 seconds while the player updates. Locators absent from the currently installed parts and NIF files that cannot load are retried quietly after the first warning for each view; switching weapons or resolving the issue resets the warning.

The player alias script calls `SGNative.ReportEquipped` when a weapon is equipped and after a save loads with a weapon already equipped. That native call computes `SmokeImpulse` and writes `SmokeImpulse` and `SmokeDecayImpulse` to the weapon graphs. `ReportEquipped` is the only Papyrus function this runtime registers.

## Weapon Profiles

Weapon profiles live in `Data/F4SE/Plugins/SmokingGuns/Weapons/*.ini`.

```ini
# .500 S&W Magnum
[SomeWeapon.esp|001234]

P-SG_EjectionPort = SmokingGuns\Effects\EjectionPortTest.nif
P-SG_MuzzleCenter = SmokingGuns\Effects\MuzzleSmoke.nif  # inline comments work
```

The section identifies the weapon by plugin and local FormID. Each entry maps an authored Parent Attach Point to an effect NIF path relative to `Data\Meshes`. Repeating the exact same key for the same weapon replaces its earlier mapping. To place multiple effects at one locator, add a dot-suffix identity; the part before the dot remains the physical locator:

```ini
P-SG_EjectionPort.Fire = SmokingGuns\Effects\SG_WeaponFire_EjectionPort_Small.nif
P-SG_EjectionPort.Constant = SmokingGuns\Effects\SG_Constant_EjectionPort.nif
```

Both entries resolve `P-SG_EjectionPort`, but receive independent runtime nodes (`SG_Runtime_EjectionPort_Fire` and `SG_Runtime_EjectionPort_Constant`).

### Reload smoke

Add `ReloadMode = Explicit` for a weapon whose animations contain purpose-authored smoke timing annotations, or `ReloadMode = Timed` for fallback timing from vanilla reload annotations plus delays authored in the behavior graph or effect NIF. Omitting `ReloadMode` (or specifying `None`) disables both reload paths. The plugin does not interpret animation annotations or schedule particle bursts itself; the weapon behavior graph must do that work.

```ini
[SomeWeapon.esp|001234]
ReloadMode = Explicit
P-SG_EjectionPort = SmokingGuns\Effects\EjectionPortSmoke.nif
```

The plugin writes two **integer** variables to each available weapon graph on its periodic reconciliation tick:

| Mode | `SG_Reload_Enabled` | `SG_Reload_Explicit` |
| --- | ---: | ---: |
| Omitted / `None` | 0 | 0 |
| `Timed` | 1 | 0 |
| `Explicit` | 1 | 1 |

Declare both as integer graph variables with default 0. Gate the explicitly annotated reload branch on `SG_Reload_Enabled == 1 && SG_Reload_Explicit == 1`; gate the vanilla-annotation fallback on `SG_Reload_Enabled == 1 && SG_Reload_Explicit == 0`. Both branches may address the same configured effect NIF if it has the needed controller sequences. The profile's `P-SG_*` entries specify the effect files and locations; the behavior graph chooses when and which sequence fires. There is no reload emission when the mode is absent, provided the graph applies these gates.

### Save-load graph refresh

Declare `SG_GraphRefresh` as an **integer** variable with default 0 in each compatible weapon graph. After loading a save, the plugin writes 0 and then 1 on successive player updates for each available first- and third-person graph with `SG_FrameworkVersion == 1`. If a graph is not ready yet, the plugin retries until it can write the value. Use the transition to 1 to initialize the smoke decay logic, and have the graph reset the variable to 0 after consuming it. Writing 0 first ensures a new transition even if the saved graph variable was already 1.

### Repeated locators on assembled parts

When several installed components expose the same `P-SG_*` Parent Attach Point, the injector selects the deepest valid matching parent in the live first- or third-person scene tree. If a rebuild changes the preferred point, it moves its retained runtime node to that point. Equal-depth matches remain ambiguous: it selects the first in stable child order and logs a warning. In particular, receiver, barrel, and muzzle components may be siblings in an assembled tree. Verify their actual hierarchy in game before relying on automatic muzzle precedence; depth alone cannot identify a muzzle among siblings.

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
