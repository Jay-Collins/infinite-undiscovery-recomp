<p align="center">
  <img src="docs/images/banner.png" alt="Infinite Undiscovery" width="512">
</p>
<h1 align="center">RECOMP</h1>

A native Windows x64 recompilation of **Infinite Undiscovery** for Xbox 360, built with ReXGlue / XenonRecomp.

This is **Jay-Collins's fork** of [doc-haz's Infinite Undiscovery Recomp](https://github.com/doc-haz/infinite-undiscovery-recomp). Credit for the original recompilation and portable setup goes to doc-haz and the contributors listed below.

This fork fixes the Orb of Patience pedestal interaction in Vesplume Tower: the Examine prompt appears and using the orb opens the door, verified in-game on the reproduction save. [Technical details and validation](docs/ORB-INVESTIGATION.md).

You must supply your own legally obtained game media. Game executables, disc data, DLC packages and saves are not included.

## Downloads and setup

The original project's [v1.0.0-rc1 portable release](https://github.com/doc-haz/infinite-undiscovery-recomp/releases/tag/v1.0.0-rc1) is an **upstream build**. It does not include this fork's orb fix; that change is available in this repository's source.

Requirements for the Windows build:

- 64-bit Windows 10 or Windows 11.
- A Direct3D 12 capable GPU.
- Microsoft Visual C++ Redistributable 2015–2022 (x64).

To use a portable build:

1. Extract it to a writable folder and run `InfiniteUndiscoveryRecomp.exe`.
2. Select your Disc 1 media in the Asset Setup Wizard. It accepts supported XDVDFS ISO images or extracted disc folders containing `default.xex`, `ud1.bin` and `ud2.bin`.
3. Add Disc 2, either during initial setup or later through the Content Profile Manager. It is needed for later progression.
4. Optionally import your **A Voucher** and **B Voucher** DLC packages in STFS format.

The wizard validates the media and identifies its edition automatically. Supported profiles are **USA, USA-UNDUB, EUROPE, JAPAN and ASIA**; arbitrary modified media or other undub variants are not guaranteed to pass validation. Setup and the utility menu offer English and Spanish.

See [Asset Setup](ASSET_SETUP.md) for the detailed workflow and troubleshooting.

## Saves and portable data

Each content profile stores its assets, saves, shaders, cache, logs and configuration under the application folder. For example, the USA profile's saves are in `USA/saves/`. Profiles do not share saves automatically; moving the complete application folder preserves its portable data.

[IU Save Bridge](https://github.com/doc-haz/iu-save-bridge) is the original project's separate save editor, with Xbox 360 / STFS save import and backup tools. Save compatibility between editions should not be assumed.

## Utility controls

These are the default registered bindings; they can be changed in the runtime configuration.

| Key | Action |
| --- | --- |
| F5 | Open or close the Game Menu |
| F6 | Save Anywhere |
| F8 | Safe Step Forward |
| F9 | Undo Debug Move |
| F10 | Return to the Content Profile Manager |
| F12 | Quit the game |

The Game Menu also provides buttons for these actions and session diagnostics. If the Save Anywhere shortcut does not respond, use its menu button, which worked during our in-game testing. The menu and recovery actions require `community_debug` to be enabled (the default).

Save Anywhere and the movement tools are recovery/debug features. They can be unavailable during native dialogs or other guarded game states.

## Compatibility

Full-game completion and later Disc 2 progression have not been verified in this fork. The [upstream release notes](https://github.com/doc-haz/infinite-undiscovery-recomp/releases/tag/v1.0.0-rc1) report a successful in-game Disc 1 to Disc 2 transition; we have not independently repeated that test here.

DLC installation is implemented, but complete in-game verification of both vouchers' effects remains outstanding. Hardware-specific rendering issues, unusual aspect ratios and later-game bugs may still occur.

## Building from source

A developer build needs a supported game executable, the ReXGlue SDK, CMake 3.25 or newer, Ninja, Clang with C++23 support, and the MSVC / Windows SDK environment.

The manifest targets **ReXGlue v0.10.0**. The runtime DLL verification script identifies SDK source revision `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2` and checks exact DLL hashes; an independently built SDK may require corresponding build configuration changes.

1. Prepare the SDK and your supported executable at `assets/default.xex`.
2. Generate the game code and SDK CMake integration with ReXGlue. The checked-in [codegen helper](run-codegen.ps1) expects the CLI at `tools/rexglue/rexglue.exe`.
3. Configure and build with CMake using the Windows x64 presets in [CMakePresets.json](CMakePresets.json).

The SDK, generated game code and proprietary game data are excluded from this repository. This is a developer workflow, not a self-contained one-command build.

[Project History](PROJECT_HISTORY.md) preserves the upstream development timeline.

## Credits

This project builds on work from the Xbox 360 recompilation community.

Special thanks to:

- **Magna** — for major help with development, testing, portability, the Asset Setup Wizard, localization and preparing the portable build.
- **Premium** — for development help and support throughout the project.
- **Diesel** — for extensive help with source auditing, ReXGlue rebuild verification, ABI compatibility checks, release engineering, compliance validation and final RC1 preparation.
- **[vs-sr-dev / pc-infiniteundiscovery](https://github.com/vs-sr-dev/pc-infiniteundiscovery)** — for extensive reverse-engineering research, documentation and analysis tools for Infinite Undiscovery and the ASKA engine.
- **[dotslash (freefrank) / LostOdysseyRecomp](https://github.com/freefrank/LostOdysseyRecomp)** — for the Lost Odyssey native PC recompilation project and its importer and portable workflow, which were useful references during development.
- **ReXGlue / XenonRecomp contributors** — for the tooling and groundwork that make projects like this possible.
- Everyone testing the recomp, reporting bugs, providing saves and documenting reproduction cases.

## License and attribution

The project code is licensed under the [BSD-3-Clause license](LICENSE). Third-party notices and license texts are in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) and [LICENSES](LICENSES/). The upstream RC release provides a separate LGPL source and relink compliance archive for its runtime binaries.

Infinite Undiscovery and its logo belong to their respective rights holders. This project is unofficial and is not affiliated with or endorsed by Square Enix, tri-Ace or Microsoft. The code license does not grant rights to the game, its artwork or external dependencies.
