$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '..\Source' | Resolve-Path
$skip = @('MelatoninBlur','shadows-main','ScreenCaptureLite','VeniceSunsetHdri')
$out = New-Object System.Collections.Generic.List[string]

function Get-QuotedStringsWithNonAscii([string]$line) {
    $bad = New-Object System.Collections.Generic.List[string]
    $i = 0
    $len = $line.Length
    while ($i -lt $len) {
        if ($line[$i] -eq [char]34) {
            $j = $i + 1
            $has = $false
            $start = $i
            while ($j -lt $len) {
                $c = $line[$j]
                if ($c -eq [char]92) { # backslash
                    $j += 2
                    continue
                }
                if ($c -eq [char]34) { break }
                if ([int][char]$c -gt 127) { $has = $true }
                $j++
            }
            if ($has) {
                $end = [Math]::Min($j + 1, $len)
                $s = $line.Substring($start, $end - $start)
                if ($s.Length -gt 140) { $s = $s.Substring(0, 137) + '...' }
                $bad.Add($s) | Out-Null
            }
            $i = $j + 1
            continue
        }
        $i++
    }
    return $bad
}

Get-ChildItem -Path $root -Recurse -Include *.h,*.cpp,*.hpp | ForEach-Object {
    $full = $_.FullName
    $skipThis = $false
    foreach ($s in $skip) {
        if ($full -like "*\$s\*") { $skipThis = $true; break }
    }
    if ($skipThis) { return }

    $rel = $full.Substring($root.Path.Length + 1)
    $n = 0
    foreach ($line in [System.IO.File]::ReadAllLines($full, [System.Text.Encoding]::UTF8)) {
        $n++
        $hasNon = $false
        $charSet = New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($ch in $line.ToCharArray()) {
            $code = [int][char]$ch
            if ($code -gt 127) {
                $hasNon = $true
                [void]$charSet.Add(('{0} U+{1:X4}' -f $ch, $code))
            }
        }
        if (-not $hasNon) { continue }

        $stripped = $line.TrimStart()
        $isComment = $stripped.StartsWith('//') -or $stripped.StartsWith('/*') -or $stripped.StartsWith('*')
        $bad = Get-QuotedStringsWithNonAscii $line
        $special = ($line -match 'CharPointer_UTF8|juce_wchar|charToString')

        if ($bad.Count -eq 0 -and -not $special) {
            if ($isComment) { continue }
            continue
        }

        $out.Add(('{0}:{1}' -f $rel, $n)) | Out-Null
        $chars = ($charSet | Sort-Object) -join ', '
        $out.Add(('  chars: {0}' -f $chars)) | Out-Null
        foreach ($b in $bad) {
            $out.Add(('  str: {0}' -f $b)) | Out-Null
        }
        if ($special -and $bad.Count -eq 0) {
            $snip = if ($line.Length -gt 200) { $line.Substring(0, 200) } else { $line }
            $out.Add(('  line: {0}' -f $snip)) | Out-Null
        }
        $out.Add('') | Out-Null
    }
}

$reportPath = Join-Path $PSScriptRoot '_nonascii_report.txt'
[System.IO.File]::WriteAllLines($reportPath, $out)
Write-Output ("Wrote {0} lines to {1}" -f $out.Count, $reportPath)
