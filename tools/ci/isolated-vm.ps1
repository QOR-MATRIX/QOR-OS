# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary

<#
.SYNOPSIS
    Makes qor-ci-vm, a clean Linux VM on this laptop with its own kernel, and attacks it from inside to see what gets out.

.DESCRIPTION
    Both of CI's self-hosted runners are stopped, because a job on them could reach the Store's signing keys and the owner's
    credentials. WSL cannot fix that: any process in any distribution opens AF_VSOCK to Windows' 9P server and attaches to C:\,
    and the distributions share a kernel (docs/CI.md). A classic Hyper-V VM has its own kernel. Whether that is enough here is
    what -Action Prove tries, and nothing here says so until it has run.

    NOTHING IN THIS SCRIPT HAS RUN AGAINST HYPER-V. Its first run is the owner's, once, as administrator, after reading it.
    Holds no Qt, no kit, no Qt login, no runner, no GitHub token and no repository variable, and sets none of them.

    Actions:

        Create   As administrator. Makes everything below, saying each step first, and stops at the first failure with the
                 reason. Run it again after fixing one: it does what is left. Leaves the VM running, for Prove.
        Check    As administrator. Reads back all of it against what it should be. Changes nothing.
        Start    As administrator. Starts the VM. See "Starting and stopping" below.
        Prove    From an ordinary terminal, NOT as administrator. Attacks from inside the guest, as a user with no privilege and
                 then as root, and powers it off after (-KeepRunning to leave it up). Exits 0 only if every line passed.
        Stop     From an ordinary terminal. Powers the guest off from inside.
        Remove   As administrator. Takes away everything below, by exact name, and nothing else.

    What it makes, all of it, and nothing else on this machine is changed:

        VM              qor-ci-vm
        Virtual switch  qor-ci-vm (internal), and its adapter "vEthernet (qor-ci-vm)" with one address
        NAT             qor-ci-vm
        Firewall rules  qor-ci-vm-block-by-adapter, qor-ci-vm-block-by-address
        Folders         -Root (default X:\qor-ci-vm): image, disk, seed, VM files
                        %LOCALAPPDATA%\qor-ci-vm: the SSH key that opens this guest and nothing else, and Prove's reports

    The image. Ubuntu 24.04's generic cloud image, ubuntu-24.04-server-cloudimg-amd64.img, by dated URL and SHA-256, the digest
    read from SHA256SUMS, whose signature by Ubuntu's image key was checked. Why this one, of what Ubuntu publishes:

        It takes its first-boot settings from a NoCloud seed: a small ISO labelled cidata, on a DVD drive. Ubuntu documents this
        for running its images locally. Canonical's Multipass does the same on Hyper-V, read in its source: an image converted to VHDX, on a
        generation 2 VM with the seed as a DVD. Its kernel package carries hv_vmbus, hv_storvsc, hv_netvsc and hv_sock (the file
        list of linux-modules-6.8.0-147-generic), and the DVD driver is built in (BLK_DEV_SR=y).

        Not the Azure VHD, though Hyper-V could attach it with no conversion: its manifest carries walinuxagent, an agent that
        waits on Azure's fabric, which would be running as root in a VM that has none. Not the Hyper-V Quick Create image: a
        desktop (Canonical's hyper-v/desktop folder), which the guides for it use with Enhanced Session, which this VM must not
        have. Not the server ISO: an unattended install from it stops at "Continue with autoinstall? (yes|no)" on the console
        unless its boot line is changed (Ubuntu's autoinstall documentation).

        It is QCOW2, and Hyper-V wants VHDX, with no converter in Windows. Convert-Qcow2ToVhd below is one, in this file. Not
        qemu-img from WSL, which is where the jobs ran, so nothing from there goes into a VM that is meant to be clean. Not a
        qemu-img downloaded and run as administrator. The reader takes only a file whose digest has been checked, refuses every
        feature it does not know, and the result's GPT checksums are checked before it is used.

    Unproven until the owner runs it: that this image boots with Secure Boot on (Multipass turns Secure Boot off, so that is no
    evidence for it, and this script will not turn it off: that is the owner's decision); that the reader is right about the real
    file, since the image was not downloaded while writing this, and it has read only files written from the format's description;
    that Linux's own ISO reader takes the seed, which only blkid and libarchive have read; that WinNAT agrees to a NAT beside WSL's
    and the Default Switch's, and that the rule on the adapter leaves it working; everything about the network and the firewall below.

    What blocks what, for a classic VM. From Microsoft's documents and this machine, 1 November 2026:

        The "Hyper-V firewall" does not cover this. Its rules are per VMCreatorId, for "containers hosted by Windows, including
        WSL", and Get-NetFirewallHyperVVMCreator here lists WSL alone. A VM from New-VM is not one of those, so this script makes
        no such rule.

        Windows Firewall's rules are inbound and outbound, for this machine's own traffic. There is no rule for traffic it
        forwards, and Microsoft's documents do not say its rules stop what WinNAT sends on to the LAN. So they are not relied on
        for the LAN. They do cover a packet that arrives for this machine, so they are what stands in front of its one address on
        this switch, and "explicit block rules take precedence over any conflicting allow rules".

        What does stand between the guest and the LAN is the virtual switch's port ACL (Add-VMNetworkAdapterAcl), which applies to
        the guest's adapter, below the guest's kernel and before NAT, so root in the guest cannot change it. "The longest match
        rule decides." Microsoft's page for it is written for Windows Server. The cmdlet is here on Windows 11 Pro, and whether
        the switch here enforces it is for Prove to show.

        So: the ACL denies every private, link-local, multicast and loopback range and all of IPv6, allows the rest, and allows
        the one address SSH's replies need. That address is what the firewall rules guard. An ACL has no state, so it cannot
        tell a reply from a new connection.

    What is left, plainly: a public address that leads back to this machine or its router is not a private range, so nothing
    above refuses it. A guest can reach the internet on any port, so it can send out whatever it is given. ssh.exe talks to the
    guest, so a bug in it is a road back. Root in the guest with code in its own kernel could speak VMBus directly, which
    Prove does not try. It shares this laptop's CPU and memory, and the hypervisor, whose own bugs nothing here can test.

    Starting and stopping. This account cannot call Hyper-V: Administrators is deny-only until elevated, and it is not in Hyper-V
    Administrators. Hyper-V has no per-VM right to start one: Grant-VMConnectAccess gives the console and nothing else. So:

        Stopping needs no privilege while the guest cooperates: SSH, then poweroff from inside. That is -Action Stop.

        Starting has no way round. The choices, none of them made here:
          1. The owner elevates for each start: -Action Start. Changes nothing. A person sees each one.
          2. A task the owner makes once, as administrator, running Start-VM -Name qor-ci-vm and nothing else, whose permissions
             give this account Read and Execute and not Write. It can start this one VM and nothing more.
          3. This account in Hyper-V Administrators, which is "complete and unrestricted access to all the features in Hyper-V",
             for everything that runs as this account. A VM can be pointed at any disk, so that is the machine.
          4. Leave it running, which holds its memory all the time.
        Recommended: 1 until a job needs it, as nothing uses this VM yet, and then 2, by a decision of its own. Not 3 or 4.

.EXAMPLE
    # In PowerShell 7, as administrator, once, after reading this file:
    .\tools\ci\isolated-vm.ps1 -Action Create
    # Then in an ordinary PowerShell 7, not as administrator:
    .\tools\ci\isolated-vm.ps1 -Action Prove
    # And to undo all of it, as administrator:
    .\tools\ci\isolated-vm.ps1 -Action Remove
#>
#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Help', 'Create', 'Check', 'Start', 'Prove', 'Stop', 'Remove')]
    [string] $Action = 'Help',
    # Where the image, the disk and the VM's files live. X: has the room here (186 GB free on 1 November 2026, C: 80).
    [string] $Root = 'X:\qor-ci-vm',
    # A /29 of its own. .1 is this machine's end, .2 the guest. Create stops if this machine already uses any of it.
    [string] $Network = '10.213.57.0',
    # This laptop has 15.7 GB, with under 1 GB free during a build, and WSL can take 7.6. Static, and stopped when idle, so a
    # start that does not fit is refused by Hyper-V instead of squeezing what is running. One heavy job at a time (docs/CI.md).
    [ValidateRange(2, 8)]
    [int] $MemoryGB = 6,
    # Prove powers the guest off when it ends, as only an administrator can start it again. This leaves it running.
    [switch] $KeepRunning
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$script:Asked = @{}
foreach ($name in $PSBoundParameters.Keys) { $script:Asked[$name] = $PSBoundParameters[$name] }

# Exact and fixed, so that nothing here can be pointed at anything else.
$script:VmName = 'qor-ci-vm'
$script:SwitchName = 'qor-ci-vm'
$script:NatName = 'qor-ci-vm'
$script:AdapterName = 'vEthernet (qor-ci-vm)'
$script:RuleByAdapter = 'qor-ci-vm-block-by-adapter'
$script:RuleByAddress = 'qor-ci-vm-block-by-address'
$script:Mark = 'Made by tools/ci/isolated-vm.ps1, and -Action Remove takes it away. Holds no credential.'
$script:RootMarker = '.made-by-qor-ci-vm'
$script:GuestUser = 'qor'

$script:ImageUrl = 'https://cloud-images.ubuntu.com/releases/noble/release-20261017/ubuntu-24.04-server-cloudimg-amd64.img'
$script:ImageName = 'ubuntu-24.04-server-cloudimg-amd64-20261017.img'
$script:ImageSha256 = '04e8f2461a16a44e78a370284471f29616cdd9baf1893df25807b71f985b6c94'
$script:ImageBytes = 621284352

# The project's ceiling for compilers (docs/CI.md), of this laptop's 20 threads.
$script:VmProcessors = 8
# A limit, not a size: the VHDX grows as the guest writes.
$script:DiskGB = 64
$script:DiskFreeNeededGB = 40

$script:SystemRoot32 = Join-Path ($env:SystemRoot ?? 'C:\Windows') 'System32'
# By path, never from PATH: Git's and WSL's are on some PATHs, and this one goes on talking to a guest.
$script:SshExe = Join-Path $script:SystemRoot32 'OpenSSH\ssh.exe'
$script:SshKeygenExe = Join-Path $script:SystemRoot32 'OpenSSH\ssh-keygen.exe'

$script:StateDir = Join-Path ($env:LOCALAPPDATA ?? '.') 'qor-ci-vm'
$script:StateFile = Join-Path $script:StateDir 'state.json'
$script:KeyDir = Join-Path $script:StateDir 'ssh'
$script:KeyFile = Join-Path $script:KeyDir 'id_ed25519'
$script:KnownHosts = Join-Path $script:KeyDir 'known_hosts'

# What Prove must see from each level, or the run is a failure. isolated-vm-guest.py has the same two lists, and a test holds
# them together. This list is here so that a guest cut off partway, or lying by omission, cannot pass by saying less.
$script:Categories = @{
    user = @('where', 'control', 'vmbus', 'mounts', 'devices', 'vsock', 'host-tcp', 'host-sweep', 'host-udp', 'lan', 'wsl')
    root = @('where', 'control', 'vmbus', 'mounts', 'devices', 'vsock', 'host-tcp', 'host-udp', 'lan', 'wsl', 'host-spoof', 'host-loopback', 'icmp', 'icmp6')
}

$script:StepNumber = 0
$script:CurrentStep = 'before the first step'

function Step([string] $Title, [scriptblock] $Body) {
    $script:StepNumber++
    $script:CurrentStep = "$script:StepNumber, $Title"
    Write-Host "== $script:StepNumber. $Title"
    $null = & $Body
}

function Say([string] $Text) { Write-Host "   $Text" }

# ---------------------------------------------------------------------------------------------------------------------
# Who, where, and what was chosen.

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal] $identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-Administrator([string] $Why) {
    if (-not (Test-Administrator)) {
        throw "$Why needs an administrator. Open PowerShell 7 with Run as administrator, after reading this script, and run the same command. This script never elevates itself."
    }
}

function Get-StartChoices {
    return @"
This account cannot start a VM. Hyper-V asks for an administrator or a member of Hyper-V Administrators, and this script changes neither.
  1. Elevate for each start: this command, in PowerShell 7 run as administrator. Changes nothing, and a person sees each one.
  2. A task the owner makes once, as administrator, running Start-VM -Name $script:VmName and nothing else, that this account may run and not edit.
  3. Hyper-V Administrators: complete and unrestricted access to Hyper-V, for everything that runs as this account. Not recommended.
  4. Leave it running: it holds its memory all the time. Not recommended.
Recommended: 1 for now, and 2 by a decision of its own when a job needs it. None of these is made by this script.
"@
}

function ConvertTo-UInt32([string] $Address) {
    $bytes = [ipaddress]::Parse($Address).GetAddressBytes()
    return ([uint32] $bytes[0] -shl 24) -bor ([uint32] $bytes[1] -shl 16) -bor ([uint32] $bytes[2] -shl 8) -bor [uint32] $bytes[3]
}

function ConvertFrom-UInt32([uint32] $Value) {
    return '{0}.{1}.{2}.{3}' -f (($Value -shr 24) -band 255), (($Value -shr 16) -band 255), (($Value -shr 8) -band 255), ($Value -band 255)
}

function Test-InRange([string] $Address, [string] $Prefix) {
    $base, $length = $Prefix -split '/'
    if ($length -eq '0') { return $true }
    # With the L: a bare 0xFFFFFFFF is -1 in PowerShell.
    $mask = [uint32] ((0xFFFFFFFFL -shl (32 - [int] $length)) -band 0xFFFFFFFFL)
    return ((ConvertTo-UInt32 $Address) -band $mask) -eq ((ConvertTo-UInt32 $base) -band $mask)
}

function Test-PrivateAddress([string] $Address) {
    foreach ($prefix in '10.0.0.0/8', '100.64.0.0/10', '169.254.0.0/16', '172.16.0.0/12', '192.168.0.0/16') {
        if (Test-InRange $Address $prefix) { return $true }
    }
    return $false
}

function Get-NetworkPlan([string] $Base) {
    $parsed = $null
    if (-not [ipaddress]::TryParse($Base, [ref] $parsed) -or $parsed.AddressFamily -ne 'InterNetwork') {
        throw "-Network must be an IPv4 address such as 10.213.57.0, and is '$Base'."
    }
    $value = ConvertTo-UInt32 $Base
    if ($value % 8 -ne 0) { throw "-Network must be the start of a /29, so its last number divides by 8, and $Base does not." }
    if (-not (Test-PrivateAddress $Base) -or (Test-InRange $Base '169.254.0.0/16')) {
        throw "-Network must be in a private range, so that the switch's ACL covers it, and $Base is not."
    }
    return [pscustomobject] @{
        Base   = $Base
        Cidr   = "$Base/29"
        Mask   = '255.255.255.248'
        Prefix = 29
        HostIp = ConvertFrom-UInt32 ($value + 1)
        VmIp   = ConvertFrom-UInt32 ($value + 2)
    }
}

function Assert-SafeRoot([string] $Path) {
    if (-not [IO.Path]::IsPathRooted($Path)) { throw "-Root must be a full path, and is '$Path'." }
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    if ($full -eq [IO.Path]::GetPathRoot($Path).TrimEnd('\', '/') -or -not $full) { throw "-Root must not be a drive's top folder: '$Path'." }
    foreach ($place in $env:SystemRoot, $env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramData, $env:USERPROFILE) {
        if ($place -and $full -eq $place.TrimEnd('\', '/')) { throw "-Root must not be $place." }
    }
    # The real guard: a folder that holds anything, and that this script did not make, is never used or emptied.
    if ((Test-Path -LiteralPath $full) -and -not (Test-Path -LiteralPath (Join-Path $full $script:RootMarker))) {
        if (Get-ChildItem -LiteralPath $full -Force | Select-Object -First 1) {
            throw "$full already holds files, and this script did not make it. Choose another -Root. Nothing was changed."
        }
    }
    return $full
}

# What Create chose, so that every later action means the same place. An argument that disagrees stops it.
function Initialize-Settings {
    $saved = $null
    if (Test-Path -LiteralPath $script:StateFile) {
        $saved = Get-Content -LiteralPath $script:StateFile -Raw | ConvertFrom-Json
        foreach ($name in 'Root', 'Network', 'MemoryGB') {
            if ($script:Asked.ContainsKey($name) -and "$($script:Asked[$name])" -ne "$($saved.$name)") {
                throw "Create chose -$name $($saved.$name) already, and this asks for $($script:Asked[$name]). -Action Remove first to change it."
            }
        }
    }
    $script:Settings = [pscustomobject] @{
        Root          = Assert-SafeRoot ($saved ? $saved.Root : $Root)
        Network       = $saved ? $saved.Network : $Network
        MemoryGB      = $saved ? [int] $saved.MemoryGB : $MemoryGB
        FirstBootDone = $saved ? [bool] $saved.FirstBootDone : $false
    }
    $script:Plan = Get-NetworkPlan $script:Settings.Network
    $script:DiskFile = Join-Path $script:Settings.Root 'disk\qor-ci-vm.vhdx'
    $script:SeedFile = Join-Path $script:Settings.Root 'seed\seed.iso'
    $script:ImageFile = Join-Path $script:Settings.Root "image\$script:ImageName"
    return ($null -ne $saved)
}

function Save-Settings {
    $null = New-Item -ItemType Directory -Force -Path $script:StateDir
    $script:Settings | ConvertTo-Json | Set-Content -LiteralPath $script:StateFile -Encoding utf8
}

function Get-FreeMemoryGB { return [math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB, 1) }

function Assert-MemoryFree {
    $free = Get-FreeMemoryGB
    $need = $script:Settings.MemoryGB + 1
    if ($free -lt $need) {
        throw "This machine has $free GB of memory free, and the VM needs $($script:Settings.MemoryGB) GB and Windows at least 1 more. One heavy job at a time (docs/CI.md): wait for what is running to finish, and run this again."
    }
    Say "$free GB of memory free, of the $need GB this needs"
}

function Get-FreeDiskGB([string] $Path) {
    $drive = [IO.Path]::GetPathRoot($Path)
    $info = [IO.DriveInfo]::new($drive)
    if (-not $info.IsReady) { throw "Drive $drive is not here. Choose another with -Root." }
    return [math]::Round($info.AvailableFreeSpace / 1GB, 1)
}

function Assert-NetworkFree {
    foreach ($nat in @(Get-NetNat)) {
        if ($nat.Name -ne $script:NatName) {
            throw "A NAT named '$($nat.Name)' is here already, and Windows supports one per host: Microsoft says a second puts the system into an unknown state. This script does not touch another's. Nothing was changed."
        }
    }
    foreach ($route in @(Get-NetRoute -AddressFamily IPv4)) {
        if ($route.InterfaceAlias -eq $script:AdapterName) { continue }
        $prefix = $route.DestinationPrefix
        if ($prefix -in '0.0.0.0/0', '255.255.255.255/32', '224.0.0.0/4') { continue }
        $base, $length = $prefix -split '/'
        $inPlan = Test-InRange $base $script:Plan.Cidr
        $planInIt = Test-InRange $script:Plan.Base $prefix
        if ($inPlan -or ($planInIt -and [int] $length -lt 32)) {
            throw "This machine already routes $prefix on '$($route.InterfaceAlias)', which overlaps $($script:Plan.Cidr). Choose another with -Network. Nothing was changed."
        }
    }
}

function Assert-Preflight {
    Assert-Administrator 'This step'
    foreach ($command in 'New-VM', 'New-VMSwitch', 'Convert-VHD', 'Add-VMNetworkAdapterAcl', 'New-NetNat', 'New-NetFirewallRule') {
        if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "$command is not on this machine, so Hyper-V's tools are not all here." }
    }
    if ("$((Get-Service vmms).Status)" -ne 'Running') { throw "Hyper-V's service, vmms, is not running." }
    Say 'Hyper-V is here and running'
    foreach ($program in $script:SshExe, $script:SshKeygenExe) {
        if (-not (Test-Path -LiteralPath $program)) { throw "$program is not here: Windows' OpenSSH client is what reaches the guest." }
    }
    foreach ($firewall in @(Get-NetFirewallProfile -PolicyStore ActiveStore)) {
        if ("$($firewall.Enabled)" -ne 'True') { throw "Windows Firewall's $($firewall.Name) profile is off, so this script's rules would do nothing. Turn it on, or say why not." }
    }
    Say 'Windows Firewall is on, in all three profiles'
    $free = Get-FreeDiskGB $script:Settings.Root
    if ($free -lt $script:DiskFreeNeededGB) { throw "$($script:Settings.Root)'s drive has $free GB free, and this wants $script:DiskFreeNeededGB. Choose another with -Root." }
    Say "$free GB free where the disk will be"
    if (-not (Get-VM -Name $script:VmName -ErrorAction SilentlyContinue | Where-Object { "$($_.State)" -eq 'Running' })) { Assert-MemoryFree }
    Assert-NetworkFree
    Say "$($script:Plan.Cidr) is free for it, and no other NAT is here"
}

# ---------------------------------------------------------------------------------------------------------------------
# The key that reaches the guest. It opens this guest and nothing else, and it is never printed. Its public half is the only
# thing of this machine's that goes into the seed.

function Invoke-Quiet([string] $Program, [string[]] $Arguments) {
    $null = & $Program @Arguments 2>&1
    if ($LASTEXITCODE) { throw "$(Split-Path -Leaf $Program) failed: exit $LASTEXITCODE" }
}

function Get-OwnSid { return [Security.Principal.WindowsIdentity]::GetCurrent().User.Value }

function New-GuestKey {
    if ((Test-Path -LiteralPath $script:KeyFile) -and (Test-Path -LiteralPath "$script:KeyFile.pub")) {
        Say "already in $script:KeyDir"
        return
    }
    $null = New-Item -ItemType Directory -Force -Path $script:KeyDir
    # This account and SYSTEM, before the key exists. OpenSSH refuses a key that others can read.
    $me = Get-OwnSid
    Invoke-Quiet (Join-Path $script:SystemRoot32 'icacls.exe') @($script:KeyDir, '/inheritance:r', '/grant:r', "*${me}:(OI)(CI)F", '*S-1-5-18:(OI)(CI)F', '/Q')
    Invoke-Quiet $script:SshKeygenExe @('-q', '-t', 'ed25519', '-N', '', '-C', 'qor-ci-vm', '-f', $script:KeyFile)
    # ssh-keygen gives its files an entry for Administrators of its own, so this account and SYSTEM are said again, alone.
    foreach ($file in $script:KeyFile, "$script:KeyFile.pub") {
        Invoke-Quiet (Join-Path $script:SystemRoot32 'icacls.exe') @($file, '/inheritance:r', '/remove:g', '*S-1-5-32-544', '/grant:r', "*${me}:F", '*S-1-5-18:F', '/Q')
    }
    if (Test-Administrator) {
        # An elevated process's files can belong to Administrators. Prove runs unelevated, and must be able to use this.
        foreach ($file in $script:KeyFile, "$script:KeyFile.pub") {
            Invoke-Quiet (Join-Path $script:SystemRoot32 'icacls.exe') @($file, '/setowner', "*$me", '/Q')
        }
    }
    Say "made in $script:KeyDir, for this account alone"
}

function Get-GuestPublicKey {
    $line = (Get-Content -LiteralPath "$script:KeyFile.pub" -Raw).Trim()
    if ($line -notmatch '^ssh-ed25519 [A-Za-z0-9+/=]+( \S+)?$') { throw "$script:KeyFile.pub is not one ed25519 public key, so it does not go into the seed." }
    return $line
}

# ---------------------------------------------------------------------------------------------------------------------
# The image, by exact URL and digest.

function Save-Image([string] $Url, [string] $Path) {
    Invoke-WebRequest -Uri $Url -OutFile $Path -SslProtocol Tls12, Tls13 -MaximumRetryCount 3 -RetryIntervalSec 10
}

function Assert-ImageDigest([string] $Path) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($actual -ne $script:ImageSha256) {
        throw "$Path has SHA-256 $actual, and the pin is $script:ImageSha256. It is refused and nothing was made from it."
    }
}

function Get-PinnedImage {
    $folder = Split-Path -Parent $script:ImageFile
    $null = New-Item -ItemType Directory -Force -Path $folder
    if (Test-Path -LiteralPath $script:ImageFile) {
        Say 'already here, and checking its digest again'
        Assert-ImageDigest $script:ImageFile
        return
    }
    $part = "$script:ImageFile.part"
    Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue
    Say "$script:ImageUrl, $([math]::Round($script:ImageBytes / 1MB)) MB, so this can take some minutes"
    Save-Image $script:ImageUrl $part
    try {
        Assert-ImageDigest $part
    }
    catch {
        Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue
        throw
    }
    Move-Item -LiteralPath $part -Destination $script:ImageFile
    Say "SHA-256 $script:ImageSha256, as pinned"
}

# ---------------------------------------------------------------------------------------------------------------------
# From QCOW2 to a fixed VHD, which Convert-VHD can read. docs/interop/qcow2.rst in QEMU is the format.
# Only ever given a file whose digest has been checked, and still refuses what it does not know rather than guess: a wrong disk
# here would boot, or nearly, and be a very long way from this code. The digest is what stands behind the data itself: QCOW2 and raw
# deflate have no checksum of their own, so damage to stored bytes would come out as wrong bytes, and nothing here could tell.

function Read-At([IO.Stream] $Stream, [long] $Offset, [int] $Count) {
    if ($Offset -lt 0 -or $Offset + $Count -gt $Stream.Length) { throw "the file ends before byte $($Offset + $Count), where it says there is data" }
    $buffer = [byte[]]::new($Count)
    $null = $Stream.Seek($Offset, [IO.SeekOrigin]::Begin)
    $got = 0
    while ($got -lt $Count) {
        $read = $Stream.Read($buffer, $got, $Count - $got)
        if ($read -le 0) { throw "the file ends early, at byte $($Offset + $got)" }
        $got += $read
    }
    return , $buffer
}

function Get-Be64([byte[]] $Bytes, [int] $Offset) { return [Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt64($Bytes, $Offset)) }
function Get-Be32([byte[]] $Bytes, [int] $Offset) { return [long] [Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($Bytes, $Offset)) -band 0xFFFFFFFFL }

function New-VhdFooter([long] $Size, [guid] $UniqueId) {
    $footer = [byte[]]::new(512)
    function Put([int] $Offset, [byte[]] $Bytes) { [Array]::Copy($Bytes, 0, $footer, $Offset, $Bytes.Length) }
    function Be32([uint32] $Value) { $b = [BitConverter]::GetBytes($Value); [Array]::Reverse($b); return , $b }
    function Be64([long] $Value) { $b = [BitConverter]::GetBytes($Value); [Array]::Reverse($b); return , $b }

    # Geometry: the algorithm in Microsoft's VHD format specification.
    $total = [math]::Min([long] ($Size / 512), [long] 65535 * 16 * 255)
    if ($total -ge [long] 65535 * 16 * 63) {
        $sectors = 255; $heads = 16; $product = [math]::Floor($total / $sectors)
    }
    else {
        $sectors = 17; $product = [math]::Floor($total / $sectors)
        $heads = [math]::Max(4, [math]::Floor(($product + 1023) / 1024))
        if ($product -ge $heads * 1024 -or $heads -gt 16) { $sectors = 31; $heads = 16; $product = [math]::Floor($total / $sectors) }
        if ($product -ge $heads * 1024) { $sectors = 63; $heads = 16; $product = [math]::Floor($total / $sectors) }
    }
    $cylinders = [math]::Floor($product / $heads)

    Put 0 ([Text.Encoding]::ASCII.GetBytes('conectix'))
    Put 8 (Be32 2)                 # features: reserved, always on
    Put 12 (Be32 0x00010000)       # format version 1.0
    Put 16 ([byte[]] (, 0xFF * 8)) # data offset: none, as a fixed disk has no header
    Put 24 (Be32 ([uint32] ([DateTimeOffset]::UtcNow - [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)).TotalSeconds))
    Put 28 ([Text.Encoding]::ASCII.GetBytes('qor '))
    Put 32 (Be32 0x00010000)
    Put 36 ([Text.Encoding]::ASCII.GetBytes('Wi2k'))
    Put 40 (Be64 $Size)
    Put 48 (Be64 $Size)
    Put 56 ([byte[]] @((([int] $cylinders -shr 8) -band 255), ([int] $cylinders -band 255), [int] $heads, [int] $sectors))
    Put 60 (Be32 2)                # disk type: fixed
    Put 68 $UniqueId.ToByteArray()
    $sum = [uint32] 0
    foreach ($byte in $footer) { $sum += $byte }
    Put 64 (Be32 ([uint32] (-bnot $sum -band 0xFFFFFFFFL)))
    return , $footer
}

function Convert-Qcow2ToVhd {
    param([string] $Source, [string] $Destination, [long] $MostBytes = 64GB)
    $in = [IO.File]::Open($Source, 'Open', 'Read', 'Read')
    $out = $null
    $done = $false
    try {
        $header = Read-At $in 0 72
        if ($header[0] -ne 0x51 -or $header[1] -ne 0x46 -or $header[2] -ne 0x49 -or $header[3] -ne 0xFB) { throw 'it is not QCOW2: the magic number is wrong' }
        $version = Get-Be32 $header 4
        if ($version -notin 2, 3) { throw "it is QCOW2 version $version, and this reads 2 and 3" }
        if ((Get-Be64 $header 8) -ne 0) { throw 'it has a backing file, so it is not the whole disk' }
        if ((Get-Be32 $header 32) -ne 0) { throw 'it is encrypted' }
        if ($version -eq 3) {
            $features = Get-Be64 (Read-At $in 72 8) 0
            # Dirty, corrupt, an external data file, a compression other than zlib, extended L2 entries, or something newer.
            if ($features -ne 0) { throw "it needs features this does not read: incompatible_features is 0x$($features.ToString('x'))" }
        }
        $clusterBits = Get-Be32 $header 20
        if ($clusterBits -lt 9 -or $clusterBits -gt 21) { throw "its cluster_bits, $clusterBits, is outside what the format allows" }
        $clusterSize = [long] 1 -shl $clusterBits
        $size = Get-Be64 $header 24
        if ($size -le 0 -or $size % 512) { throw "its size, $size, is not a whole number of sectors" }
        if ($size -gt $MostBytes) { throw "it says it holds $size bytes, more than the $MostBytes this expects of this image" }
        $l2Entries = [long] ($clusterSize / 8)
        $l1Needed = [long] [math]::Ceiling([double] $size / ($clusterSize * $l2Entries))
        $l1Size = Get-Be32 $header 36
        if ($l1Size -lt $l1Needed) { throw "its L1 table has $l1Size entries, and its size needs $l1Needed" }
        $l1 = Read-At $in (Get-Be64 $header 40) ([int] ($l1Needed * 8))

        $fileLength = $in.Length
        $vhdSize = [long] ([math]::Ceiling([double] $size / 1MB) * 1MB)
        # Offset bits 9-55. The rest of the word is flags, read below.
        $offsetMask = 0x00FFFFFFFFFFFE00L
        $compressedFlag = 0x4000000000000000L
        $x = 62 - ($clusterBits - 8)
        $compressedOffsetMask = ([long] 1 -shl $x) - 1
        $sectorsMask = ([long] 1 -shl (62 - $x)) - 1
        $plain = [byte[]]::new($clusterSize)

        $out = [IO.File]::Open($Destination, 'Create', 'ReadWrite', 'None')
        $out.SetLength($vhdSize + 512)

        for ($i = 0; $i -lt $l1Needed; $i++) {
            $l2Offset = [Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt64($l1, $i * 8)) -band $offsetMask
            if ($l2Offset -eq 0) { continue }
            if ($l2Offset % $clusterSize) { throw "an L2 table at $l2Offset is not on a cluster boundary" }
            $l2 = Read-At $in $l2Offset ([int] $clusterSize)
            for ($j = 0; $j -lt $l2Entries; $j++) {
                $guest = ($i * $l2Entries + $j) * $clusterSize
                if ($guest -ge $size) { break }
                $entry = [Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt64($l2, $j * 8))
                if ($entry -eq 0) { continue }
                $count = [int] [math]::Min($clusterSize, $size - $guest)
                if ($entry -band $compressedFlag) {
                    $hostOffset = $entry -band $compressedOffsetMask
                    if ($hostOffset -lt 512 -or $hostOffset -ge $fileLength) { throw "a compressed cluster at $hostOffset is outside the file" }
                    $sectors = ($entry -shr $x) -band $sectorsMask
                    $length = [math]::Min(($sectors + 1) * 512 - ($hostOffset % 512), $fileLength - $hostOffset)
                    $packed = Read-At $in $hostOffset ([int] $length)
                    $memory = [IO.MemoryStream]::new($packed, 0, $packed.Length)
                    $inflate = [IO.Compression.DeflateStream]::new($memory, [IO.Compression.CompressionMode]::Decompress)
                    try {
                        $got = 0
                        while ($got -lt $clusterSize) {
                            $read = $inflate.Read($plain, $got, [int] ($clusterSize - $got))
                            if ($read -le 0) { break }
                            $got += $read
                        }
                    }
                    finally { $inflate.Dispose() }
                    if ($got -ne $clusterSize) { throw "a compressed cluster at guest byte $guest gave $got bytes, not $clusterSize" }
                    $null = $out.Seek($guest, [IO.SeekOrigin]::Begin)
                    $out.Write($plain, 0, $count)
                }
                else {
                    # Bit 0 says reads as zeros, and an offset of 0 means nothing is allocated and there is no backing file.
                    if ($entry -band 1) { continue }
                    $hostOffset = $entry -band $offsetMask
                    if ($hostOffset -eq 0) { continue }
                    if ($hostOffset % $clusterSize) { throw "a cluster at $hostOffset is not on a cluster boundary" }
                    $data = Read-At $in $hostOffset $count
                    $null = $out.Seek($guest, [IO.SeekOrigin]::Begin)
                    $out.Write($data, 0, $count)
                }
            }
        }
        $footer = New-VhdFooter $vhdSize ([guid]::NewGuid())
        $null = $out.Seek($vhdSize, [IO.SeekOrigin]::Begin)
        $out.Write($footer, 0, 512)
        $done = $true
        return $size
    }
    catch {
        throw "$Source could not be converted: $($_.Exception.Message)"
    }
    finally {
        $in.Dispose()
        if ($out) { $out.Dispose() }
        if (-not $done) { Remove-Item -LiteralPath $Destination -Force -ErrorAction SilentlyContinue }
    }
}

function Get-Crc32([byte[]] $Bytes, [int] $Offset = 0, [int] $Count = -1) {
    if ($Count -lt 0) { $Count = $Bytes.Length - $Offset }
    $table = [uint32[]]::new(256)
    for ($n = 0; $n -lt 256; $n++) {
        [uint32] $c = $n
        for ($k = 0; $k -lt 8; $k++) { $c = if ($c -band 1) { 0xEDB88320L -bxor ($c -shr 1) } else { $c -shr 1 } }
        $table[$n] = $c
    }
    [uint32] $crc = [uint32]::MaxValue
    for ($n = $Offset; $n -lt $Offset + $Count; $n++) { $crc = $table[($crc -bxor $Bytes[$n]) -band 255] -bxor ($crc -shr 8) }
    return [uint32] ($crc -bxor [uint32]::MaxValue)
}

# A disk read wrongly should not get as far as a boot. A GPT has checksums over its own headers and partition list, at both
# ends of the disk, which nothing in the conversion could get right by accident. They cover those sectors and no others.
function Assert-GptDisk([string] $Path, [long] $Size) {
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'Read')
    try {
        $first = Read-At $stream 0 512
        if ($first[510] -ne 0x55 -or $first[511] -ne 0xAA) { throw 'the first sector does not end as a boot sector does' }
        foreach ($place in 'primary', 'backup') {
            $offset = if ($place -eq 'primary') { [long] 512 } else { $Size - 512 }
            $header = Read-At $stream $offset 512
            if ([Text.Encoding]::ASCII.GetString($header, 0, 8) -ne 'EFI PART') { throw "the $place GPT header is not at byte $offset" }
            $length = [BitConverter]::ToInt32($header, 12)
            if ($length -lt 92 -or $length -gt 512) { throw "the $place GPT header says it is $length bytes" }
            $stored = [BitConverter]::ToUInt32($header, 16)
            [Array]::Clear($header, 16, 4)
            if ((Get-Crc32 $header 0 $length) -ne $stored) { throw "the $place GPT header's checksum is wrong" }
            $entriesAt = [BitConverter]::ToInt64($header, 72) * 512
            $entriesLength = [long] [BitConverter]::ToUInt32($header, 80) * [BitConverter]::ToUInt32($header, 84)
            if ($entriesLength -le 0 -or $entriesLength -gt 1MB) { throw "the $place GPT says its partition list is $entriesLength bytes" }
            $entries = Read-At $stream $entriesAt ([int] $entriesLength)
            if ((Get-Crc32 $entries) -ne [BitConverter]::ToUInt32($header, 88)) { throw "the $place GPT partition list's checksum is wrong" }
        }
    }
    catch {
        throw "$Path is not the disk it should be: $($_.Exception.Message)"
    }
    finally { $stream.Dispose() }
}

function New-GuestDisk {
    if (Test-Path -LiteralPath $script:DiskFile) {
        Say 'already here'
        return
    }
    $folder = Split-Path -Parent $script:DiskFile
    $null = New-Item -ItemType Directory -Force -Path $folder
    $fixed = Join-Path $folder 'converting.vhd'
    $new = Join-Path $folder 'converting.vhdx'
    Remove-Item -LiteralPath $fixed, $new -Force -ErrorAction SilentlyContinue
    Assert-ImageDigest $script:ImageFile
    Say 'reading the image into a fixed VHD, here in PowerShell, so this takes a minute or more'
    $size = Convert-Qcow2ToVhd $script:ImageFile $fixed
    Assert-GptDisk $fixed $size
    Say 'its GPT headers and partition lists check out, at both ends of the disk'
    # Generation 2 boots only from VHDX. A 1 MB block: Microsoft's advice for Linux, so that a small write grows it by little.
    Convert-VHD -Path $fixed -DestinationPath $new -VHDType Dynamic -BlockSizeBytes 1MB
    Resize-VHD -Path $new -SizeBytes ($script:DiskGB * 1GB)
    Move-Item -LiteralPath $new -Destination $script:DiskFile
    Remove-Item -LiteralPath $fixed -Force
    Say "$script:DiskFile, up to $script:DiskGB GB, growing as it is used"
}

# ---------------------------------------------------------------------------------------------------------------------
# The seed: three small files on an ISO labelled cidata, which is what NoCloud looks for. One public key and no secret.

function Get-SeedFiles([string] $PublicKey) {
    $plan = $script:Plan
    $metaData = @"
instance-id: qor-ci-vm-1
local-hostname: qor-ci-vm
"@
    # qor is how the script on Windows gets in, with sudo for Prove's root level. runner has no key, no password and no sudo,
    # and is what a job would be. No package is installed or updated: what is booted is what was pinned.
    $userData = @"
#cloud-config
# Made by tools/ci/isolated-vm.ps1. One public key, and no secret.
ssh_pwauth: false
disable_root: true
ssh_deletekeys: true
ssh_genkeytypes: [ed25519]
users:
  - name: $script:GuestUser
    gecos: Reached by SSH by the script that made this VM
    shell: /bin/bash
    lock_passwd: true
    sudo: "ALL=(ALL) NOPASSWD:ALL"
    ssh_authorized_keys:
      - $PublicKey
  - name: runner
    uid: 2001
    shell: /bin/bash
    lock_passwd: true
package_update: false
package_upgrade: false
package_reboot_if_required: false
growpart:
  mode: auto
  devices: ['/']
write_files:
  - path: /etc/ssh/sshd_config.d/10-qor-ci-vm.conf
    permissions: '0644'
    content: |
      PasswordAuthentication no
      KbdInteractiveAuthentication no
      PermitRootLogin no
      AllowUsers $script:GuestUser
      AllowTcpForwarding no
      AllowAgentForwarding no
      X11Forwarding no
      PermitTunnel no
"@
    # A fixed address: WinNAT hands out none. Public resolvers: nothing of this machine's answers the guest.
    $networkConfig = @"
version: 2
ethernets:
  nic0:
    match:
      name: "e*"
    addresses: [$($plan.VmIp)/$($plan.Prefix)]
    routes:
      - to: default
        via: $($plan.HostIp)
    nameservers:
      addresses: [1.1.1.1, 8.8.8.8]
    dhcp4: false
    dhcp6: false
    accept-ra: false
"@
    $files = [ordered] @{}
    foreach ($pair in @(@('meta-data', $metaData), @('network-config', $networkConfig), @('user-data', $userData))) {
        $text = $pair[1].Replace("`r`n", "`n").TrimEnd("`n") + "`n"
        if ($text -match 'PRIVATE KEY') { throw "A private key was about to go into the seed's $($pair[0]). Nothing was written." }
        $files[$pair[0]] = $text
    }
    return $files
}

function New-SeedIso([System.Collections.IDictionary] $Files, [string] $Path) {
    # ISO 9660 with a Joliet tree, as cloud-localds makes with genisoimage, since a plain ISO 9660 name cannot be user-data.
    # Fixed dates, so that the same files make the same bytes.
    $sector = 2048
    function Both32([long] $Value) {
        $le = [BitConverter]::GetBytes([uint32] $Value); $be = [BitConverter]::GetBytes([uint32] $Value); [Array]::Reverse($be)
        return , [byte[]] ($le + $be)
    }
    function Both16([int] $Value) {
        $le = [BitConverter]::GetBytes([uint16] $Value); $be = [BitConverter]::GetBytes([uint16] $Value); [Array]::Reverse($be)
        return , [byte[]] ($le + $be)
    }
    function Put([byte[]] $Target, [int] $Offset, [byte[]] $Bytes) { [Array]::Copy($Bytes, 0, $Target, $Offset, $Bytes.Length) }
    function Record([long] $Lba, [long] $Length, [byte[]] $Name, [bool] $IsFolder) {
        $size = 33 + $Name.Length
        if ($size % 2) { $size++ }
        $record = [byte[]]::new($size)
        $record[0] = $size
        Put $record 2 (Both32 $Lba)
        Put $record 10 (Both32 $Length)
        Put $record 18 ([byte[]] @(126, 11, 1, 0, 0, 0, 0))
        $record[25] = $IsFolder ? 2 : 0
        Put $record 28 (Both16 1)
        $record[32] = $Name.Length
        Put $record 33 $Name
        return , $record
    }
    function Folder([long] $Self, [object[]] $Entries) {
        $folder = [byte[]]::new($sector)
        $at = 0
        foreach ($entry in @((Record $Self $sector ([byte[]] @(0)) $true), (Record $Self $sector ([byte[]] @(1)) $true)) + $Entries) {
            Put $folder $at $entry
            $at += $entry.Length
        }
        return , $folder
    }
    function PathTable([long] $Lba, [bool] $BigEndian) {
        $table = [byte[]]::new($sector)
        $table[0] = 1
        $lbaBytes = [BitConverter]::GetBytes([uint32] $Lba)
        if ($BigEndian) { [Array]::Reverse($lbaBytes) }
        Put $table 2 $lbaBytes
        if ($BigEndian) { $table[7] = 1 } else { $table[6] = 1 }
        return , $table
    }
    function Descriptor([int] $Type, [long] $Total, [long] $RootLba, [int] $TableL, [int] $TableM, [bool] $Joliet) {
        $d = [byte[]]::new($sector)
        $d[0] = $Type
        Put $d 1 ([Text.Encoding]::ASCII.GetBytes('CD001'))
        $d[6] = 1
        $text = if ($Joliet) {
            { param($s, $n) [Text.Encoding]::BigEndianUnicode.GetBytes($s.PadRight([int] ($n / 2))) }
        }
        else {
            { param($s, $n) [Text.Encoding]::ASCII.GetBytes($s.PadRight($n)) }
        }
        Put $d 8 (& $text '' 32)
        # The label. NoCloud reads either case, and this is the case cloud-localds writes.
        Put $d 40 (& $text 'cidata' 32)
        Put $d 80 (Both32 $Total)
        if ($Joliet) { Put $d 88 ([Text.Encoding]::ASCII.GetBytes('%/E')) }
        Put $d 120 (Both16 1)
        Put $d 124 (Both16 1)
        Put $d 128 (Both16 $sector)
        Put $d 132 (Both32 10)
        Put $d 140 ([BitConverter]::GetBytes([uint32] $TableL))
        $tableMBytes = [BitConverter]::GetBytes([uint32] $TableM); [Array]::Reverse($tableMBytes)
        Put $d 148 $tableMBytes
        Put $d 156 (Record $RootLba $sector ([byte[]] @(0)) $true)
        Put $d 190 (& $text '' 128)
        Put $d 318 (& $text '' 128)
        Put $d 446 (& $text '' 128)
        Put $d 574 (& $text '' 128)
        Put $d 702 (& $text '' 36)
        Put $d 739 (& $text '' 36)
        Put $d 776 (& $text '' 36)
        $stamp = [Text.Encoding]::ASCII.GetBytes('2026110100000000')
        $never = [Text.Encoding]::ASCII.GetBytes('0000000000000000')
        Put $d 813 $stamp
        Put $d 830 $stamp
        Put $d 847 $never
        Put $d 864 $never
        $d[881] = 1
        return , $d
    }

    # 16 empty sectors, the two volume descriptors, the end mark, four path tables, two root folders, then the files.
    $firstFile = 25
    $lba = $firstFile
    $placed = [System.Collections.Generic.List[object]]::new()
    foreach ($name in @($Files.Keys | Sort-Object)) {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes([string] $Files[$name])
        if ($bytes.Length -eq 0) { throw "The seed's $name is empty." }
        $short = (($name.ToUpperInvariant() -replace '[^A-Z0-9]', '_').PadRight(8, '_').Substring(0, 8)) + '.;1'
        $placed.Add([pscustomobject] @{ Name = $name; Short = $short; Bytes = $bytes; Lba = $lba })
        $lba += [int] [math]::Ceiling($bytes.Length / $sector)
    }
    $total = $lba
    $image = [byte[]]::new($total * $sector)
    Put $image (16 * $sector) (Descriptor 1 $total 23 19 20 $false)
    Put $image (17 * $sector) (Descriptor 2 $total 24 21 22 $true)
    $image[18 * $sector] = 255
    Put $image (18 * $sector + 1) ([Text.Encoding]::ASCII.GetBytes('CD001'))
    $image[18 * $sector + 6] = 1
    Put $image (19 * $sector) (PathTable 23 $false)
    Put $image (20 * $sector) (PathTable 23 $true)
    Put $image (21 * $sector) (PathTable 24 $false)
    Put $image (22 * $sector) (PathTable 24 $true)
    $plainEntries = @($placed | Sort-Object Short | ForEach-Object { , (Record $_.Lba $_.Bytes.Length ([Text.Encoding]::ASCII.GetBytes($_.Short)) $false) })
    $jolietEntries = @($placed | Sort-Object Name | ForEach-Object { , (Record $_.Lba $_.Bytes.Length ([Text.Encoding]::BigEndianUnicode.GetBytes("$($_.Name);1")) $false) })
    Put $image (23 * $sector) (Folder 23 $plainEntries)
    Put $image (24 * $sector) (Folder 24 $jolietEntries)
    foreach ($file in $placed) { Put $image ($file.Lba * $sector) $file.Bytes }
    [IO.File]::WriteAllBytes($Path, $image)
}

function New-Seed {
    $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $script:SeedFile)
    # By way of a new file: on a second run the seed can be in a running VM's DVD drive, where it cannot be written, and the same
    # settings make the same bytes.
    $new = "$script:SeedFile.new"
    New-SeedIso (Get-SeedFiles (Get-GuestPublicKey)) $new
    if ((Test-Path -LiteralPath $script:SeedFile) -and (Get-FileHash -LiteralPath $new).Hash -eq (Get-FileHash -LiteralPath $script:SeedFile).Hash) {
        Remove-Item -LiteralPath $new -Force
        Say 'already here, and the same'
        return
    }
    Move-Item -LiteralPath $new -Destination $script:SeedFile -Force
    Say $script:SeedFile
}

# ---------------------------------------------------------------------------------------------------------------------
# The network: an internal switch, one address on this machine's end, a NAT, and what stands in the way.

function Get-ExpectedAcls {
    # Direction Both of everything: nothing from these ranges reaches the guest either, whatever a neighbour sends.
    $deny = @(
        '0.0.0.0/8',       # "this network"
        '10.0.0.0/8',      # private, and with it this switch's own range
        '100.64.0.0/10',   # carrier NAT, and Tailscale's
        '127.0.0.0/8',     # loopback: Windows' 127.0.0.1, where WSL's localhost relay listens, by a frame root can write
        '169.254.0.0/16',  # link-local
        '172.16.0.0/12',   # private, and WSL's NAT
        '192.0.0.0/24',    # protocol assignments
        '192.168.0.0/16',  # private, and most LANs
        '198.18.0.0/15',   # benchmarking, which some networks use inside
        '224.0.0.0/4',     # multicast: mDNS, LLMNR and SSDP, which Windows answers on every adapter
        '240.0.0.0/4',     # reserved, and the broadcast address
        '::/0'             # all of IPv6: this NAT is IPv4 alone, so there is nowhere for it to go that this script wants
    )
    $acls = [System.Collections.Generic.List[object]]::new()
    foreach ($direction in 'Inbound', 'Outbound') {
        foreach ($prefix in $deny) { $acls.Add([pscustomobject] @{ Direction = $direction; Action = 'Deny'; Remote = $prefix }) }
        # Longest match decides, so this /32 wins over 10.0.0.0/8. SSH's replies go to it, and with no state here a reply and a
        # new connection look the same, so the firewall rules below are what stop a new connection to this one address.
        $acls.Add([pscustomobject] @{ Direction = $direction; Action = 'Allow'; Remote = $script:Plan.HostIp })
        # Everything else, which is the public internet, by way of the NAT.
        $acls.Add([pscustomobject] @{ Direction = $direction; Action = 'Allow'; Remote = '0.0.0.0/0' })
    }
    return $acls
}

function Format-Acl([string] $Direction, [string] $AclAction, [string] $Remote) {
    return ("$Direction $AclAction " + ($Remote -replace '/32$', '')).ToLowerInvariant()
}

function New-GuestNetwork {
    $switch = Get-VMSwitch -Name $script:SwitchName -ErrorAction SilentlyContinue
    if ($switch) {
        if ($switch.Notes -ne $script:Mark) { throw "A virtual switch named $script:SwitchName is here, and this script did not make it. Nothing was changed." }
        Say 'the switch is already here'
    }
    else {
        # Internal: it has this machine's adapter on it and no physical one, so a guest is on no LAN by being on it.
        New-VMSwitch -Name $script:SwitchName -SwitchType Internal -Notes $script:Mark | Out-Null
        Say 'made the switch'
    }
    foreach ($attempt in 1..30) {
        if (Get-NetAdapter -Name $script:AdapterName -ErrorAction SilentlyContinue) { break }
        if ($attempt -eq 30) { throw "The switch's adapter, $script:AdapterName, did not appear in 30 seconds." }
        Start-Sleep -Seconds 1
    }
    $address = Get-NetIPAddress -InterfaceAlias $script:AdapterName -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -eq $script:Plan.HostIp }
    if ($address) {
        Say "$($script:Plan.HostIp) is already on it"
    }
    else {
        New-NetIPAddress -InterfaceAlias $script:AdapterName -IPAddress $script:Plan.HostIp -PrefixLength $script:Plan.Prefix -AddressFamily IPv4 | Out-Null
        Say "gave it $($script:Plan.HostIp)/$($script:Plan.Prefix)"
    }
    # On this adapter alone. ms_server is Windows' SMB server, and ms_msclient its client, neither wanted on this wire.
    # ms_tcpip6, so that this machine has no IPv6 here to be reached on. ms_lltdio, ms_rspndr and ms_lldp are link-layer protocols,
    # which an IP ACL and a firewall rule never see, and root in the guest can write any frame.
    # One at a time, and only what is there and on: asking for one an adapter does not have is an error.
    foreach ($binding in @(Get-NetAdapterBinding -Name $script:AdapterName -ComponentID ms_server, ms_msclient, ms_tcpip6, ms_lltdio, ms_rspndr, ms_lldp -ErrorAction SilentlyContinue | Where-Object Enabled)) {
        Disable-NetAdapterBinding -Name $script:AdapterName -ComponentID $binding.ComponentID -Confirm:$false
    }
    Say 'took SMB, IPv6 and the link-layer protocols off this adapter alone'

    if (Get-NetNat -Name $script:NatName -ErrorAction SilentlyContinue) {
        Say 'the NAT is already here'
    }
    else {
        New-NetNat -Name $script:NatName -InternalIPInterfaceAddressPrefix $script:Plan.Cidr | Out-Null
        Say "made the NAT for $($script:Plan.Cidr)"
    }
    # Two rules, because a guest's root chooses its own source address: the first holds whatever the packet says it is from,
    # and the second holds on any interface, for a packet Windows takes in on another's address. Block beats allow, and neither
    # stops the replies to an SSH this machine started, which belong to a connection already made.
    if (-not (Get-NetFirewallRule -Name $script:RuleByAdapter -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -Name $script:RuleByAdapter -DisplayName 'QOR OS CI VM: nothing starts a connection in from its adapter' -Description $script:Mark -Group 'QOR OS CI VM' `
            -Direction Inbound -Action Block -Enabled True -Profile Any -InterfaceAlias $script:AdapterName | Out-Null
    }
    if (-not (Get-NetFirewallRule -Name $script:RuleByAddress -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -Name $script:RuleByAddress -DisplayName 'QOR OS CI VM: nothing starts a connection in from its addresses' -Description $script:Mark -Group 'QOR OS CI VM' `
            -Direction Inbound -Action Block -Enabled True -Profile Any -RemoteAddress $script:Plan.Cidr | Out-Null
    }
    Say 'the two firewall rules are in place'
}

function New-GuestVm {
    $vm = Get-VM -Name $script:VmName -ErrorAction SilentlyContinue
    if ($vm -and $vm.Notes -ne $script:Mark) { throw "A VM named $script:VmName is here, and this script did not make it. Nothing was changed." }
    if ($vm -and "$($vm.State)" -ne 'Off') {
        Say "it is already here and $($vm.State), so its settings stay as they are, and the next step reads them"
        return
    }
    $memory = [long] $script:Settings.MemoryGB * 1GB
    if ($vm) {
        Say 'it is already here, so this sets each setting again'
    }
    else {
        # Generation 2: UEFI and Secure Boot, and no emulated legacy devices for a guest to attack.
        New-VM -Name $script:VmName -Generation 2 -MemoryStartupBytes $memory -VHDPath $script:DiskFile -SwitchName $script:SwitchName -Path (Join-Path $script:Settings.Root 'vm') | Out-Null
        Say 'made it'
    }

    # How -Action Remove knows it is this script's.
    Set-VM -Name $script:VmName -Notes $script:Mark
    # Secure Boot with the template Microsoft says generation 2 Linux needs: the UEFI CA, which signs Ubuntu's shim. Windows' template
    # would refuse it. Unproven until it boots, and if it refuses, this stays on and the owner decides.
    Set-VMFirmware -VMName $script:VmName -EnableSecureBoot On -SecureBootTemplate MicrosoftUEFICertificateAuthority
    # The disk and nothing else: no network boot to answer, and no DVD to boot once the seed is in.
    Set-VMFirmware -VMName $script:VmName -BootOrder (Get-VMHardDiskDrive -VMName $script:VmName)
    # No checkpoints. One would keep a job's memory and disk where the next job could be started from them, and write them to X:.
    Set-VM -Name $script:VmName -CheckpointType Disabled -AutomaticCheckpointsEnabled $false
    # Never starts by itself, as it holds memory only when something needs it. Off, not saved, when Windows stops: a saved VM writes
    # its memory to disk, and a job's holds nothing worth keeping.
    Set-VM -Name $script:VmName -AutomaticStartAction Nothing -AutomaticStopAction TurnOff
    # Static memory. Dynamic Memory is one more channel to the host, and would take memory back from a build mid-compile.
    Set-VMMemory -VMName $script:VmName -DynamicMemoryEnabled $false -StartupBytes $memory
    Set-VMProcessor -VMName $script:VmName -Count $script:VmProcessors
    # No virtualisation inside. A hypervisor in the guest is more for the host's to deal with, and nothing here needs one.
    Set-VMProcessor -VMName $script:VmName -ExposeVirtualizationExtensions $false
    # No Enhanced Session, which brings drive sharing and the clipboard. Hyper-V has no per-VM off switch: the one at Set-VMHost would
    # change every VM, and is not touched. By Microsoft's archived linux-vm-tools and the guides for it, what a Linux guest needs is
    # this set to HvSocket and xrdp listening in the guest. This is VMBus, the default, and the image's manifest has no xrdp.
    Set-VM -Name $script:VmName -EnhancedSessionTransportType VMBus
    # This laptop's battery is none of a job's business.
    Set-VM -Name $script:VmName -BatteryPassthroughEnabled $false

    # Every integration service off, as it boots without all of them. Each is a channel to the host that a guest's root can speak on.
    #   Guest Service Interface: the host's copy of files in and out. Not wanted in either direction.
    #   Key-Value Pair Exchange: tells the guest the host's name and version.
    #   Heartbeat: only tells Hyper-V it is alive, and SSH shows that.
    #   Shutdown: lets the host ask for a clean stop. -Action Stop asks over SSH, and a guest that will not stop is turned off.
    #   Time Synchronization: the guest's clock comes from the virtual firmware at each boot and then NTP. Microsoft warns the clock
    #     drifts without it: docs/CI.md has seen what a wrong clock does to a runner, so Prove checks it as a control.
    #   VSS: lets the host back it up live, and nothing here is worth backing up.
    # By whatever Windows lists, so that the names need no translating and a new one is off too.
    foreach ($service in @(Get-VMIntegrationService -VMName $script:VmName)) {
        Disable-VMIntegrationService -VMName $script:VmName -Name $service.Name
    }

    # The guest's end of the wire. No other source MAC, so it cannot stand in for this machine's adapter. DHCP and router guards, so
    # nothing it says can change this machine's addresses or routes. No mirroring, no teaming, no device naming, which passes the
    # adapter's name down to the guest.
    Set-VMNetworkAdapter -VMName $script:VmName -MacAddressSpoofing Off -DhcpGuard On -RouterGuard On -PortMirroring None -AllowTeaming Off -DeviceNaming Off

    # What stands between it and the LAN: read the top of this file. Below the guest's kernel, so root cannot change it.
    $have = @(Get-VMNetworkAdapterAcl -VMName $script:VmName | ForEach-Object { Format-Acl $_.Direction $_.Action $_.RemoteAddress })
    foreach ($acl in Get-ExpectedAcls) {
        if ((Format-Acl $acl.Direction $acl.Action $acl.Remote) -notin $have) {
            Add-VMNetworkAdapterAcl -VMName $script:VmName -Direction $acl.Direction -Action $acl.Action -RemoteIPAddress $acl.Remote
        }
    }
    Say 'every setting is set'
}

# ---------------------------------------------------------------------------------------------------------------------
# Reading it all back. What is true is what Hyper-V says, not what the lines above asked for.

function Get-FirewallFindings {
    $findings = [System.Collections.Generic.List[object]]::new()
    function Add([string] $Name, [string] $Expected, [string] $Actual, [bool] $Ok) {
        $findings.Add([pscustomobject] @{ Name = $Name; Expected = $Expected; Actual = $Actual; Ok = $Ok })
    }
    $profiles = @(Get-NetFirewallProfile -PolicyStore ActiveStore)
    $off = @($profiles | Where-Object { "$($_.Enabled)" -ne 'True' } | ForEach-Object Name)
    Add 'firewall-profiles' 'all on' ($off ? "off: $($off -join ', ')" : 'all on') (($profiles.Count -gt 0) -and -not $off)

    foreach ($name in $script:RuleByAdapter, $script:RuleByAddress) {
        $rule = Get-NetFirewallRule -Name $name -PolicyStore ActiveStore -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $rule) {
            Add $name 'there, on, blocking inbound' 'not there' $false
            continue
        }
        $shape = "enabled $($rule.Enabled), $($rule.Action), $($rule.Direction), profile $($rule.Profile)"
        $good = "$($rule.Enabled)" -eq 'True' -and "$($rule.Action)" -eq 'Block' -and "$($rule.Direction)" -eq 'Inbound' -and "$($rule.Profile)" -in 'Any', 'Domain, Private, Public'
        if ($name -eq $script:RuleByAdapter) {
            $where = (Get-NetFirewallInterfaceFilter -AssociatedNetFirewallRule $rule).InterfaceAlias
            $good = $good -and "$where" -eq $script:AdapterName
        }
        else {
            $where = (Get-NetFirewallAddressFilter -AssociatedNetFirewallRule $rule).RemoteAddress
            # Windows can hand a prefix back as an address and a mask.
            $good = $good -and "$where" -in $script:Plan.Cidr, "$($script:Plan.Base)/$($script:Plan.Mask)"
        }
        Add $name 'on, Block, Inbound, profile Any, on its own adapter or range' "$shape, on $where" $good
    }
    return $findings
}

function Get-BoundaryFindings([switch] $SeedExpected) {
    $findings = [System.Collections.Generic.List[object]]::new()
    function Add([string] $Name, [string] $Expected, [string] $Actual) {
        $findings.Add([pscustomobject] @{ Name = $Name; Expected = $Expected; Actual = $Actual; Ok = ($Expected -eq $Actual) })
    }
    $vm = Get-VM -Name $script:VmName -ErrorAction SilentlyContinue
    if (-not $vm) {
        Add 'vm' 'there' 'not there'
    }
    else {
        Add 'vm-mark' $script:Mark "$($vm.Notes)"
        Add 'vm-generation' '2' "$($vm.Generation)"
        $firmware = Get-VMFirmware -VMName $script:VmName
        Add 'secure-boot' 'On' "$($firmware.SecureBoot)"
        Add 'secure-boot-template' 'MicrosoftUEFICertificateAuthority' "$($firmware.SecureBootTemplate)"
        Add 'boot-no-network' '0' "$(@($firmware.BootOrder | Where-Object { "$($_.BootType)" -eq 'Network' }).Count)"
        Add 'checkpoint-type' 'Disabled' "$($vm.CheckpointType)"
        Add 'automatic-checkpoints' 'False' "$($vm.AutomaticCheckpointsEnabled)"
        Add 'checkpoints-held' '0' "$(@(Get-VMSnapshot -VMName $script:VmName).Count)"
        Add 'start-action' 'Nothing' "$($vm.AutomaticStartAction)"
        Add 'stop-action' 'TurnOff' "$($vm.AutomaticStopAction)"
        $memory = Get-VMMemory -VMName $script:VmName
        Add 'dynamic-memory' 'False' "$($memory.DynamicMemoryEnabled)"
        Add 'memory-bytes' "$([long] $script:Settings.MemoryGB * 1GB)" "$($memory.Startup)"
        $processor = Get-VMProcessor -VMName $script:VmName
        Add 'processors' "$script:VmProcessors" "$($processor.Count)"
        Add 'nesting' 'False' "$($processor.ExposeVirtualizationExtensions)"
        Add 'enhanced-session-transport' 'VMBus' "$($vm.EnhancedSessionTransportType)"
        Add 'battery-passthrough' 'False' "$($vm.BatteryPassthroughEnabled)"
        Add 'tpm' 'False' "$((Get-VMSecurity -VMName $script:VmName).TpmEnabled)"
        $services = @(Get-VMIntegrationService -VMName $script:VmName)
        Add 'integration-services-listed' 'True' "$($services.Count -gt 0)"
        Add 'integration-services-on' 'none' ((@($services | Where-Object Enabled | ForEach-Object Name) -join ', ') -replace '^$', 'none')
        Add 'disks' $script:DiskFile ((@(Get-VMHardDiskDrive -VMName $script:VmName | ForEach-Object Path)) -join ', ')
        Add 'dvd-drives' ($SeedExpected ? $script:SeedFile : 'none') ((@(Get-VMDvdDrive -VMName $script:VmName | ForEach-Object Path) -join ', ') -replace '^$', 'none')
        Add 'gpu-partitions' '0' "$(@(Get-VMGpuPartitionAdapter -VMName $script:VmName).Count)"
        Add 'assigned-devices' '0' "$(@(Get-VMAssignableDevice -VMName $script:VmName).Count)"
        Add 'fibre-channel' '0' "$(@(Get-VMFibreChannelHba -VMName $script:VmName).Count)"
        $adapters = @(Get-VMNetworkAdapter -VMName $script:VmName)
        Add 'network-adapters' '1' "$($adapters.Count)"
        foreach ($adapter in $adapters) {
            Add 'adapter-switch' $script:SwitchName "$($adapter.SwitchName)"
            Add 'mac-spoofing' 'Off' "$($adapter.MacAddressSpoofing)"
            Add 'dhcp-guard' 'On' "$($adapter.DhcpGuard)"
            Add 'router-guard' 'On' "$($adapter.RouterGuard)"
            Add 'port-mirroring' 'None' "$($adapter.PortMirroringMode)"
            Add 'teaming' 'Off' "$($adapter.AllowTeaming)"
            Add 'device-naming' 'Off' "$($adapter.DeviceNaming)"
        }
        $want = (@(Get-ExpectedAcls | ForEach-Object { Format-Acl $_.Direction $_.Action $_.Remote }) | Sort-Object) -join '; '
        $have = (@(Get-VMNetworkAdapterAcl -VMName $script:VmName | ForEach-Object { Format-Acl $_.Direction $_.Action $_.RemoteAddress }) | Sort-Object) -join '; '
        Add 'port-acls' $want $have
    }

    $switch = Get-VMSwitch -Name $script:SwitchName -ErrorAction SilentlyContinue
    if (-not $switch) {
        Add 'switch' 'there' 'not there'
    }
    else {
        Add 'switch-mark' $script:Mark "$($switch.Notes)"
        Add 'switch-type' 'Internal' "$($switch.SwitchType)"
    }
    $address = Get-NetIPAddress -InterfaceAlias $script:AdapterName -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -eq $script:Plan.HostIp }
    Add 'host-address' "$($script:Plan.HostIp)/$($script:Plan.Prefix)" ($address ? "$($address.IPAddress)/$($address.PrefixLength)" : 'not there')
    $bound = @(Get-NetAdapterBinding -Name $script:AdapterName -ComponentID ms_server, ms_msclient, ms_tcpip6, ms_lltdio, ms_rspndr, ms_lldp -ErrorAction SilentlyContinue | Where-Object Enabled | ForEach-Object ComponentID)
    Add 'adapter-bindings-on' 'none' (($bound -join ', ') -replace '^$', 'none')
    $nats = @(Get-NetNat)
    Add 'nats' $script:NatName (($nats | ForEach-Object Name) -join ', ')
    $nat = $nats | Where-Object Name -EQ $script:NatName
    Add 'nat-range' $script:Plan.Cidr "$($nat.InternalIPInterfaceAddressPrefix)"
    foreach ($finding in Get-FirewallFindings) { $findings.Add($finding) }
    return $findings
}

function Show-Findings([object[]] $Findings) {
    $bad = @($Findings | Where-Object { -not $_.Ok })
    foreach ($finding in $Findings) {
        if ($finding.Ok) { Say "ok   $($finding.Name): $($finding.Actual)" }
        else { Say "BAD  $($finding.Name): should be $($finding.Expected), and is $($finding.Actual)" }
    }
    return $bad.Count
}

function Assert-Findings([switch] $SeedExpected) {
    $findings = @(Get-BoundaryFindings -SeedExpected:$SeedExpected)
    $bad = Show-Findings $findings
    if ($bad) { throw "$bad of $($findings.Count) settings read back wrong, as named above. Nothing has been booted from this read." }
    Say "all $($findings.Count) read back as they should"
}

# ---------------------------------------------------------------------------------------------------------------------
# SSH to the guest. What comes back is from a guest, so it is text at most: no agent, no forwarding, no terminal, no escapes.

function Get-SshArguments([switch] $FirstContact) {
    # Forward slashes and quotes, because ssh's option parser splits on spaces, and a profile folder can have them.
    $known = $script:KnownHosts.Replace('\', '/')
    return @(
        '-F', 'NUL', '-i', $script:KeyFile, '-l', $script:GuestUser, '-p', '22', '-T',
        '-o', 'IdentitiesOnly=yes', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', '-o', 'LogLevel=ERROR',
        '-o', "UserKnownHostsFile=`"$known`"", '-o', 'GlobalKnownHostsFile=NUL',
        # The first boot makes its host key, and it is kept from then on. Every later connection must match it.
        '-o', "StrictHostKeyChecking=$($FirstContact ? 'accept-new' : 'yes')",
        '-o', 'ForwardAgent=no', '-o', 'ForwardX11=no', '-o', 'ClearAllForwardings=yes', '-o', 'PermitLocalCommand=no',
        '-o', 'RequestTTY=no', '-o', 'UpdateHostKeys=no', '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=8',
        $script:Plan.VmIp
    )
}

function Protect-Text([string] $Text) { return $Text -replace '[\x00-\x08\x0B-\x1F\x7F-\x9F]', '?' }

function Invoke-Ssh {
    param(
        [string] $Command,
        [byte[]] $InputBytes,
        [int] $TimeoutSeconds = 120,
        [switch] $FirstContact,
        [switch] $Show
    )
    $start = [Diagnostics.ProcessStartInfo]::new($script:SshExe)
    foreach ($argument in (Get-SshArguments -FirstContact:$FirstContact)) { $start.ArgumentList.Add($argument) }
    $start.ArgumentList.Add($Command)
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.Encoding]::UTF8
    $start.StandardErrorEncoding = [Text.Encoding]::UTF8
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($start)
    try {
        $errors = $process.StandardError.ReadToEndAsync()
        if ($InputBytes) { $process.StandardInput.BaseStream.Write($InputBytes, 0, $InputBytes.Length) }
        $process.StandardInput.Close()
        $lines = [System.Collections.Generic.List[string]]::new()
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while ($true) {
            $next = $process.StandardOutput.ReadLineAsync()
            while (-not $next.Wait(1000)) {
                if ([DateTime]::UtcNow -gt $deadline) {
                    $process.Kill()
                    return [pscustomobject] @{ ExitCode = 255; Lines = $lines; Error = "no end after $TimeoutSeconds seconds, so it was stopped" }
                }
            }
            if ($null -eq $next.Result) { break }
            $line = Protect-Text $next.Result
            $lines.Add($line)
            if ($Show) { Write-Host $line }
        }
        $process.WaitForExit()
        return [pscustomobject] @{ ExitCode = $process.ExitCode; Lines = $lines; Error = (Protect-Text $errors.Result).Trim() }
    }
    finally { $process.Dispose() }
}

function Wait-Guest([int] $Seconds, [switch] $FirstContact) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    $lastSaid = $null
    $reason = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        $done = Invoke-Ssh -Command 'true' -TimeoutSeconds 30 -FirstContact:$FirstContact
        if ($done.ExitCode -eq 0) { return $true }
        $reason = $done.Error
        if (-not $lastSaid -or ([DateTime]::UtcNow - $lastSaid).TotalSeconds -ge 60) {
            Say "still waiting: $reason"
            $lastSaid = [DateTime]::UtcNow
        }
        Start-Sleep -Seconds 5
    }
    $script:LastSshError = $reason
    return $false
}

function Wait-VmState([string] $State, [int] $Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ("$((Get-VM -Name $script:VmName).State)" -eq $State) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Invoke-FirstBoot {
    if ($script:Settings.FirstBootDone) {
        Step 'First boot' { Say 'it has had its first boot already' }
        return
    }
    Step 'Add the seed on a DVD drive, and read everything back before anything boots' {
        if (-not (Get-VMDvdDrive -VMName $script:VmName)) { Add-VMDvdDrive -VMName $script:VmName -Path $script:SeedFile }
        Assert-Findings -SeedExpected
    }
    Step 'Start it for its first boot' {
        if ("$((Get-VM -Name $script:VmName).State)" -ne 'Running') { Start-VM -Name $script:VmName }
    }
    Step 'Wait for SSH, up to 15 minutes, keeping the host key it first shows' {
        if (-not (Wait-Guest 900 -FirstContact)) {
            throw "It did not answer SSH at $($script:Plan.VmIp) in 15 minutes: $script:LastSshError. Its console says why: vmconnect.exe localhost $script:VmName. If that says the image is refused, Secure Boot said no, and turning that off is the owner's decision."
        }
        Say 'it answered'
    }
    Step 'Wait for its first-boot setup to finish' {
        $done = Invoke-Ssh -Command 'cloud-init status --wait --long' -TimeoutSeconds 900 -Show
        # 2 is cloud-init finishing with something to say, which is above for reading. 1 is it failing.
        if ($done.ExitCode -notin 0, 2) { throw "cloud-init ended with exit $($done.ExitCode): $($done.Error)" }
    }
    Step 'Turn cloud-init off inside, so that no later boot takes settings from anywhere' {
        $done = Invoke-Ssh -Command 'sudo -n touch /etc/cloud/cloud-init.disabled'
        if ($done.ExitCode) { throw "That failed, exit $($done.ExitCode): $($done.Error)" }
    }
    Step 'Power it off from inside' {
        $null = Invoke-Ssh -Command 'sudo -n systemctl poweroff'
        if (-not (Wait-VmState 'Off' 180)) {
            Say 'it did not stop in 3 minutes, so it is being turned off'
            Stop-VM -Name $script:VmName -TurnOff -Force
        }
    }
    Step 'Take the seed out, and delete it' {
        foreach ($drive in @(Get-VMDvdDrive -VMName $script:VmName)) {
            Remove-VMDvdDrive -VMName $script:VmName -ControllerNumber $drive.ControllerNumber -ControllerLocation $drive.ControllerLocation
        }
        Remove-Item -LiteralPath (Split-Path -Parent $script:SeedFile) -Recurse -Force
        $script:Settings.FirstBootDone = $true
        Save-Settings
    }
}

function Invoke-Create {
    $null = Initialize-Settings
    Step 'Check this machine, before changing anything' {
        Assert-Preflight
        Save-Settings
    }
    Step "The folder $($script:Settings.Root)" {
        $null = New-Item -ItemType Directory -Force -Path $script:Settings.Root
        Set-Content -LiteralPath (Join-Path $script:Settings.Root $script:RootMarker) -Value $script:Mark
    }
    Step 'The SSH key that reaches the guest, in a folder of its own, never printed' { New-GuestKey }
    Step 'The image, by its exact URL, checked against its pinned SHA-256' { Get-PinnedImage }
    Step 'The disk, from that image' { New-GuestDisk }
    if (-not $script:Settings.FirstBootDone) {
        Step 'The seed: one public key and the network settings, and no secret' { New-Seed }
    }
    Step 'The switch, its address, the NAT and the firewall rules' { New-GuestNetwork }
    Step 'The VM, and each of its settings' { New-GuestVm }
    Invoke-FirstBoot
    Step 'Read everything back again' { Assert-Findings }
    Step 'Start it, and see it answer without its seed' {
        if ("$((Get-VM -Name $script:VmName).State)" -ne 'Running') {
            Assert-MemoryFree
            Start-VM -Name $script:VmName
        }
        if (-not (Wait-Guest 300)) { throw "It did not answer SSH after its first boot: $script:LastSshError. Its console: vmconnect.exe localhost $script:VmName" }
        Say 'it answered, and it is running now, holding its memory'
    }
    Write-Host ''
    Write-Host 'Made, and nothing has been shown yet. Now, in an ordinary PowerShell 7, NOT as administrator:'
    Write-Host "  $PSCommandPath -Action Prove"
}

# ---------------------------------------------------------------------------------------------------------------------
# What to attack, read by an account with no privilege. These go into the guest on stdin, are kept nowhere there, and are this
# machine's addresses and port numbers and no secret.

function Get-HyperVSocketPorts {
    # A host service must be registered here to take a Hyper-V socket (Microsoft: "Make your own integration services"), and a Linux
    # guest can name only those made from this template. So this is the list of what could answer, plus the ports below.
    $found = @()
    $key = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Virtualization\GuestCommunicationServices'
    foreach ($service in @(Get-ChildItem -LiteralPath $key -ErrorAction SilentlyContinue)) {
        if ($service.PSChildName -match '^([0-9a-fA-F]{8})-facb-11e6-bd58-64006a7986d3$') { $found += [Convert]::ToInt64($Matches[1], 16) }
    }
    return $found
}

function Get-RunningWslAddresses {
    # Asks only what is running. This never starts WSL.
    $env:WSL_UTF8 = '1'
    $found = @()
    $running = @(wsl.exe --list --running --quiet 2>$null)
    if ($LASTEXITCODE) { return $found }
    foreach ($name in $running) {
        $name = "$name".Trim()
        if (-not $name) { continue }
        $found += @("$(wsl.exe -d $name --exec hostname -I 2>$null)" -split '\s+' | Where-Object { $_ -match '^\d+\.\d+\.\d+\.\d+$' })
    }
    return $found
}

function Get-ProveTargets {
    $plan = $script:Plan
    $v4 = @(Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike '127.*' })
    $v6 = @(Get-NetIPAddress -AddressFamily IPv6 | Where-Object { $_.IPAddress -ne '::1' -and $_.IPAddress -notlike 'fe80:*' } | ForEach-Object { ($_.IPAddress -split '%')[0] })
    $own = @($v4 | ForEach-Object IPAddress)

    $wslAdapter = { param($alias) "$alias" -like 'vEthernet (WSL*' }
    $wsl = @($v4 | Where-Object { & $wslAdapter $_.InterfaceAlias } | ForEach-Object IPAddress)
    $wsl += @(Get-RunningWslAddresses)

    $lan = @(Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue | ForEach-Object NextHop)
    $lan += @(Get-DnsClientServerAddress -AddressFamily IPv4 | ForEach-Object ServerAddresses)
    foreach ($neighbor in @(Get-NetNeighbor -AddressFamily IPv4 -State Reachable, Stale, Delay, Probe -ErrorAction SilentlyContinue)) {
        if ($neighbor.InterfaceAlias -eq $script:AdapterName) { continue }
        if (& $wslAdapter $neighbor.InterfaceAlias) { $wsl += $neighbor.IPAddress } else { $lan += $neighbor.IPAddress }
    }
    # Private only: a public DNS server is the internet, which a guest may reach, and not the LAN.
    $lan = @($lan | Where-Object { $_ -and $_ -notin $own -and $_ -ne $plan.VmIp -and (Test-PrivateAddress $_) } | Sort-Object -Unique)
    $wsl = @($wsl | Where-Object { $_ -and $_ -ne $plan.VmIp } | Sort-Object -Unique)

    $listening = @(Get-NetTCPConnection -State Listen)
    $loopback = @($listening | Where-Object { $_.LocalAddress -in '127.0.0.1', '::1' } | ForEach-Object { [int] $_.LocalPort } | Sort-Object -Unique)
    $elsewhere = @($listening | Where-Object { $_.LocalAddress -notin '127.0.0.1', '::1' } | ForEach-Object { [int] $_.LocalPort } | Sort-Object -Unique)
    # SMB, RDP and WinRM by name, whatever is listening today, and SSH, DNS, RPC, Hyper-V's own and the rest of what Windows may offer.
    $named = 22, 53, 80, 135, 139, 443, 445, 2179, 3389, 5040, 5357, 5985, 5986, 7680, 47001

    return [ordered] @{
        vm_ip             = $plan.VmIp
        gateway           = $plan.HostIp
        control_host      = 'github.com'
        host_time         = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        host_addresses    = @($own | Where-Object { $_ -ne $plan.HostIp } | Sort-Object -Unique)
        host_addresses_v6 = @($v6 | Sort-Object -Unique)
        host_ports        = @(($named + $elsewhere) | Sort-Object -Unique)
        loopback_ports    = $loopback
        lan               = $lan
        lan_ports         = @(22, 53, 80, 139, 443, 445, 3389, 5985, 8080, 9100)
        wsl               = $wsl
        # 1 and 50000 to 50002 are what WSL's VM showed open on its VMBus on 1 November 2026, the last being the 9P server.
        vsock_ports       = @((@(Get-HyperVSocketPorts) + @(1, 22, 135, 445, 2179, 3389, 5985) + @(50000..50010)) | Sort-Object -Unique)
        sweep             = @(1, 65535)
    }
}

# The verdict on one level, from its lines and exit status alone. The exit status is what the guest says about itself; the lines
# are what it did; and a run that was cut short, or that left a category out, shows no pass however it ended.
function Get-LevelVerdict([string] $Level, [int] $ExitCode, [string[]] $Lines) {
    $reasons = [System.Collections.Generic.List[string]]::new()
    $counts = @{ PASS = 0; FAIL = 0; UNRUN = 0 }
    $seen = @{}
    $end = $null
    foreach ($line in @($Lines)) {
        if ($line -match '^(PASS|FAIL|UNRUN)\s+(\S+)\s') {
            $counts[$Matches[1]]++
            $seen[$Matches[2]] = $true
        }
        elseif ($line -match '^END level=(\S+) passed=(\d+) failed=(\d+) unrun=(\d+) stand-in=(\S+)$') {
            $end = $Matches.Clone()
        }
    }
    if ($ExitCode -ne 0) { $reasons.Add("level $Level exited $ExitCode") }
    if ($counts.FAIL) { $reasons.Add("$($counts.FAIL) attacks got through at level $Level") }
    if ($counts.UNRUN) { $reasons.Add("$($counts.UNRUN) checks could not run at level $Level, and a check that did not run shows nothing") }
    if ($counts.PASS -eq 0) { $reasons.Add("level $Level has no passing line") }
    if (-not $end) {
        $reasons.Add("level $Level has no END line, so it was cut short")
    }
    else {
        if ($end[1] -ne $Level) { $reasons.Add("level $Level says it is level $($end[1])") }
        if ([int] $end[2] -ne $counts.PASS -or [int] $end[3] -ne $counts.FAIL -or [int] $end[4] -ne $counts.UNRUN) {
            $reasons.Add("level $Level's END line does not match its lines")
        }
        if ($end[5] -ne 'no') { $reasons.Add("level $Level ran on stand-ins, not in a VM") }
    }
    foreach ($category in $script:Categories[$Level]) {
        if (-not $seen[$category]) { $reasons.Add("level $Level says nothing about $category") }
    }
    return [pscustomobject] @{ Level = $Level; Ok = ($reasons.Count -eq 0); Reasons = $reasons; Passed = $counts.PASS; Failed = $counts.FAIL; Unrun = $counts.UNRUN }
}

function Get-RunHeader {
    $commit = 'unknown'
    try { $commit = (git -C $PSScriptRoot rev-parse HEAD 2>$null) ?? 'unknown' } catch { $commit = 'unknown' }
    $self = Get-FileHash -Algorithm SHA256 -LiteralPath $PSCommandPath
    $guest = Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $PSScriptRoot 'isolated-vm-guest.py')
    return @(
        "Prove, at $([DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
        "Commit $commit",
        "isolated-vm.ps1 $($self.Hash.ToLowerInvariant())",
        "isolated-vm-guest.py $($guest.Hash.ToLowerInvariant())",
        "Windows $([Environment]::OSVersion.Version), PowerShell $($PSVersionTable.PSVersion), as $([Environment]::UserName)",
        "Image $script:ImageUrl, SHA-256 $script:ImageSha256",
        "Guest $($script:Plan.VmIp), through $($script:Plan.HostIp)"
    )
}

function Invoke-Prove {
    $exists = Initialize-Settings
    if (-not $exists -or -not (Test-Path -LiteralPath $script:KeyFile)) {
        throw 'There is nothing to attack here yet: -Action Create comes first, once, as administrator.'
    }
    if (Test-Administrator) {
        throw 'Prove runs from an ordinary terminal, not as administrator. It is a conversation with a guest it is trying to break out of, and a job would have no administrator either.'
    }
    $report = [System.Collections.Generic.List[string]]::new()
    $header = @(Get-RunHeader)
    foreach ($line in $header) { Write-Host $line; $report.Add($line) }

    Step 'Is the guest there' {
        if (-not (Wait-Guest 30)) {
            throw "It does not answer SSH at $($script:Plan.VmIp): $script:LastSshError. If it is stopped, only an administrator can start it: -Action Start. Nothing was attacked, so nothing is shown."
        }
    }
    $hostFindings = @()
    Step "Read this machine's firewall, from here, as this account" {
        $script:HostFindings = @(Get-FirewallFindings)
    }
    $hostFindings = $script:HostFindings
    foreach ($finding in $hostFindings) {
        $status = $finding.Ok ? 'PASS ' : 'FAIL '
        $line = "$status host          $($finding.Name): $($finding.Actual)"
        Write-Host $line
        $report.Add($line)
    }
    Step 'Send the guest the attacks, where root owns them, so that runner cannot change what root then runs' {
        $text = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'isolated-vm-guest.py')).Replace("`r`n", "`n")
        $done = Invoke-Ssh -Command 'sudo -n install -D -m 0755 /dev/stdin /run/qor-ci-vm/prove.py' -InputBytes ([Text.UTF8Encoding]::new($false).GetBytes($text))
        if ($done.ExitCode) { throw "Could not put them there, exit $($done.ExitCode): $($done.Error)" }
    }
    $verdicts = [System.Collections.Generic.List[object]]::new()
    foreach ($level in 'user', 'root') {
        $command = if ($level -eq 'user') { 'sudo -n -u runner -- python3 -I /run/qor-ci-vm/prove.py --level user' } else { 'sudo -n -- python3 -I /run/qor-ci-vm/prove.py --level root' }
        Step "Attack from inside as $level. Every line says what it tried and what happened" {
            $targets = Get-ProveTargets | ConvertTo-Json -Depth 3
            $script:LevelDone = Invoke-Ssh -Command $command -InputBytes ([Text.UTF8Encoding]::new($false).GetBytes($targets)) -TimeoutSeconds 1200 -Show
        }
        foreach ($line in $script:LevelDone.Lines) { $report.Add($line) }
        if ($script:LevelDone.Error) {
            $report.Add("(from ssh at level $level) $($script:LevelDone.Error)")
            Write-Host "(from ssh) $($script:LevelDone.Error)"
        }
        $verdicts.Add((Get-LevelVerdict $level $script:LevelDone.ExitCode ([string[]] $script:LevelDone.Lines)))
    }

    $reasons = [System.Collections.Generic.List[string]]::new()
    foreach ($finding in $hostFindings) { if (-not $finding.Ok) { $reasons.Add("this machine's $($finding.Name): should be $($finding.Expected), and is $($finding.Actual)") } }
    if (-not $hostFindings) { $reasons.Add("this machine's firewall could not be read") }
    foreach ($verdict in $verdicts) { foreach ($reason in $verdict.Reasons) { $reasons.Add($reason) } }

    $summary = [System.Collections.Generic.List[string]]::new()
    foreach ($verdict in $verdicts) { $summary.Add("Level $($verdict.Level): $($verdict.Passed) passed, $($verdict.Failed) failed, $($verdict.Unrun) did not run.") }
    if ($reasons.Count) {
        $summary.Add('PROVE FAILED:')
        foreach ($reason in $reasons) { $summary.Add("  $reason") }
    }
    else {
        $summary.Add('Every line passed. Each attack above failed, at this commit, on this machine, on its network as it is now. This says what was tried and no more.')
        $summary.Add('What it did not try is at the top of tools/ci/isolated-vm.ps1, and in docs/CI.md.')
    }
    foreach ($line in $summary) { Write-Host $line; $report.Add($line) }

    $file = Join-Path $script:StateDir "prove-$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')).txt"
    $report | Set-Content -LiteralPath $file -Encoding utf8
    Write-Host "All of this is in $file. It holds this machine's addresses and no credential."

    if ($KeepRunning) {
        Write-Host 'Left running, as asked. It holds its memory until -Action Stop.'
    }
    else {
        Step 'Power the guest off, so it holds no memory. Only an administrator can start it again' { $null = Stop-Guest }
    }
    return ($reasons.Count ? 1 : 0)
}

# ---------------------------------------------------------------------------------------------------------------------

function Stop-Guest {
    $done = Invoke-Ssh -Command 'sudo -n systemctl poweroff' -TimeoutSeconds 30
    # ssh's exit is whatever the connection did as the guest went away, so what counts is whether it goes on answering.
    foreach ($attempt in 1..12) {
        Start-Sleep -Seconds 5
        if ((Invoke-Ssh -Command 'true' -TimeoutSeconds 15).ExitCode -ne 0) {
            Say 'it has stopped answering SSH, which is as much as this account can see'
            return $true
        }
    }
    Say "It is still answering after a minute (poweroff said: $($done.Error))."
    Say "From here that is all that can be done. As administrator: Stop-VM -Name $script:VmName -TurnOff -Force"
    return $false
}

function Invoke-Stop {
    $exists = Initialize-Settings
    if (-not $exists -or -not (Test-Path -LiteralPath $script:KeyFile)) { throw 'There is nothing to stop here: no Create has run.' }
    $stopped = $false
    Step 'Power the guest off from inside, over SSH, which needs no privilege here' { $script:Stopped = Stop-Guest }
    $stopped = $script:Stopped
    if (-not $stopped -and (Test-Administrator)) {
        Step 'It would not go, and this is an administrator, so turn it off' { Stop-VM -Name $script:VmName -TurnOff -Force }
        $stopped = $true
    }
    return ($stopped ? 0 : 1)
}

function Invoke-Start {
    $null = Initialize-Settings
    if (-not (Test-Administrator)) {
        Write-Host (Get-StartChoices)
        return 2
    }
    Step 'Start it' {
        $vm = Get-VM -Name $script:VmName -ErrorAction SilentlyContinue
        if (-not $vm) { throw "There is no VM named $script:VmName: -Action Create comes first." }
        if ($vm.Notes -ne $script:Mark) { throw "A VM named $script:VmName is here, and this script did not make it. Nothing was changed." }
        if ("$($vm.State)" -eq 'Running') {
            Say 'it is already running'
        }
        else {
            Assert-MemoryFree
            Start-VM -Name $script:VmName
        }
        if (-not (Wait-Guest 300)) { throw "It did not answer SSH: $script:LastSshError. Its console: vmconnect.exe localhost $script:VmName" }
        Say 'it answered'
    }
    return 0
}

function Invoke-Check {
    $null = Initialize-Settings
    Assert-Administrator 'Reading a VM back'
    $findings = @()
    Step 'Read everything back, and change nothing' {
        $seed = -not $script:Settings.FirstBootDone
        $script:Found = @(Get-BoundaryFindings -SeedExpected:$seed)
        $script:BadCount = Show-Findings $script:Found
    }
    $findings = $script:Found
    Write-Host "$($findings.Count - $script:BadCount) of $($findings.Count) as they should be."
    return ($script:BadCount ? 1 : 0)
}

function Invoke-Remove {
    # The folder is checked first: a -Root that is not this script's is refused before anything else is touched.
    $null = Initialize-Settings
    Assert-Administrator 'Removing a VM, a switch, a NAT and firewall rules'
    $root = $script:Settings.Root
    Step "The VM $script:VmName, by that name alone" {
        $vm = Get-VM -Name $script:VmName -ErrorAction SilentlyContinue
        if (-not $vm) { Say 'not here'; return }
        if ($vm.Notes -ne $script:Mark) { throw "A VM named $script:VmName is here, and this script did not make it, so it stays. Nothing else was removed." }
        if ("$($vm.State)" -ne 'Off') { Stop-VM -Name $script:VmName -TurnOff -Force }
        Remove-VM -Name $script:VmName -Force
        Say 'gone'
    }
    Step 'The two firewall rules, by those names alone' {
        foreach ($name in $script:RuleByAdapter, $script:RuleByAddress) {
            $rule = Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue
            if (-not $rule) { Say "$name is not here"; continue }
            if ($rule.Description -ne $script:Mark) { throw "A firewall rule named $name is here, and this script did not make it, so it stays." }
            Remove-NetFirewallRule -Name $name
            Say "$name is gone"
        }
    }
    Step "The NAT $script:NatName, by that name alone" {
        if (Get-NetNat -Name $script:NatName -ErrorAction SilentlyContinue) {
            Remove-NetNat -Name $script:NatName -Confirm:$false
            Say 'gone'
        }
        else { Say 'not here' }
    }
    Step "The switch $script:SwitchName, its adapter and its address, by that name alone" {
        $switch = Get-VMSwitch -Name $script:SwitchName -ErrorAction SilentlyContinue
        if (-not $switch) { Say 'not here'; return }
        if ($switch.Notes -ne $script:Mark) { throw "A virtual switch named $script:SwitchName is here, and this script did not make it, so it stays." }
        Remove-NetIPAddress -InterfaceAlias $script:AdapterName -IPAddress $script:Plan.HostIp -Confirm:$false -ErrorAction SilentlyContinue
        Remove-VMSwitch -Name $script:SwitchName -Force
        Say 'gone'
    }
    Step "What this script put in $root" {
        if (-not (Test-Path -LiteralPath $root)) { Say 'not here'; return }
        foreach ($name in 'vm', 'disk', 'seed', 'image') {
            $folder = Join-Path $root $name
            if (Test-Path -LiteralPath $folder) { Remove-Item -LiteralPath $folder -Recurse -Force }
        }
        # Its mark stays if something else is there, so that running this again still knows the folder.
        if (Get-ChildItem -LiteralPath $root -Force | Where-Object Name -NE $script:RootMarker | Select-Object -First 1) {
            Say "$root holds other things, which are not this script's, so it stays"
        }
        else {
            Remove-Item -LiteralPath $root -Recurse -Force
            Say 'gone'
        }
    }
    Step "The key and Prove's reports, in $script:StateDir" {
        if (Test-Path -LiteralPath $script:StateDir) { Remove-Item -LiteralPath $script:StateDir -Recurse -Force }
        Say 'gone'
    }
    Write-Host 'All of it is gone, and nothing else was touched.'
    return 0
}

function Show-Help {
    Write-Host @'
Makes qor-ci-vm, a clean Linux VM, and attacks it from inside. Read this file before running it, and see docs/CI.md.
  -Action Create   as administrator: makes it, saying each step, and leaves it running for Prove
  -Action Prove    NOT as administrator: attacks from inside, then powers it off. Exits 0 only if every line passed
  -Action Check    as administrator: reads every setting back, and changes nothing
  -Action Start    as administrator: starts it. An ordinary account cannot, and is told the choices
  -Action Stop     from an ordinary terminal: powers it off from inside
  -Action Remove   as administrator: takes away everything it made, by exact name
'@
}

function Invoke-Main([string] $Requested) {
    $script:StepNumber = 0
    $script:CurrentStep = 'before the first step'
    try {
        switch ($Requested) {
            'Create' { $null = Invoke-Create; return 0 }
            'Check' { return (Invoke-Check) }
            'Start' { return (Invoke-Start) }
            'Prove' { return (Invoke-Prove) }
            'Stop' { return (Invoke-Stop) }
            'Remove' { return (Invoke-Remove) }
            default { Show-Help; return 0 }
        }
    }
    catch {
        Write-Host ''
        Write-Host "STOPPED at step $script:CurrentStep."
        Write-Host "Why: $($_.Exception.Message)"
        if ($Requested -in 'Create', 'Remove') {
            Write-Host 'What was done stays done, and no later step ran. Fix the reason and run the same command again. It does what is left.'
        }
        return 1
    }
}

# Dot-sourced by tests/tools/test_isolated_vm.py, with Hyper-V's commands stood in for: definitions only, and nothing runs.
if ($MyInvocation.InvocationName -eq '.') { return }

exit (Invoke-Main $Action)
