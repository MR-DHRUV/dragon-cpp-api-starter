param(
    [ValidateSet("up", "down")]
    [string]$Direction = "up",
    [int]$Steps = 1,
    [string]$ContainerName = "postgres_db",
    [string]$DbUser = "api_user",
    [string]$DbName = "api_db",
    [string]$MigrationsDir = "$PSScriptRoot\..\migrations"
)

$ErrorActionPreference = "Continue"

$upperDir = $Direction.ToUpper()
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   Database Migration Runner ($upperDir)       " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Ensure PostgreSQL container is running
$inspect = docker inspect --format='{{json .State.Running}}' $ContainerName 2>$null
if ($inspect -ne 'true') {
    Write-Host "[ERROR] PostgreSQL container '$ContainerName' is not running!" -ForegroundColor Red
    Write-Host "Please start it first using 'make db' or 'docker compose up -d'." -ForegroundColor Yellow
    exit 1
}

# 2. Ensure schema_migrations tracking table exists
$createTableSql = @"
CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(128) PRIMARY KEY,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
"@

$createTableSql | docker exec -i $ContainerName psql -U $DbUser -d $DbName -v ON_ERROR_STOP=1 -q 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to ensure 'schema_migrations' table exists." -ForegroundColor Red
    exit 1
}

# 3. Fetch currently applied migrations in ascending order
$applied = @()
$querySql = "SELECT version FROM schema_migrations ORDER BY version ASC;"
$queryOutput = docker exec -i $ContainerName psql -U $DbUser -d $DbName -t -A -c $querySql 2>$null
if ($queryOutput) {
    $applied = $queryOutput -split "`r?`n" | Where-Object { $_ -match '\S' }
}

if (-not (Test-Path $MigrationsDir)) {
    Write-Host "[ERROR] Migrations directory not found at: $MigrationsDir" -ForegroundColor Red
    exit 1
}

if ($Direction -eq "up") {
    $upFiles = Get-ChildItem -Path $MigrationsDir -Filter "*.up.sql" | Sort-Object Name
    if ($upFiles.Count -eq 0) {
        Write-Host "[INFO] No *.up.sql files found in $MigrationsDir" -ForegroundColor Yellow
        exit 0
    }

    $appliedCount = 0
    $skippedCount = 0

    foreach ($file in $upFiles) {
        $version = $file.Name -replace '\.up\.sql$', ''

        if ($applied -contains $version) {
            Write-Host "  [SKIP]  $($file.Name) (already applied)" -ForegroundColor DarkGray
            $skippedCount++
            continue
        }

        Write-Host "  [APPLY UP] $($file.Name)..." -ForegroundColor Yellow -NoNewline
        $sqlContent = Get-Content -Raw -Path $file.FullName

        $wrappedSql = @"
SET client_min_messages = WARNING;
BEGIN;
$sqlContent
INSERT INTO schema_migrations (version) VALUES ('$version') ON CONFLICT (version) DO NOTHING;
COMMIT;
"@

        $output = $wrappedSql | docker exec -i $ContainerName psql -U $DbUser -d $DbName -v ON_ERROR_STOP=1 -q 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host " [FAILED]" -ForegroundColor Red
            Write-Host "`n[ERROR] Migration UP failed on $($file.Name):" -ForegroundColor Red
            Write-Host ($output | Out-String) -ForegroundColor Red
            exit $LASTEXITCODE
        }

        Write-Host " [OK]" -ForegroundColor Green
        $appliedCount++
    }

    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Migrations UP summary: $appliedCount applied, $skippedCount up to date." -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Cyan

} elseif ($Direction -eq "down") {
    if ($applied.Count -eq 0) {
        Write-Host "[INFO] No applied migrations to revert." -ForegroundColor Yellow
        exit 0
    }

    # Revert in reverse order
    $toRevert = @($applied)
    [Array]::Reverse($toRevert)
    if ($Steps -gt 0 -and $Steps -lt $toRevert.Count) {
        $toRevert = $toRevert[0..($Steps - 1)]
    }

    $revertedCount = 0
    foreach ($version in $toRevert) {
        $downFileName = "$version.down.sql"
        $downFilePath = Join-Path $MigrationsDir $downFileName

        if (-not (Test-Path $downFilePath)) {
            Write-Host "  [ERROR] Corresponding down migration '$downFileName' not found!" -ForegroundColor Red
            exit 1
        }

        Write-Host "  [REVERT DOWN] $downFileName..." -ForegroundColor Yellow -NoNewline
        $sqlContent = Get-Content -Raw -Path $downFilePath

        $wrappedSql = @"
SET client_min_messages = WARNING;
BEGIN;
$sqlContent
DELETE FROM schema_migrations WHERE version = '$version';
COMMIT;
"@

        $output = $wrappedSql | docker exec -i $ContainerName psql -U $DbUser -d $DbName -v ON_ERROR_STOP=1 -q 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host " [FAILED]" -ForegroundColor Red
            Write-Host "`n[ERROR] Migration DOWN failed on $($downFileName):" -ForegroundColor Red
            Write-Host ($output | Out-String) -ForegroundColor Red
            exit $LASTEXITCODE
        }

        Write-Host " [OK]" -ForegroundColor Green
        $revertedCount++
    }

    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Migrations DOWN summary: $revertedCount migration(s) reverted." -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Cyan
}
