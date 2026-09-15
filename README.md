# Smoking Guns F4SE

**Smoking Guns F4SE** is the native runtime component for **Smoking Guns**, a weapon-smoke framework for Fallout 4.

The plugin provides runtime support for identifying weapons and their installed components, gathering the information needed by the smoke system, and assigning the appropriate Smoking Guns attachments automatically.

## Purpose

Smoking Guns uses modular NIF effects and weapon behavior graphs to add persistent, state-based smoke effects to firearms.

Because Fallout 4 weapons cannot reliably and automatically attach new attachments natively this plugin will handle that aspect of integration in addition to other supporting features for the Smoking Guns mod. 

## Features

* Inspects equipped weapon and attachment information at runtime.
* Determines which Smoking Guns support attachments are applicable to a weapon configuration.
* Applies the appropriate smoke-system attachments automatically.
* Provides native functionality to the accompanying Papyrus scripts.
* Designed to minimize the amount of weapon-specific Creation Kit wiring required by Smoking Guns.
* Built with multi-runtime Fallout 4 compatibility in mind.

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
