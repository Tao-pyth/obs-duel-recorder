param(
  [Parameter()]
  [string]$Root = "",

  [Parameter()]
  [switch]$SkipAnchors
)

$ErrorActionPreference = "Stop"

function Get-Utf8Text([string]$path) {
  $bytes = [System.IO.File]::ReadAllBytes($path)
  return [System.Text.Encoding]::UTF8.GetString($bytes)
}

function Remove-CodeFences([string]$text) {
  # Remove fenced code blocks to avoid parsing links inside examples.
  $text = [regex]::Replace($text, '(?s)```.*?```', "")
  $text = [regex]::Replace($text, '(?s)~~~.*?~~~', "")
  return $text
}

function Get-HeadingAnchors([string]$markdown) {
  $anchors = New-Object 'System.Collections.Generic.HashSet[string]'
  $counts = @{}

  $lines = $markdown -split "`r?`n"
  foreach ($line in $lines) {
    if ($line -match "^\s{0,3}(#{1,6})\s+(.+?)\s*$") {
      $heading = $Matches[2]
      # Strip trailing hashes: "## Title ##"
      $heading = [regex]::Replace($heading, "\s#+\s*$", "")
      # Remove Markdown inline code/backticks.
      $heading = [regex]::Replace($heading, "`{1,3}([^`]+)`{1,3}", '$1')
      # Remove emphasis markers.
      $heading = $heading -replace "\*", ""
      $heading = $heading -replace "_", ""
      $heading = $heading.Trim().ToLowerInvariant()

      # GitHub-style-ish slug: keep letters/numbers/spaces/hyphens, drop others.
      $slug = [regex]::Replace($heading, "[^\p{L}\p{Nd}\s\-]", "")
      $slug = [regex]::Replace($slug, "\s+", "-")
      $slug = [regex]::Replace($slug, "-{2,}", "-")
      $slug = $slug.Trim("-")

      if ($slug -eq "") { continue }

      if ($counts.ContainsKey($slug)) {
        $counts[$slug] += 1
        $final = "{0}-{1}" -f $slug, $counts[$slug]
      } else {
        $counts[$slug] = 0
        $final = $slug
      }

      [void]$anchors.Add($final)
    }
  }

  # Also allow explicit HTML ids: <a id="..."></a>
  foreach ($m in [regex]::Matches($markdown, "(?i)<a\s+[^>]*id\s*=\s*`\"([^`\"]+)`\"[^>]*>")) {
    $id = $m.Groups[1].Value.Trim()
    if ($id) { [void]$anchors.Add($id) }
  }

  return $anchors
}

if ([string]::IsNullOrWhiteSpace($Root)) {
  $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
  $Root = (Resolve-Path (Join-Path $scriptDir "..\\.."))..Path
}

$rootPath = (Resolve-Path $Root).Path
$docsPath = Join-Path $rootPath "docs"

$files = @()
$files += Get-ChildItem -File -Filter "README*.md" -Path $rootPath
if (Test-Path $docsPath) {
  $files += Get-ChildItem -File -Recurse -Filter "*.md" -Path $docsPath
}
$files = $files | Sort-Object FullName -Unique

$linkRe = [regex]'(?<!\!)\[[^\]]+\]\(([^)]+)\)'
$fail = @()
$anchorCache = @{}

foreach ($f in $files) {
  $raw = Get-Utf8Text $f.FullName
  $scrub = Remove-CodeFences $raw

  foreach ($m in $linkRe.Matches($scrub)) {
    $target = $m.Groups[1].Value.Trim()
    if ($target -eq "") { continue }

    # Strip optional surrounding angle brackets: (<path>)
    if ($target.StartsWith("<") -and $target.EndsWith(">")) {
      $target = $target.Substring(1, $target.Length - 2).Trim()
    }

    if ($target -match "^(https?|mailto|data):") { continue }

    $pathPart = $target
    $anchorPart = $null
    if ($target -match "^(.*?)(#.+)$") {
      $pathPart = $Matches[1]
      $anchorPart = $Matches[2].Substring(1)
    }

    $targetFile = $f.FullName
    if ($pathPart -ne "") {
      $resolved = [System.IO.Path]::GetFullPath((Join-Path $f.Directory.FullName $pathPart))
      if (-not ($resolved.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase))) {
        $fail += [pscustomobject]@{ file=$f.FullName; link=$target; type="path_outside_root"; target=$resolved }
        continue
      }
      if (-not (Test-Path $resolved)) {
        $fail += [pscustomobject]@{ file=$f.FullName; link=$target; type="missing_file"; target=$resolved }
        continue
      }
      $targetFile = $resolved
    }

    if (-not $SkipAnchors -and $anchorPart) {
      if (-not $anchorCache.ContainsKey($targetFile)) {
        $md = Get-Utf8Text $targetFile
        $anchorCache[$targetFile] = Get-HeadingAnchors $md
      }
      $anchors = $anchorCache[$targetFile]
      if (-not $anchors.Contains($anchorPart)) {
        $fail += [pscustomobject]@{ file=$f.FullName; link=$target; type="missing_anchor"; target=$targetFile; anchor=$anchorPart }
      }
    }
  }
}

if ($fail.Count -eq 0) {
  Write-Host "OK: Markdown link validation passed ($($files.Count) files)"
  exit 0
}

$fail | Sort-Object file, type, link | Format-Table -AutoSize
Write-Error ("Markdown link validation failed: {0} problem(s)" -f $fail.Count)
exit 1
