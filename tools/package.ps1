# Builds the release folder and zip for dimerge: the proxy, the setup tool, the pak tools and the docs.
#   tools\package.ps1                 release\dimerge-<version>\ and release\dimerge-<version>.zip
#   tools\package.ps1 -SkipBuild      package what src\out and tools\setup already hold
# The wheel slot drop-in for mod.io is a separate step: tools\pakpatch\wheel_slots.py --export <vanilla initial.pak> <folder>
param([switch]$SkipBuild)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$version = (Select-String -Path "$root\src\dimerge.h" -Pattern '#define DIMERGE_VERSION "([^"]+)"').Matches[0].Groups[1].Value
if (-not $SkipBuild) {
    & cmd /c "`"$root\src\build.bat`""; if ($LASTEXITCODE) { throw "proxy build failed" }
    & cmd /c "`"$root\tools\setup\build.bat`""; if ($LASTEXITCODE) { throw "setup build failed" }
    & cmd /c "`"$root\tools\dienum\build.bat`""; if ($LASTEXITCODE) { throw "dienum build failed" }
}
$out = "$root\release\dimerge-$version"
if (Test-Path $out) { Get-ChildItem $out -Force | Remove-Item -Recurse -Force }   # the folder itself may be another shell's working directory
New-Item -ItemType Directory -Force "$out\tools\pakpatch", "$out\tools\keybinds" | Out-Null
Copy-Item "$root\src\out\dinput8.dll" $out
Copy-Item "$root\tools\setup\dimerge-setup.exe" $out
Copy-Item "$root\tools\dienum\dienum.exe" "$out\tools"
Copy-Item "$root\tools\pakpatch\wheel_slots.py" "$out\tools\pakpatch"
Copy-Item "$root\tools\keybinds\keybinds.py" "$out\tools\keybinds"
Copy-Item "$root\README.md", "$root\LICENSE" $out
$zip = "$root\release\dimerge-$version.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path "$out\*" -DestinationPath $zip
Get-ChildItem -Recurse $out | Where-Object { -not $_.PSIsContainer } | ForEach-Object { "{0,10}  {1}" -f $_.Length, $_.FullName.Substring($out.Length + 1) }
"zip: $zip ($((Get-Item $zip).Length) bytes)"
