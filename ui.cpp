#include "ui.h"
#include "common.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <algorithm>
#include "windows.h"

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT UiWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return 0;

    switch (message)
    {
    case WM_KEYDOWN:
    {
        int pressed = (int)wParam;
        if (pressed == VK_ESCAPE)
        {
            PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 0, 1);
            return 0;
        }
    }
    break;

    case WM_USER_SHELLICON:
    {
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP)
        {
            PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 1, 0);
            return 0;
        }
    }
    break;

    case WM_ACTIVATE:
    {
        // hide config window when lost focus
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 0, 0);
        }
        else
        {
            PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 1, 0);
        }
    }
    break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        if (!ImGui::GetIO().WantCaptureMouse)
        {
            PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 0, 0);
        }
    }
    break;

    }

    return ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);
}

void InitUI(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // load Segoe UI font
    ImFont* arial_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f);
    io.Fonts->Build();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    static bool firstInit = true;
    if (!firstInit)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, device_context);

    // first initialization
    if (firstInit)
    {
        PostMessage(hwnd, WM_TOGGLE_CONFIG_WINDOW, 1, 0);
    }
    firstInit = false;
}

bool RenderUI(AppSettings& settings, UINT gameWidth, UINT gameHeight)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool open = true;
    ImGui::Begin("Ambient Light", &open, 0);

    ImGui::Text("Press Esc or the system tray icon to show or hide the configuration window");

    if (ImGui::CollapsingHeader("Game/Content Resolution", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Auto Detect", &settings.useAutoDetection))
            SaveSettings(settings);

        if (settings.useAutoDetection)
        {
            ImGui::Text("Detected: %d x %d", gameWidth, gameHeight);

            if (ImGui::CollapsingHeader("Detection", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Checkbox("Light Peek", &settings.autoDetectionLightMask))
                {
                    SaveSettings(settings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        "Preserve bright highlights that appear inside detected black-bar regions.\n"
                        "Enable this when you want small bright UI elements or overlays on letterboxed video\n"
                        "to remain visible instead of being masked out by the black-bar detection.");
                }

                int brightnessPercent = (int)(settings.autoDetectionBrightnessThreshold * 100);
                if (ImGui::DragInt("Detection Brightness", &brightnessPercent, 0.1f, 1, 100, "%d%%"))
                {
                    settings.autoDetectionBrightnessThreshold = float(brightnessPercent) / 100.0f;
                    SaveSettings(settings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        "Brightness threshold used to classify pixels as \"black\" for detection.\n"
                        "Pixels darker than this value are considered part of black bars.\n"
                        "Lower values make the detector more permissive (more pixels counted as black).\n"
                        "Higher values make it stricter. Range: 1%% (dark) to 100%% (bright).");
                }

                int blackRatioPercent = (int)(settings.autoDetectionBlackRatio * 100);
                if (ImGui::DragInt("Detection Ratio ", &blackRatioPercent, 0.1f, 1, 100, "%d%%%"))
                {
                    settings.autoDetectionBlackRatio = float(blackRatioPercent) / 100.0f;
                    SaveSettings(settings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        "Proportion of an analyzed area that must be below the brightness threshold to\n"
                        "classify that area as a black bar.\n"
                        "Increase this to avoid false positives (requires more of the area to be dark),\n"
                        "or decrease to detect thinner/partial bars.");
                }

                if (ImGui::Checkbox("Symmetric", &settings.autoDetectionSymmetricBars))
                {
                    SaveSettings(settings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        "Force detected black bars to be symmetric and align to the smaller side.");
                }

                if (ImGui::Checkbox("Reserved Area", &settings.autoDetectionReservedArea))
                {
                    SaveSettings(settings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        "Exclude a centered rectangular region from black-bar detection.\n"
                        "You can define it using pixel width/height (1920, 1080) or aspect ratio (e.g. 16, 9)");
                }
                if (settings.autoDetectionReservedArea)
                {
                    int reservedSize[2] = { (int)settings.autoDetectionReservedWidth, (int)settings.autoDetectionReservedHeight };
                    if (ImGui::InputInt2("", reservedSize, ImGuiInputTextFlags_CharsDecimal))
                    {
                        settings.autoDetectionReservedWidth = (UINT)max(0, reservedSize[0]);
                        settings.autoDetectionReservedHeight = (UINT)max(0, reservedSize[1]);

                        SaveSettings(settings);
                    }
                }
            }
        }
        else 
        {
            if (ImGui::BeginCombo("Presets", settings.resolutions.current.c_str(), 0))
            {
                for (auto& res : settings.resolutions.available)
                {
                    bool selected = settings.resolutions.current == res.name;
                    if (ImGui::Selectable(res.name.c_str(), &selected))
                    {
                        settings.resolutions.current = res.name;
                        SaveSettings(settings);
                    }
                }
                ImGui::EndCombo();
            }

            bool updateResolution = false;
            int res_input[2] = { (int)settings.gameWidth, (int)settings.gameHeight };
            if (ImGui::InputInt2("", res_input, ImGuiInputTextFlags_CharsDecimal))
                updateResolution = true;

            if (updateResolution)
            {
                for (auto& res : settings.resolutions.available)
                {
                    if (settings.resolutions.current == res.name)
                    {
                        res.width = res_input[0];
                        res.height = res_input[1];
                        break;
                    }
                }

                SaveSettings(settings);
            }

            ImGui::Text("Selected: %d x %d", gameWidth, gameHeight);
        }
    }

    if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SameLine(); HelpMarker(
            "Click and drag to edit a value.\n"
            "Hold Shift/Alt for faster/slower edits.\n"
            "Double-click or Ctrl+click to enter a value.");

        ImGui::SeparatorText("Blur");
        {
            if (ImGui::DragInt("Passes", (int*)&settings.blurPasses, 0.1f, 0, 128))
            {
                SaveSettings(settings);
            }
            
            if (ImGui::DragInt("Downscale", (int*)&settings.blurDownscale, 0.1f, 16, 1024))
            {
                SaveSettings(settings);
            }

            if (ImGui::DragInt("Samples", (int*)&settings.blurSamples, 0.1f, 1, 63))
            {
                SaveSettings(settings);
            }
        }

        ImGui::SeparatorText("Vignette");
        {
            if (ImGui::Checkbox("Enabled", &settings.vignetteEnabled))
                SaveSettings(settings);

            if (ImGui::DragFloat("Intensity", (float*)&settings.vignetteIntensity, 0.01f, 0.0f, 1.0f))
            {
                SaveSettings(settings);
            }

            if (ImGui::DragFloat("Radius", (float*)&settings.vignetteRadius, 0.01f, 0.0f, 1.0f))
            {
                SaveSettings(settings);
            }

            if (ImGui::DragFloat("Smoothness", (float*)&settings.vignetteSmoothness, 0.01f, 0.0f, 1.0f))
            {
                SaveSettings(settings);
            }
        }

        ImGui::SeparatorText("Misc");
        if (ImGui::DragInt("Frame rate", (int*)&settings.frameRate, 0.1f, 10, 500))
        {
            SaveSettings(settings);
        }

        if (ImGui::DragInt("Zoom", (int*)&settings.zoom, 1, 1, 16))
        {
            settings.zoom = std::clamp(settings.zoom, 0u, 16u);
            SaveSettings(settings);
        }

        if (ImGui::Checkbox("Mirrored", &settings.mirrored))
            SaveSettings(settings);

        if (ImGui::Checkbox("Stretched", &settings.stretched))
            SaveSettings(settings);
    }

    if (ImGui::CollapsingHeader("UI", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Show in taskbar", &settings.showInTaskbar))
        {
            SaveSettings(settings);
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Exit"))
        PostQuitMessage(0);

    ImGui::SameLine();
    ImGui::TextDisabled("%.1f FPS (%.2f ms/frame)", io.Framerate, 1000.0f / io.Framerate);

    ImGui::End();

    ImGui::Render();

    return open;
}

void UpdateUI(HWND hwnd, AppSettings& settings)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (settings.showInTaskbar)
    {
        exStyle |= WS_EX_APPWINDOW;
        exStyle &= ~WS_EX_TOOLWINDOW;
    }
    else
    {
        exStyle |= WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
}

