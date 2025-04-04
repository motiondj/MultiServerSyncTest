# Multi-Server Sync Framework Dependencies

## Unreal Engine Dependencies

These modules are part of Unreal Engine and are already included in the Build.cs files:

### Core Dependencies
- **Core**: Core functionality
- **CoreUObject**: Core UObject functionality
- **Engine**: Engine functionality
- **Sockets**: Socket functionality for network communication
- **Networking**: High-level networking functionality

### Editor Dependencies
- **UnrealEd**: Unreal Editor functionality
- **LevelEditor**: Level Editor functionality
- **Slate**: UI framework
- **SlateCore**: Core UI framework functionality
- **EditorStyle**: Editor styling
- **Projects**: Project management

## External Dependencies

The following external libraries are used but do not require separate installation as they are included in the Unreal Engine:

### Network Time Synchronization
- Standard C++ libraries for time functions
- Socket libraries (part of Unreal Engine)

## Optional Integration

The plugin can optionally integrate with:

### nDisplay Module
- If available, the plugin will detect and integrate with nDisplay for additional functionality.
- No specific dependency is required for basic functionality if nDisplay is not available.

### Quadro Sync Hardware
- If Quadro Sync hardware is available, the plugin will detect and use it.
- No specific software dependency is required if hardware is not available.

## Development Dependencies

These tools are needed for development but not for runtime:

- **Git**: Version control
- **Visual Studio 2022**: For Windows development
- **GCC/Clang**: For Linux development
- **Unreal Engine Build Tools**: Part of the Unreal Engine installation

## Compatibility Notes

- Designed for Unreal Engine 5.5+
- Compatible with Windows 10/11 and Linux (Ubuntu 20.04+)
- Network performance is best with gigabit Ethernet or better