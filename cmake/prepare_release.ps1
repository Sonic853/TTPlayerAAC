[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReleaseDate,
    [Parameter(Mandatory = $true)][string]$Repository,
    [Parameter(Mandatory = $true)][string]$Commit,
    [string]$ArtifactDirectory = 'artifact',
    [string]$ServerUrl = 'https://github.com'
)
$ErrorActionPreference = 'Stop'

# Same date and numeric patch rules as rebuild/.github/workflows/manual-build.yml.
function Read-VersionTag([string]$Name) {
    $match = [regex]::Match($Name, '^(?<date>[0-9]{4}\.[0-9]{2}\.[0-9]{2})(?:p(?<patch>[1-9][0-9]*))?$')
    if (-not $match.Success) { return }
    $date = [datetime]::MinValue
    if (-not [datetime]::TryParseExact($match.Groups['date'].Value, 'yyyy.MM.dd',
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::None, [ref]$date)) { return }
    $patch = 0L
    if ($match.Groups['patch'].Success -and -not [long]::TryParse($match.Groups['patch'].Value, [ref]$patch)) {
        throw 'Version patch number is too large.'
    }
    [pscustomobject]@{ Name = $Name; Date = $date; Patch = $patch }
}
$base = Read-VersionTag $ReleaseDate
if (-not $base -or $base.Patch -ne 0) { throw 'Missing or invalid Beijing build date.' }
if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $Commit -notmatch '^[0-9a-fA-F]{40}$') {
    throw 'Missing or invalid repository/commit.'
}
$archiveName = "ttp_aac-$ReleaseDate.zip"
$archive = Join-Path $ArtifactDirectory $archiveName
$sourceName = "ttp_aac-$ReleaseDate-source.zip"
$sourceArchive = Join-Path $ArtifactDirectory $sourceName
$manifest = Join-Path $ArtifactDirectory 'SHA256SUMS.txt'
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceHash = (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant()
$expectedHashes = (Get-Content -LiteralPath $manifest -Encoding UTF8 -Raw).Trim() -replace "`r", ''
if ($expectedHashes -cne "$hash  $archiveName`n$sourceHash  $sourceName") {
    throw 'AAC package SHA-256 verification failed.'
}
$tags = @(gh api "repos/$Repository/tags?per_page=100" --paginate --jq '.[].name')
if ($LASTEXITCODE -ne 0) { throw 'Could not read repository tags.' }
# Draft releases can reserve a name even before their tag exists.
$releaseTags = @(gh api "repos/$Repository/releases?per_page=100" --paginate --jq '.[].tag_name')
if ($LASTEXITCODE -ne 0) { throw 'Could not read existing releases.' }
$versions = @($tags | ForEach-Object { Read-VersionTag $_ })
$occupied = @($versions) + @($releaseTags | ForEach-Object { Read-VersionTag $_ })
$sameDay = @($occupied | Where-Object { $_.Date -eq $base.Date } | Sort-Object Patch -Descending)
$version = $base.Name
if ($sameDay.Count) {
    if ($sameDay[0].Patch -eq [long]::MaxValue) { throw 'Version patch number is exhausted.' }
    $version += 'p' + ($sameDay[0].Patch + 1).ToString([Globalization.CultureInfo]::InvariantCulture)
}
# Keep the verified ZIP bytes; only the package name and outer hash list change.
$releaseName = "ttp_aac-$version.zip"
$releaseSourceName = "ttp_aac-$version-source.zip"
if ($version -cne $ReleaseDate) {
    Rename-Item -LiteralPath $archive -NewName $releaseName
    Rename-Item -LiteralPath $sourceArchive -NewName $releaseSourceName
}
"$hash  $releaseName`n$sourceHash  $releaseSourceName" | Set-Content -LiteralPath $manifest -Encoding UTF8
$previous = $versions | Sort-Object Date, Patch -Descending | Select-Object -First 1
$repoUrl = "$ServerUrl/$Repository"
$logUrl = if ($previous) { "$repoUrl/compare/$($previous.Name)...$version" }
          else { "$repoUrl/commits/$version" }
$notes = @"
**完整更新日志**: $logUrl

下载并解压 ttp_aac-$version.zip，关闭播放器后将 AddIn/ttp_aac.dll 复制到千千静听安装目录。
同一 x86 DLL 支持普通版及 XP SP3／Win7，CPU 需要 SSE2。
ZIP 仅包含 AddIn/ttp_aac.dll 和 SHA256SUMS.txt；Nero 编码组件需保留播放器原有文件。
对应源码另见 ttp_aac-$version-source.zip，包含 FAAD2 2.11.3 原始解码源码、适配脚本及完整许可证，不含本地测试。
Code from FAAD2 is copyright (c) Nero AG, www.nero.com
FAAD2 使用 GPL-2.0-or-later；包含该核心的插件整体分发须遵循其条款，自有代码的 MIT 许可不替代第三方许可。

[本次构建的源码、许可证和构建说明]($repoUrl/tree/$Commit)
[FAAD2 完整许可证]($repoUrl/blob/$Commit/third_party/faad2/COPYING)
"@
$notes | Set-Content -LiteralPath (Join-Path $ArtifactDirectory 'release-notes.md') -Encoding UTF8
if ($env:GITHUB_OUTPUT) {
    "version=$version" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}
Write-Output "Prepared AAC release $version"
