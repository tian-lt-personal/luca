# Dev install for LUCA: builds the CLI (Release by default) with the vsdbg preset,
# installs luca.exe and the built-in library into Program Files, and adds the bin
# directory to the user PATH. Run: powershell -ExecutionPolicy Bypass -File src\dev-install.ps1

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$Prefix = (Join-Path $env:ProgramFiles 'luca-cpp')
)

$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Host "error: $Message" -ForegroundColor Red
    exit 1
}

# --- prerequisites, checked before any UAC prompt ---
if (-not $env:VCPKG_ROOT) {
    Fail "VCPKG_ROOT is not set; set it to your vcpkg checkout (clone https://github.com/microsoft/vcpkg and run bootstrap-vcpkg.bat)."
}
if (-not (Get-Command re2c -ErrorAction SilentlyContinue)) {
    Fail "re2c was not found on PATH; install it with 'choco install re2c' or from re2c.org."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "cmake was not found on PATH; install it from cmake.org or 'winget install Kitware.CMake'."
}

# --- Program Files is admin-protected: relaunch elevated and wait ---
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).
    IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    $relaunch = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Configuration $Configuration -Prefix `"$Prefix`""
    Write-Host "Requesting administrator privileges (UAC prompt)..."
    try {
        Start-Process powershell -Verb RunAs -ArgumentList $relaunch -Wait
    } catch {
        Fail "elevation was declined; nothing was installed."
    }
} else {
    # configure + build + install (vsdbg is multi-config; POST_BUILD copies std.luca next to the exe)
    Push-Location $PSScriptRoot
    cmake --preset vsdbg
    if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "cmake configure failed (see output above)." }
    cmake --build --preset vsdbg --config $Configuration --target lucacli
    if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "cmake build failed (see output above)." }
    cmake --install (Join-Path $PSScriptRoot 'b_/vsdbg') --config $Configuration --prefix $Prefix
    if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "cmake install failed (see output above)." }
    Pop-Location
}

# --- verify, then add to the user PATH (SetEnvironmentVariable, not setx: no 1024-char limit) ---
$binDir = Join-Path $Prefix 'bin'
if (-not (Test-Path (Join-Path $binDir 'luca.exe')) -or -not (Test-Path (Join-Path $binDir 'std.luca'))) {
    Fail "verification failed: '$binDir' is missing luca.exe or std.luca (check the elevated window output)."
}
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (-not $userPath) { $userPath = '' }
$onPath = ($userPath -split ';') | Where-Object { $_ -and $_.TrimEnd('\') -ieq $binDir.TrimEnd('\') }
if (-not $onPath) {
    $newPath = ($userPath.TrimEnd(';') + ';' + $binDir).TrimStart(';')
    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    Write-Host "Added '$binDir' to the user PATH."
} else {
    Write-Host "'$binDir' is already on the user PATH."
}

Write-Host ""
Write-Host "LUCA installed:"
Write-Host "  exe:  $(Join-Path $binDir 'luca.exe')"
Write-Host "  std:  $(Join-Path $binDir 'std.luca')"
Write-Host "Open a new terminal to pick up the PATH, then run 'luca'."
