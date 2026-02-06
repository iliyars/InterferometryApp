# diagnose.ps1
# Скрипт для диагностики проблем со сборкой

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "  Build Diagnostics" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# 1. Проверка Visual Studio
Write-Host "[1/7] Checking Visual Studio installation..." -ForegroundColor Yellow

$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWherePath) {
    Write-Host "  ✓ vswhere.exe found" -ForegroundColor Green

    $vsInstances = & $vsWherePath -all -format json | ConvertFrom-Json
    Write-Host "  Found $($vsInstances.Count) Visual Studio installation(s):" -ForegroundColor Green

    foreach ($vs in $vsInstances) {
        Write-Host "    - $($vs.displayName) ($($vs.installationVersion))" -ForegroundColor Gray
        Write-Host "      Path: $($vs.installationPath)" -ForegroundColor Gray
    }
} else {
    Write-Host "  ✗ vswhere.exe NOT found" -ForegroundColor Red
    exit 1
}

Write-Host ""

# 2. Проверка MSBuild
Write-Host "[2/7] Checking MSBuild..." -ForegroundColor Yellow

$msbuildPath = & $vsWherePath -latest -products * `
    -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1

if ($msbuildPath -and (Test-Path $msbuildPath)) {
    Write-Host "  ✓ MSBuild found: $msbuildPath" -ForegroundColor Green

    $version = & $msbuildPath -version | Select-Object -Last 1
    Write-Host "    Version: $version" -ForegroundColor Gray
} else {
    Write-Host "  ✗ MSBuild NOT found" -ForegroundColor Red
    Write-Host "    Please install 'Desktop development with C++' workload" -ForegroundColor Yellow
    exit 1
}

Write-Host ""

# 3. Проверка Solution файла
Write-Host "[3/7] Checking solution file..." -ForegroundColor Yellow

$solutionPath = Join-Path $PSScriptRoot "InterferometryApp.sln"
if (Test-Path $solutionPath) {
    Write-Host "  ✓ Solution file found: $solutionPath" -ForegroundColor Green
} else {
    Write-Host "  ✗ Solution file NOT found" -ForegroundColor Red
    exit 1
}

Write-Host ""

# 4. Проверка проектных файлов
Write-Host "[4/7] Checking project files..." -ForegroundColor Yellow

$vcxprojPath = Join-Path $PSScriptRoot "InterferometryApp.vcxproj"
if (Test-Path $vcxprojPath) {
    Write-Host "  ✓ Project file found: InterferometryApp.vcxproj" -ForegroundColor Green

    # Читаем проект и ищем важные элементы
    [xml]$project = Get-Content $vcxprojPath

    # Проверяем Platform Toolset
    $toolsets = $project.Project.PropertyGroup | Where-Object { $_.PlatformToolset } |
                Select-Object -ExpandProperty PlatformToolset -Unique

    if ($toolsets) {
        Write-Host "    Platform Toolset(s): $($toolsets -join ', ')" -ForegroundColor Gray
    }

    # Проверяем Windows SDK
    $sdks = $project.Project.PropertyGroup | Where-Object { $_.WindowsTargetPlatformVersion } |
            Select-Object -ExpandProperty WindowsTargetPlatformVersion -Unique

    if ($sdks) {
        Write-Host "    Windows SDK: $($sdks -join ', ')" -ForegroundColor Gray
    }
} else {
    Write-Host "  ✗ Project file NOT found" -ForegroundColor Red
    exit 1
}

Write-Host ""

# 5. Проверка исходных файлов
Write-Host "[5/7] Checking source files..." -ForegroundColor Yellow

$srcPath = Join-Path $PSScriptRoot "src"
if (Test-Path $srcPath) {
    $cppFiles = Get-ChildItem -Path $srcPath -Recurse -Filter "*.cpp" | Measure-Object
    $hFiles = Get-ChildItem -Path $srcPath -Recurse -Filter "*.h" | Measure-Object

    Write-Host "  ✓ Source directory found" -ForegroundColor Green
    Write-Host "    CPP files: $($cppFiles.Count)" -ForegroundColor Gray
    Write-Host "    H files: $($hFiles.Count)" -ForegroundColor Gray
} else {
    Write-Host "  ⚠ Source directory NOT found" -ForegroundColor Yellow
}

Write-Host ""

# 6. Проверка OpenCV
Write-Host "[6/7] Checking OpenCV..." -ForegroundColor Yellow

$opencvPath = Join-Path $PSScriptRoot "external\opencv"
if (Test-Path $opencvPath) {
    Write-Host "  ✓ OpenCV directory found" -ForegroundColor Green

    $opencvLib = Join-Path $opencvPath "lib\opencv_world4120.lib"
    if (Test-Path $opencvLib) {
        Write-Host "    ✓ OpenCV library found (Release)" -ForegroundColor Gray
    }

    $opencvLibDebug = Join-Path $opencvPath "lib\opencv_world4120d.lib"
    if (Test-Path $opencvLibDebug) {
        Write-Host "    ✓ OpenCV library found (Debug)" -ForegroundColor Gray
    }
} else {
    Write-Host "  ⚠ OpenCV directory NOT found" -ForegroundColor Yellow
    Write-Host "    Project may fail to link" -ForegroundColor Yellow
}

Write-Host ""

# 7. Попытка сборки с детальным логом
Write-Host "[7/7] Attempting build with detailed logging..." -ForegroundColor Yellow
Write-Host ""

$logPath = Join-Path $PSScriptRoot "build_diagnostic.log"

Write-Host "Building project (this may take a minute)..." -ForegroundColor Cyan
Write-Host "Log will be saved to: $logPath" -ForegroundColor Gray
Write-Host ""

& $msbuildPath $solutionPath `
    /p:Configuration=Debug `
    /p:Platform=x64 `
    /v:detailed `
    /fl `
    /flp:"LogFile=$logPath;Verbosity=diagnostic"

$buildResult = $LASTEXITCODE

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan

if ($buildResult -eq 0) {
    Write-Host "  ✓ BUILD SUCCESSFUL" -ForegroundColor Green
    Write-Host ""
    Write-Host "Your executable should be at:" -ForegroundColor Green
    Write-Host "  x64\Debug\InterferometryApp.exe" -ForegroundColor White
} else {
    Write-Host "  ✗ BUILD FAILED" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please check the log file for errors:" -ForegroundColor Yellow
    Write-Host "  $logPath" -ForegroundColor White
    Write-Host ""
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "  1. Missing Windows SDK" -ForegroundColor White
    Write-Host "  2. Missing MFC/ATL support" -ForegroundColor White
    Write-Host "  3. Wrong Platform Toolset version" -ForegroundColor White
    Write-Host "  4. Missing OpenCV libraries" -ForegroundColor White
    Write-Host ""
    Write-Host "Opening log file..." -ForegroundColor Cyan

    if (Test-Path $logPath) {
        # Показать последние 50 строк лога с ошибками
        Write-Host ""
        Write-Host "Last errors from build log:" -ForegroundColor Red
        Write-Host "----------------------------" -ForegroundColor Red

        $errors = Get-Content $logPath | Select-String "error" | Select-Object -Last 20
        if ($errors) {
            $errors | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
        } else {
            Write-Host "No explicit errors found. Check warnings in log file." -ForegroundColor Yellow
        }

        # Открыть в блокноте
        Start-Process notepad $logPath
    }
}

Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

exit $buildResult
