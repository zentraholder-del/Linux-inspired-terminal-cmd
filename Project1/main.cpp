#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>

// DirectX11 globals
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Terminal state
static std::vector<std::string> g_outputLines;
static char g_inputBuffer[256] = "";
static std::string g_currentDir = "C:\\Users\\User";

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void ProcessCommand(const std::string& cmd);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window dragging
static bool g_isDragging = false;
static POINT g_dragOffset = {0, 0};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Create window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGuiTerminal", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Terminal",
        WS_POPUP,
        100, 100, 1000, 600,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;

    // Setup style - DARK THEME WITH TRANSPARENCY
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 5.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0, 0);
    
    // Semi-transparent dark colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.1f, 0.92f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.9f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.22f, 0.9f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.27f, 0.9f);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Initial output
    g_outputLines.push_back("Terminal v1.0 - Type 'help' for commands");
    g_outputLines.push_back("");

    // Main loop
    bool done = false;
    while (!done)
    {
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

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Create fullscreen window
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
        
        // Draw blurred background effect (fake blur using gradient overlay)
        for (int i = 0; i < 50; i++)
        {
            float alpha = 0.003f;
            float offset = i * 3.0f;
            draw_list->AddRectFilled(
                ImVec2(window_pos.x + offset, window_pos.y + offset),
                ImVec2(window_pos.x + window_size.x - offset, window_pos.y + window_size.y - offset),
                IM_COL32(15, 15, 20, (int)(alpha * 255))
            );
        }
        
        // Main dark background
        draw_list->AddRectFilled(
            window_pos,
            ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
            IM_COL32(20, 20, 25, 235),
            10.0f
        );

        // Custom title bar
        draw_list->AddRectFilled(
            window_pos,
            ImVec2(window_pos.x + window_size.x, window_pos.y + 40),
            IM_COL32(25, 25, 30, 240),
            10.0f,
            ImDrawFlags_RoundCornersTop
        );

        // Title text
        draw_list->AddText(
            ImVec2(window_pos.x + 15, window_pos.y + 12),
            IM_COL32(220, 220, 220, 255),
            "Terminal"
        );

        // Window control buttons (macOS style dots)
        float button_y = window_pos.y + 12;
        float button_x = window_pos.x + window_size.x - 70;

        ImVec2 mouse_pos = ImGui::GetMousePos();
        
        // Close button (red)
        ImVec2 close_center(button_x, button_y + 8);
        bool close_hovered = (mouse_pos.x - close_center.x) * (mouse_pos.x - close_center.x) + 
                            (mouse_pos.y - close_center.y) * (mouse_pos.y - close_center.y) <= 64;
        draw_list->AddCircleFilled(close_center, 8, 
            close_hovered ? IM_COL32(255, 80, 80, 255) : IM_COL32(200, 50, 50, 200));
        if (close_hovered && ImGui::IsMouseClicked(0))
            done = true;

        // Maximize button (green)
        ImVec2 max_center(button_x + 25, button_y + 8);
        bool max_hovered = (mouse_pos.x - max_center.x) * (mouse_pos.x - max_center.x) + 
                          (mouse_pos.y - max_center.y) * (mouse_pos.y - max_center.y) <= 64;
        draw_list->AddCircleFilled(max_center, 8,
            max_hovered ? IM_COL32(80, 255, 80, 255) : IM_COL32(50, 200, 50, 200));
        if (max_hovered && ImGui::IsMouseClicked(0))
        {
            WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
            GetWindowPlacement(hwnd, &wp);
            if (wp.showCmd == SW_MAXIMIZE)
                ShowWindow(hwnd, SW_RESTORE);
            else
                ShowWindow(hwnd, SW_MAXIMIZE);
        }

        // Minimize button (yellow)
        ImVec2 min_center(button_x + 50, button_y + 8);
        bool min_hovered = (mouse_pos.x - min_center.x) * (mouse_pos.x - min_center.x) + 
                          (mouse_pos.y - min_center.y) * (mouse_pos.y - min_center.y) <= 64;
        draw_list->AddCircleFilled(min_center, 8,
            min_hovered ? IM_COL32(255, 255, 80, 255) : IM_COL32(200, 200, 50, 200));
        if (min_hovered && ImGui::IsMouseClicked(0))
            ::ShowWindow(hwnd, SW_MINIMIZE);

        // Handle window dragging
        if (ImGui::IsMouseDown(0) && mouse_pos.y >= window_pos.y && mouse_pos.y < window_pos.y + 40 && 
            !close_hovered && !max_hovered && !min_hovered)
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

        // Terminal content area
        ImGui::SetCursorPos(ImVec2(15, 50));
        ImGui::BeginChild("Content", ImVec2(window_size.x - 30, window_size.y - 65), false, ImGuiWindowFlags_NoScrollbar);
        
        // Output area
        ImGui::BeginChild("Output", ImVec2(0, -35), false);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        for (const auto& line : g_outputLines)
        {
            ImGui::TextWrapped("%s", line.c_str());
        }
        ImGui::PopStyleColor();
        
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        // Input area
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
        ImGui::Text("%s> ", g_currentDir.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        
        ImGui::PushItemWidth(-1);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        if (ImGui::InputText("##input", g_inputBuffer, IM_ARRAYSIZE(g_inputBuffer), 
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string cmd(g_inputBuffer);
            if (!cmd.empty())
            {
                g_outputLines.push_back(g_currentDir + "> " + cmd);
                ProcessCommand(cmd);
                g_inputBuffer[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        // Auto-focus input
        if (!ImGui::IsAnyItemActive())
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::EndChild();
        
        ImGui::End();
        ImGui::PopStyleVar();

        // Rendering
        ImGui::Render();
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

void ProcessCommand(const std::string& cmd)
{
    if (cmd == "help")
    {
        g_outputLines.push_back("Available commands:");
        g_outputLines.push_back("  help  - Show this help");
        g_outputLines.push_back("  clear - Clear terminal");
        g_outputLines.push_back("  exit  - Close terminal");
        g_outputLines.push_back("  echo <text>  - Echo text");
        g_outputLines.push_back("  dir   - List directory");
    }
    else if (cmd == "clear")
    {
        g_outputLines.clear();
    }
    else if (cmd == "exit")
    {
        PostQuitMessage(0);
    }
    else if (cmd == "dir")
    {
        g_outputLines.push_back("Directory of " + g_currentDir);
        g_outputLines.push_back("  Documents");
        g_outputLines.push_back("  Downloads");
        g_outputLines.push_back("  Pictures");
    }
    else if (cmd.substr(0, 5) == "echo ")
    {
        g_outputLines.push_back(cmd.substr(5));
    }
    else
    {
        g_outputLines.push_back("Unknown command: " + cmd);
        g_outputLines.push_back("Type 'help' for available commands");
    }
    g_outputLines.push_back("");
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
