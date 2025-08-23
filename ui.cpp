#include "ui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <algorithm>
#include "windows.h"

bool RenderUI(AppSettings& settings, UINT gameWidth, UINT gameHeight)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    center.y /= 2;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool open = true;
    ImGui::Begin("Ambient light", &open, 0);

    ImGui::Text("Press Esc to toggle showing config window");

    if (ImGui::CollapsingHeader("Game/Content Resolution", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Auto Detect", &settings.useAutoDetection))
            SaveSettings(settings);

        if (settings.useAutoDetection)
        {
            ImGui::Text("Detected: %d x %d", gameWidth, gameHeight);
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
        ImGui::SeparatorText("Blur");
        {
            if (ImGui::InputInt("Passes", (int*)&settings.blurPasses, 1, 1, ImGuiInputTextFlags_CharsDecimal))
            {
                settings.blurPasses = std::clamp(settings.blurPasses, 0u, 128u);
                SaveSettings(settings);
            }

            if (ImGui::InputInt("Downscale", (int*)&settings.blurDownscale, 16, 16, ImGuiInputTextFlags_CharsDecimal))
            {
                settings.blurDownscale = std::clamp(settings.blurDownscale, 16u, 1024u);
                SaveSettings(settings);
            }

            if (ImGui::InputInt("Samples", (int*)&settings.blurSamples, 2, 2, ImGuiInputTextFlags_CharsDecimal))
            {
                settings.blurSamples = std::clamp(settings.blurSamples / 2 * 2 + 1, 1u, 63u);
                SaveSettings(settings);
            }
        }

        ImGui::SeparatorText("Vignette");
        {
            if (ImGui::Checkbox("Enabled", &settings.vignetteEnabled))
                SaveSettings(settings);

            if (ImGui::InputFloat("Intensity", (float*)&settings.vignetteIntensity, 0.01f, 0.1f, "%.2f", ImGuiInputTextFlags_CharsDecimal))
            {
                settings.vignetteIntensity = std::clamp(settings.vignetteIntensity, 0.0f, 1.0f);
                SaveSettings(settings);
            }

            if (ImGui::InputFloat("Radius", (float*)&settings.vignetteRadius, 0.01f, 0.1f, "%.2f", ImGuiInputTextFlags_CharsDecimal))
            {
                settings.vignetteRadius = std::clamp(settings.vignetteRadius, 0.0f, 1.0f);
                SaveSettings(settings);
            }

            if (ImGui::InputFloat("Smoothness", (float*)&settings.vignetteSmoothness, 0.01f, 0.1f, "%.2f", ImGuiInputTextFlags_CharsDecimal))
            {
                settings.vignetteSmoothness = std::clamp(settings.vignetteSmoothness, 0.0f, 1.0f);
                SaveSettings(settings);
            }
        }

        ImGui::SeparatorText("Misc");
        if (ImGui::InputInt("Frame rate", (int*)&settings.frameRate, 1, 5, ImGuiInputTextFlags_CharsDecimal))
        {
            settings.frameRate = std::clamp(settings.frameRate, 10u, 1000u);
            SaveSettings(settings);
        }

        if (ImGui::InputInt("Zoom", (int*)&settings.zoom, 1, 1, ImGuiInputTextFlags_CharsDecimal))
        {
            settings.zoom = std::clamp(settings.zoom, 0u, 16u);
            SaveSettings(settings);
        }

        if (ImGui::Checkbox("Mirrored", &settings.mirrored))
            SaveSettings(settings);

        if (ImGui::Checkbox("Stretched", &settings.stretched))
            SaveSettings(settings);
    }



    if (ImGui::Button("Exit"))
        PostQuitMessage(0);

    ImGui::SameLine();
    ImGui::Text("%.1f FPS (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);

    ImGui::End();

    ImGui::Render();

    return open;
}
