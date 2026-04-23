# =============================================================================
# Bench Thread Scaling Script for FrankyCPP
# Runs --bench with 1-N threads and collects NPS results
# Usage: powershell -ExecutionPolicy Bypass -File ".\scripts\bench_thread_scaling.ps1" [-Depth 12] [-Hash 256] [-MaxThreads 16]
# =============================================================================

param(
    [int]$Depth = 12,
    [int]$Hash = 256,
    [int]$MaxThreads = 16
)

$EXECUTABLE = "D:\_DEV\FrankyCPP\cmake-build-win-release\src\FrankyCPP_v1.8.exe"

if (-not (Test-Path $EXECUTABLE)) {
    Write-Error "Executable not found at: $EXECUTABLE"
    exit 1
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "FrankyCPP Bench Thread Scaling" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Executable: $EXECUTABLE" -ForegroundColor Yellow
Write-Host "Depth:      $Depth" -ForegroundColor Yellow
Write-Host "Hash:       ${Hash} MB" -ForegroundColor Yellow
Write-Host "MaxThreads: $MaxThreads" -ForegroundColor Yellow
Write-Host ""

$results = @()

for ($t = 1; $t -le $MaxThreads; $t++) {
    Write-Host "Running bench with $t thread(s)..." -ForegroundColor Green -NoNewline

    $output = & $EXECUTABLE --bench --benchDepth $Depth --benchHash $Hash --threads $t -l warn -s warn 2>&1 | Out-String

    # Extract values from bench output (uses dots as thousands separator)
    # Format: "Nodes/second   :        2.548.591"
    $nps = ""
    $nodes = ""
    $time = ""
    if ($output -match "Nodes/second\s*:\s*([\d.]+)") {
        $nps = $Matches[1]
    }
    if ($output -match "Total nodes\s*:\s*([\d.]+)") {
        $nodes = $Matches[1]
    }
    if ($output -match "Total time \[s\]\s*:\s*([\d.]+)") {
        $time = $Matches[1]
    }

    # FrankyCPP outputs numbers with dots as thousands separators — use as-is for display
    $results += [PSCustomObject]@{
        Threads    = $t
        NPS        = $nps
        NPSNumeric = if ($nps) { [int64]($nps -replace '\.', '') } else { 0 }
        Nodes      = $nodes
        Time       = $time
    }

    Write-Host " NPS: $nps  Nodes: $nodes  Time: ${time}s" -ForegroundColor White
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
