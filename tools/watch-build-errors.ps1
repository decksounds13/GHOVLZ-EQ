# Watch GhovlzDyn MSBuild logs. Print only FAILED + compact error lines.
# Token-cheap: never dumps the log; only `: error` / `: fatal error` lines.

$ErrorActionPreference = 'SilentlyContinue'
$proj = Split-Path $PSScriptRoot -Parent
$vs = Join-Path $proj 'Builds\VisualStudio2022\x64'
$logs = @(
    (Join-Path $vs 'Debug\Shared Code\GhovlzDyn_SharedCode.log'),
    (Join-Path $vs 'Release\Shared Code\GhovlzDyn_SharedCode.log'),
    (Join-Path $vs 'Debug\VST3\GhovlzDyn_VST3.log'),
    (Join-Path $vs 'Release\VST3\GhovlzDyn_VST3.log')
)
$errPat = ':\s+(fatal )?error\s'
$seen = @{}

function Stamp([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    $i = Get-Item -LiteralPath $path
    return '{0}:{1}' -f $i.LastWriteTimeUtc.Ticks, $i.Length
}

function CompactLine([string]$line, [string]$projRoot) {
    $t = $line.Trim()
    if ($projRoot -and $t.StartsWith($projRoot, [StringComparison]::OrdinalIgnoreCase)) {
        $t = $t.Substring($projRoot.Length).TrimStart('\', '/')
    }
    if ($t.Length -gt 220) { $t = $t.Substring(0, 217) + '...' }
    return $t
}

function Get-Errors([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return @() }
    $hits = Select-String -LiteralPath $path -Pattern $errPat -Encoding Default |
            Select-Object -First 20
    $out = @()
    foreach ($h in $hits) {
        $out += (CompactLine $h.Line $proj)
    }
    return $out
}

# Seed so an old successful/failed log is not replayed on start.
foreach ($p in $logs) { $seen[$p] = Stamp $p }

while ($true) {
    Start-Sleep -Milliseconds 900
    foreach ($p in $logs) {
        $now = Stamp $p
        if (-not $now -or $now -eq $seen[$p]) { continue }

        # Wait until MSBuild stops appending (log rewrite + compile).
        Start-Sleep -Milliseconds 1600
        $settled = Stamp $p
        if ($settled -ne $now) { continue }

        $seen[$p] = $settled
        $errs = @(Get-Errors $p)
        if ($errs.Count -eq 0) { continue }

        # VST3 "cannot open GhovlzDyn.lib" is a cascade; SharedCode already has the real errors.
        $onlyLib = ($errs.Count -gt 0) -and
                   ($errs | Where-Object { $_ -notmatch 'LNK1181|GhovlzDyn\.lib' }).Count -eq 0
        if ($p -match 'VST3' -and $onlyLib) { continue }

        $name = Split-Path $p -Leaf
        [Console]::WriteLine('FAILED')
        [Console]::WriteLine($name + ' ' + $errs.Count + ' error(s)')
        foreach ($e in $errs) { [Console]::WriteLine($e) }
        [Console]::Out.Flush()
    }
}
