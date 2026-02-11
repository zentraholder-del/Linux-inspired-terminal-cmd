# Linux-Inspired Terminal for Windows

A modern, customizable terminal emulator for Windows with a Linux-inspired aesthetic and smooth visual effects.

## ⚠️ Work in Progress

This project is actively under development. Some features may not work as expected, and bugs are likely present. Use at your own risk and feel free to report issues!

**Known Limitations:**
- Some edge cases in command execution may behave unexpectedly
- Autocomplete might not handle all path formats correctly
- Performance may vary depending on system configuration
- Not all Windows commands are fully tested

## Overview

This project brings a sleek, glass-morphic terminal interface to Windows, featuring real-time command execution, intelligent autocomplete, and extensive customization options. Built because I wanted a beautiful Linux-style terminal experience on Windows without dual-booting or WSL overhead.

## Features

### Core Functionality
- **Real Command Execution** - Execute actual Windows commands (cmd.exe wrapper)
- **Smart Autocomplete** - Context-aware suggestions for both commands and file paths
- **Command History** - Navigate previous commands with Ctrl+Z/Ctrl+X
- **Custom Commands** - Built-in commands like `system`, `version`, `settings`
- **Search** - Find text in terminal output with Ctrl+F

### Visual Effects
- **Glass Blur Background** - Windows Aero-style blur effect (toggleable)
- **Smooth Animations** - Animated caret with customizable speed
- **Cursor Trail** - Optional trailing effect for the caret
- **Color Customization** - Fully customizable color scheme (caret, background, text, blur tint)
- **Timestamps** - Optional timestamp display for command output

### UI/UX
- **Draggable Window** - Custom title bar with smooth window controls
- **macOS-Style Buttons** - Close, minimize, maximize with glow effects
- **Live Preview** - Settings window shows changes in real-time
- **Persistent Settings** - Automatically saves preferences to AppData

## Tech Stack

- **C++** - Core application logic
- **ImGui** - Immediate mode GUI framework
- **DirectX 11** - Hardware-accelerated rendering
- **Win32 API** - Windows integration and DWM blur effects

## Building

Requires:
- Visual Studio 2019+ with C++ tools
- Windows SDK
- ImGui library

## Contributing

Since this is a WIP, contributions are welcome! If you find bugs or have feature suggestions, please open an issue or submit a PR.

## Screenshots

This does have background blur as seen in the last image.

<img width="1018" height="612" alt="image" src="https://github.com/user-attachments/assets/36f72d1e-da5c-413e-b232-29d33e3640ac" />

<img width="1019" height="618" alt="image" src="https://github.com/user-attachments/assets/a271249f-2ddb-4913-8736-7712b76d10d9" />

<img width="1033" height="618" alt="image" src="https://github.com/user-attachments/assets/2053672f-b802-4095-9d4d-1aba5d4bd486" />




## Why?

I wanted a terminal that looked good, felt responsive, and didn't require running a full Linux VM or WSL. This combines the aesthetic appeal of modern Linux terminals with native Windows command execution.

## Disclaimer

This is a hobby project made for fun and learning. It's not meant to replace production terminals like Windows Terminal, PowerShell, or ConEmu. Expect bugs, missing features, and occasional weirdness!
