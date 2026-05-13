# MSVC 14.51 module IFC repro

This is a minimal repro for the GalTranslPP x64 Release build failure seen with
`D:\Microsoft Visual Studio` Insiders.

The key trigger is importing a C++20 module interface whose IFC contains a
function-template specialization shaped like:

```cpp
repro::PiecewiseCombiner::finalize<repro::MixingHashState>
```

`minihash.hpp` is a small local extraction of that template shape. It does not
depend on Abseil or vcpkg. GalTranslPP hits the same compiler path through
`absl::flat_hash_map`, where MSVC reports:

```text
absl::hash_internal::PiecewiseCombiner::finalize<absl::hash_internal::MixingHashState>
```

Expected results on this machine:

```powershell
& 'D:\VisualStudio2026\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'MsvcModuleBugRepro\MsvcModuleBugRepro.vcxproj' `
  /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal

# succeeds with MSVC 14.50.35717

& 'D:\Microsoft Visual Studio\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'MsvcModuleBugRepro\MsvcModuleBugRepro.vcxproj' `
  /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal

# fails with MSVC 14.51.36231:
# error C1116: unrecoverable error importing module "Exporter"
# specialization of repro::PiecewiseCombiner::finalize
# with repro::MixingHashState
```
