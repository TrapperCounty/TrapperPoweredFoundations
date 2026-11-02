# Building TrapperPoweredFoundations

This repository contains the mod's C++ runtime/editor code, automated tests, configuration (where needed), and uncooked Unreal assets. It is a plugin, not a standalone Unreal project.

## Prerequisites

The released build used Satisfactory CL 513885, SML 3.12.0, the custom Coffee Stain Unreal Engine 5.6.1-CSS, Visual Studio 2022, and Wwise 2025.1.6.9117. Linux cross-compilation additionally uses v25 clang-18.1.0-rockylinux8. Follow the [official development setup](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html) for access and installation. Obtain the engine and game dependencies through their own authorized channels; they are not bundled here.

The local starter-project Git revision recorded at publication is `8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49` of [SatisfactoryModLoader](https://github.com/satisfactorymodding/SatisfactoryModLoader). That identifies the base checkout, not a claim that every local SDK file was tracked there.

## Layout and commands

Clone this repository into `<starter-project>/Mods/TrapperPoweredFoundations`. Do not nest another directory between the mod folder and its `.uplugin` file. Enable the plugin in the starter project and generate the project files using the configured custom engine. Finish Wwise integration/soundbank setup as required by the starter project.

From this repository, using PowerShell on Windows:

```powershell
.\scripts\Build-Mod.ps1 -ProjectRoot 'D:/Modding/SML' -EngineRoot 'D:/Modding/UnrealEngine-CSS' -Action Validate
.\scripts\Build-Mod.ps1 -ProjectRoot 'D:/Modding/SML' -EngineRoot 'D:/Modding/UnrealEngine-CSS' -Action BuildEditor
.\scripts\Build-Mod.ps1 -ProjectRoot 'D:/Modding/SML' -EngineRoot 'D:/Modding/UnrealEngine-CSS' -Action TestEditor
.\scripts\Build-Mod.ps1 -ProjectRoot 'D:/Modding/SML' -EngineRoot 'D:/Modding/UnrealEngine-CSS' -Action Package -Target Clients
```

Use `-Target WindowsServer` or `-Target LinuxServer` for dedicated-server builds; configure `LINUX_MULTIARCH_ROOT` for Linux. The client package builds Steam and Epic targets. Packaging uses the starter project's Alpakit `PackagePlugin` command; find outputs under its `Saved/ArchivedPlugins` directory. The helper does not install into or launch the game.

The committed assets are already generated. `-Action GenerateAssets` invokes the mod's editor commandlet and rewrites generated assets, so use it only when deliberately regenerating content. Review the asset diff afterward.

These portable helper commands were syntax/path-checked when published. The existing release binaries were retained, not rebuilt merely to publish the repository. See [validation](VALIDATION.md) for gameplay coverage.

## QA source

Opt-in integration harnesses are included in `Source/`. Some historical server/client helpers contain development-workstation paths and require a locally generated server join file. No credentials or join file are distributed. Ordinary tester play should use normal game launch with no QA flags. The private-save migration and factory-repair modes are development utilities, not installation steps. See [development notes](DEVELOPMENT-NOTES.md) and [testing](TESTING.md).
