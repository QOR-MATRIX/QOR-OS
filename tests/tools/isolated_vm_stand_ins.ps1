# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#
# Stands in for Hyper-V, Windows' network commands and SSH, for tests/tools/test_isolated_vm.py. Dot-sourced after
# tools/ci/isolated-vm.ps1, so that functions here win over that file's and over any cmdlet of the same name.
#
# Nothing here reaches Hyper-V. The test's driver turns module autoloading off first, so a command with no stand-in fails as
# unknown instead of finding the real one, even for someone running the tests as an administrator.
#
# A new VM has Hyper-V's own unsafe defaults, on purpose: a setting the script forgets to make reads back wrong.

$global:World = @{
    Calls       = [System.Collections.Generic.List[string]]::new()
    Admin       = $true
    FreeMemory  = 12.0
    FreeDisk    = 186.0
    FailAt      = ''
    GuestUp     = $false
    Ssh         = [System.Collections.Generic.List[string]]::new()
    SshAnswers  = @{}
    ImageSource = $env:QOR_TEST_IMAGE
    VMs         = @{ 'someone-elses-vm' = [pscustomobject] @{ Name = 'someone-elses-vm'; Notes = ''; State = 'Running' } }
    Switches    = @{ 'someone-elses-switch' = [pscustomobject] @{ Name = 'someone-elses-switch'; Notes = ''; SwitchType = 'External' } }
    Nats        = @{}
    Rules       = @{ 'someone-elses-rule' = [pscustomobject] @{ Name = 'someone-elses-rule'; Enabled = 'True'; Action = 'Allow'; Direction = 'Inbound'; Profile = 'Any'; Description = '' } }
    Addresses   = [System.Collections.Generic.List[object]]::new()
    Bindings    = @{}
    Profiles    = @(
        [pscustomobject] @{ Name = 'Domain'; Enabled = 'True' },
        [pscustomobject] @{ Name = 'Private'; Enabled = 'True' },
        [pscustomobject] @{ Name = 'Public'; Enabled = 'True' }
    )
    Routes      = @([pscustomobject] @{ DestinationPrefix = '192.168.1.0/24'; InterfaceAlias = 'Wi-Fi'; NextHop = '0.0.0.0' })
}

function Get-StandInArguments([object[]] $Given) {
    $parsed = @{}
    $at = 0
    while ($at -lt $Given.Count) {
        $token = $Given[$at]
        if ($token -is [string] -and $token -match '^-([A-Za-z0-9]+)(:?)$') {
            $name = $Matches[1]
            $colon = $Matches[2]
            $next = $at + 1 -lt $Given.Count ? $Given[$at + 1] : $null
            $nextIsName = $next -is [string] -and $next -match '^-[A-Za-z]'
            if ($colon -or ($at + 1 -lt $Given.Count -and -not $nextIsName)) {
                $parsed[$name] = $next
                $at += 2
                continue
            }
            $parsed[$name] = $true
        }
        $at++
    }
    return $parsed
}

function Write-Call([string] $Name, [object[]] $Given) {
    $shown = @($Given | ForEach-Object { ($_ -is [array]) ? ($_ -join ',') : "$_" }) -join ' '
    $global:World.Calls.Add("$Name $shown".Trim())
    if ($global:World.FailAt -eq $Name) { throw "the stand-in for $Name was told to fail" }
    return (Get-StandInArguments $Given)
}

# ---- What tools/ci/isolated-vm.ps1 would otherwise do to the machine it is on.
function Test-Administrator { return $global:World.Admin }
function Get-FreeMemoryGB { return $global:World.FreeMemory }
function Get-FreeDiskGB { return $global:World.FreeDisk }
function Get-OwnSid { return 'S-1-5-21-1-2-3-1000' }
function Start-Sleep { }
function Get-Service { return [pscustomobject] @{ Status = 'Running' } }
function Get-HyperVSocketPorts { return @(2761) }
function Get-RunningWslAddresses { return @('172.22.12.5') }
function Save-Image([string] $Url, [string] $Path) {
    $global:World.Calls.Add("Save-Image $Url")
    Copy-Item -LiteralPath $global:World.ImageSource -Destination $Path
}
function Invoke-Quiet([string] $Program, [string[]] $Arguments) {
    $global:World.Calls.Add("Invoke-Quiet $(Split-Path -Leaf $Program)")
    if ($Program -like '*ssh-keygen*') {
        $key = $Arguments[$Arguments.IndexOf('-f') + 1]
        Set-Content -LiteralPath $key -Value 'stand-in for a private key, which no test ever prints' -NoNewline
        Set-Content -LiteralPath "$key.pub" -Value 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIStandInPublicKeyStandInPublicKeyStandIn qor-ci-vm'
    }
}
function Invoke-Ssh {
    param([string] $Command, [byte[]] $InputBytes, [int] $TimeoutSeconds = 120, [switch] $FirstContact, [switch] $Show)
    $global:World.Ssh.Add(($FirstContact ? 'first: ' : '') + $Command)
    if (-not $global:World.GuestUp) { return [pscustomobject] @{ ExitCode = 255; Lines = @(); Error = 'connection timed out' } }
    if ($Command -like '*poweroff*') {
        if ($global:World.GuestRefusesToStop) { return [pscustomobject] @{ ExitCode = 1; Lines = @(); Error = 'refused' } }
        $global:World.GuestUp = $false
        if ($global:World.VMs['qor-ci-vm']) { $global:World.VMs['qor-ci-vm'].State = 'Off' }
        return [pscustomobject] @{ ExitCode = 255; Lines = @(); Error = 'connection closed' }
    }
    foreach ($pattern in $global:World.SshAnswers.Keys) {
        if ($Command -like $pattern) {
            $answer = $global:World.SshAnswers[$pattern]
            if ($Show) { foreach ($line in $answer.Lines) { Write-Host $line } }
            return $answer
        }
    }
    return [pscustomobject] @{ ExitCode = 0; Lines = @(); Error = '' }
}

# ---- Disks.
function Convert-VHD {
    $a = Write-Call 'Convert-VHD' $args
    Copy-Item -LiteralPath $a.Path -Destination $a.DestinationPath
}
function Resize-VHD { $null = Write-Call 'Resize-VHD' $args }

# ---- The VM.
function New-StandInVm([string] $Name, [hashtable] $Given) {
    return [pscustomobject] @{
        Name = $Name; Notes = ''; State = 'Off'; Generation = [int] $Given.Generation
        CheckpointType = 'Production'; AutomaticCheckpointsEnabled = $true
        AutomaticStartAction = 'StartIfRunning'; AutomaticStopAction = 'Save'
        EnhancedSessionTransportType = 'HvSocket'; BatteryPassthroughEnabled = $true
        Firmware = [pscustomobject] @{ SecureBoot = 'On'; SecureBootTemplate = 'MicrosoftWindows'; BootOrder = @([pscustomobject] @{ BootType = 'Drive' }, [pscustomobject] @{ BootType = 'Network' }) }
        Memory = [pscustomobject] @{ DynamicMemoryEnabled = $true; Startup = [long] $Given.MemoryStartupBytes }
        Processor = [pscustomobject] @{ Count = 1; ExposeVirtualizationExtensions = $true }
        Services = @('Guest Service Interface', 'Heartbeat', 'Key-Value Pair Exchange', 'Shutdown', 'Time Synchronization', 'VSS' | ForEach-Object { [pscustomobject] @{ Name = $_; Enabled = $true } })
        Disks = @([pscustomobject] @{ Path = $Given.VHDPath })
        Dvds = [System.Collections.Generic.List[object]]::new()
        Adapter = [pscustomobject] @{ SwitchName = $Given.SwitchName; MacAddressSpoofing = 'On'; DhcpGuard = 'Off'; RouterGuard = 'Off'; PortMirroringMode = 'Source'; AllowTeaming = 'On'; DeviceNaming = 'On' }
        Acls = [System.Collections.Generic.List[object]]::new()
        Snapshots = @(); Gpu = @(); Assigned = @(); Fibre = @(); Tpm = $false
    }
}
function Get-VM {
    $a = Get-StandInArguments $args
    return $global:World.VMs[$a.Name]
}
function New-VM {
    $a = Write-Call 'New-VM' $args
    $global:World.VMs[$a.Name] = New-StandInVm $a.Name $a
    return $global:World.VMs[$a.Name]
}
function Set-VM {
    $a = Write-Call 'Set-VM' $args
    $vm = $global:World.VMs[$a.Name]
    foreach ($key in 'Notes', 'CheckpointType', 'AutomaticCheckpointsEnabled', 'AutomaticStartAction', 'AutomaticStopAction', 'EnhancedSessionTransportType', 'BatteryPassthroughEnabled') {
        if ($a.ContainsKey($key)) { $vm.$key = $a[$key] }
    }
}
function Remove-VM { $a = Write-Call 'Remove-VM' $args; $global:World.VMs.Remove($a.Name) }
function Start-VM {
    $a = Write-Call 'Start-VM' $args
    $global:World.VMs[$a.Name].State = 'Running'
    $global:World.GuestUp = -not $global:World.GuestNeverAnswers
}
function Stop-VM {
    $a = Write-Call 'Stop-VM' $args
    $global:World.VMs[$a.Name].State = 'Off'
    $global:World.GuestUp = $false
}
function Get-VMFirmware { return $global:World.VMs[(Get-StandInArguments $args).VMName].Firmware }
function Set-VMFirmware {
    $a = Write-Call 'Set-VMFirmware' $args
    $f = $global:World.VMs[$a.VMName].Firmware
    if ($a.ContainsKey('EnableSecureBoot')) { $f.SecureBoot = $a.EnableSecureBoot }
    if ($a.ContainsKey('SecureBootTemplate')) { $f.SecureBootTemplate = $a.SecureBootTemplate }
    if ($a.ContainsKey('BootOrder')) { $f.BootOrder = @([pscustomobject] @{ BootType = 'Drive' }) }
}
function Get-VMSnapshot { return $global:World.VMs[(Get-StandInArguments $args).VMName].Snapshots }
function Get-VMMemory { return $global:World.VMs[(Get-StandInArguments $args).VMName].Memory }
function Set-VMMemory {
    $a = Write-Call 'Set-VMMemory' $args
    $m = $global:World.VMs[$a.VMName].Memory
    if ($a.ContainsKey('DynamicMemoryEnabled')) { $m.DynamicMemoryEnabled = $a.DynamicMemoryEnabled }
    if ($a.ContainsKey('StartupBytes')) { $m.Startup = $a.StartupBytes }
}
function Get-VMProcessor { return $global:World.VMs[(Get-StandInArguments $args).VMName].Processor }
function Set-VMProcessor {
    $a = Write-Call 'Set-VMProcessor' $args
    $p = $global:World.VMs[$a.VMName].Processor
    if ($a.ContainsKey('Count')) { $p.Count = $a.Count }
    if ($a.ContainsKey('ExposeVirtualizationExtensions')) { $p.ExposeVirtualizationExtensions = $a.ExposeVirtualizationExtensions }
}
function Get-VMSecurity { return [pscustomobject] @{ TpmEnabled = $global:World.VMs[(Get-StandInArguments $args).VMName].Tpm } }
function Get-VMIntegrationService { return $global:World.VMs[(Get-StandInArguments $args).VMName].Services }
function Disable-VMIntegrationService {
    $a = Write-Call 'Disable-VMIntegrationService' $args
    ($global:World.VMs[$a.VMName].Services | Where-Object Name -EQ $a.Name).Enabled = $false
}
function Get-VMHardDiskDrive { return $global:World.VMs[(Get-StandInArguments $args).VMName].Disks }
function Get-VMDvdDrive { return $global:World.VMs[(Get-StandInArguments $args).VMName].Dvds }
function Add-VMDvdDrive {
    $a = Write-Call 'Add-VMDvdDrive' $args
    $global:World.VMs[$a.VMName].Dvds.Add([pscustomobject] @{ Path = $a.Path; ControllerNumber = 0; ControllerLocation = 1 })
}
function Remove-VMDvdDrive {
    $a = Write-Call 'Remove-VMDvdDrive' $args
    $global:World.VMs[$a.VMName].Dvds.Clear()
}
function Get-VMGpuPartitionAdapter { return $global:World.VMs[(Get-StandInArguments $args).VMName].Gpu }
function Get-VMAssignableDevice { return $global:World.VMs[(Get-StandInArguments $args).VMName].Assigned }
function Get-VMFibreChannelHba { return $global:World.VMs[(Get-StandInArguments $args).VMName].Fibre }
function Get-VMNetworkAdapter { return $global:World.VMs[(Get-StandInArguments $args).VMName].Adapter }
function Set-VMNetworkAdapter {
    $a = Write-Call 'Set-VMNetworkAdapter' $args
    $n = $global:World.VMs[$a.VMName].Adapter
    foreach ($key in 'MacAddressSpoofing', 'DhcpGuard', 'RouterGuard', 'AllowTeaming', 'DeviceNaming') {
        if ($a.ContainsKey($key)) { $n.$key = $a[$key] }
    }
    if ($a.ContainsKey('PortMirroring')) { $n.PortMirroringMode = $a.PortMirroring }
}
function Get-VMNetworkAdapterAcl { return $global:World.VMs[(Get-StandInArguments $args).VMName].Acls }
function Add-VMNetworkAdapterAcl {
    $a = Write-Call 'Add-VMNetworkAdapterAcl' $args
    $global:World.VMs[$a.VMName].Acls.Add([pscustomobject] @{ Direction = $a.Direction; Action = $a.Action; RemoteAddress = $a.RemoteIPAddress })
}

# ---- The switch and the network.
function Get-VMSwitch { return $global:World.Switches[(Get-StandInArguments $args).Name] }
function New-VMSwitch {
    $a = Write-Call 'New-VMSwitch' $args
    $global:World.Switches[$a.Name] = [pscustomobject] @{ Name = $a.Name; Notes = $a.Notes; SwitchType = $a.SwitchType }
    return $global:World.Switches[$a.Name]
}
function Remove-VMSwitch { $a = Write-Call 'Remove-VMSwitch' $args; $global:World.Switches.Remove($a.Name) }
function Get-NetAdapter {
    $a = Get-StandInArguments $args
    if ($a.Name -eq 'vEthernet (qor-ci-vm)' -and $global:World.Switches['qor-ci-vm']) { return [pscustomobject] @{ Name = $a.Name } }
}
function Get-NetAdapterBinding {
    $a = Get-StandInArguments $args
    if (-not $global:World.Bindings.ContainsKey($a.Name)) {
        $global:World.Bindings[$a.Name] = @($a.ComponentID | ForEach-Object { [pscustomobject] @{ ComponentID = $_; Enabled = $true } })
    }
    return $global:World.Bindings[$a.Name]
}
function Disable-NetAdapterBinding {
    $a = Write-Call 'Disable-NetAdapterBinding' $args
    ($global:World.Bindings[$a.Name] | Where-Object ComponentID -EQ $a.ComponentID).Enabled = $false
}
function Get-NetNat {
    $a = Get-StandInArguments $args
    if ($a.Name) { return $global:World.Nats[$a.Name] }
    return @($global:World.Nats.Values)
}
function New-NetNat {
    $a = Write-Call 'New-NetNat' $args
    $global:World.Nats[$a.Name] = [pscustomobject] @{ Name = $a.Name; InternalIPInterfaceAddressPrefix = $a.InternalIPInterfaceAddressPrefix }
}
function Remove-NetNat { $a = Write-Call 'Remove-NetNat' $args; $global:World.Nats.Remove($a.Name) }
function Get-NetIPAddress {
    $a = Get-StandInArguments $args
    if ($a.InterfaceAlias) { return @($global:World.Addresses | Where-Object InterfaceAlias -EQ $a.InterfaceAlias) }
    if ($a.AddressFamily -eq 'IPv6') { return @([pscustomobject] @{ IPAddress = 'fe80::1%5'; InterfaceAlias = 'Wi-Fi' }, [pscustomobject] @{ IPAddress = '::1'; InterfaceAlias = 'Loopback' }, [pscustomobject] @{ IPAddress = '2001:db8::5'; InterfaceAlias = 'Wi-Fi' }) }
    return @([pscustomobject] @{ IPAddress = '127.0.0.1'; InterfaceAlias = 'Loopback' }, [pscustomobject] @{ IPAddress = '192.168.1.50'; InterfaceAlias = 'Wi-Fi' }, [pscustomobject] @{ IPAddress = '172.22.0.1'; InterfaceAlias = 'vEthernet (WSL (Hyper-V firewall))' }) + @($global:World.Addresses)
}
function New-NetIPAddress {
    $a = Write-Call 'New-NetIPAddress' $args
    $global:World.Addresses.Add([pscustomobject] @{ IPAddress = $a.IPAddress; PrefixLength = $a.PrefixLength; InterfaceAlias = $a.InterfaceAlias })
}
function Remove-NetIPAddress {
    $a = Write-Call 'Remove-NetIPAddress' $args
    @($global:World.Addresses | Where-Object IPAddress -EQ $a.IPAddress) | ForEach-Object { $null = $global:World.Addresses.Remove($_) }
}
function Get-NetRoute {
    $a = Get-StandInArguments $args
    if ($a.DestinationPrefix -eq '0.0.0.0/0') { return [pscustomobject] @{ NextHop = '192.168.1.1'; InterfaceAlias = 'Wi-Fi' } }
    return $global:World.Routes
}
function Get-NetNeighbor {
    return @(
        [pscustomobject] @{ IPAddress = '192.168.1.20'; InterfaceAlias = 'Wi-Fi' },
        [pscustomobject] @{ IPAddress = '172.22.12.5'; InterfaceAlias = 'vEthernet (WSL (Hyper-V firewall))' },
        [pscustomobject] @{ IPAddress = '239.255.255.250'; InterfaceAlias = 'Wi-Fi' },
        [pscustomobject] @{ IPAddress = '10.213.57.2'; InterfaceAlias = 'vEthernet (qor-ci-vm)' }
    )
}
function Get-NetTCPConnection {
    return @(
        [pscustomobject] @{ LocalAddress = '0.0.0.0'; LocalPort = 445 },
        [pscustomobject] @{ LocalAddress = '::'; LocalPort = 49664 },
        [pscustomobject] @{ LocalAddress = '127.0.0.1'; LocalPort = 8123 }
    )
}
function Get-DnsClientServerAddress { return @([pscustomobject] @{ ServerAddresses = @('192.168.1.1', '1.1.1.1') }) }
function Get-NetFirewallProfile { return $global:World.Profiles }
function Get-NetFirewallRule { return $global:World.Rules[(Get-StandInArguments $args).Name] }
function New-NetFirewallRule {
    $a = Write-Call 'New-NetFirewallRule' $args
    $global:World.Rules[$a.Name] = [pscustomobject] @{
        Name = $a.Name; Description = $a.Description; Enabled = $a.Enabled; Action = $a.Action; Direction = $a.Direction; Profile = $a.Profile
        InterfaceAlias = $a.InterfaceAlias ?? 'Any'; RemoteAddress = $a.RemoteAddress ?? 'Any'
    }
}
function Remove-NetFirewallRule { $a = Write-Call 'Remove-NetFirewallRule' $args; $global:World.Rules.Remove($a.Name) }
function Get-NetFirewallInterfaceFilter { return [pscustomobject] @{ InterfaceAlias = (Get-StandInArguments $args).AssociatedNetFirewallRule.InterfaceAlias } }
function Get-NetFirewallAddressFilter { return [pscustomobject] @{ RemoteAddress = (Get-StandInArguments $args).AssociatedNetFirewallRule.RemoteAddress } }
