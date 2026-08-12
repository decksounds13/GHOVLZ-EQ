# Normalize user-facing UI strings to plain ASCII English.
# Operates on whole listed files (also cleans comments there — harmless).
# Does NOT collapse whitespace (preserves C++ indentation).
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '..\Source' | Resolve-Path

$files = @(
    'ColourRamp\Spec3DRampTimelineComponent.cpp',
    'Learn\EqLearnController.cpp',
    'Learn\EqSourceClassifier.cpp',
    'Menu\Gui\ParticleForceStackComponent.cpp',
    'Menu\Gui\Spectrogram3DSettingsComponent.cpp',
    'Menu\Gui\Spectrogram3DSettingsComponent.h',
    'Menu\Menu.cpp',
    'ParticleNodeGraph\ParticleNodeGraphCanvas.cpp',
    'ParticleNodeGraph\ParticleNodeGraphCanvas.h',
    'ParticleNodeGraph\ParticleNodeGraphCompiler.cpp',
    'ParticleNodeGraph\ParticleNodeTypes.h',
    'EqEditor.cpp',
    'FrequencyResponseComponent.cpp',
    'FrequencyResponseComponent.h',
    'MainComponent.cpp',
    'MainComponent.h',
    'ModSectionComponent.h',
    'OptionBoxMenu.cpp',
    'RetrigButton.h',
    'Spectrogram3DComponent.cpp',
    'BrandWordmark.h'
)

function Normalize-UiText([string]$text) {
    # Mojibake for UTF-8 ellipsis mis-decoded as Windows-1252 (e2 80 a6)
    $mojibakeEllipsis = ([char]0x00E2).ToString() + ([char]0x20AC) + ([char]0x00A6)
    $text = $text.Replace($mojibakeEllipsis, '...')

    # Phrase-level first (spacing preserved by matching surrounding spaces)
    $text = $text.Replace(' ' + [char]0x2014 + ' ', ' - ')   # " — "
    $text = $text.Replace(' ' + [char]0x00B7 + ' ', ' | ')   # " · "
    $text = $text.Replace(' ' + [char]0x2192 + ' ', ' -> ')  # " → "
    $text = $text.Replace(' ' + [char]0x2190 + ' ', ' <- ')  # " ← "

    # Remaining single-character replacements
    $pairs = @(
        @{ From = [char]0x2014; To = '-' }      # em dash
        @{ From = [char]0x2013; To = '-' }      # en dash
        @{ From = [char]0x2026; To = '...' }    # ellipsis
        @{ From = [char]0x00B7; To = '|' }      # middle dot
        @{ From = [char]0x2192; To = '->' }     # right arrow
        @{ From = [char]0x2190; To = '<-' }     # left arrow
        @{ From = [char]0x00B1; To = '+/-' }    # plus-minus
        @{ From = [char]0x00D7; To = 'x' }      # multiplication sign
        @{ From = [char]0x2248; To = '~' }      # almost equal
        @{ From = [char]0x00B2; To = '^2' }     # superscript 2
        @{ From = [char]0x00B0; To = '' }       # degree (drop)
        @{ From = [char]0x2713; To = 'OK' }     # check mark
        @{ From = [char]0x2717; To = 'X' }      # ballot x
        @{ From = [char]0x25B2; To = '^' }      # up triangle
        @{ From = [char]0x25BC; To = 'v' }      # down triangle
        @{ From = [char]0x2122; To = '(TM)' }   # trademark
        @{ From = [char]0x21B6; To = '<<' }     # undo arrow
        @{ From = [char]0x21B7; To = '>>' }     # redo arrow
        @{ From = [char]0x2195; To = '<>' }     # up-down arrow
    )

    foreach ($p in $pairs) {
        $text = $text.Replace([string]$p.From, [string]$p.To)
    }

    return $text
}

$changed = 0
foreach ($rel in $files) {
    $path = Join-Path $root.Path $rel
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Warning "Missing: $rel"
        continue
    }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $orig = [System.IO.File]::ReadAllText($path, $utf8)
    $new = Normalize-UiText $orig
    if ($new -ne $orig) {
        [System.IO.File]::WriteAllText($path, $new, $utf8)
        $changed++
        Write-Output "Updated: $rel"
    } else {
        Write-Output "Unchanged: $rel"
    }
}
Write-Output "Done. Files changed: $changed"
