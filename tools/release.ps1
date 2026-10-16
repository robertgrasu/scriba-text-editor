# Makes the zip that is given to somebody to try Scriba:  .\tools\release.ps1 [-Name Scriba-test]
# The program is built WITHOUT the test bypass of the license (SCRIBA_TEST_BYPASS=OFF) in the same build folder; at the end the folder is made again
# with the bypass ON, so the scripted tests (tests\run-all.ps1) keep working.
# Result: dist\<Name>-<date>.zip (Scriba.exe, README.txt, the licenses of the third party parts) and dist\symbols-<date>\ (Scriba.map and .pdb of exactly
# this build: keep them, they are needed to read a crash.txt that comes from this exe; they do NOT go to the other person).
param([string]$Name = "Scriba-test")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$b = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
$cm = "$b\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$nj = "$b\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$env:VSCMD_ARG_HOST_ARCH = "x64"
function Build([string]$bypass) {
  $out = cmd /c "`"$b\VC\Auxiliary\Build\vcvars64.bat`" >nul && cd /d `"$root`" && `"$cm`" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=`"$nj`" -DSCRIBA_TEST_BYPASS=$bypass >nul && `"$cm`" --build build 2>&1"
  if ($LASTEXITCODE -ne 0) { $out | Select-Object -Last 15; throw "the build failed" }
}
# the program must not be running (the linker cannot replace it)
Get-Process Scriba -ErrorAction SilentlyContinue | ForEach-Object { $_.CloseMainWindow() | Out-Null }
Start-Sleep 3
Get-Process Scriba -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

Build "OFF"
$date = Get-Date -Format "yyyyMMdd-HHmm"
$dist = "$root\dist"
$pack = "$dist\$Name-$date"
$sym = "$dist\symbols-$date"
New-Item -ItemType Directory $pack, $sym -Force | Out-Null
Copy-Item "$root\build\Scriba.exe" "$pack\Scriba.exe"
Copy-Item "$root\build\Scriba.map", "$root\build\Scriba.pdb" $sym -ErrorAction SilentlyContinue
Copy-Item "$PSScriptRoot\release-readme.txt" "$pack\README.txt"
Copy-Item "$root\THIRD_PARTY.md" "$pack\THIRD_PARTY.md"
Copy-Item "$root\third_party\licenses" "$pack\licenses" -Recurse
$zip = "$dist\$Name-$date.zip"
Compress-Archive -Path "$pack\*" -DestinationPath $zip -Force
$hash = (Get-FileHash "$pack\Scriba.exe" -Algorithm SHA256).Hash
"{0}  Scriba.exe" -f $hash | Set-Content "$pack\..\$Name-$date.sha256.txt" -Encoding ascii

Build "ON"   # the folder is for the tests again
"zip:     $zip  ({0:N1} MB)" -f ((Get-Item $zip).Length / 1MB)
"symbols: $sym"
"sha256 of Scriba.exe: $hash"
