# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
<# Runs the product's registered CTest suites and requires fresh, complete matching evidence. -InvalidateOnly clears old
   records before configuration/build, so a failed build cannot leave earlier passing evidence looking current. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Build,
    [string] $CTest = 'ctest',
    [string] $Python = 'python',
    [switch] $InvalidateOnly
)
$ErrorActionPreference = 'Stop'
$testing = Join-Path $Build 'Testing'
if (Test-Path -LiteralPath $testing) {
    foreach ($attempt in Get-ChildItem -LiteralPath $testing -Directory -Force) {
        $record = Join-Path $attempt.FullName 'Test.xml'
        if (Test-Path -LiteralPath $record) { Remove-Item -LiteralPath $record -Force -ErrorAction Stop }
    }
}
if ($InvalidateOnly) { return }

# Product suites drive the same desktop. An inherited CTEST_PARALLEL_LEVEL must not overlap their windows and input.
& $CTest --test-dir $Build -T Test --no-tests=error --parallel 1 --output-on-failure
$testCode = $LASTEXITCODE
& $Python (Join-Path $PSScriptRoot 'products-report.py') $Build
$reportCode = $LASTEXITCODE
if ($testCode -ne 0) { throw "CTest failed ($testCode)." }
if ($reportCode -ne 0) { throw "The product test report could not be read ($reportCode)." }
& $Python (Join-Path $PSScriptRoot 'products-report.py') --passed $Build
if ($LASTEXITCODE -ne 0) { throw 'The CTest log and record do not prove a complete passing run.' }
