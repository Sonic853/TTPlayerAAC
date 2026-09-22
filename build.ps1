[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build'),
    [string]$Generator = 'Visual Studio 18 2026',
    [string[]]$CMakeArguments = @(),
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
function Invoke-CMake([string[]]$Arguments) {
    # Windows PowerShell 5 treats redirected native stderr as ErrorRecord even
    # for CMake's successful status messages. The process exit code is decisive.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & cmake @Arguments 2>&1 | ForEach-Object { $_.ToString() }
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    if ($code -ne 0) { throw "CMake failed ($code): $Arguments" }
}
Invoke-CMake (@('-S',$PSScriptRoot,'-B',$BuildDirectory,'-G',$Generator,'-A','Win32') + $CMakeArguments)
Invoke-CMake @('--build',$BuildDirectory,'--config','Release','--target','ttp_aac','--parallel','4')
if (-not $Package) { return }

$project = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'CMakeLists.txt') -Raw
if ($project -notmatch 'project\(TtpAac VERSION ([0-9.]+)') { throw 'Missing project version.' }
$version = $Matches[1]
$output = Join-Path $BuildDirectory 'Release'
$stage = Join-Path $BuildDirectory ('package-' + [guid]::NewGuid().ToString('N'))
$binary = Join-Path $stage 'binary'
$source = Join-Path $stage 'source/ttp_aac'
New-Item -ItemType Directory -Path $binary,$source -Force | Out-Null
Invoke-CMake @('--install',$BuildDirectory,'--config','Release','--prefix',$binary)
Copy-Item -LiteralPath (Join-Path $output 'legacy-imports.json') -Destination $binary
$hash = (Get-FileHash -LiteralPath (Join-Path $binary 'AddIn/ttp_aac.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  AddIn/ttp_aac.dll" | Set-Content -LiteralPath (Join-Path $binary 'SHA256SUMS.txt') -Encoding UTF8

# Explicit source allowlist: no build trees, original DLLs, private evidence or tests.
foreach ($entry in @('CMakeLists.txt','build.ps1','README.md','LICENSE','.gitignore','.clang-format','cmake','src','include','third_party','docs','.github')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $entry) -Destination $source -Recurse
}
Compress-Archive -Path (Join-Path $binary '*') -DestinationPath (Join-Path $output "ttp_aac-$version.zip") -Force
Compress-Archive -LiteralPath $source -DestinationPath (Join-Path $output "ttp_aac-$version-source.zip") -Force
Get-FileHash -LiteralPath (Join-Path $output "ttp_aac-$version.zip"),(Join-Path $output "ttp_aac-$version-source.zip") -Algorithm SHA256 |
    ForEach-Object { '{0}  {1}' -f $_.Hash.ToLowerInvariant(),[IO.Path]::GetFileName($_.Path) } |
    Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding UTF8
Write-Output "AAC packages: $output"
