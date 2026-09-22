[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build'),
    [string]$Generator = 'Visual Studio 18 2026',
    [string[]]$CMakeArguments = @(),
    [switch]$Package,
    [switch]$SourcePackage,
    [string]$PackageVersion = ([DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString(
        'yyyy.MM.dd', [Globalization.CultureInfo]::InvariantCulture))
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
if (-not ($Package -or $SourcePackage)) { return }

if ($PackageVersion -notmatch '^[0-9]{4}\.[0-9]{2}\.[0-9]{2}(?:p[1-9][0-9]*)?$') {
    throw 'PackageVersion must use yyyy.MM.dd or yyyy.MM.ddpN.'
}
$null = [datetime]::ParseExact(($PackageVersion -split 'p')[0], 'yyyy.MM.dd',
    [Globalization.CultureInfo]::InvariantCulture)
$output = Join-Path $BuildDirectory 'Release'
$stage = Join-Path $BuildDirectory ('package-' + [guid]::NewGuid().ToString('N'))
$binary = Join-Path $stage 'binary'
New-Item -ItemType Directory -Path $binary -Force | Out-Null
# Explicit component and fresh staging keep licenses, docs and stale build files
# out of the runtime ZIP. All license texts remain in the source repository.
Invoke-CMake @('--install',$BuildDirectory,'--config','Release','--component','Runtime','--prefix',$binary)
$hash = (Get-FileHash -LiteralPath (Join-Path $binary 'AddIn/ttp_aac.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  AddIn/ttp_aac.dll" | Set-Content -LiteralPath (Join-Path $binary 'SHA256SUMS.txt') -Encoding UTF8
Compress-Archive -LiteralPath (Join-Path $binary 'AddIn'),(Join-Path $binary 'SHA256SUMS.txt') `
    -DestinationPath (Join-Path $output "ttp_aac-$PackageVersion.zip") -Force
$archives = @((Join-Path $output "ttp_aac-$PackageVersion.zip"))
if ($SourcePackage) {
    $source = Join-Path $stage 'source/ttp_aac'
    New-Item -ItemType Directory -Path $source -Force | Out-Null
    # Explicit allowlist: no local tests, samples, binaries, caches or evidence.
    foreach ($entry in @('CMakeLists.txt','build.ps1','README.md','LICENSE','.gitignore',
                         '.clang-format','cmake','src','include','docs','.github')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $entry) -Destination $source -Recurse
    }
    $notices = Join-Path $source 'third_party/faad2'
    $decoderSource = Join-Path $notices 'source'
    New-Item -ItemType Directory -Path $decoderSource -Force | Out-Null
    foreach ($entry in @('COPYING','AUTHORS','ORIGIN.txt')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "third_party/faad2/$entry") -Destination $notices
    }
    $upstream = (Get-Content -LiteralPath (Join-Path $BuildDirectory 'faad2-source-path.txt') -Encoding UTF8 -Raw).Trim()
    foreach ($entry in @('include','libfaad','COPYING','AUTHORS','README','properties.json')) {
        Copy-Item -LiteralPath (Join-Path $upstream $entry) -Destination $decoderSource -Recurse
    }
    $sourceArchive = Join-Path $output "ttp_aac-$PackageVersion-source.zip"
    Compress-Archive -LiteralPath $source -DestinationPath $sourceArchive -Force
    $archives += $sourceArchive
}
Get-FileHash -LiteralPath $archives -Algorithm SHA256 |
    ForEach-Object { '{0}  {1}' -f $_.Hash.ToLowerInvariant(),[IO.Path]::GetFileName($_.Path) } |
    Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding UTF8
Write-Output "AAC packages: $output"
