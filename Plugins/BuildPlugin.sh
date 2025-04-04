#!/bin/bash

# Set environment variables
ENGINE_DIR="/opt/unreal-engine/UE_5.5"
PROJECT_PATH="$(cd "$(dirname "$0")/../.." && pwd)"
PLUGIN_PATH="$(cd "$(dirname "$0")" && pwd)"

if [ ! -d "$ENGINE_DIR" ]; then
    echo "ERROR: Unreal Engine directory not found at $ENGINE_DIR."
    echo "Please modify this script to point to your Unreal Engine installation."
    exit 1
fi

# Find the project file
PROJECT_FILE=$(find "$PROJECT_PATH" -maxdepth 1 -name "*.uproject" | head -n 1)
if [ -z "$PROJECT_FILE" ]; then
    echo "ERROR: Unreal project file not found."
    echo "Please ensure this plugin is in the Plugins directory of an Unreal project."
    exit 1
fi

echo "Building Multi-Server Sync Plugin..."
echo "Engine: $ENGINE_DIR"
echo "Project: $PROJECT_PATH"
echo "Project File: $PROJECT_FILE"

# Clean previous build files
if [ -d "$PLUGIN_PATH/Binaries" ]; then
    echo "Cleaning previous build..."
    rm -rf "$PLUGIN_PATH/Binaries"
fi
if [ -d "$PLUGIN_PATH/Intermediate" ]; then
    rm -rf "$PLUGIN_PATH/Intermediate"
fi

# Build the plugin
echo "Building plugin..."
"$ENGINE_DIR/Engine/Build/BatchFiles/RunUAT.sh" BuildPlugin -Plugin="$PLUGIN_PATH/MultiServerSync.uplugin" -Package="$PLUGIN_PATH/Build" -TargetPlatforms=Linux -Rocket

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed with error code $?."
    exit 1
fi

echo "Build completed successfully!"
echo "Plugin output directory: $PLUGIN_PATH/Build"