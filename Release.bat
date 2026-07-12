cd /d %~dp0

robocopy "vcpkg_installed\gpp-x64-windows-release\gpp-x64-windows-release\share\opencc" "Example\BaseConfig\opencc" /E

robocopy "Example\BaseConfig" "Release\GPPCLI\BaseConfig" /E /XF "Python-3.12.10-embed-amd64.zip"
robocopy "Example\BaseConfig" "Release\GPPGUI\BaseConfig" /E /XF "Python-3.12.10-embed-amd64.zip"
robocopy "Example\BaseConfig" "Release\GUICORE\BaseConfig" /E ^
/XF "Python-3.12.10-embed-amd64.zip" "GlobalConfig.toml" ^
/XD "mecab" "Python-3.12.10-embed-amd64"

robocopy "3rdParty" "Release\GPPCLI" 7z.dll
robocopy "3rdParty" "Release\GPPGUI" 7z.dll
robocopy "3rdParty" "Release\GUICORE" 7z.dll

robocopy "Example\SampleProject" "Release\GPPCLI\SampleProject" /E

pause
