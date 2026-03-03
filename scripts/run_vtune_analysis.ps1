# =============================================================================
# VTune Performance Analysis Script for FrankyCPP
# Runs 5 VTune analyses: hotspots, microarchitecture, memory-access, threading, hpc
#
# IMPORTANT: Run this script in an Administrator PowerShell terminal!
# Hardware event-based analyses (microarchitecture, memory-access, hpc) require
# admin privileges for access to CPU performance counters.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File ".\scripts\run_vtune_analysis.ps1"
# =============================================================================

# -----------------------------------------------------------------------------
# Configuration - Modify these paths as needed
# -----------------------------------------------------------------------------
$THREADS = 8
$PARAMS = "--bench --threads $THREADS -l warn -s warn"
$VTUNE_PATH = "C:\Program Files (x86)\Intel\oneAPI\vtune\2025.9\bin64"
$EXECUTABLE = "D:\_DEV\FrankyCPP\cmake-build-win-relwithdebinfo\src\FrankyCPP_v1.4.exe"
$RESULTS_BASE = "D:\_DEV\FrankyCPP\results\vtune"

# -----------------------------------------------------------------------------
# Setup
# -----------------------------------------------------------------------------
$ErrorActionPreference = "Continue"

# Check for admin privileges
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Host ""
if (-not $isAdmin) {
    Write-Host "WARNING: Not running as Administrator!" -ForegroundColor Red
    Write-Host "Hardware event analyses (microarchitecture, memory-access, hpc-performance)" -ForegroundColor Red
    Write-Host "require admin privileges and will likely fail." -ForegroundColor Red
    Write-Host ""
    Write-Host "Please restart PowerShell as Administrator and run again." -ForegroundColor Yellow
    Write-Host ""
    $response = Read-Host "Continue anyway? (y/N)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        exit 0
    }
}

# Verify VTune exists
$vtuneCli = Join-Path $VTUNE_PATH "vtune.exe"
if (-not (Test-Path $vtuneCli)) {
    Write-Error "VTune not found at: $vtuneCli"
    exit 1
}

# Verify executable exists
if (-not (Test-Path $EXECUTABLE)) {
    Write-Error "Executable not found at: $EXECUTABLE"
    exit 1
}

# Create timestamped results directory
$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$resultsDir = Join-Path $RESULTS_BASE $timestamp
New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "VTune Performance Analysis for FrankyCPP" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Executable: $EXECUTABLE" -ForegroundColor Yellow
Write-Host "Threads:    $THREADS" -ForegroundColor Yellow
Write-Host "Parameters: $PARAMS" -ForegroundColor Yellow
Write-Host "Results:    $resultsDir" -ForegroundColor Yellow
Write-Host "Admin:      $isAdmin" -ForegroundColor $(if ($isAdmin) { "Green" } else { "Red" })
Write-Host ""

# -----------------------------------------------------------------------------
# Analysis Configurations
#
# Note: hotspots and threading use hardware sampling mode (-knob sampling-mode=hw)
# instead of stack collection which can cause data corruption on some systems.
# -----------------------------------------------------------------------------
$analyses = @(
    @{
        Name = "hotspots"
        Collection = "hotspots"
        CollectKnobs = @("-knob", "sampling-mode=hw")
        ReportType = "hotspots"
        Description = "CPU hotspots with hardware sampling"
        RequiresAdmin = $true  # hw sampling needs admin
    },
    @{
        Name = "microarchitecture"
        Collection = "uarch-exploration"
        CollectKnobs = @()
        ReportType = "hotspots"
        Description = "CPU microarchitecture bottleneck analysis"
        RequiresAdmin = $true
    },
    @{
        Name = "memory-access"
        Collection = "memory-access"
        CollectKnobs = @()
        ReportType = "hotspots"
        Description = "Memory bound and cache miss analysis"
        RequiresAdmin = $true
    },
    @{
        Name = "threading"
        Collection = "hotspots"
        CollectKnobs = @("-knob", "sampling-mode=hw", "-knob", "sampling-interval=1")
        ReportType = "hotspots"
        Description = "Thread activity via hardware sampling"
        RequiresAdmin = $true
    },
    @{
        Name = "hpc-performance"
        Collection = "hpc-performance"
        CollectKnobs = @()
        ReportType = "hotspots"
        Description = "HPC characterization with CPI and vectorization"
        RequiresAdmin = $true
    }
)

# -----------------------------------------------------------------------------
# Run Each Analysis
# -----------------------------------------------------------------------------
$totalAnalyses = $analyses.Count
$currentAnalysis = 0
$startTime = Get-Date
$results = @()

foreach ($analysis in $analyses) {
    $currentAnalysis++
    $analysisName = $analysis.Name
    $collection = $analysis.Collection
    $collectKnobs = $analysis.CollectKnobs
    $reportType = $analysis.ReportType
    $description = $analysis.Description
    $requiresAdmin = $analysis.RequiresAdmin

    $resultPath = Join-Path $resultsDir $analysisName
    $csvPath = Join-Path $resultsDir "$analysisName.csv"

    Write-Host ""
    Write-Host "[$currentAnalysis/$totalAnalyses] Running: $analysisName" -ForegroundColor Green
    Write-Host "  Description: $description" -ForegroundColor Gray
    if ($requiresAdmin -and -not $isAdmin) {
        Write-Host "  WARNING: Requires admin - may fail!" -ForegroundColor Yellow
    }
    Write-Host "  Output: $resultPath" -ForegroundColor Gray

    $analysisStart = Get-Date
    $collectSuccess = $false
    $csvSuccess = $false

    # Build collection arguments
    $collectArgs = @("-collect", $collection)
    if ($collectKnobs.Count -gt 0) {
        $collectArgs += $collectKnobs
    }
    $collectArgs += @("-result-dir", $resultPath, "--", $EXECUTABLE)
    $collectArgs += ($PARAMS -split " ")

    # Run VTune collection
    Write-Host "  Collecting data..." -ForegroundColor Gray

    try {
        $output = & $vtuneCli @collectArgs 2>&1
        $outputStr = $output | Out-String

        # Check for actual errors in output
        if ($outputStr -match "Error:" -or $outputStr -match "No data") {
            Write-Host "  Collection error detected:" -ForegroundColor Red
            $output | Where-Object { $_ -match "Error|No data|Warning|Assertion" } | ForEach-Object {
                Write-Host "    $_" -ForegroundColor DarkRed
            }
        } elseif (Test-Path $resultPath) {
            $collectSuccess = $true
        } else {
            Write-Host "  Collection failed - no result directory created" -ForegroundColor Red
        }
    }
    catch {
        Write-Host "  Collection exception: $_" -ForegroundColor Red
    }

    # Export to CSV if collection succeeded
    if ($collectSuccess) {
        Write-Host "  Exporting to CSV (report type: $reportType)..." -ForegroundColor Gray
        $reportArgs = @(
            "-report", $reportType,
            "-result-dir", $resultPath,
            "-format", "csv",
            "-csv-delimiter", "comma",
            "-report-output", $csvPath
        )

        try {
            $output = & $vtuneCli @reportArgs 2>&1
            $outputStr = $output | Out-String

            if ($outputStr -match "Error:" -or $outputStr -match "No data") {
                Write-Host "  CSV export error:" -ForegroundColor Yellow
                $output | Where-Object { $_ -match "Error|No data" } | ForEach-Object {
                    Write-Host "    $_" -ForegroundColor DarkYellow
                }
            } elseif (Test-Path $csvPath) {
                $fileSize = (Get-Item $csvPath).Length
                if ($fileSize -gt 200) {  # More than just headers
                    $csvSuccess = $true
                } else {
                    Write-Host "  CSV file is nearly empty ($fileSize bytes)" -ForegroundColor Yellow
                }
            }
        }
        catch {
            Write-Host "  CSV export exception: $_" -ForegroundColor Yellow
        }
    }

    $analysisEnd = Get-Date
    $analysisDuration = $analysisEnd - $analysisStart

    # Report status
    $status = @{
        Name = $analysisName
        CollectSuccess = $collectSuccess
        CsvSuccess = $csvSuccess
        Duration = $analysisDuration.TotalSeconds
    }
    $results += $status

    if ($collectSuccess -and $csvSuccess) {
        $csvSize = (Get-Item $csvPath).Length / 1KB
        Write-Host "  SUCCESS - $($analysisDuration.TotalSeconds.ToString('F1'))s - CSV: $($csvSize.ToString('F0')) KB" -ForegroundColor Green
    } elseif ($collectSuccess) {
        Write-Host "  PARTIAL - Collection OK, CSV issue - $($analysisDuration.TotalSeconds.ToString('F1'))s" -ForegroundColor Yellow
    } else {
        Write-Host "  FAILED - $($analysisDuration.TotalSeconds.ToString('F1'))s" -ForegroundColor Red
    }
}

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
$endTime = Get-Date
$totalDuration = $endTime - $startTime

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Analysis Complete!" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Total time: $($totalDuration.TotalMinutes.ToString('F1')) minutes" -ForegroundColor Yellow
Write-Host "Results directory: $resultsDir" -ForegroundColor Yellow
Write-Host ""

# Summary table
Write-Host "Results Summary:" -ForegroundColor Gray
Write-Host "  Analysis              Collection  CSV Export" -ForegroundColor Gray
Write-Host "  --------------------  ----------  ----------" -ForegroundColor Gray
foreach ($r in $results) {
    $collectStatus = if ($r.CollectSuccess) { "OK" } else { "FAILED" }
    $csvStatus = if ($r.CsvSuccess) { "OK" } else { "FAILED" }
    $collectColor = if ($r.CollectSuccess) { "Green" } else { "Red" }
    $csvColor = if ($r.CsvSuccess) { "Green" } else { "Red" }
    Write-Host ("  {0,-20}  " -f $r.Name) -NoNewline
    Write-Host ("{0,-10}  " -f $collectStatus) -ForegroundColor $collectColor -NoNewline
    Write-Host $csvStatus -ForegroundColor $csvColor
}

Write-Host ""
Write-Host "Generated CSV files:" -ForegroundColor Gray
$csvFiles = Get-ChildItem -Path $resultsDir -Filter "*.csv" -ErrorAction SilentlyContinue
if ($csvFiles) {
    $csvFiles | ForEach-Object {
        $sizeKB = $_.Length / 1KB
        Write-Host "  - $($_.Name) ($($sizeKB.ToString('F0')) KB)" -ForegroundColor Gray
    }
} else {
    Write-Host "  (no CSV files generated)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "To open results in VTune GUI:" -ForegroundColor Gray
foreach ($analysis in $analyses) {
    $resultPath = Join-Path $resultsDir $analysis.Name
    if (Test-Path $resultPath) {
        Write-Host "  vtune-gui `"$resultPath`"" -ForegroundColor DarkGray
    }
}

Write-Host ""
if (-not $isAdmin) {
    Write-Host "NOTE: Some analyses may have failed due to missing admin privileges." -ForegroundColor Yellow
    Write-Host "Re-run in an Administrator terminal for complete results." -ForegroundColor Yellow
    Write-Host ""
}

# Clean up temporary log config file created by VTune
$logCfgPath = "D:\_DEV\FrankyCPP\config\log.cfg"
if (Test-Path $logCfgPath) {
    Remove-Item $logCfgPath -Force
    Write-Host "Cleaned up temporary file: $logCfgPath" -ForegroundColor Gray
}

Write-Host "Done!" -ForegroundColor Green
