# PowerShell script to start PostgreSQL container and await readiness
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   Starting PostgreSQL Docker Container    " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Start the docker compose services
Write-Host "[INFO] Running: docker compose up -d" -ForegroundColor Cyan
docker compose up -d

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] docker compose failed. Please ensure Docker is running and in your PATH." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "[INFO] Awaiting PostgreSQL container health status..." -ForegroundColor Cyan
$maxAttempts = 30
$attempt = 0
$healthy = $false

while ($attempt -lt $maxAttempts) {
    Start-Sleep -Seconds 1
    $status = docker inspect --format='{{json .State.Health.Status}}' postgres_db 2>$null
    if ($status -eq '"healthy"') {
        $healthy = $true
        break
    }
    $attempt++
    Write-Host -NoNewline "."
}
Write-Host ""

if ($healthy) {
    Write-Host "[SUCCESS] PostgreSQL is healthy and accepting connections on port 5432!" -ForegroundColor Green
    Write-Host "Database: api_db"
    Write-Host "Username: api_user"
    Write-Host "Password: api_secret_pass"

    # Automatically apply any pending migrations
    & "$PSScriptRoot\migrate.ps1" -Direction up
} else {
    Write-Host "[WARNING] Container is still initializing. Check logs with 'docker logs postgres_db'." -ForegroundColor Yellow
}
