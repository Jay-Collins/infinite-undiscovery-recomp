# Windows build

Install Visual Studio 2022 Build Tools with the C++ workload and a Windows SDK,
and LLVM/Clang. This release was built with Clang 20.1.8, MSVC 14.44.35207,
Windows SDK 10.0.26100.0, CMake 3.31.6 and Ninja 1.12.1. CMake and Ninja can
come from the Visual Studio tools.

## SDK

Run from the repository root in PowerShell:

```powershell
git clone https://github.com/rexglue/rexglue-sdk.git tools/rexglue-sdk
git -C tools/rexglue-sdk checkout f5337cdc947ff6d4c4196737e2c807a48f2a1fc2
git -C tools/rexglue-sdk submodule update --init --recursive
./build-sdk.ps1 -Jobs 8
```

The helper finds the compiler environment, applies
`patches/rexglue/content-header-fallback.patch`, builds and installs the SDK
under `tools/rexglue-install`, and records its revision, patches, toolchain and
DLL hashes in `local-build.json`. The patch fixes enumeration of imported
saves with missing or truncated content headers. No saves are distributed.

The build and packaging steps check those DLL hashes. Without
`IU_LOCAL_REXGLUE_HASH_FILE`, the original upstream binary hashes remain the
default; the obsolete precompiled SDK binaries are rejected in either mode.

## Game and developer build

1. Put your supported Disc 1 executable at `assets/default.xex`.
2. Run `./build-local.ps1 -Jobs 8`.
3. Run `out/build/local-release/InfiniteUndiscoveryRecomp.exe` and import your
   own media with the setup wizard.

The manifest and function boundaries originate from the upstream PAL build.
The USA executable was validated through code generation and gameplay in this
fork. Do not assume arbitrary modified executables are compatible.

`run-codegen.ps1` generates the CMake integration and game code with the pinned
CLI. It stops on validation failures. Its `-Diagnostic` mode permits output
despite codegen errors and is only for investigation.

The local build enables developer diagnostics. To exercise the checks:

```powershell
. ./build-env.ps1
cmake --build out/build/local-release --target iu_trace_control_tests iu_orb_release_tests iu_content_header_tests iu_asset_setup_tests
ctest --test-dir out/build/local-release --output-on-failure
```

`./build-local.ps1 -SetupTestsOnly` runs the asset/profile checks without game
files. On a fresh checkout, the codegen bootstrap emits an expected missing-XEX
error while creating the SDK CMake integration.

## Public release

```powershell
./release/make-release.ps1 -Jobs 8
```

This creates a fresh Release build with `IU_ENABLE_DIAGNOSTICS=OFF` and no
recompilation debug information. The orb fix and fiber continuation fix remain
enabled. Recovery UI, trace wrappers and profiling hooks are compiled out;
SDK debug-overlay and console shortcuts are unregistered. Runtime settings,
achievements, disc switching, profile selection and normal saves remain.

Packaging copies an explicit allowlist into a new directory under
`out/releases`, verifies SDK hashes and diagnostic-marker absence, and scans
the staging directory and ZIP with Microsoft Defender. It never copies the
runtime's profiles, game assets, saves, shaders, logs, configuration or test
executables. Existing package paths are not overwritten.

For an already built Release target, source `build-env.ps1` and use
`release/package-release.ps1 -BuildDir <build-directory>`.

Publish matching LGPL source/relink materials alongside the playable ZIP; see
`release/COMPLIANCE.md`. The SDK source changes and link inputs must correspond
to the DLLs actually shipped, rather than another release's binaries.

## Verified for rc5

- Seventeen host regression checks pass, including Save Anywhere safety/dispatch, difficulty unlock scoping and persistence, save backup retention, restore recovery after interruption, process restart handoff, menu pause and save-in-progress exit protection.
- Diagnostics and timing experiments are disabled in the public Release build.
- Save backups, settings restart, menu pause and save-in-progress protection still need in-game verification, including pause behavior during cutscenes.
- The user confirmed Save Anywhere during safe gameplay. Broader live combat/cutscene checks and manual difficulty-selection verification remain pending; see `SAVE-ANYWHERE.md` and `DIFFICULTY-UNLOCK.md`.
- The SDK binaries are unchanged from rc2; its matching LGPL source/relink archive also applies to rc5.

## Previously verified for rc2

- Ten host regression checks and the asset/profile suite pass.
- The orb test also compiles without any diagnostic headers.
- A fresh installation reaches the Content Profile Manager without game data.
- The user confirmed the release build can load the private reproduction save,
  examine the pedestal and open the door with the Orb of Patience.
- See [orb investigation](ORB-INVESTIGATION.md) for the original diagnosis.

SDK sources, game inputs, saves, generated code, local evidence and build
outputs remain under ignored directories.
