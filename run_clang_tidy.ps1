#==============================================================================
# FrankyCPP Clang-Tidy Analysis Script
#==============================================================================
# Run clang-tidy on the codebase and generate a report for analysis
#
# Usage:
#   .\run_clang_tidy.ps1                     # Analyze all files
#   .\run_clang_tidy.ps1 -SourceOnly         # Only src/ files
#   .\run_clang_tidy.ps1 -TestsOnly          # Only test/ files
#   .\run_clang_tidy.ps1 -File "path/to.cpp" # Single file
#   .\run_clang_tidy.ps1 -MaxFiles 5         # Limit files (for testing)
#   .\run_clang_tidy.ps1 -Jobs 4             # Run 4 parallel jobs
#
# Output:
#   - Text report with prioritized warnings
#   - JSON file for programmatic analysis
#==============================================================================

param(
    [switch]$SourceOnly,
    [switch]$TestsOnly,
    [string]$File = "",
    [int]$MaxFiles = 0,
    [int]$Jobs = 1,
    [string]$OutputDir = "clang-tidy-reports",
    [switch]$Help
)

$ErrorActionPreference = "Continue"

if ($Help) {
    @"
FrankyCPP Clang-Tidy Analysis Script

USAGE: .\run_clang_tidy.ps1 [options]

OPTIONS:
  -SourceOnly   Only analyze src/ files (skip tests)
  -TestsOnly    Only analyze test/ files
  -File <path>  Analyze single file
  -MaxFiles <n> Limit files to analyze (useful for testing)
  -Jobs <n>     Number of parallel jobs (default: 1, max: 8)
  -OutputDir    Output directory (default: clang-tidy-reports)
  -Help         Show this help

EXAMPLES:
  .\run_clang_tidy.ps1 -SourceOnly              # Analyze all src files
  .\run_clang_tidy.ps1 -SourceOnly -Jobs 4      # Use 4 parallel jobs
  .\run_clang_tidy.ps1 -MaxFiles 5              # Quick test on 5 files
  .\run_clang_tidy.ps1 -File src/engine/Search.cpp

OUTPUT:
  Reports are saved to clang-tidy-reports/ directory:
  - clang-tidy-report_<timestamp>.txt  (human readable)
  - clang-tidy-report_<timestamp>.json (for tools/scripts)
"@
    return
}

# Configuration
$ClangTidy = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"

# Compiler flags - note: clang-tidy will auto-discover .clang-tidy config file
# from the current directory, but we run from project root to ensure this works
$Flags = "-std=c++20 -Isrc -Icmake-build-win-release -Icmake-build-win-release/vcpkg_installed/x64-windows-static-md/include -DWIN32 -D_WINDOWS -DNDEBUG -DHAS_EXECUTION_LIB -DHAS_PEXT -DSPDLOG_HEADER_ONLY=1 -DSPDLOG_USE_STD_FORMAT=1 -DYAML_CPP_STATIC_DEFINE -fms-compatibility -fms-compatibility-version=19.44"

# Clang-tidy options (before the --)
# Use .clang-tidy config file (which matches CLion's configuration)
# This ensures the script finds the same issues as CLion's editor shows
$ClangTidyOptions = "--config-file=.clang-tidy"

# Verify clang-tidy
if (-not (Test-Path $ClangTidy)) {
    Write-Host "ERROR: clang-tidy not found at: $ClangTidy" -ForegroundColor Red
    return
}

Write-Host "=========================================="
Write-Host "FrankyCPP Clang-Tidy Analysis"
Write-Host "=========================================="

# Collect files
$fileList = @()

if ($File -ne "") {
    if (Test-Path $File) {
        $fileList = @($File)
    } else {
        Write-Host "ERROR: File not found: $File" -ForegroundColor Red
        return
    }
} else {
    # Use cmd to collect files (more reliable)
    if ($TestsOnly) {
        $fileList = @(cmd /c "dir test\*.cpp /s /b 2>nul")
    } elseif ($SourceOnly) {
        $fileList = @(cmd /c "dir src\*.cpp /s /b 2>nul")
    } else {
        $srcFiles = @(cmd /c "dir src\*.cpp /s /b 2>nul")
        $testFiles = @(cmd /c "dir test\*.cpp /s /b 2>nul")
        $fileList = $srcFiles + $testFiles
    }
}

# Filter empty entries
$fileList = @($fileList | Where-Object { $_ -and $_.Trim() -ne "" })

if ($MaxFiles -gt 0) {
    $fileList = @($fileList | Select-Object -First $MaxFiles)
}

Write-Host "Files to analyze: $($fileList.Count)"
Write-Host "Output directory: $OutputDir"
Write-Host "Timestamp: $Timestamp"
if ($Jobs -gt 1) {
    Write-Host "Parallel jobs: $Jobs"
}
Write-Host "=========================================="

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Limit parallel jobs
if ($Jobs -lt 1) { $Jobs = 1 }
if ($Jobs -gt 12) { $Jobs = 12 }

# Results storage (thread-safe for parallel)
$allWarnings = [System.Collections.Concurrent.ConcurrentBag[hashtable]]::new()
$fileSummaries = [System.Collections.Concurrent.ConcurrentBag[hashtable]]::new()
$warningCounts = [System.Collections.Concurrent.ConcurrentDictionary[string,int]]::new()
$processedCount = [ref]0
$totalFiles = $fileList.Count
$startTime = Get-Date

# Function to analyze a single file
$analyzeFile = {
    param($filepath, $ClangTidy, $Flags, $pwd)

    $relPath = $filepath
    if ($filepath.StartsWith($pwd)) {
        $relPath = $filepath.Substring($pwd.Length + 1)
    }
    $relPath = $relPath.Replace("\", "/")

    # Run clang-tidy
    $output = & $ClangTidy $relPath "--" $Flags.Split(" ") 2>&1 | Out-String

    # Parse output
    $warnings = @()
    $lines = $output -split "`r?`n"

    foreach ($line in $lines) {
        if ($line -match "^(.+?):(\d+):(\d+):\s*(warning|error):\s*(.+?)\s*\[([^\]]+)\]$") {
            $warnFile = $Matches[1]
            if ($warnFile -match "vcpkg_installed|\\include\\") { continue }

            $warnings += @{
                File = $warnFile.Replace("\", "/")
                Line = [int]$Matches[2]
                Col = [int]$Matches[3]
                Severity = $Matches[4]
                Message = $Matches[5].Trim()
                Check = $Matches[6]
                Category = ($Matches[6] -split "-")[0]
            }
        }
    }

    return @{
        RelPath = $relPath
        Warnings = $warnings
        Count = $warnings.Count
    }
}

# Process files
if ($Jobs -eq 1) {
    # Sequential processing with progress
    $i = 0
    foreach ($filepath in $fileList) {
        $i++
        $pct = [math]::Round(($i / $totalFiles) * 100)

        # Get relative path for display
        $pwd = (Get-Location).Path
        $relPath = $filepath
        if ($filepath.StartsWith($pwd)) {
            $relPath = $filepath.Substring($pwd.Length + 1)
        }
        $relPath = $relPath.Replace("\", "/")

        Write-Host "[$i/$totalFiles] ($pct%) $relPath" -NoNewline

        # Run clang-tidy with project config
        $output = & $ClangTidy $ClangTidyOptions $relPath "--" $Flags.Split(" ") 2>&1 | Out-String

        # Parse output
        $warnings = @()
        $lines = $output -split "`r?`n"

        foreach ($line in $lines) {
            if ($line -match "^(.+?):(\d+):(\d+):\s*(warning|error):\s*(.+?)\s*\[([^\]]+)\]$") {
                $warnFile = $Matches[1]
                if ($warnFile -match "vcpkg_installed|\\include\\") { continue }

                $warn = @{
                    File = $warnFile.Replace("\", "/")
                    Line = [int]$Matches[2]
                    Col = [int]$Matches[3]
                    Severity = $Matches[4]
                    Message = $Matches[5].Trim()
                    Check = $Matches[6]
                    Category = ($Matches[6] -split "-")[0]
                }
                $warnings += $warn
                [void]$allWarnings.Add($warn)

                [void]$warningCounts.AddOrUpdate($warn.Check, 1, { param($k, $v) $v + 1 })
            }
        }

        [void]$fileSummaries.Add(@{ File = $relPath; Count = $warnings.Count })
        Write-Host " - $($warnings.Count) warnings"
    }
} else {
    # Parallel processing
    Write-Host "Running with $Jobs parallel jobs..."
    Write-Host ""

    $pwd = (Get-Location).Path

    $fileList | ForEach-Object -ThrottleLimit $Jobs -Parallel {
        $filepath = $_
        $ClangTidy = $using:ClangTidy
        $ClangTidyOptions = $using:ClangTidyOptions
        $Flags = $using:Flags
        $pwd = $using:pwd
        $allWarnings = $using:allWarnings
        $fileSummaries = $using:fileSummaries
        $warningCounts = $using:warningCounts
        $processedCount = $using:processedCount
        $totalFiles = $using:totalFiles

        $relPath = $filepath
        if ($filepath.StartsWith($pwd)) {
            $relPath = $filepath.Substring($pwd.Length + 1)
        }
        $relPath = $relPath.Replace("\", "/")

        # Run clang-tidy with project config
        $output = & $ClangTidy $ClangTidyOptions $relPath "--" $Flags.Split(" ") 2>&1 | Out-String

        # Parse output
        $warnings = @()
        $lines = $output -split "`r?`n"

        foreach ($line in $lines) {
            if ($line -match "^(.+?):(\d+):(\d+):\s*(warning|error):\s*(.+?)\s*\[([^\]]+)\]$") {
                $warnFile = $Matches[1]
                if ($warnFile -match "vcpkg_installed|\\include\\") { continue }

                $warn = @{
                    File = $warnFile.Replace("\", "/")
                    Line = [int]$Matches[2]
                    Col = [int]$Matches[3]
                    Severity = $Matches[4]
                    Message = $Matches[5].Trim()
                    Check = $Matches[6]
                    Category = ($Matches[6] -split "-")[0]
                }
                $warnings += $warn
                [void]$allWarnings.Add($warn)

                [void]$warningCounts.AddOrUpdate($warn.Check, 1, { param($k, $v) $v + 1 })
            }
        }

        [void]$fileSummaries.Add(@{ File = $relPath; Count = $warnings.Count })

        # Update progress
        $current = [System.Threading.Interlocked]::Increment($processedCount)
        $pct = [math]::Round(($current / $totalFiles) * 100)
        Write-Host "[$current/$totalFiles] ($pct%) $relPath - $($warnings.Count) warnings"
    }
}

$endTime = Get-Date
$elapsed = $endTime - $startTime

Write-Host ""
Write-Host "=========================================="
Write-Host "ANALYSIS COMPLETE"
Write-Host "=========================================="
Write-Host "Total warnings: $($allWarnings.Count)"
Write-Host "Unique checks:  $($warningCounts.Count)"
Write-Host "Elapsed time:   $($elapsed.ToString('mm\:ss'))"

# Convert concurrent collections to regular collections for report generation
$allWarningsList = @($allWarnings.ToArray())
$fileSummariesList = @($fileSummaries.ToArray())
$warningCountsDict = @{}
foreach ($kvp in $warningCounts.GetEnumerator()) {
    $warningCountsDict[$kvp.Key] = $kvp.Value
}

# Generate report
$reportPath = Join-Path $OutputDir "clang-tidy-report_$Timestamp.txt"

$report = @"
================================================================================
FRANKYCPP CLANG-TIDY ANALYSIS REPORT
================================================================================
Generated: $Timestamp
Files Analyzed: $($fileList.Count)
Total Warnings: $($allWarningsList.Count)
Unique Check Types: $($warningCountsDict.Count)
Elapsed Time: $($elapsed.ToString('mm\:ss'))

================================================================================
TOP WARNINGS BY FREQUENCY (Prioritized for Fixing)
================================================================================
Priority | Count | Check Name
---------|-------|------------------------------------------------------------
"@

$sorted = $warningCountsDict.GetEnumerator() | Sort-Object -Property Value -Descending
foreach ($w in $sorted) {
    $priority = if ($w.Value -ge 50) { "HIGH  " } elseif ($w.Value -ge 10) { "MEDIUM" } else { "LOW   " }
    $report += "`n$priority | $($w.Value.ToString().PadLeft(5)) | $($w.Key)"
}

$report += @"


================================================================================
FILES WITH MOST WARNINGS
================================================================================
"@

$sortedFiles = $fileSummariesList | Sort-Object -Property { $_.Count } -Descending | Where-Object { $_.Count -gt 0 }
foreach ($f in $sortedFiles | Select-Object -First 20) {
    $report += "`n$($f.Count.ToString().PadLeft(5)) | $($f.File)"
}

$report += @"


================================================================================
ALL WARNINGS GROUPED BY CHECK
================================================================================
"@

$grouped = $allWarningsList | Group-Object -Property { $_.Check } | Sort-Object -Property Count -Descending
foreach ($g in $grouped) {
    $report += "`n`n=== $($g.Name) ($($g.Count) occurrences) ==="
    $byFile = $g.Group | Group-Object -Property { $_.File }
    foreach ($fg in ($byFile | Sort-Object -Property Count -Descending | Select-Object -First 5)) {
        $report += "`n  $($fg.Name):"
        foreach ($w in ($fg.Group | Select-Object -First 3)) {
            $report += "`n    Line $($w.Line): $($w.Message)"
        }
        if ($fg.Count -gt 3) {
            $report += "`n    ... and $($fg.Count - 3) more in this file"
        }
    }
}

$report += @"


================================================================================
RECOMMENDATIONS
================================================================================
HIGH PRIORITY (50+ occurrences):
  - These indicate systematic patterns - consider code-wide fixes
  - If intentional, add to .clang-tidy exceptions

MEDIUM PRIORITY (10-49 occurrences):
  - Review case-by-case
  - Good candidates for refactoring

LOW PRIORITY (<10 occurrences):
  - Fix during normal maintenance
  - May be false positives from macros (esp. GTest)

================================================================================
"@

# Save report
$report | Out-File -FilePath $reportPath -Encoding UTF8
Write-Host ""
Write-Host "Report saved to: $reportPath"

# Also save JSON for programmatic analysis
$jsonPath = Join-Path $OutputDir "clang-tidy-report_$Timestamp.json"
$jsonData = @{
    timestamp = $Timestamp
    filesAnalyzed = $fileList.Count
    totalWarnings = $allWarningsList.Count
    uniqueChecks = $warningCountsDict.Count
    elapsedSeconds = [int]$elapsed.TotalSeconds
    warningsByCheck = $warningCountsDict
    filesSummary = $fileSummariesList
    allWarnings = $allWarningsList
}
$jsonData | ConvertTo-Json -Depth 5 | Out-File -FilePath $jsonPath -Encoding UTF8
Write-Host "JSON saved to: $jsonPath"

# Print top 10 warnings
Write-Host ""
Write-Host "TOP 10 WARNINGS:"
$rank = 0
foreach ($w in ($sorted | Select-Object -First 10)) {
    $rank++
    Write-Host "  $rank. $($w.Key): $($w.Value)"
}

Write-Host ""
Write-Host "Check $OutputDir for full reports."
