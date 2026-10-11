# Keeps Prism Launcher (fork with the Servers panel) up to date from
# https://github.com/Shalev-idk/PrismLauncher/releases  -- runs as a scheduled task, per user.
$ErrorActionPreference = 'Stop'
$repo   = 'Shalev-idk/PrismLauncher'
$state  = Join-Path $env:LOCALAPPDATA 'PrismForkUpdater'
$marker = Join-Path $state 'installed-tag.txt'
$log    = Join-Path $state 'update.log'
New-Item -ItemType Directory -Force -Path $state | Out-Null
function Log($m) { Add-Content $log ("{0}  {1}" -f (Get-Date -Format s), $m) }

try {
  $rel = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest" -Headers @{ 'User-Agent' = 'prism-fork-updater' }
  $tag = $rel.tag_name
  $have = if (Test-Path $marker) { (Get-Content $marker -Raw).Trim() } else { '' }
  if ($tag -eq $have) { exit 0 }

  # Never replace Prism while it is open; try again at the next run.
  if (Get-Process prismlauncher -ErrorAction SilentlyContinue) { Log "Prism is open, will retry ($tag waiting)"; exit 0 }

  $asset = $rel.assets | Where-Object { $_.name -match 'Setup.*\.zip$' -and $_.name -match 'MSVC' -and $_.name -notmatch 'arm64' } | Select-Object -First 1
  if (-not $asset) { Log "No Windows setup asset in $tag"; exit 1 }

  $tmp = Join-Path $env:TEMP "prism-fork-$tag"
  Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $tmp | Out-Null
  Invoke-WebRequest $asset.browser_download_url -OutFile "$tmp\setup.zip"
  Expand-Archive "$tmp\setup.zip" -DestinationPath $tmp -Force
  $exe = Get-ChildItem $tmp -Filter *Setup*.exe -Recurse | Select-Object -First 1
  # Inno Setup silent install over the existing one; instances and accounts live in AppData and are kept.
  Start-Process $exe.FullName -ArgumentList '/VERYSILENT','/NORESTART','/SUPPRESSMSGBOXES' -Wait
  Set-Content $marker $tag
  Log "Updated to $tag"
  Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
} catch { Log "ERROR: $_"; exit 1 }
