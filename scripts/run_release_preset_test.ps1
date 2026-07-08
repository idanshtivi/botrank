$ErrorActionPreference = 'Stop'

# Preset-authoring workflow launcher: always builds and runs the Release
# Standalone with diagnostic tracing disabled. Preset edits only touch
# PresetManager.cpp/PresetBrowser.cpp (plugin-side, no DSP), so there is
# never a reason for preset testing to run a Debug binary or a trace-
# instrumented build — both add per-sample overhead that can miss the
# audio callback's real-time deadline and produce an audible crackle that
# has nothing to do with the preset content itself.

$repoRoot  = Join-Path $PSScriptRoot '..'
$buildDir  = Join-Path $repoRoot 'build'
$exePath   = Join-Path $repoRoot 'trusted_bin\LadderVoice.exe'
$cmake     = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

Write-Host "== run_release_preset_test =="

# 1. Kill any running instance so the linker can overwrite the exe and so
#    stale Debug/traced processes can't be mistaken for the fresh one.
$existing = Get-Process LadderVoice -ErrorAction SilentlyContinue
if ($existing) {
    Write-Host "Stopping running LadderVoice.exe (PID $($existing.Id))..."
    Stop-Process -Id $existing.Id -Force -Confirm:$false
    Start-Sleep -Milliseconds 500
}

# 2. Make sure tracing is off in the CMake cache before configuring/building —
#    this flag is sticky across configures unless explicitly overridden.
& $cmake -S $repoRoot -B $buildDir -DLADDERVOICE_ENABLE_POLY_TRACE=OFF | Out-Host

# 3. Diagnostic runtime env var must not leak into the launched process.
#    (Only matters if this script is dot-sourced into a session that
#    exported it; a plain child process wouldn't inherit it anyway.)
if ($env:LADDERVOICE_POLY_TRACE) {
    Write-Host "Clearing stray LADDERVOICE_POLY_TRACE env var from this session."
    Remove-Item Env:\LADDERVOICE_POLY_TRACE -ErrorAction SilentlyContinue
}

# 4. Build Release.
Write-Host "Building LadderVoice_Standalone (Release)..."
& $cmake --build $buildDir --config Release --target LadderVoice_Standalone | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Error "Release build failed — not launching a stale/wrong binary."
    exit 1
}

# 5. Confirm what's about to launch before launching it.
$cacheTrace = (Select-String -Path (Join-Path $buildDir 'CMakeCache.txt') `
    -Pattern 'LADDERVOICE_ENABLE_POLY_TRACE:BOOL=(.*)').Matches.Groups[1].Value
$fileInfo = Get-Item $exePath
Write-Host ""
Write-Host "== Launch check =="
Write-Host "  Binary:        $exePath"
Write-Host "  Last built:    $($fileInfo.LastWriteTime)"
Write-Host "  Config:        Release"
Write-Host "  Trace compiled ON in cache: $cacheTrace"
Write-Host "  LADDERVOICE_POLY_TRACE env (this launch): $(if ($env:LADDERVOICE_POLY_TRACE) { $env:LADDERVOICE_POLY_TRACE } else { '<unset>' })"

if ($cacheTrace -eq 'ON') {
    Write-Warning "LADDERVOICE_ENABLE_POLY_TRACE is ON in the CMake cache. Re-run this script — it just reconfigured to OFF, so this should not persist."
}

# 6. Launch.
Write-Host ""
Write-Host "Launching Release Standalone for preset testing..."
Start-Process -FilePath $exePath
