@echo off
setlocal enabledelayedexpansion

REM Set environment variables
set ENGINE_DIR=C:\Program Files\Epic Games\UE_5.5
set PROJECT_PATH=%~dp0..\..
set PLUGIN_PATH=%~dp0

if not exist "%ENGINE_DIR%" (
    echo ERROR: Unreal Engine directory not found at %ENGINE_DIR%.
    echo Please modify this script to point to your Unreal Engine installation.
    goto :exit
)

if not exist "%PROJECT_PATH%\%PROJECT_PATH:~-5,-1%.uproject" (
    echo ERROR: Unreal project file not found.
    echo Please ensure this plugin is in the Plugins directory of an Unreal project.
    goto :exit
)

echo Building Multi-Server Sync Plugin...
echo Engine: %ENGINE_DIR%
echo Project: %PROJECT_PATH%

REM Clean previous build files
if exist "%PLUGIN_PATH%\Binaries" (
    echo Cleaning previous build...
    rmdir /s /q "%PLUGIN_PATH%\Binaries"
)
if exist "%PLUGIN_PATH%\Intermediate" (
    rmdir /s /q "%PLUGIN_PATH%\Intermediate"
)

REM Build the plugin
echo Building plugin...
"%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_PATH%\MultiServerSync.uplugin" -Package="%PLUGIN_PATH%\Build" -TargetPlatforms=Win64 -Rocket

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed with error code %ERRORLEVEL%.
    goto :exit
)

echo Build completed successfully!
echo Plugin output directory: %PLUGIN_PATH%\Build

:exit
pause
endlocal