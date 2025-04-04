# Multi-Server Synchronization Framework for Unreal Engine

This plugin provides precise frame synchronization for Unreal Engine 5.5+ projects running across multiple servers.

## Features

- Hardware genlock (Quadro Sync) support with automatic detection
- Software-based synchronization when hardware genlock is unavailable
- PTP (Precision Time Protocol) for accurate time synchronization
- Software PLL (Phase-Locked Loop) for continuous fine adjustments
- Master-slave architecture with automatic failover
- nDisplay compatibility with extended functionality
- Frame rendering synchronization across multiple machines

## Requirements

- Unreal Engine 5.5 or higher
- Windows 10/11 or Linux (Ubuntu 20.04+)
- Network connectivity (1Gbps Ethernet recommended)
- Quadro Sync hardware (optional)

## Installation

1. Create a `Plugins` folder in your Unreal Engine project if it doesn't already exist
2. Clone this repository into the `Plugins` folder
3. Rename the folder to `MultiServerSync`
4. Regenerate your project files and build your project
5. Enable the plugin in the Unreal Engine Editor under Plugins > Networking

## Configuration

Detailed configuration instructions will be provided after installation. Access the plugin settings through the editor menu (Window > Multi-Server Sync).

## Development Status

This plugin is currently in development. See the [Development Roadmap](ROADMAP.md) for more details.

## License

[Your License]

## Support

For support, please [contact information]