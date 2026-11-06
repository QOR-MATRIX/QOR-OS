# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Deterministic-size derivative of the reviewed logo, without a graphics window.
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string] $Output)
$ErrorActionPreference = 'Stop'
$qorRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$qorBuild = [IO.Path]::GetFullPath((Join-Path $qorRepo 'build')) + [IO.Path]::DirectorySeparatorChar
$qorOutput = [IO.Path]::GetFullPath($Output)
if (-not $qorOutput.StartsWith($qorBuild, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The menu derivative must stay in the QOR build workspace.'
}
foreach ($qorPath in @($qorOutput, "$qorOutput.json", "$qorOutput.license")) {
    if (Test-Path -LiteralPath $qorPath) { throw 'Preserving existing output; use a fresh path.' }
}
$qorParent = [IO.DirectoryInfo]([IO.Path]::GetDirectoryName($qorOutput))
if (-not $qorParent.Exists) { throw 'Create the fresh output directory first.' }
while ($qorParent) {
    if ($qorParent.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked output ancestry is refused.' }
    $qorParent = $qorParent.Parent
}
& python (Join-Path $qorRepo 'tools/branding/check_assets.py')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$qorSource = Join-Path $qorRepo 'kit/branding/qor-mark.png'
Add-Type -AssemblyName System.Drawing
$qorImage = $null
$qorBitmap = $null
$qorGraphics = $null
$qorStream = $null
try {
    $qorImage = [Drawing.Image]::FromFile($qorSource)
    if ($qorImage.Width -ne 1024 -or $qorImage.Height -ne 1024) { throw 'The approved square source is required.' }
    $qorBitmap = [Drawing.Bitmap]::new(112, 112, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $qorBitmap.SetResolution(96, 96)
    $qorGraphics = [Drawing.Graphics]::FromImage($qorBitmap)
    $qorGraphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
    $qorGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $qorGraphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $qorGraphics.DrawImage($qorImage, [Drawing.Rectangle]::new(0, 0, 112, 112), 0, 0, 1024, 1024, [Drawing.GraphicsUnit]::Pixel)
    $qorStream = [IO.File]::Open($qorOutput, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $qorBitmap.Save($qorStream, [Drawing.Imaging.ImageFormat]::Png)
} finally {
    if ($qorStream) { $qorStream.Dispose() }
    if ($qorGraphics) { $qorGraphics.Dispose() }
    if ($qorBitmap) { $qorBitmap.Dispose() }
    if ($qorImage) { $qorImage.Dispose() }
}
[IO.File]::Copy("$qorSource.license", "$qorOutput.license", $false)
$qorRecord = [ordered]@{
    'SPDX-License-Identifier' = 'LicenseRef-QOR-OS-Proprietary'
    schema = 1
    source = 'kit/branding/qor-mark.png'
    source_sha256 = (Get-FileHash -LiteralPath $qorSource -Algorithm SHA256).Hash.ToLowerInvariant()
    width = 112
    height = 112
    sha256 = (Get-FileHash -LiteralPath $qorOutput -Algorithm SHA256).Hash.ToLowerInvariant()
    transform = 'GDI+ HighQualityBicubic; square aspect retained; no redraw'
}
[IO.File]::WriteAllText("$qorOutput.json", ($qorRecord | ConvertTo-Json) + "`n", [Text.UTF8Encoding]::new($false))
