Infinite Undiscovery Recomp v1.0.0-rc2 - Runtime source/relink materials
===================================================================

This archive corresponds to the rexruntime.dll shipped with Jay-Collins's rc2.
It is separate from the playable download and contains no game data or saves.

Contents:
- sdk-source/: ReXGlue at f5337cdc947ff6d4c4196737e2c807a48f2a1fc2 and all
  vendored submodule sources, including FFmpeg and libmspack.
- SDK_BUILD.json: compiler versions, submodule revisions, patch hashes and
  the SHA-256 hashes of the shipped DLLs.
- SDK_CHANGES.patch: runtime source differences from the pinned SDK.
- objects/, libraries/: the linkable work using the LGPL libraries, plus
  the original static libraries needed by the exact link recipe.
- exports.def, relink.rsp: runtime export list and portable link arguments.
- LICENSES/: third-party license texts.

The runtime patch corrects content-header enumeration for imported saves.
Windows symlink placeholders in libmspack were materialized as source files.
The source archive's thirdparty/CMakeLists.txt checks for source directories
instead of .git metadata; this packaging-only change allows offline rebuilding.
The library sources themselves are unchanged by that adjustment.

Rebuild/relink:
1. Extract to a short path, such as C:/iu-lgpl, to avoid Windows path limits.
2. Open an x64 Visual Studio 2022 developer PowerShell. Add LLVM 20.1.8,
   CMake and Ninja to PATH. See SDK_BUILD.json for the tested versions.
3. Run ./relink.ps1 to relink with the supplied libraries and objects.
4. To modify FFmpeg or libmspack, edit sdk-source/thirdparty/FFmpeg or
   sdk-source/thirdparty/libmspack and run ./rebuild-lgpl.ps1. It rebuilds
   all three LGPL libraries, replaces those copies in libraries/, and relinks.
5. Back up the runtime DLL in your portable game folder before replacing it
   with your relinked rexruntime.dll. Keep the matching rexgpu-xenos.dll.

The scripts need the Windows SDK and MSVC system libraries, which are not
redistributed here. The SDK version resolver may warn that Git tags are absent
in this source archive; this does not prevent the library-only rebuild.

Validation performed:
- Relinking the original supplied objects/libraries succeeded.
- Rebuilding FFmpeg's libavcodec/libavutil and libmspack from the bundled
  sources and relinking succeeded.
- Both outputs preserve the shipped runtime's 10,169 exported symbols.
These checks do not guarantee gameplay compatibility for arbitrary changes.

FFmpeg: 0604b464c7cb4ebc94940cf1f324a3b26b87717c, LGPL-2.1-or-later.
libmspack: 305907723a4e7ab2018e58040059ffb5e77db837, LGPL-2.1.
See the included license texts and the playable ZIP's THIRD_PARTY_NOTICES.txt.
