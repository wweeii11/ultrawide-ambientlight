#pragma once
#include "settings.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

LRESULT UiWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void InitUI(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* device_context);
bool RenderUI(AppSettings& settings, UINT gameWidth, UINT gameHeight);
void UpdateUI(HWND hwnd, AppSettings& settings);