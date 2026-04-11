<#
.SYNOPSIS
    Runs FrankyCPP benchmark multiple times and reports individual + average KPIs.

.PARAMETER Runs
    Number of benchmark runs (default: 5).

.PARAMETER BenchHash
    Hash size in MB for the benchmark (default: 256).

.PARAMETER Threads
    Number of threads (default: 1).

.EXAMPLE
    .\run_bench.ps1
    .\run_bench.ps1 -Runs 10
    .\run_bench.ps1 -Runs 3 -BenchHash 512 -Threads 2
#>
param(
    [int]$Runs = 5,
    [int]$BenchHash = 256,
    [int]$Threads = 1
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$exe = ".\cmake-build-win-release\src\FrankyCPP_v1.8.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Executable not found: $exe"
    exit 1
}

# Storage for parsed KPIs per run (always kept as array)
[System.Collections.ArrayList]$results = @()

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " FrankyCPP Benchmark Runner" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Runs       : $Runs"
Write-Host " BenchHash  : $BenchHash MB"
Write-Host " Threads    : $Threads"
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

for ($i = 1; $i -le $Runs; $i++) {
    Write-Host "--- Run $i / $Runs ---" -ForegroundColor Yellow

    $output = & $exe --bench --benchHash $BenchHash --threads $Threads 2>&1 | ForEach-Object { $_.ToString() }

    # Parse KPIs from output (numbers use dots as thousand separators, e.g. 38.314.796)
    $totalNodes  = $null
    $totalTime   = $null
    $nodesPerSec = $null
    $benchSig    = $null

    foreach ($line in $output) {
        if ($line -match 'Total nodes\s*:\s*([\d.]+)') {
            $totalNodes = [long]($Matches[1] -replace '\.', '')
        }
        elseif ($line -match 'Total time \[s\]\s*:\s*([\d.,]+)') {
            $totalTime = [double]($Matches[1] -replace ',', '.')
        }
        elseif ($line -match 'Nodes/second\s*:\s*([\d.]+)') {
            $nodesPerSec = [long]($Matches[1] -replace '\.', '')
        }
        elseif ($line -match '^Bench:\s*(\d+)') {
            $benchSig = [long]$Matches[1]
        }
    }

    if ($null -eq $totalNodes -or $null -eq $totalTime -or $null -eq $nodesPerSec) {
        Write-Warning "Failed to parse KPIs from run $i. Raw output:"
        $output | ForEach-Object { Write-Host $_ }
        continue
    }

    [void]$results.Add([PSCustomObject]@{
        Run         = $i
        TotalNodes  = $totalNodes
        TimeSec     = $totalTime
        NodesPerSec = $nodesPerSec
        BenchSig    = $benchSig
    })

    # Format numbers with dot thousand separators
    $fmtNodes = $totalNodes.ToString("N0")
    $fmtNps   = $nodesPerSec.ToString("N0")

    Write-Host ("  Total nodes    : {0,20}" -f $fmtNodes)
    Write-Host ("  Total time [s] : {0,20:F2}" -f $totalTime)
    Write-Host ("  Nodes/second   : {0,20}" -f $fmtNps)
    Write-Host ("  Bench signature: {0}" -f $benchSig)
    Write-Host ""
}

# --- Summary ---
if ($results.Count -eq 0) {
    Write-Error "No successful runs to summarize."
    exit 1
}

$avgNodes  = ($results | Measure-Object -Property TotalNodes  -Average).Average
$avgTime   = ($results | Measure-Object -Property TimeSec     -Average).Average
$avgNps    = ($results | Measure-Object -Property NodesPerSec -Average).Average
$minNps    = ($results | Measure-Object -Property NodesPerSec -Minimum).Minimum
$maxNps    = ($results | Measure-Object -Property NodesPerSec -Maximum).Maximum
$minTime   = ($results | Measure-Object -Property TimeSec     -Minimum).Minimum
$maxTime   = ($results | Measure-Object -Property TimeSec     -Maximum).Maximum

# Check if all bench signatures match
$signatures = @($results | Select-Object -ExpandProperty BenchSig -Unique)
$sigMatch = ($signatures.Count -eq 1)

$fmtAvgNodes = ([long]$avgNodes).ToString("N0")
$fmtAvgNps   = ([long]$avgNps).ToString("N0")
$fmtMinNps   = ([long]$minNps).ToString("N0")
$fmtMaxNps   = ([long]$maxNps).ToString("N0")

Write-Host "=============================================" -ForegroundColor Green
Write-Host " Summary ($($results.Count) successful runs)" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
Write-Host ("  Avg total nodes  : {0,20}" -f $fmtAvgNodes)
Write-Host ("  Avg time [s]     : {0,20:F2}" -f $avgTime)
Write-Host ("  Min time [s]     : {0,20:F2}" -f $minTime)
Write-Host ("  Max time [s]     : {0,20:F2}" -f $maxTime)
Write-Host ""
Write-Host ("  Avg nodes/sec    : {0,20}" -f $fmtAvgNps)
Write-Host ("  Min nodes/sec    : {0,20}" -f $fmtMinNps)
Write-Host ("  Max nodes/sec    : {0,20}" -f $fmtMaxNps)
Write-Host ""

if ($sigMatch) {
    Write-Host "  Bench signature  : $($signatures[0]) (all runs match)" -ForegroundColor Green
} else {
    Write-Host "  Bench signatures : MISMATCH across runs!" -ForegroundColor Red
    foreach ($r in $results) {
        Write-Host ("    Run {0}: {1}" -f $r.Run, $r.BenchSig)
    }
}

Write-Host "=============================================" -ForegroundColor Green
Write-Host ""
