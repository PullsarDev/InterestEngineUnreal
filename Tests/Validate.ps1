# SPDX-FileCopyrightText: 2026 PullsarDev
# SPDX-License-Identifier: Apache-2.0

<#
.SYNOPSIS
Builds a disposable external C++ consumer of this plugin and, for Editor Development, runs its
automation tests.

.DESCRIPTION
The plugin ships no test project. This script generates a minimal host project, copies the plugin
into it and builds a separate game module (InterestEngineHost) that calls the exported adapter API
from the module boundary, the way a real consumer does. It also compiles every C++ block of
README.md and runs the first one, so the documented examples cannot drift from the code.

Nothing is built inside the plugin directory: the plugin is copied into the host, so a validation
build never touches the Binaries or Intermediate folders of the plugin you develop with.

Requirements: Windows 10/11 (64-bit), PowerShell 5.1 or 7, Unreal Engine 5.8 installed from the Epic
Games Launcher or built from source, and the Visual Studio C++ toolchain and Windows SDK that
Unreal Engine 5.8 requires. A standalone clone of this repository is enough; no Unreal project is
needed. Each configuration leaves about 3 GB under -WorkRoot (mostly the engine's shared
precompiled headers), so -Matrix needs about 17 GB; delete the work root when you are done.

Path length: Unreal Build Tool writes deeply nested intermediate files. Keep -WorkRoot short
(for example C:\IEUV); a clone under a long path such as Documents\GitHub\... can exceed the
Windows MAX_PATH limit when the default work root inside the clone is used.

.PARAMETER Target
Editor or Game. Default Editor.

.PARAMETER Configuration
Development or Shipping. Default Development. Shipping is Game only.

.PARAMETER Mode
Unity (-ForceUnity -DisableAdaptiveUnity) or NonUnity (-DisableUnity). Default Unity.

.PARAMETER RunTests
Run the automation tests after building. Requires Editor and Development.

.PARAMETER Matrix
Run all six configurations (Editor Development, Game Development and Game Shipping, each Unity and
NonUnity); Editor Development also runs the tests. -Target, -Configuration, -Mode and -RunTests
are ignored. Every configuration runs even if an earlier one fails; the exit code is non-zero if
any failed.

.PARAMETER EngineRoot
Root of an Unreal Engine 5.8 installation (the folder that contains Engine\). When omitted, an
installation is read from the Epic Games Launcher metadata.

.PARAMETER WorkRoot
Folder for the generated host projects and logs. Default: .local\Validate inside the plugin
directory (ignored by Git). One subfolder is created per configuration.

.PARAMETER Clean
Delete the configuration's generated host and reports before building.

.PARAMETER AdditionalBuildArguments
Extra arguments for Unreal Build Tool, for example -DisablePlugin=Fab on an installation whose
engine plugins do not match its precompiled rules.

.EXAMPLE
.\Tests\Validate.ps1 -Matrix -WorkRoot C:\IEUV

.EXAMPLE
.\Tests\Validate.ps1 -Target Editor -Configuration Development -Mode Unity -RunTests -EngineRoot "D:\UE_5.8"
#>
[CmdletBinding()]
param(
    [ValidateSet('Editor', 'Game')] [string]$Target = 'Editor',
    [ValidateSet('Development', 'Shipping')] [string]$Configuration = 'Development',
    [ValidateSet('Unity', 'NonUnity')] [string]$Mode = 'Unity',
    [switch]$RunTests,
    [switch]$Matrix,
    [string]$EngineRoot,
    [string]$WorkRoot,
    [switch]$Clean,
    [string[]]$AdditionalBuildArguments = @()
)
$ErrorActionPreference = 'Stop'
$PluginRoot = (Resolve-Path (Split-Path $PSScriptRoot -Parent)).Path
$ExpectedTests = 8
$MaxWorkRootLength = 60

function Resolve-Engine([string]$Requested) {
    $Candidates = @()
    if ($Requested) {
        $Candidates += $Requested
    }
    else {
        $InstallFile = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (!(Test-Path -LiteralPath $InstallFile)) {
            throw 'No -EngineRoot given and no Epic Games Launcher metadata found. Pass -EngineRoot <folder containing Engine\>.'
        }
        $Candidates += @((Get-Content $InstallFile -Raw | ConvertFrom-Json).InstallationList | ForEach-Object { $_.InstallLocation }) | Select-Object -Unique
    }
    foreach ($Candidate in $Candidates) {
        if (!$Candidate) { continue }
        $VersionFile = Join-Path $Candidate 'Engine\Build\Build.version'
        if (!(Test-Path -LiteralPath $VersionFile)) {
            if ($Requested) { throw "'$Candidate' is not an Unreal Engine root (Engine\Build\Build.version not found)." }
            continue
        }
        $Version = Get-Content $VersionFile -Raw | ConvertFrom-Json
        if ($Version.MajorVersion -eq 5 -and $Version.MinorVersion -eq 8) {
            return [pscustomobject]@{ Root = (Resolve-Path $Candidate).Path; Version = "$($Version.MajorVersion).$($Version.MinorVersion).$($Version.PatchVersion) (CL $($Version.Changelist))" }
        }
        if ($Requested) { throw "'$Candidate' is Unreal Engine $($Version.MajorVersion).$($Version.MinorVersion); this plugin is verified on 5.8 only." }
    }
    throw 'No Unreal Engine 5.8 installation found. Pass -EngineRoot <folder containing Engine\>.'
}

# Runs a native command, tees its output to a log and returns the exit code. Native stderr output
# must not become a terminating error under $ErrorActionPreference = 'Stop'.
function Invoke-Logged([string]$Log, [string]$Command, [string[]]$Arguments) {
    $Previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Command @Arguments 2>&1 | Tee-Object -FilePath $Log | Out-Host
        return $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $Previous }
}

function Invoke-Validation([string]$Target, [string]$Configuration, [string]$Mode, [bool]$RunTests) {
    if ($Target -eq 'Editor' -and $Configuration -eq 'Shipping') { throw 'Shipping is a Game-only configuration.' }
    if ($RunTests -and ($Target -ne 'Editor' -or $Configuration -ne 'Development')) { throw 'Automation requires Editor Development.' }
    $Name = "$Target-$Configuration-$Mode"
    $RunRoot = Join-Path $WorkRoot $Name
    $HostRoot = Join-Path $RunRoot 'Host'
    $HostPlugin = Join-Path $HostRoot 'Plugins\InterestEngineUnreal'
    $ModuleRoot = Join-Path $HostRoot 'Source\InterestEngineHost'
    $Reports = Join-Path $RunRoot 'Reports'
    if ($Clean -and (Test-Path -LiteralPath $RunRoot)) { Remove-Item -LiteralPath $RunRoot -Recurse -Force }
    if (Test-Path -LiteralPath $Reports) { Remove-Item -LiteralPath $Reports -Recurse -Force }
    New-Item -ItemType Directory -Force $ModuleRoot, $HostPlugin, "$HostRoot\Content", $Reports | Out-Null
    Write-Host "=== $Name ===" -ForegroundColor Cyan

    # Mirror the plugin sources; the excluded folders are neither copied nor purged, so an
    # incremental rebuild keeps the host's own Binaries and Intermediate.
    & robocopy $PluginRoot $HostPlugin /MIR /XD Binaries Intermediate Saved .local .git .vs /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

@'
{
  "FileVersion": 3,
  "EngineAssociation": "5.8",
  "Modules": [{ "Name": "InterestEngineHost", "Type": "Runtime", "LoadingPhase": "Default" }],
  "Plugins": [
    { "Name": "InterestEngineUnreal", "Enabled": true, "SupportedTargetPlatforms": ["Win64"] },
    { "Name": "AndroidFileServer", "Enabled": false }
  ]
}
'@ | Set-Content "$HostRoot\InterestEngineHost.uproject"
@'
using UnrealBuildTool;
public class InterestEngineHost : ModuleRules
{
    public InterestEngineHost(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GameplayTags", "InterestEngineRuntime" });
    }
}
'@ | Set-Content "$ModuleRoot\InterestEngineHost.Build.cs"
    foreach ($Kind in @('Game', 'Editor')) {
        $Suffix = if ($Kind -eq 'Editor') { 'Editor' } else { '' }
@"
using UnrealBuildTool;
public class InterestEngineHost${Suffix}Target : TargetRules
{
    public InterestEngineHost${Suffix}Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.$Kind;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("InterestEngineHost");
    }
}
"@ | Set-Content "$HostRoot\Source\InterestEngineHost$Suffix.Target.cs"
    }
    Copy-Item -LiteralPath "$PSScriptRoot\Consumer\InterestEngineConsumer.cpp" -Destination "$ModuleRoot\InterestEngineConsumer.cpp"

    # Compile every C++ block of the README, each in its own namespace and translation unit.
    # Blocks are declarations (includes, functions, classes), so they can be pasted into a project.
    $ReadmeText = Get-Content "$PluginRoot\README.md" -Raw
    $Blocks = [regex]::Matches($ReadmeText, '(?s)```cpp\r?\n(.*?)```')
    if ($Blocks.Count -eq 0) { throw 'README.md has no C++ example.' }
    if ($Blocks[0].Groups[1].Value -notmatch 'MinimalInterestExample') { throw 'The first README C++ block must define MinimalInterestExample().' }
    for ($Index = 0; $Index -lt $Blocks.Count; $Index++) {
        $Lines = $Blocks[$Index].Groups[1].Value -split '\r?\n'
        $Includes = ($Lines | Where-Object { $_ -match '^\s*#include' }) -join "`n"
        $Body = ($Lines | Where-Object { $_ -notmatch '^\s*#include' }) -join "`n"
        $Number = $Index + 1
        "$Includes`n`nnamespace ReadmeExample$Number`n{`n$Body`n}`n" | Set-Content "$ModuleRoot\ReadmeExample$Number.cpp"
    }

    $BuildTarget = if ($Target -eq 'Editor') { 'InterestEngineHostEditor' } else { 'InterestEngineHost' }
    [string[]]$UnityArgs = if ($Mode -eq 'Unity') { @('-ForceUnity', '-DisableAdaptiveUnity') } else { @('-DisableUnity') }
    [string[]]$BuildArgs = @($BuildTarget, 'Win64', $Configuration, "-Project=$HostRoot\InterestEngineHost.uproject", '-WaitMutex', '-NoHotReloadFromIDE') + $UnityArgs + $AdditionalBuildArguments
    $ExitCode = Invoke-Logged "$Reports\Build.log" "$($Engine.Root)\Engine\Build\BatchFiles\Build.bat" $BuildArgs
    if ($ExitCode -ne 0) { throw "UBT failed with exit code $ExitCode (see $Reports\Build.log)" }

    # The runtime module must compile with precise floating point and C++20 in every target.
    $BinaryTarget = if ($Target -eq 'Editor') { 'UnrealEditor' } else { 'UnrealGame' }
    $RuntimeRspRoot = Join-Path $HostPlugin "Intermediate\Build\Win64\x64\$BinaryTarget\$Configuration\InterestEngineRuntime"
    foreach ($RspName in @('InterestEngineRuntime.Shared.rsp', 'PCH.InterestEngineRuntime.h.obj.rsp')) {
        $RspText = Get-Content (Join-Path $RuntimeRspRoot $RspName) -Raw
        if ($RspText -notmatch '(?m)^/fp:precise\s*$' -or $RspText -match '/fp:fast') {
            throw "Expected precise FP semantics in $RspName"
        }
        "$RspName`: /fp:precise" | Add-Content "$Reports\CompilerFlags.txt"
    }
    # Each per-source command line (the shared options are referenced from it) selects C++20.
    $SourceRsps = @(Get-ChildItem $RuntimeRspRoot -Filter '*.obj.rsp')
    if ($SourceRsps.Count -eq 0) { throw "No compile response files found in $RuntimeRspRoot" }
    foreach ($SourceRsp in $SourceRsps) {
        if ((Get-Content $SourceRsp.FullName -Raw) -notmatch '(?m)^/std:c\+\+20\s*$') { throw "Expected C++20 in $($SourceRsp.Name)" }
        "$($SourceRsp.Name): /std:c++20" | Add-Content "$Reports\CompilerFlags.txt"
    }

    if ($RunTests) {
        $ReportFile = Join-Path $Reports 'index.json'
        [string[]]$TestArgs = @("$HostRoot\InterestEngineHost.uproject", '-ExecCmds=Automation RunTests InterestEngine.Adapter; Quit', '-TestExit=Automation Test Queue Empty', '-unattended', '-nopause', '-nullrhi', '-nosplash', '-nosound', "-ReportExportPath=$Reports", "-abslog=$Reports\Automation.log", '-stdout', '-FullStdOutLogOutput')
        $ExitCode = Invoke-Logged "$Reports\Console.log" "$($Engine.Root)\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $TestArgs
        if ($ExitCode -ne 0) { throw "Automation process failed with exit code $ExitCode" }
        if (!(Test-Path -LiteralPath $ReportFile)) { throw "Automation wrote no report ($ReportFile)." }
        $Report = Get-Content $ReportFile -Raw | ConvertFrom-Json
        if ($Report.failed -ne 0 -or $Report.succeeded -ne $ExpectedTests) {
            throw "Expected exactly $ExpectedTests passing consumer automation tests; got $($Report.succeeded) passed, $($Report.failed) failed."
        }
    }
}

$Engine = Resolve-Engine $EngineRoot
if (!$WorkRoot) { $WorkRoot = Join-Path $PluginRoot '.local\Validate' }
$WorkRoot = [System.IO.Path]::GetFullPath($WorkRoot)
if ($WorkRoot.Length -gt $MaxWorkRootLength) {
    throw "-WorkRoot is $($WorkRoot.Length) characters; use at most $MaxWorkRootLength so Unreal's nested intermediate paths stay under MAX_PATH. Example: -WorkRoot C:\IEUV"
}
if ($PluginRoot -eq $WorkRoot -or $PluginRoot.StartsWith($WorkRoot.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw '-WorkRoot must not be, or contain, the plugin directory.'
}
Write-Host "Unreal Engine $($Engine.Version) at $($Engine.Root)"
Write-Host "Work root: $WorkRoot"

if (!$Matrix) {
    Invoke-Validation $Target $Configuration $Mode $RunTests.IsPresent
    Write-Host "PASSED: $Target-$Configuration-$Mode" -ForegroundColor Green
    return
}

$Results = foreach ($Row in @(
        @('Editor', 'Development', 'Unity', $true), @('Editor', 'Development', 'NonUnity', $true),
        @('Game', 'Development', 'Unity', $false), @('Game', 'Development', 'NonUnity', $false),
        @('Game', 'Shipping', 'Unity', $false), @('Game', 'Shipping', 'NonUnity', $false))) {
    $Timer = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        Invoke-Validation $Row[0] $Row[1] $Row[2] $Row[3]
        $Outcome = 'PASSED'; $Detail = ''
    }
    catch { $Outcome = 'FAILED'; $Detail = $_.Exception.Message }
    [pscustomobject]@{ Configuration = "$($Row[0]) $($Row[1]) $($Row[2])"; Tests = $(if ($Row[3]) { "$ExpectedTests automation tests" } else { 'build/link only' }); Result = $Outcome; Seconds = [int]$Timer.Elapsed.TotalSeconds; Detail = $Detail }
}
$Summary = $Results | Format-Table -AutoSize -Wrap | Out-String
Write-Host $Summary
$Summary | Set-Content (Join-Path $WorkRoot 'Matrix-Summary.txt')
if ($Results | Where-Object { $_.Result -ne 'PASSED' }) { exit 1 }
