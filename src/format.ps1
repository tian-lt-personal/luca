# Format the repository's C and C++ sources with the newest MSVC LLVM clang-format.
# Run: powershell -ExecutionPolicy Bypass -File src\format.ps1

$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Host "error: $Message" -ForegroundColor Red
    exit 1
}

$vsWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$vsWhere = if ($vsWhereCommand) { $vsWhereCommand.Path } else {
    @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $vsWhere) {
    Fail "vswhere.exe was not found; install Visual Studio or add its Installer directory to PATH."
}

$installationPath = & $vsWhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null |
    Select-Object -First 1
if ($null -eq $installationPath) {
    Fail "the newest Visual Studio installation with the MSVC x64 tools was not found."
}
$installationPath = ([string]$installationPath).Trim()
if (-not $installationPath) {
    Fail "the newest Visual Studio installation with the MSVC x64 tools was not found."
}

$llvmRoot = Join-Path $installationPath 'VC\Tools\Llvm'
$clangFormat = @(
    (Join-Path $llvmRoot 'x64\bin\clang-format.exe'),
    (Join-Path $llvmRoot 'ARM64\bin\clang-format.exe')
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $clangFormat) {
    Fail "clang-format.exe was not found under '$llvmRoot'."
}

$sourceRoot = $PSScriptRoot
$generatedLexer = Join-Path $sourceRoot 'luca\lexer.cpp'
$buildRoot = Join-Path $sourceRoot 'b_'
$extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx')
$files = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | Where-Object {
    $path = $_.FullName
    $underBuild = $path.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
    -not $underBuild -and
        $path -ine $generatedLexer -and
        $extensions -contains $_.Extension.ToLowerInvariant()
} | Sort-Object FullName

if (-not $files) {
    Fail "no C/C++ source files were found under '$sourceRoot'."
}

Write-Host "Using $clangFormat"
Write-Host "Formatting $($files.Count) C/C++ files under $sourceRoot"

$failed = [System.Collections.Generic.List[string]]::new()
foreach ($file in $files) {
    & $clangFormat -i --style=file -- $file.FullName
    if ($LASTEXITCODE -ne 0) {
        $failed.Add($file.FullName)
    }
}

if ($failed.Count -ne 0) {
    Fail "clang-format failed for: $($failed -join ', ')"
}

Write-Host 'Formatting complete.'
