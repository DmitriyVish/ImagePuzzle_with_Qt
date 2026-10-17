param(
    [string]$QtPrefix = 'C:\Qt\6.10.1\mingw_64',
    [string]$CompilerBin = 'C:\Qt\Tools\mingw1310_64\bin',
    [string]$CMakeBin = 'C:\Qt\Tools\CMake_64\bin',
    [string]$Ninja = 'C:\mingw64\bin\ninja.exe',
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build\release'
$packageDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $packageDirectory) {
    throw 'Выходной каталог уже существует. Укажите новый каталог для чистой сборки.'
}
$env:PATH = "$CMakeBin;$CompilerBin;$QtPrefix\bin;$(Split-Path -Parent $Ninja);" + $env:PATH
# Все операции остаются в PowerShell; существующие файлы пользователя не удаляются.
& "$QtPrefix\bin\qt-cmake.bat" -S $projectRoot -B $buildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DCMAKE_CXX_COMPILER=$CompilerBin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$Ninja"
if ($LASTEXITCODE -ne 0) { throw 'Не удалось сконфигурировать проект.' }
& "$CMakeBin\cmake.exe" --build $buildDirectory --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Ошибка сборки.' }
& "$CMakeBin\ctest.exe" --test-dir $buildDirectory --output-on-failure --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Тесты завершились с ошибкой.' }
New-Item -ItemType Directory -Path $packageDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory 'ImagePuzzle.exe') -Destination $packageDirectory
& "$QtPrefix\bin\windeployqt.exe" --release --compiler-runtime --translations ru --dir $packageDirectory (Join-Path $packageDirectory 'ImagePuzzle.exe')
if ($LASTEXITCODE -ne 0) { throw 'Не удалось развернуть библиотеки Qt.' }
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packageDirectory
New-Item -ItemType Directory -Path (Join-Path $packageDirectory 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\fonts\OFL.txt') -Destination (Join-Path $packageDirectory 'licenses\Geologica-OFL.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\icons\Fluent-Emoji-MIT.txt') -Destination (Join-Path $packageDirectory 'licenses\Fluent-Emoji-MIT.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\SOURCES.md') -Destination $packageDirectory
Write-Output "Готово: $packageDirectory\ImagePuzzle.exe"
