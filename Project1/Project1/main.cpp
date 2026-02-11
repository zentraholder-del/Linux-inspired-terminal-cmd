// all the imgui stuff we need
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// windows and graphics stuff
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <wingdi.h>
#include <dwmapi.h>
#include <windows.h>
#include <sstream>
#include <algorithm>
#include <memory>
#include <fstream>
#include <shlobj.h>

// gotta link these libraries or nothing works
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

// directx stuff for rendering
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// terminal pane struct - holds state for the terminal
struct TerminalPane
{
    std::vector<std::string> outputLines;
    char inputBuffer[256];
    std::string currentDir;
    float caretTime;
    int caretPos;
    std::vector<std::string> commandHistory;
    int historyIndex;
    bool isActive;

    TerminalPane()
        : currentDir("C:\\Users\\User"), caretTime(0.0f), caretPos(0), historyIndex(-1), isActive(false)
    {
        inputBuffer[0] = '\0';
    }
};

// terminal panes array (only using pane 0 now)
static TerminalPane g_panes[2];
static int g_activePane = 0;  // active pane index (always 0)

// forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void ProcessCommand(const std::string& cmd);
void RenderBlur(HWND hwnd);
void AddOutputLine(const std::string& line);
void AddOutputLineToPane(int paneIdx, const std::string& line);
void RenderTerminalPane(int paneIdx, float width, float height, ImGuiIO& io);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// window dragging
static bool g_isDragging = false;
static POINT g_dragOffset = { 0, 0 };

// settings
static bool g_blurEnabled = true;
static bool g_showTimestamp = false;
static ImVec4 g_caretColor = ImVec4(0.196f, 1.0f, 0.392f, 1.0f);  // green
static ImVec4 g_bgColor = ImVec4(0.02f, 0.02f, 0.031f, 1.0f);     // dark blue-black
static ImVec4 g_textColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);       // white-ish
static ImVec4 g_blurTintColor = ImVec4(0.15f, 0.17f, 0.22f, 0.3f); // blue-tinted blur
static bool g_showSettingsWindow = false;

// temporary settings (for preview before saving)
static bool g_tempBlurEnabled = true;
static bool g_tempShowTimestamp = false;
static ImVec4 g_tempCaretColor = ImVec4(0.196f, 1.0f, 0.392f, 1.0f);
static ImVec4 g_tempBgColor = ImVec4(0.02f, 0.02f, 0.031f, 1.0f);
static ImVec4 g_tempTextColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
static ImVec4 g_tempBlurTintColor = ImVec4(0.15f, 0.17f, 0.22f, 0.3f);
static bool g_tempCursorTrailEnabled = false;
static bool g_settingsChanged = false;

// smooth toggle animation states (0.0 to 1.0)
static float g_toggleBlurAnim = 1.0f;
static float g_toggleTsAnim = 0.0f;
static float g_toggleTrailAnim = 0.0f;

// preview typing animation
static float g_previewTypeTime = 0.0f;
static std::string g_previewFullText = "";
static int g_previewCharIndex = 0;
static std::string g_previewCurrentText = "";
static float g_previewSmoothCaretX = 0.0f;  // for smooth caret movement

// how many commands to remember in history
static int g_maxHistorySize = 50;

// font stuff - consolas looks like a proper terminal
static float g_fontSize = 16.0f;
static std::string g_fontName = "Consolas";

// caret blink speed - how fast that cursor flashes
static float g_caretAnimSpeed = 2.0f;

// cursor trail effect - looks cool but can be distracting
static bool g_cursorTrailEnabled = false;
static std::vector<std::pair<ImVec2, float>> g_cursorTrail;  // where the trail dots are and how old they are
static int g_maxTrailLength = 8;  // max number of trail dots

// search stuff for finding text in output
static bool g_showSearch = false;
static char g_searchBuffer[256] = "";
static std::vector<int> g_searchResults;  // which lines matched
static int g_currentSearchResult = -1;  // currently highlighted match

// autocomplete - all the windows commands we know about
static std::vector<std::string> g_commonCommands = {
    // custom terminal commands
    "cmds", "cls", "quit", "version", "system", "settings", "time", "clear",
    
    // a
    "append", "arp", "assoc", "at", "atmadm", "attrib", "auditpol", "autoconv", "autofmt",
    
    // b
    "bcdboot", "bcdedit", "bdehdcfg", "bitsadmin", "bootcfg", "break", "bulkadmin",
    
    // c
    "cacls", "call", "cd", "certreq", "certutil", "change", "chcp", "chdir", 
    "checknetisolation", "chglogon", "chgport", "chgusr", "chkdsk", "chkntfs", 
    "choice", "cipher", "clean", "cleanmgr", "clip", "cls", "cmd", "cmdkey", 
    "color", "comp", "compact", "convert", "copy", "cprofile", "csencrypt", 
    "cscript", "csvde", "ctty",
    
    // d
    "date", "dcdiag", "dcgpofix", "dcomcnfg", "defrag", "del", "dfsradmin", 
    "dfsrdiag", "dfsrmig", "diantz", "dir", "diskcomp", "diskcopy", "diskpart", 
    "diskperf", "diskraid", "dism", "dispdiag", "dnscmd", "doskey", "driverquery", 
    "dsacls", "dsadd", "dsget", "dsmod", "dsmove", "dsquery", "dsrm", 
    "dvedit", "dxdiag",
    
    // e
    "echo", "edit", "edlin", "efsrecover", "endlocal", "erase", "eventcreate", 
    "eventquery", "eventtriggers", "evntcmd", "exe2bin", "exit", "expand", "explorer",
    
    // f
    "fc", "fdisk", "find", "findstr", "finger", "flattemp", "fondue", "for", 
    "forfiles", "format", "fp", "freedisk", "fsutil", "ftp", "ftype", "fveupdate",
    
    // g
    "getmac", "gettype", "global", "goto", "gpfixup", "gpresult", "gpupdate", 
    "graftabl", "graphics",
    
    // h
    "help", "hostname",
    
    // i
    "iCACLS", "iexpress", "if", "inuse", "ipconfig", "ipxroute", "irftp", 
    "iscsicli", "iscsicpl",
    
    // j
    "jetpack", "join",
    
    // k
    "klist", "ksetup", "ktmutil", "ktpass",
    
    // l
    "label", "lodctr", "logman", "logoff", "lpq", "lpr", 
    
    // m
    "macfile", "makecab", "manage-bde", "mapadmin", "md", "mkdir", "mklink", 
    "mmc", "mode", "more", "mount", "mountvol", "move", "mqbkup", "mqsvc", 
    "mqtgsvc", "msdt", "msg", "msiexec", "msinfo32", "mstsc",
    
    // n
    "nbtstat", "net", "net1", "netcfg", "netsh", "netstat", "nfsadmin", 
    "nfsshare", "nfsstat", "nlbmgr", "nltest", "nslookup", "ntbackup", 
    "ntcmdprompt", "ntdsutil", "ntfrsutl",
    
    // o
    "openfiles",
    
    // p
    "pagefileconfig", "path", "pathping", "pause", "pbadmin", "pentnt", 
    "perfmon", "ping", "pkgmgr", "pnpunattend", "pnputil", "popd", "powercfg", 
    "print", "prncnfg", "prndrvr", "prnjobs", "prnmngr", "prnport", "prnqctl", 
    "prompt", "pubprn", "pushd", "pwlauncher",
    
    // q
    "qappsrv", "qprocess", "query", "quser", "qwinsta",
    
    // r
    "rasautou", "rasdial", "rcp", "rd", "rdpsign", "recover", "reg", 
    "regini", "regsvr32", "relog", "rem", "ren", "rename", "repair-bde", 
    "replace", "reset", "restore", "rexec", "risetup", "rmdir", "robocopy", 
    "route", "rpcinfo", "rpcping", "rsh", "rundll32", "rwinsta",
    
    // s
    "sc", "schtasks", "sdbinst", "secedit", "set", "setlocal", "setspn", 
    "setx", "sfc", "shadow", "share", "shift", "showmount", "shutdown", 
    "sort", "start", "subst", "sxstrace", "sysocmgr", "systeminfo",
    
    // t
    "takeown", "tapicfg", "taskkill", "tasklist", "tcmsetup", "telnet", 
    "tftp", "time", "timeout", "title", "tlntadmn", "tpmvscmgr", "tracerpt", 
    "tracert", "tree", "tscon", "tsdiscon", "tsecimp", "tskill", "tsprof", 
    "type", "typeperf", "tzutil",
    
    // u
    "umount", "undelete", "unlodctr",
    
    // v
    "ver", "verify", "vol", "vssadmin",
    
    // w
    "w32tm", "waitfor", "wbadmin", "wdsutil", "wevtutil", "where", "whoami", 
    "winmgmt", "winrm", "winrs", "winsat", "wlbs", "wmic", "wscript",
    
    // x
    "xcopy",
    
    // y
    
    // z
    
    // common utilities and programs
    "calc", "notepad", "explorer", "control", "mspaint", "wordpad", "write",
    "msconfig", "msinfo32", "devmgmt.msc", "diskmgmt.msc", "eventvwr.msc",
    "services.msc", "taskmgr", "regedit", "appwiz.cpl", "inetcpl.cpl",
    "ncpa.cpl", "sysdm.cpl", "timedate.cpl", "firewall.cpl", "powercfg.cpl",
    
    // development tools
    "git", "npm", "node", "npx", "python", "py", "pip", "pip3", "code", 
    "dotnet", "java", "javac", "gcc", "g++", "cmake", "make", 
    
    // powerShell
    "powershell", "pwsh",
    
    // help
    "help", "/?", "?"
};
static std::vector<std::string> g_suggestions;
static int g_selectedSuggestion = -1;
static bool g_showSuggestions = false;

// helper function to add output with optional timestamp (adds to active pane)
void AddOutputLine(const std::string& line)
{
    TerminalPane& pane = g_panes[g_activePane];
    if (g_showTimestamp)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timestamp[32];
        sprintf_s(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        pane.outputLines.push_back(std::string(timestamp) + line);
    }
    else
    {
        pane.outputLines.push_back(line);
    }
}

// helper function to add output to a specific pane
// add a line of text to a specific pane's output
void AddOutputLineToPane(int paneIdx, const std::string& line)
{
    if (paneIdx < 0 || paneIdx > 1) return;  // safety check
    TerminalPane& pane = g_panes[paneIdx];
    if (g_showTimestamp)
    {
        // add timestamp if user wants it
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timestamp[32];
        sprintf_s(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        pane.outputLines.push_back(std::string(timestamp) + line);
    }
    else
    {
        pane.outputLines.push_back(line);
    }
}

// figure out where to save settings on this computer
std::string GetSettingsPath()
{
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        // save in appdata so it persists between sessions
        std::string settingsDir = std::string(path) + "\\LinuxTerminal";
        CreateDirectoryA(settingsDir.c_str(), NULL);  // make dir if it doesn't exist
        return settingsDir + "\\settings.cfg";
    }
    return "settings.cfg";  // fallback to current dir
}

// save all the user preferences to a file
void SaveSettings()
{
    std::ofstream file(GetSettingsPath());
    if (file.is_open())
    {
        // write all the settings as key=value pairs
        file << "blur=" << (g_blurEnabled ? "1" : "0") << std::endl;
        file << "timestamp=" << (g_showTimestamp ? "1" : "0") << std::endl;

        file << "caret_r=" << g_caretColor.x << std::endl;
        file << "caret_g=" << g_caretColor.y << std::endl;
        file << "caret_b=" << g_caretColor.z << std::endl;
        file << "bg_r=" << g_bgColor.x << std::endl;
        file << "bg_g=" << g_bgColor.y << std::endl;
        file << "bg_b=" << g_bgColor.z << std::endl;
        file << "text_r=" << g_textColor.x << std::endl;
        file << "text_g=" << g_textColor.y << std::endl;
        file << "text_b=" << g_textColor.z << std::endl;
        file << "blur_tint_r=" << g_blurTintColor.x << std::endl;
        file << "blur_tint_g=" << g_blurTintColor.y << std::endl;
        file << "blur_tint_b=" << g_blurTintColor.z << std::endl;
        file << "blur_tint_a=" << g_blurTintColor.w << std::endl;
        file << "font_size=" << g_fontSize << std::endl;
        file << "caret_anim_speed=" << g_caretAnimSpeed << std::endl;
        file << "cursor_trail=" << (g_cursorTrailEnabled ? "1" : "0") << std::endl;
        file.close();
    }
}

// load settings from the file we saved earlier
void LoadSettings()
{
    std::ifstream file(GetSettingsPath());
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            size_t pos = line.find('=');
            if (pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // parse each setting back into the globals
                if (key == "blur") g_blurEnabled = (value == "1");
                else if (key == "timestamp") g_showTimestamp = (value == "1");

                else if (key == "caret_r") g_caretColor.x = std::stof(value);
                else if (key == "caret_g") g_caretColor.y = std::stof(value);
                else if (key == "caret_b") g_caretColor.z = std::stof(value);
                else if (key == "bg_r") g_bgColor.x = std::stof(value);
                else if (key == "bg_g") g_bgColor.y = std::stof(value);
                else if (key == "bg_b") g_bgColor.z = std::stof(value);
                else if (key == "text_r") g_textColor.x = std::stof(value);
                else if (key == "text_g") g_textColor.y = std::stof(value);
                else if (key == "text_b") g_textColor.z = std::stof(value);
                else if (key == "blur_tint_r") g_blurTintColor.x = std::stof(value);
                else if (key == "blur_tint_g") g_blurTintColor.y = std::stof(value);
                else if (key == "blur_tint_b") g_blurTintColor.z = std::stof(value);
                else if (key == "blur_tint_a") g_blurTintColor.w = std::stof(value);
                else if (key == "font_size") g_fontSize = std::stof(value);
                else if (key == "caret_anim_speed") g_caretAnimSpeed = std::stof(value);
                else if (key == "cursor_trail") g_cursorTrailEnabled = (value == "1");
            }
        }
        file.close();
    }
}

// main entry point - this is where the program starts
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // load saved settings first
    LoadSettings();

    // create the main window - layered so we can do transparency
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGuiTerminal", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(
        WS_EX_LAYERED,
        wc.lpszClassName,
        L"Terminal",
        WS_POPUP,  // no borders or title bar - we draw our own
        100, 100, 1000, 600,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // make the window transparent
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // initialize directx for rendering
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // apply that nice blur effect to the window background
    RenderBlur(hwnd);

    // setup imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;

    // load a nicer font
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);

    // setup the visual style - clean modern look
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;  // rounded corners
    style.FrameRounding = 6.0f;
    style.WindowBorderSize = 0.0f;  // no borders
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(8, 8);

    // set up the color scheme
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.1f, 0.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.6f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.22f, 0.7f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.27f, 0.8f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);

    // hook up imgui to windows and directx
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // apply font size from settings
    ImGui::GetIO().FontGlobalScale = g_fontSize / 16.0f;

    // show the welcome message in the first pane
    g_panes[0].outputLines.push_back("Linux Terminal v2.0 - by @ducky6163");
    g_panes[0].outputLines.push_back("Type '$help' for custom commands, 'help' for Windows commands");
    g_panes[0].outputLines.push_back("Type 'settings' to configure terminal options");
    g_panes[0].outputLines.push_back("Click the + button to open another terminal window");
    g_panes[0].outputLines.push_back("");
    
    // pane 0 is active by default
    g_panes[0].isActive = true;
    g_panes[1].isActive = false;

    // main loop - keeps running until user closes
    bool done = false;
    while (!done)
    {
        // handle windows messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // start a new imgui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // create fullscreen window that covers everything
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("Terminal", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();

        // draw the background - either blur or solid color
        if (g_blurEnabled)
        {
            // use the blur tint color they picked in settings
            int tint_r = (int)(g_blurTintColor.x * 255);
            int tint_g = (int)(g_blurTintColor.y * 255);
            int tint_b = (int)(g_blurTintColor.z * 255);
            int tint_a = (int)(g_blurTintColor.w * 255);
            draw_list->AddRectFilled(
                window_pos,
                ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
                IM_COL32(tint_r, tint_g, tint_b, tint_a),
                10.0f
            );
        }
        else
        {
            // solid background when blur is turned off
            int bg_r = (int)(g_bgColor.x * 255);
            int bg_g = (int)(g_bgColor.y * 255);
            int bg_b = (int)(g_bgColor.z * 255);
            draw_list->AddRectFilled(
                window_pos,
                ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
                IM_COL32(bg_r, bg_g, bg_b, 255),
                10.0f
            );
        }

        // draw the title bar at the top
        draw_list->AddRectFilled(
            window_pos,
            ImVec2(window_pos.x + window_size.x, window_pos.y + 45),
            IM_COL32(18, 20, 24, 200),
            12.0f,
            ImDrawFlags_RoundCornersTop
        );

        // show the title text
        draw_list->AddText(
            ImVec2(window_pos.x + 20, window_pos.y + 14),
            IM_COL32(235, 235, 235, 255),
            "Linux Terminal"
        );

        // window buttons - new window, minimize, maximize, close
        float button_y = window_pos.y + 14;
        float button_x = window_pos.x + window_size.x - 25;
        float button_spacing = 28.0f;

        ImVec2 mouse_pos = ImGui::GetMousePos();
        static float hover_states[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // tracks hover state for each button
        
        // helper to draw those glowing buttons in the title bar
        auto DrawGlowButton = [&](float x, float y, ImU32 base_color, ImU32 hover_color, bool& hovered, int button_idx) {
            ImVec2 center(x, y);
            float dist_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
            bool is_hovering = dist_sq <= 100;
            hovered = is_hovering;
            
            // smooth animation when hovering
            float target_hover = is_hovering ? 1.0f : 0.0f;
            hover_states[button_idx] += (target_hover - hover_states[button_idx]) * 12.0f * io.DeltaTime;
            float hover_t = hover_states[button_idx];

            // blend between normal and hover colors
            int base_r = (base_color >> 0) & 0xFF;
            int base_g = (base_color >> 8) & 0xFF;
            int base_b = (base_color >> 16) & 0xFF;
            int hover_r = (hover_color >> 0) & 0xFF;
            int hover_g = (hover_color >> 8) & 0xFF;
            int hover_b = (hover_color >> 16) & 0xFF;
            
            int r = base_r + (int)((hover_r - base_r) * hover_t);
            int g = base_g + (int)((hover_g - base_g) * hover_t);
            int b = base_b + (int)((hover_b - base_b) * hover_t);

            // draw that nice glow effect around the button
            const float base_glow_radius = 6.0f;
            const float hover_glow_radius = 16.0f;
            float glow_radius = base_glow_radius + (hover_glow_radius - base_glow_radius) * hover_t;
            
            // multiple layers for the glow - makes it look soft
            const int glow_layers = 8;
            for (int i = glow_layers; i >= 0; i--) {
                float t = (float)i / glow_layers;
                float radius = 8 + t * glow_radius;
                
                // fade out as we go further from the center
                float alpha_f = expf(-t * t * 3.0f) * (0.06f + 0.22f * hover_t);
                int alpha = (int)(alpha_f * 255.0f);
                if (alpha < 2) continue;
                
                draw_list->AddCircleFilled(center, radius, IM_COL32(r, g, b, alpha), 64);
            }

            // draw the actual button circle
            draw_list->AddCircleFilled(center, 8, IM_COL32(r, g, b, 255), 64);
            // add a little shine effect inside
            draw_list->AddCircleFilled(ImVec2(center.x - 2, center.y - 2), 3, IM_COL32(255, 255, 255, 50 + (int)(40 * hover_t)), 32);
            };

        // new window button - opens another terminal
        float newBtnX = button_x - button_spacing * 3;
        float newBtnY = button_y + 8;
        ImVec2 newBtnMin(newBtnX - 10, newBtnY - 10);
        ImVec2 newBtnMax(newBtnX + 10, newBtnY + 10);
        bool newwin_hovered = ImGui::IsMouseHoveringRect(newBtnMin, newBtnMax);
        
        // draw the button as a rounded square
        ImU32 btnCol = newwin_hovered ? IM_COL32(130, 130, 130, 255) : IM_COL32(100, 100, 100, 255);
        draw_list->AddRectFilled(newBtnMin, newBtnMax, btnCol, 4.0f);
        
        // draw the plus icon
        draw_list->AddLine(ImVec2(newBtnX - 4, newBtnY), ImVec2(newBtnX + 4, newBtnY), IM_COL32(255, 255, 255, 255), 2);
        draw_list->AddLine(ImVec2(newBtnX, newBtnY - 4), ImVec2(newBtnX, newBtnY + 4), IM_COL32(255, 255, 255, 255), 2);
        
        // show tooltip on hover
        if (newwin_hovered)
        {
            ImGui::SetTooltip("open another session of the terminal");
        }
        
        // handle the click
        if (newwin_hovered && ImGui::IsMouseClicked(0))
        {
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            ShellExecuteA(NULL, "open", exePath, NULL, NULL, SW_SHOW);
        }

        // minimize button - yellow
        bool min_hovered = false;
        DrawGlowButton(button_x - button_spacing * 2, button_y + 8,
            IM_COL32(220, 220, 60, 200),
            IM_COL32(255, 255, 95, 255),
            min_hovered, 1);
        if (min_hovered && ImGui::IsMouseClicked(0))
            ::ShowWindow(hwnd, SW_MINIMIZE);

        // maximize/restore button - green
        bool max_hovered = false;
        DrawGlowButton(button_x - button_spacing, button_y + 8,
            IM_COL32(60, 220, 60, 200),
            IM_COL32(95, 255, 95, 255),
            max_hovered, 2);
        if (max_hovered && ImGui::IsMouseClicked(0))
        {
            WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
            GetWindowPlacement(hwnd, &wp);
            if (wp.showCmd == SW_MAXIMIZE)
                ShowWindow(hwnd, SW_RESTORE);
            else
                ShowWindow(hwnd, SW_MAXIMIZE);
        }

        // close button - red
        bool close_hovered = false;
        DrawGlowButton(button_x, button_y + 8,
            IM_COL32(220, 60, 60, 200),
            IM_COL32(255, 95, 95, 255),
            close_hovered, 3);
        if (close_hovered && ImGui::IsMouseClicked(0))
            done = true;

        // let user drag the window by the title bar
        if (ImGui::IsMouseDown(0) && mouse_pos.y >= window_pos.y && mouse_pos.y < window_pos.y + 45 &&
            !close_hovered && !max_hovered && !min_hovered && !newwin_hovered)
        {
            if (!g_isDragging)
            {
                g_isDragging = true;
                POINT cursor;
                GetCursorPos(&cursor);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                g_dragOffset.x = cursor.x - rect.left;
                g_dragOffset.y = cursor.y - rect.top;
            }

            POINT cursor;
            GetCursorPos(&cursor);
            SetWindowPos(hwnd, nullptr, cursor.x - g_dragOffset.x, cursor.y - g_dragOffset.y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            g_isDragging = false;
        }

        // now start drawing the actual terminal content
        ImGui::SetCursorPos(ImVec2(20, 55));
        
                // search bar - searches in active pane
        TerminalPane& activePane = g_panes[g_activePane];
        if (g_showSearch)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.16f, 0.18f, 1.0f));
            ImGui::SetNextItemWidth(window_size.x - 180);
            if (ImGui::InputText("##search", g_searchBuffer, sizeof(g_searchBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                // search for matches in active pane
                g_searchResults.clear();
                std::string searchStr = g_searchBuffer;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                
                for (int i = 0; i < (int)activePane.outputLines.size(); i++)
                {
                    std::string lineLower = activePane.outputLines[i];
                    std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);
                    if (lineLower.find(searchStr) != std::string::npos)
                    {
                        g_searchResults.push_back(i);
                    }
                }
                
                if (!g_searchResults.empty())
                {
                    g_currentSearchResult = 0;
                    // scroll to first result
                    ImGui::SetScrollY(g_searchResults[0] * ImGui::GetTextLineHeight());
                }
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            
            if (ImGui::Button("Prev", ImVec2(50, 0)))
            {
                if (!g_searchResults.empty() && g_currentSearchResult > 0)
                {
                    g_currentSearchResult--;
                    ImGui::SetScrollY(g_searchResults[g_currentSearchResult] * ImGui::GetTextLineHeight());
                }
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Next", ImVec2(50, 0)))
            {
                if (!g_searchResults.empty() && g_currentSearchResult < (int)g_searchResults.size() - 1)
                {
                    g_currentSearchResult++;
                    ImGui::SetScrollY(g_searchResults[g_currentSearchResult] * ImGui::GetTextLineHeight());
                }
            }
            ImGui::SameLine();
            
            if (ImGui::Button("X", ImVec2(30, 0)))
            {
                g_showSearch = false;
                g_searchBuffer[0] = '\0';
                g_searchResults.clear();
            }
            
            if (!g_searchResults.empty())
            {
                ImGui::SameLine();
                ImGui::Text("%d/%d", g_currentSearchResult + 1, (int)g_searchResults.size());
            }
            
            ImGui::Spacing();
        }
        
        float contentHeight = g_showSearch ? window_size.y - 105 : window_size.y - 65;
        float paneWidth = window_size.x - 30;
        
        // render single pane
        RenderTerminalPane(0, paneWidth, contentHeight, io);
        
        ImGui::End();
        ImGui::PopStyleVar();

        // clean Settings Modal - No blur, solid colors
        if (g_showSettingsWindow)
        {
            // initialize temp settings on first open
            static bool settingsInitialized = false;
            if (!settingsInitialized)
            {
                g_tempBlurEnabled = g_blurEnabled;
                g_tempShowTimestamp = g_showTimestamp;
                g_tempCaretColor = g_caretColor;
                g_tempBgColor = g_bgColor;
                g_tempTextColor = g_textColor;
                g_tempBlurTintColor = g_blurTintColor;
                g_tempCursorTrailEnabled = g_cursorTrailEnabled;
                settingsInitialized = true;
            }
            
            // update animations based on temp settings
            float dt = io.DeltaTime;
            float speed = 12.0f;
            g_toggleBlurAnim += ((g_tempBlurEnabled ? 1.0f : 0.0f) - g_toggleBlurAnim) * speed * dt;
            g_toggleTsAnim += ((g_tempShowTimestamp ? 1.0f : 0.0f) - g_toggleTsAnim) * speed * dt;
            g_toggleTrailAnim += ((g_tempCursorTrailEnabled ? 1.0f : 0.0f) - g_toggleTrailAnim) * speed * dt;
            
            ImVec2 displaySize = io.DisplaySize;
            float modalW = 480;
            float modalH = 580;
            ImVec2 modalPos((displaySize.x - modalW) / 2, (displaySize.y - modalH) / 2);
            
            ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
            
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.12f, 0.14f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            
            if (!ImGui::Begin("Settings", &g_showSettingsWindow, 
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoTitleBar))
            {
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                continue;
            }
            
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float mx = ImGui::GetWindowPos().x;
            float my = ImGui::GetWindowPos().y;
            float mw = modalW;
            
            // simple close button
            float closeX = mx + mw - 20;
            float closeY = my + 15;
            bool closeHover = ImGui::IsMouseHoveringRect(ImVec2(closeX - 12, closeY - 10), ImVec2(closeX + 12, closeY + 10));
            
            dl->AddRectFilled(ImVec2(closeX - 15, closeY - 10), ImVec2(closeX + 15, closeY + 10), 
                closeHover ? IM_COL32(255, 60, 60, 255) : IM_COL32(200, 50, 50, 255), 3);
            dl->AddText(ImVec2(closeX - 4, closeY - 6), IM_COL32(255, 255, 255, 255), "X");
            
            if (closeHover && ImGui::IsMouseClicked(0)) {
                g_showSettingsWindow = false;
            }
            
            // content area
            float cx = mx + 15;
            float cy = my + 40;
            float cw = mw - 30;
            
            float contentY = 0;
            
            // preview section
            dl->AddText(ImVec2(cx, cy + contentY), IM_COL32(150, 155, 170, 255), "Preview");
            
            ImVec2 prect(cx, cy + contentY + 18);
            float prectW = cw;
            float prectH = 50;
            
            char username[256];
            DWORD usernameLen = 256;
            GetUserNameA(username, &usernameLen);
            
            char computerName[256];
            DWORD computerNameLen = 256;
            GetComputerNameA(computerName, &computerNameLen);
            
            std::string prompt;
            if (g_showTimestamp) {
                prompt = "[14:32:10] ";
            }
            prompt += std::string(username) + "@" + computerName + ":~$ ";
            
            std::string fullCommand = prompt + "echo Hello World";
            
            g_previewTypeTime += dt;
            float typeSpeed = 0.08f;
            float resetDelay = 2.0f;
            
            if (g_previewFullText != fullCommand) {
                g_previewFullText = fullCommand;
                g_previewCharIndex = 0;
                g_previewTypeTime = 0;
            }
            
            if (g_previewCharIndex < (int)fullCommand.length() && g_previewTypeTime > typeSpeed) {
                g_previewCharIndex++;
                g_previewTypeTime = 0;
            } else if (g_previewCharIndex >= (int)fullCommand.length() && g_previewTypeTime > resetDelay) {
                g_previewCharIndex = 0;
                g_previewTypeTime = 0;
            }
            
            g_previewCurrentText = fullCommand.substr(0, g_previewCharIndex);
            
            if (g_tempBlurEnabled) {
                int tint_r = (int)(g_tempBlurTintColor.x * 255);
                int tint_g = (int)(g_tempBlurTintColor.y * 255);
                int tint_b = (int)(g_tempBlurTintColor.z * 255);
                int tint_a = (int)(g_tempBlurTintColor.w * 255);
                dl->AddRectFilled(prect, ImVec2(prect.x + prectW, prect.y + prectH), IM_COL32(25, 28, 35, 240), 8);
                dl->AddRectFilled(prect, ImVec2(prect.x + prectW, prect.y + prectH), IM_COL32(tint_r, tint_g, tint_b, tint_a), 8);
            } else {
                int pbr = (int)(g_tempBgColor.x * 255), pbg = (int)(g_tempBgColor.y * 255), pbb = (int)(g_tempBgColor.z * 255);
                dl->AddRectFilled(prect, ImVec2(prect.x + prectW, prect.y + prectH), IM_COL32(pbr, pbg, pbb, 255), 8);
            }
            dl->AddRect(prect, ImVec2(prect.x + prectW, prect.y + prectH), IM_COL32(50, 55, 65, 255), 8, 0, 1);
            
            int ptr = (int)(g_tempTextColor.x * 255), ptg = (int)(g_tempTextColor.y * 255), ptb = (int)(g_tempTextColor.z * 255);
            float textY = prect.y + 15;
            
            float textStartX = prect.x + 10;
            if (g_tempShowTimestamp) {
                dl->AddText(ImVec2(textStartX, textY), IM_COL32(100, 110, 130, 255), "[14:32:10]");
                textStartX += ImGui::CalcTextSize("[14:32:10] ").x;
            }
            
            dl->AddText(ImVec2(textStartX, textY), IM_COL32(ptr, ptg, ptb, 255), g_previewCurrentText.c_str());
            
            int pcr = (int)(g_tempCaretColor.x * 255), pcg = (int)(g_tempCaretColor.y * 255), pcb = (int)(g_tempCaretColor.z * 255);
            float caretAlpha = 0.85f + 0.15f * sinf(g_previewTypeTime * 3.14159f * g_caretAnimSpeed);
            float targetCaretX = textStartX + ImGui::CalcTextSize(g_previewCurrentText.c_str()).x;
            
            float lerp_speed = 18.0f;
            g_previewSmoothCaretX += (targetCaretX - g_previewSmoothCaretX) * lerp_speed * dt;
            float caretX = g_previewSmoothCaretX;
            
            float caret_width = 3.0f;
            float line_height = 14.0f;
            float padding = 2.0f;
            float caret_height = line_height + padding * 2.0f;
            float caret_top = textY - padding;
            float caret_bottom = caret_top + caret_height;
            float caret_center_y = caret_top + caret_height * 0.5f;
            float radius = caret_width * 0.5f;
            float center_x = caretX + radius;
            
            if (g_tempCursorTrailEnabled) {
                for (int i = 5; i >= 0; i--) {
                    float trail_t = (float)i / 5.0f;
                    float trail_x = caretX - (i * 4.0f);
                    if (trail_x < textStartX) continue;
                    
                    float trail_alpha = (1.0f - trail_t) * 0.4f * caretAlpha;
                    float trail_radius = radius * (1.0f - trail_t * 0.5f);
                    
                    dl->AddCircleFilled(
                        ImVec2(trail_x + radius, caret_center_y),
                        trail_radius,
                        IM_COL32(pcr, pcg, pcb, (int)(trail_alpha * 255)),
                        12
                    );
                }
            }
            
            for (int i = 6; i >= 0; i--) {
                float t = (float)i / 6.0f;
                float glow_r = t * 4.0f;
                
                float alpha_f = expf(-t * t * 4.0f) * 0.15f * caretAlpha;
                int alpha = (int)(alpha_f * 255.0f);
                if (alpha < 1) continue;
                
                float x1 = center_x - caret_width * 0.5f - glow_r;
                float y1 = caret_center_y - caret_height * 0.5f - glow_r;
                float x2 = center_x + caret_width * 0.5f + glow_r;
                float y2 = caret_center_y + caret_height * 0.5f + glow_r;
                
                dl->AddRectFilled(
                    ImVec2(x1, y1), ImVec2(x2, y2),
                    IM_COL32(pcr, pcg, pcb, alpha),
                    glow_r + 2.0f
                );
            }
            
            dl->AddRectFilled(
                ImVec2(caretX, caret_top + radius),
                ImVec2(caretX + caret_width, caret_bottom - radius),
                IM_COL32(pcr, pcg, pcb, (int)(caretAlpha * 255)),
                0.0f
            );
            
            dl->AddCircleFilled(
                ImVec2(center_x, caret_top + radius),
                radius,
                IM_COL32(pcr, pcg, pcb, (int)(caretAlpha * 255)),
                24
            );
            
            dl->AddCircleFilled(
                ImVec2(center_x, caret_bottom - radius),
                radius,
                IM_COL32(pcr, pcg, pcb, (int)(caretAlpha * 255)),
                24
            );
            
            // colors section
            float colorsY = contentY + 75;
            dl->AddText(ImVec2(cx, cy + colorsY), IM_COL32(150, 155, 170, 255), "Colors");
            
            auto drawColorBtn = [&](float x, float y, const char* label, ImVec4& color, const char* id) {
                ImGui::PushID(id);
                int r = (int)(color.x * 255), g = (int)(color.y * 255), b = (int)(color.z * 255);
                float btnW = 90, btnH = 55;
                
                dl->AddRectFilled(ImVec2(cx + x, cy + y), ImVec2(cx + x + btnW, cy + y + btnH), IM_COL32(30, 33, 38, 255), 6);
                dl->AddRect(ImVec2(cx + x, cy + y), ImVec2(cx + x + btnW, cy + y + btnH), IM_COL32(55, 60, 70, 255), 6, 0, 1);
                
                dl->AddCircleFilled(ImVec2(cx + x + btnW/2, cy + y + 22), 14, IM_COL32(r, g, b, 255), 24);
                dl->AddCircle(ImVec2(cx + x + btnW/2, cy + y + 22), 14, IM_COL32(80, 85, 95, 255), 24, 2);
                
                dl->AddText(ImVec2(cx + x + btnW/2 - ImGui::CalcTextSize(label).x / 2, cy + y + 40), IM_COL32(200, 205, 215, 255), label);
                
                ImGui::SetCursorScreenPos(ImVec2(cx + x, cy + y));
                if (ImGui::InvisibleButton("##btn", ImVec2(btnW, btnH)))
                    ImGui::OpenPopup("Picker");
                
                if (ImGui::BeginPopup("Picker"))
                {
                    ImGui::Text("%s", label);
                    ImGui::Separator();
                    if (ImGui::ColorPicker3("##col", (float*)&color, ImGuiColorEditFlags_DisplayRGB))
                        g_settingsChanged = true;
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            };
            
            float btnY = colorsY + 20;
            drawColorBtn(0, btnY, "Caret", g_tempCaretColor, "c1");
            drawColorBtn(95, btnY, "Background", g_tempBgColor, "c2");
            drawColorBtn(190, btnY, "Text", g_tempTextColor, "c3");
            
            {
                ImGui::PushID("c4");
                float x = cx + 285, y = cy + btnY;
                float btnW = 90, btnH = 55;
                int r = (int)(g_tempBlurTintColor.x * 255), g = (int)(g_tempBlurTintColor.y * 255), b = (int)(g_tempBlurTintColor.z * 255);
                
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + btnW, y + btnH), IM_COL32(30, 33, 38, 255), 6);
                dl->AddRect(ImVec2(x, y), ImVec2(x + btnW, y + btnH), IM_COL32(55, 60, 70, 255), 6, 0, 1);
                dl->AddCircleFilled(ImVec2(x + btnW/2, y + 22), 14, IM_COL32(r, g, b, 255), 24);
                dl->AddCircle(ImVec2(x + btnW/2, y + 22), 14, IM_COL32(80, 85, 95, 255), 24, 2);
                dl->AddText(ImVec2(x + btnW/2 - ImGui::CalcTextSize("Blur Tint").x / 2, y + 40), IM_COL32(200, 205, 215, 255), "Blur Tint");
                
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                if (ImGui::InvisibleButton("##btn", ImVec2(btnW, btnH)))
                    ImGui::OpenPopup("Picker");
                
                if (ImGui::BeginPopup("Picker"))
                {
                    ImGui::Text("Blur Tint");
                    ImGui::Separator();
                    if (ImGui::ColorEdit4("##col", (float*)&g_tempBlurTintColor, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
                        g_settingsChanged = true;
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            
            // toggles section
            float toggleY = btnY + 70;
            dl->AddText(ImVec2(cx, cy + toggleY), IM_COL32(150, 155, 170, 255), "Options");
            
            auto drawToggle = [&](float y, const char* label, float& anim, bool& val, const char* id) {
                ImGui::PushID(id);
                float rowW = cw;
                float rowH = 36;
                
                dl->AddRectFilled(ImVec2(cx, cy + y), ImVec2(cx + rowW, cy + y + rowH), IM_COL32(28, 31, 36, 255), 5);
                
                dl->AddText(ImVec2(cx + 35, cy + y + 13), IM_COL32(220, 225, 235, 255), label);
                
                float tw = 44, th = 22;
                float tx = cx + 20 + rowW - tw - 15;
                float ty = cy + y + 11;
                
                float target = val ? 1.0f : 0.0f;
                anim += (target - anim) * 15.0f * dt;
                
                int tr = (int)(55 + 45 * anim);
                int tg = (int)(60 + 100 * anim);
                int tb = (int)(70 + 30 * anim);
                dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tw, ty + th), IM_COL32(tr, tg, tb, 255), th/2);
                
                float kx = tx + 2 + (tw - th - 2) * anim;
                dl->AddCircleFilled(ImVec2(kx + th/2, ty + th/2), th/2 - 3, IM_COL32(255, 255, 255, 255), 16);
                
                ImGui::SetCursorScreenPos(ImVec2(cx, cy + y));
                if (ImGui::InvisibleButton("##t", ImVec2(rowW, rowH))) {
                    val = !val;
                    g_settingsChanged = true;
                }
                ImGui::PopID();
            };
            
            drawToggle(toggleY + 18, "Glass blur", g_toggleBlurAnim, g_tempBlurEnabled, "t1");
            drawToggle(toggleY + 56, "Show timestamps", g_toggleTsAnim, g_tempShowTimestamp, "t2");
            drawToggle(toggleY + 94, "Cursor trail", g_toggleTrailAnim, g_tempCursorTrailEnabled, "t3");
            
            // sliders section
            float sliderY = toggleY + 140;
            dl->AddText(ImVec2(cx, cy + sliderY), IM_COL32(200, 205, 215, 255), "Caret speed");
            ImGui::SetCursorScreenPos(ImVec2(cx, cy + sliderY + 16));
            ImGui::PushItemWidth(cw);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.22f, 0.25f, 1.0f));
            ImGui::SliderFloat("##speed", &g_caretAnimSpeed, 0.5f, 5.0f, "%.1fx");
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            
            dl->AddText(ImVec2(cx, cy + sliderY + 42), IM_COL32(200, 205, 215, 255), "Font size");
            ImGui::SetCursorScreenPos(ImVec2(cx, cy + sliderY + 58));
            ImGui::PushItemWidth(cw);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.22f, 0.25f, 1.0f));
            float prevSize = g_fontSize;
            ImGui::SliderFloat("##fontsize", &g_fontSize, 10.0f, 24.0f, "%.0fpt");
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            if (prevSize != g_fontSize) ImGui::GetIO().FontGlobalScale = g_fontSize / 16.0f;
            
            // save and Reset buttons
            float buttonRowY = sliderY + 90;
            float btnW = 100, btnH = 32;
            float spacing = 20;
            float totalWidth = btnW * 2 + spacing;
            float startX = cx + (cw - totalWidth) / 2;
            
            bool saveHover = ImGui::IsMouseHoveringRect(ImVec2(startX, cy + buttonRowY), ImVec2(startX + btnW, cy + buttonRowY + btnH));
            dl->AddRectFilled(ImVec2(startX, cy + buttonRowY), ImVec2(startX + btnW, cy + buttonRowY + btnH), 
                saveHover ? IM_COL32(60, 180, 100, 255) : IM_COL32(50, 150, 80, 255), 6);
            dl->AddRect(ImVec2(startX, cy + buttonRowY), ImVec2(startX + btnW, cy + buttonRowY + btnH), 
                IM_COL32(80, 200, 120, 255), 6, 0, 2);
            dl->AddText(ImVec2(startX + (btnW - ImGui::CalcTextSize("Save").x) / 2, cy + buttonRowY + 9), 
                IM_COL32(255, 255, 255, 255), "Save");
            
            ImGui::SetCursorScreenPos(ImVec2(startX, cy + buttonRowY));
            if (ImGui::InvisibleButton("##save", ImVec2(btnW, btnH))) {
                g_blurEnabled = g_tempBlurEnabled;
                g_showTimestamp = g_tempShowTimestamp;
                g_caretColor = g_tempCaretColor;
                g_bgColor = g_tempBgColor;
                g_textColor = g_tempTextColor;
                g_blurTintColor = g_tempBlurTintColor;
                g_cursorTrailEnabled = g_tempCursorTrailEnabled;
                g_toggleBlurAnim = g_tempBlurEnabled ? 1.0f : 0.0f;
                g_toggleTsAnim = g_tempShowTimestamp ? 1.0f : 0.0f;
                g_toggleTrailAnim = g_tempCursorTrailEnabled ? 1.0f : 0.0f;
                SaveSettings();
                g_settingsChanged = false;
            }
            
            float resetX = startX + btnW + spacing;
            bool resetHover = ImGui::IsMouseHoveringRect(ImVec2(resetX, cy + buttonRowY), ImVec2(resetX + btnW, cy + buttonRowY + btnH));
            dl->AddRectFilled(ImVec2(resetX, cy + buttonRowY), ImVec2(resetX + btnW, cy + buttonRowY + btnH), 
                resetHover ? IM_COL32(200, 60, 60, 255) : IM_COL32(170, 55, 55, 255), 6);
            dl->AddRect(ImVec2(resetX, cy + buttonRowY), ImVec2(resetX + btnW, cy + buttonRowY + btnH), 
                IM_COL32(240, 90, 90, 255), 6, 0, 2);
            dl->AddText(ImVec2(resetX + (btnW - ImGui::CalcTextSize("Reset").x) / 2, cy + buttonRowY + 9), 
                IM_COL32(255, 255, 255, 255), "Reset");
            
            ImGui::SetCursorScreenPos(ImVec2(resetX, cy + buttonRowY));
            if (ImGui::InvisibleButton("##reset", ImVec2(btnW, btnH))) {
                g_tempCaretColor = ImVec4(0.196f, 1.0f, 0.392f, 1.0f);
                g_tempBgColor = ImVec4(0.02f, 0.02f, 0.031f, 1.0f);
                g_tempTextColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
                g_tempBlurTintColor = ImVec4(0.15f, 0.17f, 0.22f, 0.3f);
                g_tempBlurEnabled = true;
                g_tempShowTimestamp = false;
                g_tempCursorTrailEnabled = false;
                g_toggleBlurAnim = 1.0f;
                g_toggleTsAnim = 0.0f;
                g_toggleTrailAnim = 0.0f;
                g_settingsChanged = true;
            }
            
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        // rendering
        ImGui::Render();
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// render a single terminal pane
void RenderTerminalPane(int paneIdx, float width, float height, ImGuiIO& io)
{
    TerminalPane& pane = g_panes[paneIdx];
    bool isActive = (g_activePane == paneIdx);
    
    // create unique IDs for this pane
    std::string childId = "Content" + std::to_string(paneIdx);
    std::string outputId = "Output" + std::to_string(paneIdx);
    std::string inputId = "##inputarea" + std::to_string(paneIdx);
    
    ImGui::BeginChild(childId.c_str(), ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar);
    
    // draw active indicator border if this is the active pane
    if (false && isActive)
    {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(
            ImVec2(pos.x - 2, pos.y - 2),
            ImVec2(pos.x + size.x + 2, pos.y + size.y + 2),
            IM_COL32(60, 220, 100, 200),
            4.0f,
            0,
            2.0f
        );
    }
    
    // output area
    float outputHeight = height - 35;
    ImGui::BeginChild(outputId.c_str(), ImVec2(0, outputHeight), false);
    ImGui::PushStyleColor(ImGuiCol_Text, g_textColor);
    
    for (const auto& line : pane.outputLines)
    {
        ImGui::TextWrapped("%s", line.c_str());
        
        // right-click context menu
        if (ImGui::BeginPopupContextItem(("line_ctx_" + std::to_string(paneIdx) + "_" + std::to_string(reinterpret_cast<uintptr_t>(&line))).c_str()))
        {
            if (ImGui::MenuItem("Copy Line"))
            {
                ImGui::SetClipboardText(line.c_str());
            }
            if (ImGui::MenuItem("Copy All Output"))
            {
                std::string allOutput;
                for (const auto& l : pane.outputLines)
                {
                    allOutput += l + "\n";
                }
                ImGui::SetClipboardText(allOutput.c_str());
            }
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleColor();
    
    // context menu for empty space
    if (ImGui::BeginPopupContextWindow(("OutputContext" + std::to_string(paneIdx)).c_str()))
    {
        if (ImGui::MenuItem("Copy All Output"))
        {
            std::string allOutput;
            for (const auto& l : pane.outputLines)
            {
                allOutput += l + "\n";
            }
            ImGui::SetClipboardText(allOutput.c_str());
        }
        if (ImGui::MenuItem("Clear Output"))
        {
            pane.outputLines.clear();
        }
        ImGui::EndPopup();
    }
    
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    
    // input area
    char username[256];
    DWORD usernameLen = 256;
    GetUserNameA(username, &usernameLen);
    char computerName[256];
    DWORD computerNameLen = 256;
    GetComputerNameA(computerName, &computerNameLen);
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
    ImGui::Text("%s@%s:~$ ", username, computerName);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    // update caret animation
    pane.caretTime += io.DeltaTime;
    
    // get input area info
    ImVec2 input_pos = ImGui::GetCursorScreenPos();
    ImVec2 input_size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
    
    // create invisible button to capture focus and handle clicks
    ImGui::InvisibleButton(inputId.c_str(), input_size);
    bool item_focused = ImGui::IsItemActive() || ImGui::IsItemFocused();
    
    // handle clicking on this pane to activate it (disabled for now)
    if (false && ImGui::IsItemClicked() && !isActive)
    {
        g_activePane = paneIdx;
        g_panes[0].isActive = (g_activePane == 0);
        g_panes[1].isActive = (g_activePane == 1);
    }
    
    // draw the input text
    std::string display_text(pane.inputBuffer);
    ImGui::GetWindowDrawList()->AddText(
        input_pos,
        IM_COL32(230, 230, 230, 255),
        display_text.c_str()
    );
    
    // handle keyboard input for this pane
    if (item_focused && isActive)
    {
        // handle text input
        if (io.InputQueueCharacters.Size > 0)
        {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++)
            {
                ImWchar c = io.InputQueueCharacters[i];
                if (c == '\r' || c == '\n')
                {
                    std::string cmd(pane.inputBuffer);
                    if (!cmd.empty())
                    {
                        char outUsername[256];
                        DWORD outUsernameLen = 256;
                        GetUserNameA(outUsername, &outUsernameLen);
                        char outComputerName[256];
                        DWORD outComputerNameLen = 256;
                        GetComputerNameA(outComputerName, &outComputerNameLen);
                        
                        std::string fullLine = std::string(outUsername) + "@" + outComputerName + ":~$ " + cmd;
                        if (g_showTimestamp)
                        {
                            SYSTEMTIME st;
                            GetLocalTime(&st);
                            char timestamp[32];
                            sprintf_s(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
                            fullLine = std::string(timestamp) + fullLine;
                        }
                        pane.outputLines.push_back(fullLine);
                        
                        int prevActive = g_activePane;
                        g_activePane = paneIdx;
                        ProcessCommand(cmd);
                        g_activePane = prevActive;
                        
                        if (pane.commandHistory.empty() || pane.commandHistory.back() != cmd)
                        {
                            pane.commandHistory.push_back(cmd);
                            if (pane.commandHistory.size() > g_maxHistorySize)
                                pane.commandHistory.erase(pane.commandHistory.begin());
                        }
                        pane.historyIndex = -1;
                        
                        pane.inputBuffer[0] = '\0';
                        pane.caretPos = 0;
                    }
                }
                else if (c >= 32 && c < 128)
                {
                    int len = strlen(pane.inputBuffer);
                    if (len < 255)
                    {
                        memmove(&pane.inputBuffer[pane.caretPos + 1], &pane.inputBuffer[pane.caretPos], len - pane.caretPos + 1);
                        pane.inputBuffer[pane.caretPos] = (char)c;
                        pane.caretPos++;
                        pane.caretTime = 0.0f;
                    }
                }
            }
        }
        
        // handle special keys
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && pane.caretPos > 0)
        {
            int len = strlen(pane.inputBuffer);
            memmove(&pane.inputBuffer[pane.caretPos - 1], &pane.inputBuffer[pane.caretPos], len - pane.caretPos + 1);
            pane.caretPos--;
            pane.caretTime = 0.0f;
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            int len = strlen(pane.inputBuffer);
            if (pane.caretPos < len)
            {
                memmove(&pane.inputBuffer[pane.caretPos], &pane.inputBuffer[pane.caretPos + 1], len - pane.caretPos);
                pane.caretTime = 0.0f;
            }
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && pane.caretPos > 0)
        {
            pane.caretPos--;
            pane.caretTime = 0.0f;
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            int len = strlen(pane.inputBuffer);
            if (pane.caretPos < len)
            {
                pane.caretPos++;
                pane.caretTime = 0.0f;
            }
        }
        
        // handle command history with ctrl+z (back) and ctrl+x (forward)
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            if (!pane.commandHistory.empty() && pane.historyIndex < (int)pane.commandHistory.size() - 1)
            {
                pane.historyIndex++;
                strncpy_s(pane.inputBuffer, pane.commandHistory[pane.commandHistory.size() - 1 - pane.historyIndex].c_str(), sizeof(pane.inputBuffer) - 1);
                pane.caretPos = strlen(pane.inputBuffer);
                pane.caretTime = 0.0f;
            }
        }
        
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_X))
        {
            if (pane.historyIndex > 0)
            {
                pane.historyIndex--;
                strncpy_s(pane.inputBuffer, pane.commandHistory[pane.commandHistory.size() - 1 - pane.historyIndex].c_str(), sizeof(pane.inputBuffer) - 1);
                pane.caretPos = strlen(pane.inputBuffer);
                pane.caretTime = 0.0f;
            }
            else if (pane.historyIndex == 0)
            {
                pane.historyIndex = -1;
                pane.inputBuffer[0] = '\0';
                pane.caretPos = 0;
                pane.caretTime = 0.0f;
            }
        }
        
        // home/end
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            pane.caretPos = 0;
            pane.caretTime = 0.0f;
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_End))
        {
            pane.caretPos = strlen(pane.inputBuffer);
            pane.caretTime = 0.0f;
        }
        
        // autocomplete with tab
        if (ImGui::IsKeyPressed(ImGuiKey_Tab) && g_showSuggestions && !g_suggestions.empty())
        {
            std::string suggestion = (g_selectedSuggestion >= 0 && g_selectedSuggestion < (int)g_suggestions.size()) 
                ? g_suggestions[g_selectedSuggestion] 
                : g_suggestions[0];
            
            int wordStart = pane.caretPos;
            while (wordStart > 0 && pane.inputBuffer[wordStart - 1] != ' ')
                wordStart--;
            
            std::string newText = std::string(pane.inputBuffer, wordStart) + suggestion;
            if (pane.caretPos < (int)strlen(pane.inputBuffer))
                newText += std::string(pane.inputBuffer + pane.caretPos);
            
            strncpy_s(pane.inputBuffer, newText.c_str(), sizeof(pane.inputBuffer) - 1);
            pane.caretPos = wordStart + suggestion.length();
            pane.caretTime = 0.0f;
            g_showSuggestions = false;
        }
        
        // navigate autocomplete suggestions with up/down arrows
        if (g_showSuggestions && !g_suggestions.empty())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                g_selectedSuggestion--;
                if (g_selectedSuggestion < 0) g_selectedSuggestion = (int)g_suggestions.size() - 1;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                g_selectedSuggestion++;
                if (g_selectedSuggestion >= (int)g_suggestions.size()) g_selectedSuggestion = 0;
            }
        }
    }
    
    // calculate caret position and draw it (even when not actively typing)
    ImFont* font = ImGui::GetFont();
    float font_size = ImGui::GetFontSize();
    std::string before_caret = display_text.substr(0, pane.caretPos);
    float target_caret_x = input_pos.x + font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, before_caret.c_str()).x;
    
    // smooth caret movement with lerp
    static float smooth_caret_x = target_caret_x;
    float lerp_speed = 18.0f;
    smooth_caret_x += (target_caret_x - smooth_caret_x) * lerp_speed * io.DeltaTime;
    
    // cursor trail effect
    if (g_cursorTrailEnabled)
    {
        static std::vector<std::pair<ImVec2, float>> cursor_trail;
        ImVec2 current_pos = ImVec2(smooth_caret_x, input_pos.y);
        if (cursor_trail.empty() || 
            std::abs(cursor_trail.back().first.x - current_pos.x) > 1.0f)
        {
            cursor_trail.push_back({current_pos, 1.0f});
            if (cursor_trail.size() > 8) cursor_trail.erase(cursor_trail.begin());
        }
        for (auto& t : cursor_trail) t.second -= io.DeltaTime * 3.0f;
        cursor_trail.erase(std::remove_if(cursor_trail.begin(), cursor_trail.end(), 
            [](auto& t) { return t.second <= 0; }), cursor_trail.end());
        
        int cr = (int)(g_caretColor.x * 255), cg = (int)(g_caretColor.y * 255), cb = (int)(g_caretColor.z * 255);
        for (size_t i = 0; i < cursor_trail.size(); i++)
        {
            float alpha = cursor_trail[i].second * (0.3f + 0.7f * ((float)i / cursor_trail.size()));
            int rad = 2 + (int)(i * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(cursor_trail[i].first.x + 1.5f, cursor_trail[i].first.y + 8),
                rad, IM_COL32(cr, cg, cb, (int)(alpha * 100)), 16);
        }
    }
    
    // draw smooth animated caret
    float caret_alpha = 0.9f + 0.1f * sinf(pane.caretTime * 3.14159f * g_caretAnimSpeed);
    float caret_width = 3.0f;
    float line_height = ImGui::GetTextLineHeight();
    float padding = 2.0f;
    float caret_height = line_height + padding * 2.0f;
    float caret_top = input_pos.y - padding;
    float caret_center_x = smooth_caret_x + caret_width * 0.5f;
    float caret_center_y = caret_top + caret_height * 0.5f;
    
    int cr = (int)(g_caretColor.x * 255);
    int cg = (int)(g_caretColor.y * 255);
    int cb = (int)(g_caretColor.z * 255);
    
    // draw caret glow
    const float glow_radius = 4.0f;
    const int glow_layers = 6;
    for (int i = glow_layers; i >= 0; i--) {
        float t = (float)i / glow_layers;
        float radius = t * glow_radius;
        float alpha_f = expf(-t * t * 4.0f) * 0.15f * caret_alpha;
        int alpha = (int)(alpha_f * 255.0f);
        if (alpha < 1) continue;
        
        float x1 = caret_center_x - caret_width * 0.5f - radius;
        float y1 = caret_center_y - caret_height * 0.5f - radius;
        float x2 = caret_center_x + caret_width * 0.5f + radius;
        float y2 = caret_center_y + caret_height * 0.5f + radius;
        
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(x1, y1), ImVec2(x2, y2),
            IM_COL32(cr, cg, cb, alpha),
            radius + 2.0f
        );
    }
    
    // draw main caret body
    float caret_bottom = caret_top + caret_height;
    float radius = caret_width * 0.5f;
    float center_x = smooth_caret_x + radius;
    
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(smooth_caret_x, caret_top + radius),
        ImVec2(smooth_caret_x + caret_width, caret_bottom - radius),
        IM_COL32(cr, cg, cb, (int)(caret_alpha * 255)),
        0.0f
    );
    
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(center_x, caret_top + radius),
        radius,
        IM_COL32(cr, cg, cb, (int)(caret_alpha * 255)),
        24
    );
    
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(center_x, caret_bottom - radius),
        radius,
        IM_COL32(cr, cg, cb, (int)(caret_alpha * 255)),
        24
    );
    
    // draw autocomplete suggestions dropdown
    if (g_showSuggestions && !g_suggestions.empty())
    {
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize();
        
        float maxTextWidth = 200.0f;
        for (const auto& s : g_suggestions)
        {
            float textWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, s.c_str()).x;
            if (textWidth > maxTextWidth) maxTextWidth = textWidth;
        }
        if (maxTextWidth > 400.0f) maxTextWidth = 400.0f;
        
        float padding = 12.0f;
        float itemHeight = ImGui::GetTextLineHeight() + 8.0f;
        float dropdownWidth = maxTextWidth + padding * 2;
        float maxDropdownHeight = 200.0f;
        float dropdownHeight = (g_suggestions.size() * itemHeight + padding < maxDropdownHeight) ? (g_suggestions.size() * itemHeight + padding) : maxDropdownHeight;
        float hintBarHeight = 20.0f;
        float totalHeight = dropdownHeight + hintBarHeight + 8;
        
        ImVec2 dropdownPos(input_pos.x, input_pos.y - totalHeight - 6);
        ImVec2 hintBarPos(dropdownPos.x, dropdownPos.y);
        
        // hint bar
        draw_list->AddRectFilled(
            hintBarPos,
            ImVec2(hintBarPos.x + dropdownWidth, hintBarPos.y + hintBarHeight),
            IM_COL32(30, 35, 45, 255), 6.0f, ImDrawFlags_RoundCornersTop);
        draw_list->AddRect(
            hintBarPos,
            ImVec2(hintBarPos.x + dropdownWidth, hintBarPos.y + hintBarHeight),
            IM_COL32(60, 70, 85, 255), 6.0f, ImDrawFlags_RoundCornersTop, 1.0f);
        draw_list->AddText(
            ImVec2(hintBarPos.x + 8, hintBarPos.y + 2),
            IM_COL32(180, 180, 180, 255),
            "Tab = Accept  |  \xe2\x86\x91\xe2\x86\x93 = Navigate");
        
        // dropdown
        ImVec2 listPos(dropdownPos.x, hintBarPos.y + hintBarHeight + 4);
        
        draw_list->AddRectFilled(
            ImVec2(listPos.x + 4, listPos.y + 4),
            ImVec2(listPos.x + dropdownWidth + 4, listPos.y + dropdownHeight + 4),
            IM_COL32(0, 0, 0, 100), 8.0f);
        
        draw_list->AddRectFilled(
            listPos,
            ImVec2(listPos.x + dropdownWidth, listPos.y + dropdownHeight),
            IM_COL32(35, 38, 45, 255), 8.0f);
        
        draw_list->AddRect(
            listPos,
            ImVec2(listPos.x + dropdownWidth, listPos.y + dropdownHeight),
            IM_COL32(70, 75, 85, 255), 8.0f, 0, 1.0f);
        
        float visibleItems = (dropdownHeight - padding) / itemHeight;
        int scrollOffset = 0;
        if (g_selectedSuggestion >= (int)visibleItems)
            scrollOffset = g_selectedSuggestion - (int)visibleItems + 1;
        
        float y = listPos.y + padding * 0.5f;
        for (int i = scrollOffset; i < (int)g_suggestions.size() && y < listPos.y + dropdownHeight - padding * 0.5f; i++)
        {
            bool isSelected = (i == g_selectedSuggestion);
            
            if (isSelected)
            {
                draw_list->AddRectFilled(
                    ImVec2(listPos.x + 4, y),
                    ImVec2(listPos.x + dropdownWidth - 4, y + itemHeight),
                    IM_COL32(60, 130, 250, 220), 6.0f);
            }
            
            bool isDir = (g_suggestions[i].length() > 0 && g_suggestions[i].back() == '\\');
            
            if (isDir)
            {
                draw_list->AddRectFilled(
                    ImVec2(listPos.x + padding, y + itemHeight * 0.35f),
                    ImVec2(listPos.x + padding + 10, y + itemHeight * 0.65f),
                    IM_COL32(255, 200, 80, 255), 2.0f);
            }
            else if (g_suggestions[i].find('\\') != std::string::npos || g_suggestions[i].find(':') != std::string::npos)
            {
                draw_list->AddRectFilled(
                    ImVec2(listPos.x + padding, y + itemHeight * 0.3f),
                    ImVec2(listPos.x + padding + 8, y + itemHeight * 0.7f),
                    IM_COL32(200, 200, 200, 255), 1.0f);
            }
            else
            {
                draw_list->AddCircleFilled(
                    ImVec2(listPos.x + padding + 4, y + itemHeight * 0.5f),
                    4.0f, IM_COL32(100, 220, 100, 255));
            }
            
            draw_list->AddText(
                ImVec2(listPos.x + padding + 18, y + itemHeight * 0.15f),
                isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 220, 220, 255),
                g_suggestions[i].c_str());
            
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= listPos.x + 4 && mousePos.x <= listPos.x + dropdownWidth - 4 &&
                mousePos.y >= y && mousePos.y <= y + itemHeight)
            {
                if (!isSelected)
                {
                    draw_list->AddRectFilled(
                        ImVec2(listPos.x + 4, y),
                        ImVec2(listPos.x + dropdownWidth - 4, y + itemHeight),
                        IM_COL32(80, 90, 110, 150), 6.0f);
                }
                
                if (ImGui::IsMouseClicked(0))
                {
                    g_selectedSuggestion = i;
                    std::string suggestion = g_suggestions[g_selectedSuggestion];
                    
                    int wordStart = pane.caretPos;
                    while (wordStart > 0 && pane.inputBuffer[wordStart - 1] != ' ')
                        wordStart--;
                    
                    std::string newText = std::string(pane.inputBuffer, wordStart) + suggestion;
                    if (pane.caretPos < (int)strlen(pane.inputBuffer))
                        newText += std::string(pane.inputBuffer + pane.caretPos);
                    
                    strncpy_s(pane.inputBuffer, newText.c_str(), sizeof(pane.inputBuffer) - 1);
                    pane.caretPos = wordStart + suggestion.length();
                    pane.caretTime = 0.0f;
                    g_showSuggestions = false;
                }
            }
            
            y += itemHeight;
        }
    }
    
    // auto-focus on startup (only for pane 0)
    if (paneIdx == 0 && !ImGui::IsAnyItemActive() && !ImGui::IsAnyItemFocused())
    {
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    // generate autocomplete suggestions
    if (strlen(pane.inputBuffer) > 0)
    {
        int wordStart = pane.caretPos;
        while (wordStart > 0 && pane.inputBuffer[wordStart - 1] != ' ')
            wordStart--;
        std::string currentWord(pane.inputBuffer + wordStart, pane.caretPos - wordStart);
        
        // clear previous suggestions
        g_suggestions.clear();
        
        if (currentWord.length() > 0)
        {
                // check if it's a path (contains \ or : or /)
                bool isPath = (currentWord.find('\\') != std::string::npos || 
                               currentWord.find('/') != std::string::npos || 
                               currentWord.find(':') != std::string::npos);
                
                if (isPath)
                {
                    // directory autocomplete
                    std::string pathToComplete = currentWord;
                    
                    // normalize path separators
                    std::replace(pathToComplete.begin(), pathToComplete.end(), '/', '\\');
                    
                    // extract the directory part and the partial filename
                    std::string dirPath;
                    std::string partialName;
                    
                    size_t lastSlash = pathToComplete.find_last_of('\\');
                    if (lastSlash != std::string::npos)
                    {
                        dirPath = pathToComplete.substr(0, lastSlash + 1);
                        partialName = pathToComplete.substr(lastSlash + 1);
                    }
                    else
                    {
                        // no slash, just a drive letter like "C:"
                        dirPath = pathToComplete;
                        if (dirPath.back() != '\\' && dirPath.back() != ':')
                        {
                            // it's something like "C:Use" - split into "C:" and "Use"
                            size_t colonPos = dirPath.find(':');
                            if (colonPos != std::string::npos && colonPos == dirPath.length() - 1)
                            {
                                dirPath += "\\";
                            }
                        }
                        partialName = "";
                    }
                    
                    // ensure directory ends with backslash
                    if (!dirPath.empty() && dirPath.back() != '\\')
                    {
                        if (dirPath.back() == ':')
                            dirPath += "\\";
                    }
                    
                    // search in the directory
                    std::string searchPath = dirPath + "*";
                    
                    WIN32_FIND_DATAA findData;
                    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                    
                    if (hFind != INVALID_HANDLE_VALUE)
                    {
                        do
                        {
                            std::string name = findData.cFileName;
                            
                            // skip . and ..
                            if (name == "." || name == "..")
                                continue;
                            
                            // check if it starts with partial name (case-insensitive)
                            if (partialName.empty() || _strnicmp(name.c_str(), partialName.c_str(), partialName.length()) == 0)
                            {
                                std::string fullPath = dirPath + name;
                                
                                // add \ for directories
                                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                                {
                                    fullPath += "\\";
                                }
                                
                                // only add if not already in list
                                bool exists = false;
                                for (const auto& s : g_suggestions)
                                {
                                    if (_stricmp(s.c_str(), fullPath.c_str()) == 0)
                                    {
                                        exists = true;
                                        break;
                                    }
                                }
                                if (!exists)
                                    g_suggestions.push_back(fullPath);
                            }
                        } while (FindNextFileA(hFind, &findData) && g_suggestions.size() < 50);
                        
                        FindClose(hFind);
                    }
                }
                else
                {
                    // command autocomplete
                    for (const auto& cmd : g_commonCommands)
                    {
                        if (cmd.length() >= currentWord.length() && 
                            _strnicmp(cmd.c_str(), currentWord.c_str(), currentWord.length()) == 0)
                        {
                            g_suggestions.push_back(cmd);
                        }
                    }
            }
        }
        
        // limit suggestions (allow more for directories)
        if (g_suggestions.size() > 30)
            g_suggestions.resize(30);
        
        g_showSuggestions = !g_suggestions.empty();
        if (g_selectedSuggestion >= (int)g_suggestions.size())
            g_selectedSuggestion = 0;
    }
    else
    {
        g_showSuggestions = false;
        g_suggestions.clear();
    }

    ImGui::EndChild();
}

std::string ExecuteCommand(const std::string& cmd)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return "Error: Failed to create pipe";

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };

    // build command line
    std::string fullCmd = "cmd.exe /c " + cmd;
    char cmdLine[1024];
    strncpy_s(cmdLine, fullCmd.c_str(), sizeof(cmdLine) - 1);
    cmdLine[sizeof(cmdLine) - 1] = '\0';

    BOOL success = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success)
    {
        CloseHandle(hWrite);
        CloseHandle(hRead);
        DWORD error = GetLastError();
        if (error == 2)
            return "'" + cmd + "' is not recognized as an internal or external command";
        return "Error: Failed to execute command (code " + std::to_string(error) + ")";
    }

    CloseHandle(hWrite);

    // read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // trim trailing newlines
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();

    if (output.empty() && exitCode != 0)
        return "'" + cmd + "' is not recognized as an internal or external command";

    return output;
}

void ProcessCommand(const std::string& cmd)
{
    if (cmd == "$help")
    {
        AddOutputLine("Custom Terminal Commands:");
        AddOutputLine("  $help     - Show this custom command list");
        AddOutputLine("  cmds      - Show this command list");
        AddOutputLine("  cls       - Clear terminal screen");
        AddOutputLine("  quit      - Close terminal");
        AddOutputLine("  version   - Show terminal version info");
        AddOutputLine("  system    - Display real system information");
        AddOutputLine("  settings  - Configure terminal (blur, timestamps, etc)");
        AddOutputLine("  time      - Show current date and time");
        AddOutputLine("  <any cmd> - Execute real Windows commands");
        AddOutputLine("");
        AddOutputLine("Keyboard Shortcuts:");
        AddOutputLine("  Ctrl+Z    - Go back in command history");
        AddOutputLine("  Ctrl+X    - Go forward in command history");
        AddOutputLine("  Up/Down   - Navigate autocomplete suggestions");
        AddOutputLine("  Tab       - Accept autocomplete suggestion");
        AddOutputLine("  Ctrl+F    - Toggle search");
    }
    else if (cmd == "cmds")
    {
        AddOutputLine("Custom Terminal Commands:");
        AddOutputLine("  cmds      - Show this command list");
        AddOutputLine("  cls       - Clear terminal screen");
        AddOutputLine("  quit      - Close terminal");
        AddOutputLine("  version   - Show terminal version info");
        AddOutputLine("  system    - Display real system information");
        AddOutputLine("  settings  - Configure terminal (blur, timestamps, etc)");
        AddOutputLine("  time      - Show current date and time");
        AddOutputLine("  <any cmd> - Execute real Windows commands");
    }
    else if (cmd == "cls")
    {
        g_panes[g_activePane].outputLines.clear();
    }
    else if (cmd == "quit")
    {
        PostQuitMessage(0);
    }
    else if (cmd == "version")
    {
        AddOutputLine("Linux Terminal v2.0");
        AddOutputLine("Built with ImGui + DirectX11");
        AddOutputLine("Author: @ducky6163");
    }
    else if (cmd == "system")
    {
        AddOutputLine("    ___    ");
        AddOutputLine("   (o o)   Fetching system information...");
        AddOutputLine("  (  >  )  ");
        AddOutputLine("   -----   ");
        AddOutputLine("");
        
        // oS Info - read from registry (GetVersionEx is deprecated)
        HKEY hKeyOS;
        DWORD majorVersion = 0, minorVersion = 0, buildNumber = 0;
        DWORD dwSize = sizeof(DWORD);
        
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKeyOS) == ERROR_SUCCESS)
        {
            RegQueryValueExA(hKeyOS, "CurrentMajorVersionNumber", NULL, NULL, (LPBYTE)&majorVersion, &dwSize);
            RegQueryValueExA(hKeyOS, "CurrentMinorVersionNumber", NULL, NULL, (LPBYTE)&minorVersion, &dwSize);
            RegQueryValueExA(hKeyOS, "CurrentBuildNumber", NULL, NULL, (LPBYTE)&buildNumber, &dwSize);
            RegCloseKey(hKeyOS);
        }
        
        char info[512];
        if (majorVersion > 0)
            sprintf_s(info, "OS: Windows %lu.%lu (Build %lu)", majorVersion, minorVersion, buildNumber);
        else
            sprintf_s(info, "OS: Windows (Version info unavailable)");
        AddOutputLine(info);
        
        // cPU Info
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        sprintf_s(info, "CPU: %lu cores, Architecture: %s", 
            sysInfo.dwNumberOfProcessors,
            sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "x86");
        AddOutputLine(info);
        
        // rAM Info
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memStatus);
        sprintf_s(info, "RAM: %llu MB / %llu MB (%llu%% used)",
            (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024),
            memStatus.ullTotalPhys / (1024 * 1024),
            memStatus.dwMemoryLoad);
        AddOutputLine(info);
        
        // gPU Info from registry
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Video", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            AddOutputLine("GPU: (Check Device Manager for details)");
            RegCloseKey(hKey);
        }
        
        // uptime
        DWORD uptime = GetTickCount() / 1000;
        DWORD hours = uptime / 3600;
        DWORD minutes = (uptime % 3600) / 60;
        sprintf_s(info, "Uptime: %luh %lum", hours, minutes);
        AddOutputLine(info);
    }
    else if (cmd == "settings")
    {
        g_showSettingsWindow = true;
        AddOutputLine("Settings window opened!");
        AddOutputLine("You can also use: settings <option> <on/off>");
    }
    else if (cmd.substr(0, 9) == "settings ")
    {
        std::string args = cmd.substr(9);
        size_t spacePos = args.find(' ');
        if (spacePos != std::string::npos)
        {
            std::string setting = args.substr(0, spacePos);
            std::string value = args.substr(spacePos + 1);
            
            bool enable = (value == "on" || value == "1" || value == "true");
            
            if (setting == "blur")
            {
                g_blurEnabled = enable;
                SaveSettings();
                AddOutputLine(std::string("Blur background set to: ") + (enable ? "ON" : "OFF"));
                AddOutputLine("Restart terminal to apply changes.");
            }
            else if (setting == "timestamp")
            {
                g_showTimestamp = enable;
                SaveSettings();
                AddOutputLine(std::string("Timestamps set to: ") + (enable ? "ON" : "OFF"));
            }
            else
            {
                AddOutputLine("Unknown setting: " + setting);
                AddOutputLine("Type 'settings' for available options.");
            }
        }
        else
        {
            AddOutputLine("Usage: settings <option> <on/off>");
            AddOutputLine("Type 'settings' to see available options.");
        }
    }
    else if (cmd == "time")
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeStr[256];
        sprintf_s(timeStr, "Date: %02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);
        AddOutputLine(timeStr);
        sprintf_s(timeStr, "Time: %02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
        AddOutputLine(timeStr);
    }
    else if (cmd.substr(0, 3) == "cd ")
    {
        // handle cd command specially - it's a shell builtin
        std::string path = cmd.substr(3);
        // remove quotes if present
        if (!path.empty() && path[0] == '"' && path.back() == '"')
        {
            path = path.substr(1, path.length() - 2);
        }
        
        if (SetCurrentDirectoryA(path.c_str()))
        {
            // update current directory display for active pane
            char currentPath[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, currentPath);
            g_panes[g_activePane].currentDir = currentPath;
        }
        else
        {
            AddOutputLine("cd: " + path + ": No such file or directory");
        }
    }
    else if (cmd == "cd")
    {
        // just cd with no args - show current directory
        AddOutputLine(g_panes[g_activePane].currentDir);
    }
    else
    {
        // execute real cmd command and display output
        std::string result = ExecuteCommand(cmd);
        
        // split output into lines
        std::istringstream stream(result);
        std::string line;
        while (std::getline(stream, line))
        {
            // remove carriage returns
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            if (!line.empty())
                AddOutputLine(line);
        }
    }
    AddOutputLine("");
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // changed to BGRA for transparency
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void RenderBlur(HWND hwnd)
{
    if (g_blurEnabled)
    {
        // method 1: DWM Blur (most compatible)
        DWM_BLURBEHIND bb = { 0 };
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = TRUE;
        bb.hRgnBlur = NULL;
        DwmEnableBlurBehindWindow(hwnd, &bb);

        // method 2: Extend frame into client area
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        // method 3: Windows composition attribute
        struct ACCENTPOLICY {
            int AccentState;
            int AccentFlags;
            int GradientColor;
            int AnimationId;
        };
        struct WINCOMPATTRDATA {
            int Attrib;
            PVOID pvData;
            ULONG cbData;
        };

        const HINSTANCE hm = LoadLibraryW(L"user32.dll");
        if (hm) {
            typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);
            const pSetWindowCompositionAttribute SetWindowCompositionAttribute =
                (pSetWindowCompositionAttribute)GetProcAddress(hm, "SetWindowCompositionAttribute");
            if (SetWindowCompositionAttribute) {
                ACCENTPOLICY policy = { 3, 2, 0x01000000, 0 }; // state 3 = blur
                WINCOMPATTRDATA data = { 19, &policy, sizeof(ACCENTPOLICY) };
                SetWindowCompositionAttribute(hwnd, &data);
            }
            FreeLibrary(hm);
        }
    }
    else
    {
        // disable blur - solid window
        DWM_BLURBEHIND bb = { 0 };
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        
        // reset margins
        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
