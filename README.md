<p align="center">
  <img src="docs/images/banner.png" alt="Infinite Undiscovery" width="512">
</p>
<h1 align="center">RECOMP</h1>

A native Windows x64 recompilation of **Infinite Undiscovery** for Xbox 360, built with ReXGlue / XenonRecomp.

This is **Jay-Collins's fork** of [doc-haz's Infinite Undiscovery Recomp](https://github.com/doc-haz/infinite-undiscovery-recomp). Credit for the original recompilation and portable setup goes to doc-haz and the contributors listed below.

This fork fixes the Orb of Patience pedestal interaction in Vesplume Tower: the Examine prompt appears and using the orb opens the door, verified in-game on the reproduction save. [Technical details and validation](docs/ORB-INVESTIGATION.md).

You must supply your own legally obtained game media. Game executables, disc data, DLC packages and saves are not included.

## Downloads and setup

Download the [v1.0.0-rc3 Windows x64 release](https://github.com/Jay-Collins/infinite-undiscovery-recomp/releases/tag/v1.0.0-rc3) for this fork's orb fix, unified menu, save backups, settings restart action and menu pause. The playable ZIP contains no game files, saves or developer diagnostic tools. The separate LGPL source/relink archive is for developers and is not needed to play.

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

The game automatically backs up existing save slots at launch and changed slots when the game closes their save container. Backups live in `<profile>/backups/saves/`, include the slot's files and content header, and retain the latest 10 distinct versions per slot. Unchanged saves do not create duplicate versions. These are copies of saved progress, not autosaves or save states.

Use **F4 → Game → Save backups** to choose a timestamped backup and **Restore and restart**. Only that slot is replaced, after the old game process exits. The current slot is copied first; this recovery copy may temporarily exceed normal retention. Interrupted restores roll back at the next launch. Backup integrity is checked, but this does not establish that the game's saved progress is valid. Empty/incomplete recovery copies are retained for inspection and cannot be restored through the menu. Backups stay within their content profile, and only one game session can use a profile at a time.

[IU Save Bridge](https://github.com/doc-haz/iu-save-bridge) is the original project's separate save editor, with Xbox 360 / STFS save import and backup tools. Save compatibility between editions should not be assumed.

## Utility controls

Player utilities share one tabbed menu:

| Key | Action |
| --- | --- |
| F4 | Open / close the unified menu |
| F7 | Open its Achievements tab |
| F10 | Return to the Content Profile Manager |
| F12 | Quit the game (with confirmation) |

The top tabs are **Settings**, **Controls**, **Achievements** and **Game**. Settings cover display, graphics and audio; Controls configures keyboard/mouse emulation. The Game tab provides profile-manager and quit actions. Use **Save settings** to keep changes for the active content profile; starred settings require a restart. Opening the menu pauses gameplay at the next frame boundary and blocks game input; closing it resumes play. This also applies when opening it with Back + Start or F7. Background music continues. The footer shows when the game has reached the pause point.

When starred settings differ from the running session, **Save settings and restart...** appears. Confirming saves the settings and relaunches the same executable and content profile. It does **not** save game progress: save in the game first and wait for saving to finish. A settings-write or launch failure keeps the current session running.

Display settings separate **Aspect ratio**, **Display resolution** and **Internal resolution**. Aspect ratio preserves the original picture or stretches it to the display; it does not add an ultrawide field of view. Borderless fullscreen uses the desktop resolution, while windowed mode offers size presets. Internal resolution offers uniform 1–8× native scaling, independent of window size.

Graphics offers **30 FPS (original)** and **60 FPS (experimental)**. Save settings and restart to apply the frame-rate target. Keep VSync enabled. The 60 FPS option uses the game's existing elapsed-time scheduler; first-use shader/asset loading hitches may remain, and videos retain their own playback rate. Full-game compatibility at 60 FPS is not yet verified.

Save backups, settings restart and menu pause pass automated host tests; their in-game integration and pause behavior during cutscenes still need verification. This is a release candidate.

The release omits the diagnostic/recovery menu, Save Anywhere, Safe Step and trace recorders. The SDK debug-overlay and console shortcuts are unregistered. Normal runtime error logging remains available.

Developers can enable the extra tools with `IU_ENABLE_DIAGNOSTICS=ON`; `build-local.ps1` does this for local investigation builds. They are disabled by default and in `release/make-release.ps1`.

## Compatibility

Full-game completion and later Disc 2 progression have not been verified in this fork. The [upstream release notes](https://github.com/doc-haz/infinite-undiscovery-recomp/releases/tag/v1.0.0-rc1) report a successful in-game Disc 1 to Disc 2 transition; we have not independently repeated that test here.

DLC installation is implemented, but complete in-game verification of both vouchers' effects remains outstanding. Hardware-specific rendering issues, unusual aspect ratios and later-game bugs may still occur.

## Building from source

A developer build needs a supported game executable, the ReXGlue SDK, CMake 3.25 or newer, Ninja, Clang with C++23 support, and the MSVC / Windows SDK environment.

The SDK is pinned to **ReXGlue v0.10.0**, revision `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`, with the checked-in content-header patch for imported save enumeration. SDK DLL hashes are recorded and checked during the build and packaging process.

See [Windows build instructions](docs/LOCAL-BUILD.md) for toolchain setup, SDK compilation, code generation and release packaging. The SDK, generated game code and proprietary game data are excluded from this repository.

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
