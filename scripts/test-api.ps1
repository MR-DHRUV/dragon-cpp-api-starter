param(
    [string]$BaseUrl = "http://localhost:8080"
)

$ErrorActionPreference = "Continue"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "       Backend Server Test Suite          " -ForegroundColor Cyan
Write-Host "    Target: $BaseUrl                      " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

function Invoke-ApiTest {
    param(
        [string]$Name,
        [string]$Method,
        [string]$Endpoint,
        [object]$Body = $null,
        [int]$ExpectedStatus = 200
    )

    $url = "$BaseUrl$Endpoint"
    $headers = @{ "Content-Type" = "application/json" }
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    try {
        $params = @{
            Uri = $url
            Method = $Method
            Headers = $headers
        }
        if ($Body) {
            $params.Body = ($Body | ConvertTo-Json -Compress)
        }

        $res = Invoke-RestMethod @params
        $stopwatch.Stop()
        $elapsedMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)

        Write-Host "[PASS] ($elapsedMs ms) [$Method $Endpoint] - $Name" -ForegroundColor Green
        return $res
    } catch {
        $stopwatch.Stop()
        $elapsedMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)
        $statusCode = $_.Exception.Response.StatusCode.value__
        if ($statusCode -eq $ExpectedStatus) {
            Write-Host "[PASS] ($elapsedMs ms) Expected HTTP $statusCode on [$Method $Endpoint] - $Name" -ForegroundColor Green
            return $null
        } else {
            Write-Host "[FAIL] ($elapsedMs ms) [$Method $Endpoint] - $Name" -ForegroundColor Red
            Write-Host "       Error: $_" -ForegroundColor Red
            return $null
        }
    }
}

# 1. Ping
Write-Host "`n--- Testing System Latency & Health ---" -ForegroundColor Yellow
$pingRes = Invoke-ApiTest -Name "Ping endpoint" -Method "GET" -Endpoint "/api/v1/ping"
Write-Host "Ping response: $($pingRes | ConvertTo-Json -Compress)" -ForegroundColor Gray

# 2. Health
$healthRes = Invoke-ApiTest -Name "Healthcheck & DB Latency" -Method "GET" -Endpoint "/api/v1/health"
Write-Host "Health status: $($healthRes.data.status) | DB Connected: $($healthRes.data.database.connected) | DB Latency: $($healthRes.data.database.latencyMs) ms" -ForegroundColor Gray

# 3. Users CRUD & Username Availability
Write-Host "`n--- Testing Users REST Endpoints & Username Availability ---" -ForegroundColor Yellow
$users = Invoke-ApiTest -Name "Get All Users" -Method "GET" -Endpoint "/api/v1/users?limit=10"
Write-Host "Retrieved $(@($users.data).Count) users from PostgreSQL." -ForegroundColor Gray

# Test Username Availability Check (Path parameter)
$takenCheck = Invoke-ApiTest -Name "Check Taken Username (john_doe)" -Method "GET" -Endpoint "/api/v1/users/availability/john_doe"
Write-Host "Check result for 'john_doe': available=$($takenCheck.data.available), reason='$($takenCheck.data.reason)'" -ForegroundColor Gray

$timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
$testUsername = "test_user_$timestamp"

# Test Username Availability Check (Query parameter)
$availCheck = Invoke-ApiTest -Name "Check Available Username ($testUsername)" -Method "GET" -Endpoint "/api/v1/users/check-username?username=$testUsername"
Write-Host "Check result for '$testUsername': available=$($availCheck.data.available)" -ForegroundColor Gray

# Test Invalid Username Format Check
Invoke-ApiTest -Name "Check Invalid Username Format (bad*name!)" -Method "GET" -Endpoint "/api/v1/users/availability/bad*name!" -ExpectedStatus 400

$createPayload = @{
    username = $testUsername
    email = "$testUsername@test.io"
    fullName = "Automated Test Runner"
}
$createdUser = Invoke-ApiTest -Name "Create New User" -Method "POST" -Endpoint "/api/v1/users" -Body $createPayload -ExpectedStatus 201

# Test Reverification on Creation: Duplicate Username should be rejected with 409 Conflict
$duplicatePayload = @{
    username = $testUsername
    email = "different_$timestamp@test.io"
    fullName = "Duplicate Runner"
}
Invoke-ApiTest -Name "Re-verification on Create (Duplicate Username Rejection)" -Method "POST" -Endpoint "/api/v1/users" -Body $duplicatePayload -ExpectedStatus 409

# Test Reverification on Create: Invalid Username Format rejection
$invalidFormatPayload = @{
    username = "inv@lid!"
    email = "invalid_$timestamp@test.io"
    fullName = "Invalid Format Runner"
}
Invoke-ApiTest -Name "Re-verification on Create (Invalid Username Format Rejection)" -Method "POST" -Endpoint "/api/v1/users" -Body $invalidFormatPayload -ExpectedStatus 400

if ($createdUser -and $createdUser.data.id) {
    $userId = $createdUser.data.id
    Write-Host "Created User ID: $userId" -ForegroundColor Gray

    # Get Single User
    $singleUser = Invoke-ApiTest -Name "Get User By ID" -Method "GET" -Endpoint "/api/v1/users/$userId"

    # Update User
    $updatePayload = @{
        fullName = "Automated Test Runner (Updated)"
        isActive = $true
    }
    $updatedUser = Invoke-ApiTest -Name "Update User" -Method "PUT" -Endpoint "/api/v1/users/$userId" -Body $updatePayload

    # Delete User
    Invoke-ApiTest -Name "Delete User" -Method "DELETE" -Endpoint "/api/v1/users/$userId"
}

Write-Host "`n==========================================" -ForegroundColor Green
Write-Host "       Integration Tests Completed        " -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green