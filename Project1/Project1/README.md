# Custom Terminal with Acrylic Blur

A custom terminal application for Windows with real-time acrylic blur effects and macOS-style window controls.

## Features

- **Acrylic Blur Effect**: Real-time background blur using Windows composition API
- **Custom Window Controls**: macOS-style colored dots (red, green, yellow) for close, maximize, and minimize
- **Borderless Window**: Modern frameless design with custom title bar
- **Terminal Functionality**: Basic command processing with input/output
- **Direct2D Rendering**: Hardware-accelerated graphics using Direct2D and DirectWrite

## Requirements

- Windows 10 or later (for acrylic blur effects)
- Visual Studio 2019 or later
- Windows SDK 10.0 or later

## Building the Project

### Method 1: Using Visual Studio

1. Open Visual Studio
2. Create a new "Empty Project (C++)"
3. Add all `.cpp` and `.h` files to the project
4. Configure project settings:
   - Go to Project Properties → Linker → System → SubSystem
   - Set to "Windows (/SUBSYSTEM:WINDOWS)"
5. Build and run (F5)

### Method 2: Using Command Line (cl.exe)

Open Developer Command Prompt for Visual Studio and run:

```cmd
cl /EHsc /D_UNICODE /DUNICODE main.cpp CustomTerminal.cpp /link user32.lib gdi32.lib d2d1.lib dwrite.lib dwmapi.lib ole32.lib /SUBSYSTEM:WINDOWS /OUT:CustomTerminal.exe
```

## Usage

### Terminal Commands

- `help` - Display available commands
- `clear` - Clear the terminal screen
- `echo <text>` - Echo text to output
- `dir` - List directory contents (simulated)
- `exit` - Close the terminal

### Window Controls

- **Red dot** (leftmost): Close window
- **Green dot** (middle): Maximize/Restore window
- **Yellow dot** (rightmost): Minimize window
- **Title bar**: Drag to move window

### Keyboard Input

- Type commands and press Enter to execute
- Backspace to delete characters
- All standard keyboard input supported

## Code Structure

### Files

- `main.cpp` - Entry point with WinMain
- `CustomTerminal.h` - Class declaration and interfaces
- `CustomTerminal.cpp` - Main implementation

### Key Components

#### CustomTerminal Class

The main class that handles:
- Window creation and management
- Direct2D rendering pipeline
- Input processing
- Command execution

#### Acrylic Blur Implementation

Uses Windows undocumented API through `SetWindowCompositionAttribute` with fallback to DWM blur:

```cpp
ACCENT_POLICY accent;
accent.AccentState = 4; // ACCENT_ENABLE_ACRYLICBLURBEHIND
accent.AccentFlags = 2;
accent.GradientColor = 0x01000000;
```

#### Rendering Pipeline

1. Clear background with semi-transparent color
2. Draw title bar with blur overlay
3. Draw window control buttons (colored dots)
4. Render terminal text output
5. Draw current input line with cursor

## Customization

### Colors

Modify in `CustomTerminal.cpp`:

```cpp
// Background color
m_pRenderTarget->Clear(D2D1::ColorF(0.05f, 0.05f, 0.05f, 0.7f));

// Title bar background
m_pButtonBrush->SetColor(D2D1::ColorF(0.1f, 0.1f, 0.1f, 0.6f));

// Button colors
D2D1::ColorF(0.8f, 0.2f, 0.2f, 0.8f) // Red (close)
D2D1::ColorF(0.2f, 0.8f, 0.2f, 0.8f) // Green (maximize)
D2D1::ColorF(0.8f, 0.8f, 0.2f, 0.8f) // Yellow (minimize)
```

### Fonts

Change terminal font in `CreateDeviceResources()`:

```cpp
m_pDWriteFactory->CreateTextFormat(
    L"Consolas",  // Change font family
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    14.0f,  // Change font size
    L"en-us",
    &m_pTextFormat
);
```

### Window Size

Modify in `Initialize()`:

```cpp
m_hwnd = CreateWindowEx(
    WS_EX_LAYERED,
    L"CustomTerminalClass",
    L"Terminal",
    WS_POPUP | WS_VISIBLE,
    100, 100,   // X, Y position
    1000, 600,  // Width, Height
    nullptr, nullptr, hInstance, this
);
```

### Blur Intensity

Adjust transparency in `EnableBlur()`:

```cpp
// Change alpha value (0-255)
SetLayeredWindowAttributes(m_hwnd, 0, 245, LWA_ALPHA);

// Adjust gradient color
accent.GradientColor = 0x01000000; // AABBGGRR format
```

## Advanced Features

### Adding New Commands

Add to `ProcessCommand()` method:

```cpp
else if (command == L"mycommand") {
    AddOutputLine(L"Executing my command...");
    // Add your logic here
}
```

### Custom Button Actions

Modify `HandleMouseDown()` to change button behavior:

```cpp
if (IsPointInButton(x, y, 1)) {
    // Custom maximize button action
}
```

## Troubleshooting

### Blur Not Working

- Ensure you're running Windows 10 or later
- Acrylic effects require transparency-capable desktop
- Try running as administrator
- Check Windows Settings → Transparency effects are enabled

### Rendering Issues

- Update graphics drivers
- Ensure Windows SDK is properly installed
- Check Direct2D support: `dxdiag.exe` → Display tab

### Build Errors

Common solutions:
- Add required libraries to linker input
- Set correct Windows SDK version
- Enable Unicode support: `/D_UNICODE /DUNICODE`
- Set subsystem to Windows, not Console

## Performance Notes

- Direct2D uses hardware acceleration when available
- Blur effects may impact performance on low-end systems
- Terminal scrollback limited to 100 lines for performance
- Rendering optimized with dirty rectangle updates

## License

This is example code for educational purposes. Feel free to modify and use in your projects.

## Credits

Created using:
- Windows API
- Direct2D and DirectWrite
- Desktop Window Manager (DWM)
