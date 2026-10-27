# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
<# Remove superseded release bundles only after the retained bundle is complete.
   Windows uses the Recycle Bin. Unknown files and development trees are excluded. #>
param([Parameter(Mandatory=$true)][string]$KeepName, [switch]$PlanOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$distRoot = (Resolve-Path -LiteralPath (Join-Path $projectRoot 'dist')).Path
if ($distRoot -ne (Join-Path $projectRoot 'dist')) { throw 'Unexpected distribution path' }
if ((Get-Item -LiteralPath $distRoot).Attributes -band [IO.FileAttributes]::ReparsePoint) {
  throw 'Distribution directory must not be a reparse point'
}
if ($KeepName -match '^SLX-SolarPilot-(\d+\.\d+\.\d+)-rpi3a$') {
  # Embedded and Windows are separate products; retain one release per platform.
  $keepVersion = [version]$Matches[1]
  $embeddedRoot = (Resolve-Path -LiteralPath (Join-Path $distRoot 'embedded')).Path
  if ($embeddedRoot -ne (Join-Path $distRoot 'embedded')) { throw 'Unexpected embedded path' }
  if ((Get-Item -LiteralPath $embeddedRoot).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Embedded root is a reparse point' }
  $keepPath = Join-Path $embeddedRoot $KeepName
  $keepImage = Join-Path $keepPath ($KeepName + '.img')
  foreach ($required in @($keepPath, $keepImage)) {
    if ((Get-Item -LiteralPath $required).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Retained image is a reparse point' }
  }
  $reportRoot = Join-Path $projectRoot ('validation/embedded/' + $keepVersion)
  $manifest = Get-Content -LiteralPath (Join-Path $reportRoot 'release-manifest.json') -Raw | ConvertFrom-Json
  $bootReport = Get-Content -LiteralPath (Join-Path $reportRoot 'boot-report.json') -Raw | ConvertFrom-Json
  $imageReport = Get-Content -LiteralPath (Join-Path $reportRoot 'image-report.json') -Raw | ConvertFrom-Json
  if ([version]$manifest.version -ne $keepVersion -or $manifest.image -ne ($KeepName + '.img')) { throw 'Embedded manifest mismatch' }
  if ($bootReport.result -ne 'PASS' -or $imageReport.result -ne 'PASS' -or -not $imageReport.arm_hmi_runtime_tested) { throw 'Embedded checks not complete' }
  if ((Get-Item -LiteralPath $keepImage).Length -ne $manifest.bytes -or (Get-FileHash -LiteralPath $keepImage -Algorithm SHA256).Hash -ne $manifest.sha256) { throw 'Embedded image integrity mismatch' }
  $targets = @()
  foreach ($entry in Get-ChildItem -LiteralPath $embeddedRoot -Force) {
    if ($entry.Name -notmatch '^SLX-SolarPilot-(\d+\.\d+\.\d+)-rpi3a$') { continue }
    $entryVersion = [version]$Matches[1]
    if ($entryVersion -gt $keepVersion) { throw 'Newer embedded release exists' }
    if ($entryVersion -lt $keepVersion) {
      $resolved = (Resolve-Path -LiteralPath $entry.FullName).Path
      if ((Split-Path -Parent $resolved) -ne $embeddedRoot) { throw 'Cleanup target outside embedded distribution' }
      if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint -or -not $entry.PSIsContainer) { throw 'Unexpected embedded release type' }
      $targets += $entry
    }
  }
  if (-not $PlanOnly) {
    Add-Type -AssemblyName Microsoft.VisualBasic
    foreach ($entry in $targets) {
      [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($entry.FullName,
        [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,
        [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
    }
  }
  [pscustomobject]@{Retained=$KeepName; Platform='rpi3a'; PlanOnly=[bool]$PlanOnly; Removed=@($targets | ForEach-Object { $_.Name })} | ConvertTo-Json -Depth 3
  exit
}
if ($KeepName -notmatch '^SLX-SolarPilot-(\d+\.\d+\.\d+)-windows-x64$') {
  throw 'Only a final, versioned Windows release can replace earlier releases'
}
$keepVersion = [version]$Matches[1]
$keepPath = Join-Path $distRoot $KeepName
$archivePath = Join-Path $distRoot ($KeepName + '.zip')
$checksumPath = $archivePath + '.sha256'
foreach ($required in @($keepPath, $archivePath, $checksumPath)) {
  $item = Get-Item -LiteralPath $required
  if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Release contains a reparse point' }
}
$manifest = Get-Content -LiteralPath (Join-Path $keepPath 'release-manifest.json') -Raw | ConvertFrom-Json
if ($manifest.product -ne 'SLX SolarPilot' -or [version]$manifest.version -ne $keepVersion) {
  throw 'Retained release manifest does not match its folder'
}
$checksum = (Get-Content -LiteralPath $checksumPath -Raw).Trim()
$checksumPattern = '^([a-fA-F0-9]{64})\s{2}' + [regex]::Escape($KeepName + '.zip') + '$'
if ($checksum -notmatch $checksumPattern) { throw 'Invalid release archive checksum file' }
$expectedHash = $Matches[1]
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $expectedHash) {
  throw 'Release archive checksum mismatch; previous releases retained'
}
$targets = @()
foreach ($entry in Get-ChildItem -LiteralPath $distRoot -Force) {
  if ($entry.Name -notmatch '^SLX-SolarPilot-(\d+\.\d+\.\d+)(-(candidate|validation))?-windows-x64(\.zip(\.sha256)?)?$') {
    continue
  }
  $entryVersion = [version]$Matches[1]
  $isDraft = $Matches.ContainsKey(3)
  if ($entryVersion -gt $keepVersion) { throw 'A newer release exists; refusing a downgrade' }
  if ($entryVersion -lt $keepVersion -or ($entryVersion -eq $keepVersion -and $isDraft)) {
    $resolved = (Resolve-Path -LiteralPath $entry.FullName).Path
    if ((Split-Path -Parent $resolved) -ne $distRoot) { throw 'Cleanup target outside distribution directory' }
    if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Cleanup target is a reparse point' }
    $targets += $entry
  }
}
# Validate the complete target set before moving the first entry.
if (-not $PlanOnly) {
  # Preserve local identities, pairing, preferences and recordings outside the
  # factory ZIP before recycling the previous installation directory.
  $runtimeTarget = Join-Path $keepPath 'workspace'
  foreach ($previous in @($targets | Where-Object { $_.PSIsContainer } | Sort-Object @{Expression={[version]([regex]::Match($_.Name,'^SLX-SolarPilot-(\d+\.\d+\.\d+)').Groups[1].Value)};Descending=$true})) {
    $runtimeSource = Join-Path $previous.FullName 'workspace'
    if (-not (Test-Path -LiteralPath $runtimeSource)) { continue }
    $runtimeSource = (Resolve-Path -LiteralPath $runtimeSource).Path
    if ((Split-Path -Parent $runtimeSource) -ne $previous.FullName) { throw 'Unexpected runtime source' }
    $runtimeEntries = @(Get-Item -LiteralPath $runtimeSource) + @(Get-ChildItem -LiteralPath $runtimeSource -Recurse -Force)
    if (@($runtimeEntries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Runtime links cannot be migrated' }
    foreach ($runtimeFile in @($runtimeEntries | Where-Object { -not $_.PSIsContainer })) {
      $relative = $runtimeFile.FullName.Substring($runtimeSource.Length + 1)
      $runtimeDestination = [IO.Path]::GetFullPath((Join-Path $runtimeTarget $relative))
      if (-not $runtimeDestination.StartsWith($runtimeTarget + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Runtime target outside installation' }
      if (Test-Path -LiteralPath $runtimeDestination) {
        if ((Get-FileHash -LiteralPath $runtimeDestination).Hash -eq (Get-FileHash -LiteralPath $runtimeFile.FullName).Hash) { continue }
        $runtimeDestination = Join-Path (Join-Path $runtimeTarget ('migrated-' + $previous.Name)) $relative
        if ((Test-Path -LiteralPath $runtimeDestination) -and (Get-FileHash -LiteralPath $runtimeDestination).Hash -ne (Get-FileHash -LiteralPath $runtimeFile.FullName).Hash) { throw 'A different runtime backup exists; previous release retained' }
      }
      New-Item -ItemType Directory -Path (Split-Path $runtimeDestination) -Force | Out-Null
      Copy-Item -LiteralPath $runtimeFile.FullName -Destination $runtimeDestination
      if ((Get-FileHash -LiteralPath $runtimeDestination).Hash -ne (Get-FileHash -LiteralPath $runtimeFile.FullName).Hash) { throw 'Runtime migration incomplete; old release retained' }
    }
  }
  Add-Type -AssemblyName Microsoft.VisualBasic
  foreach ($entry in $targets) {
    if ($entry.PSIsContainer) {
      [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($entry.FullName,
        [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,
        [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
    } else {
      [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($entry.FullName,
        [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,
        [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
    }
  }
}
[pscustomobject]@{Retained=$KeepName; PlanOnly=[bool]$PlanOnly; Removed=@($targets | ForEach-Object { $_.Name })} | ConvertTo-Json -Depth 3
