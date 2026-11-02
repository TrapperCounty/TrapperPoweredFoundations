# Tester checklist

Install **0.1.4** using [INSTALLATION.md](INSTALLATION.md), then use a backed-up or separate test save. No Unreal/editor tools are needed to play-test. Use normal launch without developer QA flags.

Test 1m/2m/4m snapping and mixed-height stacks, feed poles and side outlets, power loss/reconnection, all five Customizer finishes, blueprint placement and reload, and inventory/refund behavior.

For multiplayer, install the same mod version on the server and every client. Record whether an operation was performed by a host or joining client. Test both mods together only after checking each alone so a failure can be isolated.

The priority gaps are listed in [VALIDATION.md](VALIDATION.md). Do not treat packaged targets as gameplay-tested platforms.

## Report a result

Open [Issues](https://github.com/TrapperCounty/TrapperPoweredFoundations/issues/new/choose), choose Bug report, and include:

- Mod version, game build, SML version, platform and host/client role.
- Reproduction steps and expected versus actual behavior.
- Whether the issue survives saving/reloading and whether other mods are installed.
- Relevant screenshots and a small relevant log excerpt, after removing private information.

Windows logs are normally in `%LOCALAPPDATA%/FactoryGame/Saved/Logs/FactoryGame.log`. Saves are optional; do not post credentials, server join data or private saves publicly. A successful test report can be filed as an ordinary issue describing the environment and checks completed.

Developers can find opt-in test code in Source and historical harness instructions in DEVELOPMENT-NOTES.md. Some QA modes create fixtures, change inventory temporarily, or perform save-specific repairs. Those modes are not needed for normal testing.
