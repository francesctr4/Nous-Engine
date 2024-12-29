#include "ImGuiCustom.h"

#include "ImGui.h"

void ImGuiCustom::ImGuiTheme_OrangeGrey()
{
    // Red Font style by aiekick from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6000000238418579f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.FrameRounding = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 14.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.7490196228027344f, 0.7490196228027344f, 0.7490196228027344f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.3490196168422699f, 0.3490196168422699f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05882352963089943f, 0.05882352963089943f, 0.05882352963089943f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5400000214576721f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.369f, 0.303f, 0.137f, 0.67f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.937f, 0.754f, 0.297f, 0.67f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03921568766236305f, 0.03921568766236305f, 0.03921568766236305f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.91f, 0.527f, 0.129f, 0.90f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.675f, 0.405f, 0.151f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1372549086809158f, 0.1372549086809158f, 0.1372549086809158f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.91f, 0.527f, 0.129f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.972f, 0.561f, 0.134f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.527f, 0.129f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.629f, 0.188f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.91f, 0.527f, 0.129f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3294117748737335f, 0.3490196168422699f, 0.3568627536296844f, 0.5299999713897705f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.594f, 0.26f, 0.70f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.8f, 0.443f, 0.0f, 0.70f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.3294117748737335f, 0.3490196168422699f, 0.3568627536296844f, 0.5299999713897705f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.996f, 0.782f, 0.087f, 0.3f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.633f, 0.107f, 0.107f, 0.7f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.3176470696926117f, 0.3176470696926117f, 0.3176470696926117f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.3176470696926117f, 0.3176470696926117f, 0.3176470696926117f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.3176470696926117f, 0.3176470696926117f, 0.3176470696926117f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 0.8500000238418579f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.6000000238418579f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.8999999761581421f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.06666667014360428f, 0.06666667014360428f, 0.06666667014360428f, 0.5099999904632568f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.996f, 0.782f, 0.087f, 0.3f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.633f, 0.107f, 0.107f, 0.7f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0470588244497776f, 0.0470588244497776f, 0.0470588244497776f, 0.8999999761581421f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1294117718935013f, 0.1294117718935013f, 0.1294117718935013f, 0.7400000095367432f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.07000000029802322f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 0.829f, 0.0f, 0.9f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
}

void ImGuiCustom::ImGuiTheme_RedGrey()
{
    // Red and Grey Font style
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.FrameRounding = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 14.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    // Keep dark, black colors as is
    style.Colors[ImGuiCol_Text] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Grey Text
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // Darker Grey
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.0f); // Darker Grey Background
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.9f); // Slightly lighter for contrast
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.6f, 0.2f, 0.2f, 1.0f); // Active Red Title
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.2f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.3f, 0.3f, 0.7f); // Grey Button

    // Replace orange/yellow with red shades
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.2f, 0.2f, 0.8f); // Red Button Hover
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.9f, 0.3f, 0.3f, 0.9f); // Active Red Button
    style.Colors[ImGuiCol_Header] = ImVec4(0.3f, 0.3f, 0.3f, 0.7f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.6f, 0.2f, 0.2f, 0.8f); // Header Red Hover
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.7f, 0.2f, 0.2f, 0.9f); // Active Header Red
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.7f, 0.2f, 0.2f, 1.0f); // Red Slider
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Active Red Slider
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 0.7f); // Grey Tab
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.7f, 0.2f, 0.2f, 0.8f); // Red Hover Tab
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.9f, 0.3f, 0.3f, 0.9f); // Active Red Tab
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.8f, 0.1f, 0.1f, 1.0f); // Red Checkmark
    style.Colors[ImGuiCol_Separator] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Darker Separator
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.8f, 0.1f, 0.1f, 0.7f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9f, 0.2f, 0.2f, 0.8f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 0.3f, 0.3f, 0.9f);
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void ImGuiCustom::ImGuiHelpMarker(const char* description)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}