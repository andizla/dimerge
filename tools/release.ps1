# dimerge release: builds the package, tags the commit and publishes a GitHub release with the zip.
#
#   .\tools\release.ps1 -Version 2.0.0 -Message "short release title"
#   .\tools\release.ps1 -Version 2.0.0 -DryRun          build and check, no git or gh
#
# Before running: CHANGELOG.md has a "## [<Version>]" section (its text becomes the release notes), DIMERGE_VERSION in
# src\dimerge.h starts with the same major.minor, the working tree is committed and pushed. gh needs keyring access,
# so under the Claude Code sandbox run it with the sandbox off. Windows PowerShell 5.1.
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Message = '',
    [switch]$DryRun,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
function Fail($m) { Write-Host "FAIL: $m" -ForegroundColor Red; exit 1 }
function Step($m) { Write-Host "`n== $m" -ForegroundColor Cyan }

if ($Version -notmatch '^\d+\.\d+\.\d+$') { Fail "Version must look like 2.0.0 (got '$Version')" }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$header = (Select-String -Path "$root\src\dimerge.h" -Pattern '#define DIMERGE_VERSION "([^"]+)"').Matches[0].Groups[1].Value
if (-not $Version.StartsWith($header)) { Fail "src\dimerge.h says $header, the release says $Version" }

Step "release notes from CHANGELOG.md"
$lines = Get-Content "$root\CHANGELOG.md"
$start = -1; $end = $lines.Count
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match "^## \[$([regex]::Escape($Version))\]") { $start = $i + 1; continue }
    if ($start -ge 0 -and $lines[$i] -match '^## \[') { $end = $i; break }
}
if ($start -lt 0) { Fail "CHANGELOG.md has no '## [$Version]' section" }
if ($lines[$start - 1] -match 'unreleased') { Fail "the CHANGELOG header still says unreleased; date it first" }
$notes = ($lines[$start..($end - 1)] -join "`n").Trim()
if (-not $notes) { Fail "the CHANGELOG section is empty" }
Write-Host $notes

Step "git state"
Push-Location $root
try {
    $dirty = git status --porcelain
    if ($dirty) { Fail "uncommitted changes:`n$dirty" }
    $branch = git rev-parse --abbrev-ref HEAD
    git fetch --quiet origin
    $behind = git rev-list --count "HEAD..origin/$branch"
    $ahead = git rev-list --count "origin/$branch..HEAD"
    if ($behind -ne '0') { Fail "the branch is behind origin/$branch" }
    if ($ahead -ne '0') { Fail "unpushed commits on $branch; push first" }
    if (git tag -l "v$Version") { Fail "tag v$Version exists" }

    Step "package"
    if ($SkipBuild) { & "$root\tools\package.ps1" -SkipBuild | Out-Host } else { & "$root\tools\package.ps1" | Out-Host }
    $zip = "$root\release\dimerge-$header.zip"
    if (-not (Test-Path $zip)) { Fail "no zip at $zip" }
    $asset = "$root\release\dimerge-$Version.zip"
    Copy-Item $zip $asset -Force
    Write-Host "asset: $asset ($((Get-Item $asset).Length) bytes)"

    if ($DryRun) { Write-Host "`ndry run: nothing tagged or published"; exit 0 }

    Step "tag and publish"
    $title = if ($Message) { "dimerge $Version: $Message" } else { "dimerge $Version" }
    git tag -a "v$Version" -m $title
    git push origin "v$Version"
    $notesFile = "$root\release\notes-$Version.md"
    Set-Content -Path $notesFile -Value $notes -Encoding UTF8
    gh release create "v$Version" $asset --title $title --notes-file $notesFile
    if ($LASTEXITCODE) { Fail "gh release create failed" }
    Write-Host "`nreleased v$Version"
} finally { Pop-Location }
