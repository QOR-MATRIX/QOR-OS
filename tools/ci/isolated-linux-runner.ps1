# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary

<#
.SYNOPSIS
    Creates qor-ci, a separate WSL distribution where a Linux CI job has no route to the owner's files or Windows programs.

.DESCRIPTION
    The Linux runner in the Ubuntu distribution runs as the owner, with C:\ and X:\ mounted, in the sudo and docker groups,
    and able to start Windows programs. So a job there can reach whatever the owner's Windows account can, and a job runs
    third-party code: pinned actions, and Rust crates' build scripts through the QOR core. Running a job hidden, serially
    or away from the GPU does not change that. This script builds a place where it is different.

    It is not done by /etc/wsl.conf. Measured on 1 November 2026: with automount and interop off, a process with no
    privilege still opened AF_VSOCK to Windows' 9P file server and attached to C:\. So a job runs in a sandbox the kernel
    enforces. tools/ci/isolated-linux-runner.sh, which this script sends in, explains what and why, and
    tools/ci/check_runner_boundary.sh tries each door.

    What remains, plainly: qor-ci shares one kernel and one VM with the other distributions, so a kernel exploit gets
    everything. A job has the internet. It is an honest step, and it is not a VM per job.

    Actions, each safe to run again:

        Create    (default) Install Ubuntu 24.04 from Microsoft's catalogue as qor-ci, provision and seal it, copy the Qt
                  kit in, and check the boundary. Never exports a distribution: the other one's home holds credentials.
        Check     Only the boundary check, from inside the sandbox. Exits non-zero and names each failure.
        Prove     A clone of this checkout's HEAD: configures, builds and tests the ci-linux preset, in the sandbox.
        Remove    Unregisters qor-ci, and with it everything in it.

    Second stage, kept apart. It registers a runner with GitHub, so nothing runs it unless someone asks for it by name:

        Register  Installs the Actions runner, then for each job in turn: an empty home, a registration token piped from
                  gh and never printed, one --ephemeral job in the sandbox. It runs until stopped. The Ubuntu runner and
                  the repository's variables are not touched: moving Build to this runner is a separate decision.

    It never runs wsl --shutdown, and terminates only qor-ci. Everything but Register needs no administrator and no GitHub
    login. The kit is streamed from the other distribution by name, and from nothing else in that home.

    Starting a second distribution is not free of effects, though. The kernel keeps one binfmt_misc table, which is how the
    other distribution starts Windows programs, and a distribution's first start flushes it before anything can be done
    about that. So Create repairs it at the end, whatever happened, and says what is in it. Measured on 1 November 2026,
    one sample a second through a removal and a Create from nothing: WSLInterop was never seen gone, and the other
    distribution's entry for Python's compiled files was gone for three minutes. If Windows programs started from the other
    distribution ever answer "Exec format error", this table is the first place to look.

.EXAMPLE
    .\tools\ci\isolated-linux-runner.ps1
    .\tools\ci\isolated-linux-runner.ps1 -Action Check
    .\tools\ci\isolated-linux-runner.ps1 -Action Prove
    .\tools\ci\isolated-linux-runner.ps1 -Action Register -Repo qor-os
    .\tools\ci\isolated-linux-runner.ps1 -Action Remove
#>
#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Create', 'Check', 'Prove', 'Remove', 'Register')]
    [string] $Action = 'Create',
    # Where the Qt kit is copied from.
    [string] $KitDistro = 'Ubuntu',
    # Under the kit distribution's home.
    [string] $KitRoot = 'Qt',
    [string] $Org = 'QOR-MATRIX',
    [string] $Repo = 'qor-os',
    [string] $RunnerName = "$env:COMPUTERNAME-qor-ci",
    # No "gpu" label: this runner has none. "isolated" lets a workflow ask for this runner alone.
    [string] $RunnerLabels = 'qt,gcc,wsl,isolated'
)

$ErrorActionPreference = 'Stop'
$env:WSL_UTF8 = '1'
$here = $PSScriptRoot
# Fixed, so that nothing here can ever be pointed at another distribution's terminate or unregister.
$distro = 'qor-ci'

function Test-Distro {
    return (wsl.exe --list --quiet | ForEach-Object { "$_".Trim() }) -contains $distro
}

function Assert-Distro {
    if (-not (Test-Distro)) { throw "There is no $distro yet. Run this script with no -Action first." }
}

# Seen once, on 1 November 2026, with another build filling WSL's memory: Wsl/Service/0x8007274c, a timeout starting the
# distribution. Why is not known. Three starts straight after, at the same memory, took under two seconds each, so it is
# not the usual thing, and a second try costs little.
function Start-Distro {
    foreach ($attempt in 1..3) {
        wsl.exe -d $distro -u root --exec true | Out-Host
        if ($LASTEXITCODE -eq 0) { return }
        Write-Host "$distro did not start: exit $LASTEXITCODE. Trying again in 30 seconds."
        Start-Sleep -Seconds 30
    }
    throw "$distro would not start. wsl --list --verbose says how things stand."
}

# Puts a file into the distribution, root's, with its bytes exact: LF line ends, nothing added.
function Send-File([string] $source, [string] $target, [string] $mode) {
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes([IO.File]::ReadAllText($source).Replace("`r`n", "`n"))
    $start = [Diagnostics.ProcessStartInfo]::new('wsl.exe')
    foreach ($argument in @('-d', $distro, '-u', 'root', '--exec', 'install', '-D', '-m', $mode, '/dev/stdin', $target)) {
        $start.ArgumentList.Add($argument)
    }
    $start.RedirectStandardInput = $true
    $start.UseShellExecute = $false
    $process = [Diagnostics.Process]::Start($start)
    $process.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    $process.StandardInput.Close()
    $process.WaitForExit()
    if ($process.ExitCode) { throw "Could not put $target into ${distro}: exit $($process.ExitCode)" }
}

function Invoke-Root {
    wsl.exe -d $distro -u root --cd / --exec @args
    if ($LASTEXITCODE) { throw "In ${distro}, as root, '$($args -join ' ')' failed: exit $LASTEXITCODE" }
}

function Install-Distro {
    if (Test-Distro) {
        Write-Host "$distro is already here"
        return
    }
    if (-not ((wsl.exe --list --online | ForEach-Object { "$_" }) -match '^\s*Ubuntu-24\.04\s')) {
        throw "Ubuntu-24.04 is not in Microsoft's catalogue on this machine: wsl --list --online"
    }
    Write-Host "Installing a fresh Ubuntu 24.04 from Microsoft's catalogue, as $distro"
    wsl.exe --install -d Ubuntu-24.04 --name $distro --no-launch
    if ($LASTEXITCODE) { throw "wsl --install failed: exit $LASTEXITCODE" }
}

function Copy-Kit {
    $kit = '/opt/qt/6.12.0/gcc_64'
    wsl.exe -d $distro -u root --exec test -x "$kit/bin/qmake"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "The Qt kit is already in $kit"
    }
    else {
        Write-Host "Streaming $KitRoot/6.12.0/gcc_64 from $KitDistro, and nothing else in that home"
        Invoke-Root mkdir -p /opt/qt
        # cmd's pipe, which carries bytes as they are. No Tools folder: the job needs none (docs/CI.md).
        cmd.exe /c "wsl.exe -d $KitDistro --cd ~/$KitRoot --exec tar --create --file=- 6.12.0/gcc_64 | wsl.exe -d $distro -u root --cd /opt/qt --exec tar --extract --file=- --no-same-owner"
        if ($LASTEXITCODE) { throw "Copying the kit failed: exit $LASTEXITCODE" }
        # A job must not be able to change the tools the next job trusts.
        Invoke-Root chmod -R go-w /opt/qt
    }
    # Refuses, rather than trusts, an account file that came with it.
    $found = wsl.exe -d $distro -u root --exec find /opt/qt /home/runner /root -iname 'qtaccount*' -o -iname 'network.xml' -o -iname 'MaintenanceTool*'
    if ($found) { throw "An account or installer file is in ${distro}: $found. Remove it and find out why." }
}

function Install-Inside {
    Start-Distro
    Send-File (Join-Path $here 'isolated-linux-runner.sh') '/usr/local/sbin/qor-ci-provision' '0700'
    Send-File (Join-Path $here 'check_runner_boundary.sh') '/usr/local/libexec/qor-ci-check' '0755'
}

function Restart-Distro {
    # The seal first: stopping with the kernel's binfmt_misc table mounted here empties it for everyone. Only qor-ci stops.
    Invoke-Root /usr/local/libexec/qor-ci-seal
    wsl.exe --terminate $distro | Out-Null
    if ($LASTEXITCODE) { throw "wsl --terminate $distro failed: exit $LASTEXITCODE" }
}

# What the other distribution's own boot registered, read from its files as its normal user, into the kernel's one table.
function Restore-SharedTable {
    try { Start-Distro }
    catch {
        Write-Warning "The kernel's binfmt_misc table was not checked: $_ In $KitDistro, try: cmd.exe /c ver"
        return
    }
    cmd.exe /c "wsl.exe -d $KitDistro --exec sh -c ""cat /usr/lib/binfmt.d/*.conf /etc/binfmt.d/*.conf 2>/dev/null"" | wsl.exe -d $distro -u root --exec /usr/local/sbin/qor-ci-provision restore-binfmt"
    if ($LASTEXITCODE) { Write-Warning "The kernel's binfmt_misc table may not be as it was. In $KitDistro, try: cmd.exe /c ver" }
}

function Invoke-Check {
    # Out-Host: in a function, a program's output would otherwise become the return value, and the report would be lost.
    wsl.exe -d $distro -u root --exec /usr/local/sbin/qor-ci-job /usr/local/libexec/qor-ci-check | Out-Host
    $code = $LASTEXITCODE
    if ($code) { Write-Host "The boundary check failed: exit $code" }
    return $code
}

function Invoke-Prove {
    # A bundle, because qor-os is private and nothing in qor-ci holds a credential.
    $bundle = Join-Path ([IO.Path]::GetTempPath()) "qor-ci-$PID.bundle"
    $commit = (git -C (Join-Path $here '..\..') rev-parse HEAD).Trim()
    git -C (Join-Path $here '..\..') bundle create --quiet $bundle HEAD | Out-Host
    if ($LASTEXITCODE) { throw "git bundle failed: exit $LASTEXITCODE" }
    try {
        cmd.exe /c "wsl.exe -d $distro -u root --exec install -o runner -g runner -m 0600 /dev/stdin /home/runner/qor-os.bundle < `"$bundle`"" | Out-Host
        if ($LASTEXITCODE) { throw "Copying the bundle failed: exit $LASTEXITCODE" }
    }
    finally { Remove-Item $bundle -Force -ErrorAction SilentlyContinue }
    Write-Host "Building $commit in the sandbox, in runner's home"
    wsl.exe -d $distro -u root --exec /usr/local/sbin/qor-ci-job /usr/local/libexec/qor-ci-prove $commit | Out-Host
    return $LASTEXITCODE
}

function Invoke-Register {
    # Second stage. The token goes from gh to stdin and is never in a variable here, on a command line or in a log.
    Invoke-Root /usr/local/sbin/qor-ci-provision runner-install
    $url = "https://github.com/$Org/$Repo"
    Write-Host "Serving $url as $RunnerName, one job at a time, each from an empty home. Ctrl+C stops it between jobs."
    while ($true) {
        Start-Distro
        # One clock for every distribution, and it drifts after sleep. GitHub refuses a runner whose clock is wrong (docs/CI.md).
        Invoke-Root date -u -s ((Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm:ss')) | Out-Null
        gh api -X POST "repos/$Org/$Repo/actions/runners/registration-token" --jq .token 2>$null |
            wsl.exe -d $distro -u root --exec /usr/local/sbin/qor-ci-provision runner-once $url $RunnerName $RunnerLabels
        if ($LASTEXITCODE) {
            Write-Host "That job's runner ended with exit $LASTEXITCODE. Trying again in a minute."
            Start-Sleep -Seconds 60
        }
    }
}

switch ($Action) {
    'Create' {
        Install-Distro
        try {
            Install-Inside
            Invoke-Root /usr/local/sbin/qor-ci-provision provision
            Copy-Kit
            Restart-Distro
        }
        finally { Restore-SharedTable }
        exit (Invoke-Check)
    }
    'Check' {
        Assert-Distro
        Install-Inside
        exit (Invoke-Check)
    }
    'Prove' {
        Assert-Distro
        Install-Inside
        exit (Invoke-Prove)
    }
    'Register' {
        Assert-Distro
        Install-Inside
        Invoke-Register
    }
    'Remove' {
        if (Test-Distro) {
            wsl.exe --unregister $distro
            if ($LASTEXITCODE) { throw "wsl --unregister $distro failed: exit $LASTEXITCODE" }
        }
        else { Write-Host "There is no $distro." }
    }
}
