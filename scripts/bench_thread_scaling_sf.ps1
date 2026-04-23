# =============================================================================
# Bench Thread Scaling Script for Stockfish
# Runs bench with 1-N threads and collects NPS results
# Usage: powershell -ExecutionPolicy Bypass -File ".\scripts\bench_thread_scaling_sf.ps1" [-Hash 256] [-MaxThreads 16]
# =============================================================================

param(
    [int]$Hash = 256,
    [int]$MaxThreads = 16
)

$EXECUTABLE = "D:\_DEV\Stockfish\src\stockfish.exe"

if (-not (Test-Path $EXECUTABLE)) {
    Write-Error "Executable not found at: $EXECUTABLE"
    exit 1
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Stockfish Bench Thread Scaling" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Executable: $EXECUTABLE" -ForegroundColor Yellow
Write-Host "Depth:      default" -ForegroundColor Yellow
Write-Host "Hash:       ${Hash} MB" -ForegroundColor Yellow
Write-Host "MaxThreads: $MaxThreads" -ForegroundColor Yellow
Write-Host ""

$results = @()

for ($t = 1; $t -le $MaxThreads; $t++) {
    Write-Host "Running bench with $t thread(s)..." -ForegroundColor Green -NoNewline

    # Stockfish bench syntax: bench [hash] [threads]
    # Uses Stockfish's default depth
    $output = & $EXECUTABLE bench $Hash $t 2>&1 | Out-String

    # Stockfish output format:
    # Total time (ms) : 12345
    # Nodes searched  : 12345678
    # Nodes/second    : 1234567
    $nps = ""
    $nodes = ""
    $time = ""
    if ($output -match "Nodes/second\s*:\s*([\d]+)") {
        $nps = $Matches[1]
    }
    if ($output -match "Nodes searched\s*:\s*([\d]+)") {
        $nodes = $Matches[1]
    }
    if ($output -match "Total time \(ms\)\s*:\s*([\d]+)") {
        $time = $Matches[1]
    }

    # Stockfish outputs raw numbers — format with locale separators for display
    $npsDisplay = if ($nps) { [int64]$nps | ForEach-Object { $_.ToString("N0") } } else { "n/a" }
    $nodesDisplay = if ($nodes) { [int64]$nodes | ForEach-Object { $_.ToString("N0") } } else { "n/a" }
    $timeDisplay = if ($time) { "{0:F2}" -f ([int64]$time / 1000.0) } else { "n/a" }

    $results += [PSCustomObject]@{
        Threads    = $t
        NPS        = $npsDisplay
        NPSNumeric = if ($nps) { [int64]$nps } else { 0 }
        Nodes      = $nodesDisplay
        Time       = $timeDisplay
    }

    Write-Host " NPS: $npsDisplay  Nodes: $nodesDisplay  Time: ${timeDisplay}s" -ForegroundColor White
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Summary" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host ("{0,-10} {1,15} {2,18} {3,10} {4,10} {5,12}" -f "Threads", "NPS", "Nodes", "Time(s)", "Scaling", "Efficiency")
Write-Host ("{0,-10} {1,15} {2,18} {3,10} {4,10} {5,12}" -f "-------", "---", "-----", "-------", "-------", "----------")

$baseNps = 0
foreach ($r in $results) {
    if ($r.Threads -eq 1) { $baseNps = $r.NPSNumeric }
    $scaling = if ($baseNps -gt 0 -and $r.NPSNumeric -gt 0) { "{0:F2}x" -f ($r.NPSNumeric / $baseNps) } else { "n/a" }
    $efficiency = if ($baseNps -gt 0 -and $r.NPSNumeric -gt 0) { "{0:F1}%" -f (($r.NPSNumeric / $baseNps) / $r.Threads * 100) } else { "n/a" }
    Write-Host ("{0,-10} {1,15} {2,18} {3,10} {4,10} {5,12}" -f $r.Threads, $r.NPS, $r.Nodes, $r.Time, $scaling, $efficiency)
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
