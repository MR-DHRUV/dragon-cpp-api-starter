param(
    [string]$BuildType = "Release",
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "       Backend Server Build Script        " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$sourceDir = Resolve-Path "$PSScriptRoot\.."
$buildDir = "$sourceDir\build"

# If VcpkgRoot is not provided via param or env, try detecting from vcpkg in PATH
if (-not $VcpkgRoot) {
    $vcpkgCmd = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($vcpkgCmd) {
        $VcpkgRoot = Split-Path $vcpkgCmd.Source -Parent
    }
}

$cmakeArgs = @(
    "-B", $buildDir,
    "-S", $sourceDir,
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if ($VcpkgRoot) {
    $toolchainFile = "$VcpkgRoot/scripts/buildsystems/vcpkg.cmake"
    if (Test-Path $toolchainFile) {
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
        $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
        $cmakeArgs += "-DVCPKG_MANIFEST_MODE=OFF"
        Write-Host "[INFO] Vcpkg Toolchain: $toolchainFile" -ForegroundColor Gray
    }
}

Write-Host "[INFO] Build Directory: $buildDir" -ForegroundColor Gray
Write-Host "[INFO] Build Type:      $BuildType" -ForegroundColor Gray

Write-Host "`n[1/2] Configuring CMake..." -ForegroundColor Cyan
cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n[2/2] Compiling and linking backend_server ($BuildType)..." -ForegroundColor Cyan
cmake --build $buildDir --config $BuildType

if ($LASTEXITCODE -eq 0) {
    $binaryPath = "$buildDir\$BuildType\backend_server.exe"
    if (-not (Test-Path $binaryPath)) {
        $binaryPath = "$buildDir\backend_server.exe"
    }
    Write-Host "`n==========================================" -ForegroundColor Green
    Write-Host "   [SUCCESS] Build Completed!             " -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "Executable: $binaryPath" -ForegroundColor Green
    Write-Host "To run the server:" -ForegroundColor Cyan
    Write-Host "  & `"$binaryPath`"" -ForegroundColor Yellow
} else {
    Write-Host "`n[ERROR] Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
