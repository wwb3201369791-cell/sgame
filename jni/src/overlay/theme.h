#pragma once
#include "imgui.h"

// 哈皮哈啤哈屁 定制主题 — 淡白·柔粉·暖黄
inline void ApplyHPHPTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // ===== 圆角 & 间距 =====
    style.WindowRounding    = 12.0f;
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ChildRounding     = 8.0f;
    style.ScrollbarRounding = 12.0f;

    style.WindowPadding     = ImVec2(16, 12);
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.5f;

    ImVec4* c = style.Colors;

    // 窗口背景: 淡白半透明
    c[ImGuiCol_WindowBg]         = ImVec4(1.00f, 0.98f, 0.96f, 0.92f);
    c[ImGuiCol_ChildBg]          = ImVec4(1.00f, 0.97f, 0.94f, 0.60f);
    c[ImGuiCol_PopupBg]          = ImVec4(1.00f, 0.98f, 0.95f, 0.95f);

    // 边框: 淡粉
    c[ImGuiCol_Border]           = ImVec4(1.00f, 0.78f, 0.82f, 0.50f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // 标题栏: 粉黄渐变感
    c[ImGuiCol_TitleBg]          = ImVec4(1.00f, 0.85f, 0.75f, 0.90f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(1.00f, 0.75f, 0.70f, 0.95f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.90f, 0.80f, 0.50f);

    // 菜单栏
    c[ImGuiCol_MenuBarBg]        = ImVec4(1.00f, 0.93f, 0.88f, 0.90f);

    // 帧 (输入框/滑条背景): 淡黄
    c[ImGuiCol_FrameBg]          = ImVec4(1.00f, 0.93f, 0.80f, 0.60f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(1.00f, 0.85f, 0.70f, 0.80f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(1.00f, 0.78f, 0.60f, 0.90f);

    // 文字
    c[ImGuiCol_Text]             = ImVec4(0.30f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.60f, 0.55f, 0.55f, 1.00f);

    // 按钮: 柔粉主色
    c[ImGuiCol_Button]           = ImVec4(1.00f, 0.75f, 0.78f, 0.70f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(1.00f, 0.65f, 0.70f, 0.85f);
    c[ImGuiCol_ButtonActive]     = ImVec4(1.00f, 0.55f, 0.60f, 1.00f);

    // 勾选/单选: 暖黄高亮
    c[ImGuiCol_CheckMark]        = ImVec4(1.00f, 0.60f, 0.65f, 1.00f);

    // 滑条: 粉黄
    c[ImGuiCol_SliderGrab]       = ImVec4(1.00f, 0.72f, 0.55f, 0.90f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.60f, 0.45f, 1.00f);

    // 标签页
    c[ImGuiCol_Tab]              = ImVec4(1.00f, 0.90f, 0.82f, 0.80f);
    c[ImGuiCol_TabHovered]       = ImVec4(1.00f, 0.78f, 0.70f, 0.90f);
    c[ImGuiCol_TabActive]        = ImVec4(1.00f, 0.72f, 0.65f, 1.00f);
    c[ImGuiCol_TabUnfocused]     = ImVec4(1.00f, 0.92f, 0.85f, 0.70f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(1.00f, 0.85f, 0.78f, 0.85f);

    // 头部 (CollapsingHeader)
    c[ImGuiCol_Header]           = ImVec4(1.00f, 0.87f, 0.75f, 0.60f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(1.00f, 0.80f, 0.70f, 0.80f);
    c[ImGuiCol_HeaderActive]     = ImVec4(1.00f, 0.72f, 0.62f, 1.00f);

    // 分隔线
    c[ImGuiCol_Separator]        = ImVec4(1.00f, 0.82f, 0.78f, 0.50f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 0.70f, 0.65f, 0.80f);
    c[ImGuiCol_SeparatorActive]  = ImVec4(1.00f, 0.60f, 0.55f, 1.00f);

    // 滚动条
    c[ImGuiCol_ScrollbarBg]      = ImVec4(1.00f, 0.96f, 0.93f, 0.40f);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(1.00f, 0.80f, 0.75f, 0.60f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 0.72f, 0.68f, 0.80f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(1.00f, 0.65f, 0.60f, 1.00f);

    // 拖拽目标
    c[ImGuiCol_DragDropTarget]   = ImVec4(1.00f, 0.65f, 0.50f, 0.90f);

    // 导航
    c[ImGuiCol_NavHighlight]     = ImVec4(1.00f, 0.70f, 0.65f, 1.00f);
}
