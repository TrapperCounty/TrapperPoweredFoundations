# Install the GitHub beta

Download the named platform ZIP from [release 0.1.5](https://github.com/TrapperCounty/TrapperPoweredFoundations/releases/tag/v0.1.5).

| File | Purpose |
|---|---|
| TrapperPoweredFoundations-0.1.5-Windows.zip | Windows Steam or Epic game client |
| TrapperPoweredFoundations-0.1.5-WindowsServer.zip | Windows dedicated server |
| TrapperPoweredFoundations-0.1.5.zip | Combined Windows client/server archive for SMR |
| TrapperPoweredFoundations-0.1.5-Source.zip | Developer source snapshot; not installable |
| SHA256SUMS.txt | Package checksums |

1. Back up your save and close Satisfactory.
2. Install a game-compatible Satisfactory Mod Loader, normally through Mod Manager. This release was tested with CL 513885 / SML 3.12.0 and requires SML ^3.12.0. Newer game versions are not automatically validated.
3. Create `<game install>/FactoryGame/Mods/TrapperPoweredFoundations/`.
4. Extract the **contents** of the Windows ZIP into that folder. The descriptor must be at `<game install>/FactoryGame/Mods/TrapperPoweredFoundations/TrapperPoweredFoundations.uplugin`, with Binaries and Content beside it. Avoid an extra nested Windows or mod-name folder.
5. Start normally and confirm Trapper's Powered Foundations 0.1.5 in the loaded mod list. Buy the shop unlock and follow the player guide.

For the combined archive, extract only the appropriate target folder's contents. For a Windows server, stop it, back up its saves and use the WindowsServer ZIP in the equivalent server mod folder; install the same version on every client. No Linux server build is included.

This mod's ID is **TrapperPoweredFoundations**. **PoweredFoundations** belongs to a different author. Do not rename folders or enable private migration flags as part of installation. Existing installations of TrapperPoweredFoundations can be backed up and replaced with this version while the game is closed. Keep matching mods installed when loading saves that use their buildings.

Do not install GitHub's automatic Source code downloads as a game mod. Compare downloaded files with SHA256SUMS.txt using `Get-FileHash -Algorithm SHA256` in PowerShell.

Folder placement follows the [official manual-install guide](https://docs.ficsit.app/satisfactory-modding/latest/ManualInstallDirections.html). The wall fix was tested on Windows Steam and a local Windows dedicated server. The Windows client package includes the Epic binary.
