# Makes a zip of the program.
#   .\tools\release.ps1                       a build WITH the license check, to give to somebody to try  (dist\Scriba-test-<date>.zip)
#   .\tools\release.ps1 -Free -Version 0.2.0  the public build WITHOUT any license check               (dist\Scriba-0.2.0-windows-x64.zip)
# Both are built without the test bypass of the license (SCRIBA_TEST_BYPASS=OFF), in the build folder; at the end the folder is made again
# the way the scripted tests need it (bypass ON, license check ON).
# Next to the zip: dist\symbols-<name>\ (Scriba.map and .pdb of exactly this build: keep them, they are needed to read a crash.txt that comes from
# this exe; they do NOT go to the other person) and a .sha256.txt of the exe.
param([switch]$Free, [string]$Version = "0.2.0")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$b = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
$cm = "$b\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$nj = "$b\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$env:VSCMD_ARG_HOST_ARCH = "x64"
function Build([string]$bypass, [string]$free) {
  $out = cmd /c "`"$b\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && cd /d `"$root`" && `"$cm`" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=`"$nj`" -DSCRIBA_TEST_BYPASS=$bypass -DSCRIBA_FREE=$free >nul && `"$cm`" --build build 2>&1"
  if ($LASTEXITCODE -ne 0) { $out | Select-Object -Last 15; throw "the build failed" }
}
# the program must not be running (the linker cannot replace it)
Get-Process Scriba -ErrorAction SilentlyContinue | ForEach-Object { $_.CloseMainWindow() | Out-Null }
Start-Sleep 3
Get-Process Scriba -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

Build "OFF" $(if ($Free) { "ON" } else { "OFF" })
$date = Get-Date -Format "yyyyMMdd-HHmm"
$name = if ($Free) { "Scriba-$Version-windows-x64" } else { "Scriba-test-$date" }
$dist = "$root\dist"
$pack = "$dist\$name"
$sym = "$dist\symbols-$name"
Remove-Item $pack, $sym -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $pack, $sym -Force | Out-Null
Copy-Item "$root\build\Scriba.exe" "$pack\Scriba.exe"
Copy-Item "$root\build\Scriba.map", "$root\build\Scriba.pdb" $sym -ErrorAction SilentlyContinue
Copy-Item "$PSScriptRoot\$(if ($Free) { 'release-readme-free.txt' } else { 'release-readme.txt' })" "$pack\README.txt"
Copy-Item "$root\THIRD_PARTY.md" "$pack\THIRD_PARTY.md"
Copy-Item "$root\third_party\licenses" "$pack\licenses" -Recurse
$zip = "$dist\$name.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path "$pack\*" -DestinationPath $zip -Force
$hash = (Get-FileHash "$pack\Scriba.exe" -Algorithm SHA256).Hash
"{0}  Scriba.exe" -f $hash | Set-Content "$dist\$name.sha256.txt" -Encoding ascii
# a copy of the exe that was just built, to test it (the folder build\ is changed back below)
Copy-Item "$root\build\Scriba.exe" "$dist\$name-check.exe" -Force

Build "ON" "OFF"   # the folder is for the tests again
"zip:     $zip  ({0:N1} MB)" -f ((Get-Item $zip).Length / 1MB)
"symbols: $sym"
"sha256 of Scriba.exe: $hash"
