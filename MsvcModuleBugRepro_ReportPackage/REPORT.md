# MSVC 14.51 regression: C1116 importing module IFC containing a function template specialization

## Summary

MSVC 14.51.36231 fails with error C1116 when importing a C++20 module IFC that contains a function template specialization:

```text
repro::PiecewiseCombiner::finalize<repro::MixingHashState>
```

The same minimal project builds successfully with MSVC 14.50.35717.

This appears to be a regression in C++20 module IFC import/deserialization of a function template specialization. The original real-world project hits the same compiler path through Abseil, where the failing specialization is:

```text
absl::lts_20260107::hash_internal::PiecewiseCombiner::finalize<absl::lts_20260107::hash_internal::MixingHashState>
```

The attached repro is independent of Abseil and vcpkg.

## Environment

Good toolset:

```text
D:\VisualStudio2026\MSBuild\Current\Bin\amd64\MSBuild.exe
MSBuild 18.5.4
MSVC 14.50.35717
```

Bad toolset:

```text
D:\Microsoft Visual Studio\MSBuild\Current\Bin\amd64\MSBuild.exe
MSBuild 18.6.3
MSVC 14.51.36231
```

Target:

```text
Configuration: Release
Platform: x64
PlatformToolset: v145
LanguageStandard: /std:c++latest
C++ modules: enabled
STL modules: enabled
```

## Repro Steps

From the parent directory of `MsvcModuleBugRepro`, run:

```powershell
& 'D:\VisualStudio2026\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'MsvcModuleBugRepro\MsvcModuleBugRepro.vcxproj' `
  /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal
```

Expected/actual with MSVC 14.50.35717: build succeeds.

Then run:

```powershell
& 'D:\Microsoft Visual Studio\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'MsvcModuleBugRepro\MsvcModuleBugRepro.vcxproj' `
  /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal
```

Expected with MSVC 14.51.36231: build succeeds.

Actual with MSVC 14.51.36231:

```text
minihash.hpp(84,7): error C1116: unrecoverable error importing module "Exporter".
Specialization of "repro::PiecewiseCombiner::finalize" with arguments "repro::MixingHashState".
IFC import detected. If possible, follow https://aka.ms/report-cpp-modules-problem
```

## Notes

The repro has only local source code:

```text
MsvcModuleBugRepro.vcxproj
Exporter.ixx
Importer.ixx
minihash.hpp
```

`Exporter.ixx` includes `minihash.hpp` in the global module fragment, exports module `Exporter`, and exports an inline function that instantiates the template path.

`Importer.ixx` includes the same header in the global module fragment, exports module `Importer`, and imports `Exporter`.

MSVC 14.51 fails while compiling `Importer.ixx`, when importing `Exporter.ixx.ifc`.

This is not a syntax error in the source: MSVC 14.50 accepts the same project, and MSVC 14.51 reports an unrecoverable IFC import error instead of a normal diagnostic.
