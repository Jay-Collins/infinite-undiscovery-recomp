# Runtime compliance packaging

The playable release links a source-built ReXGlue runtime containing LGPL
FFmpeg and libmspack. Publish matching source and relink materials as a separate
archive, alongside the playable ZIP.

After building the SDK, source `build-env.ps1` and run:

```powershell
python release/make-compliance.py
```

Use a Python 3 interpreter. The script expects the SDK's pinned Windows x64
Release Ninja build and recorded `local-build.json`. It creates fresh staging
under `out/releases`, copies tracked SDK and submodule source files, and extracts
the actual runtime link command and its object/library inputs. Source archives
omit Git metadata, build products, installed game files and runtime profiles.

Run `relink.ps1` and `rebuild-lgpl.ps1` from the staging archive in the developer
environment. Compare the resulting exports with the shipped runtime using
`llvm-readobj --coff-exports`. Include only source, scripts, notices, original
link objects/libraries, exports.def, relink.rsp and SDK provenance in the ZIP;
exclude verification outputs and the `sdk-source/out` and `rebuild` directories.
The rc2 runtime has 10,169 exports. Keep checksums for both published archives.
