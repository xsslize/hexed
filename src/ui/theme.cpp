#include "ui/theme.hpp"

#include "core/settings.hpp"

#include "imgui.h"

#include <fstream>

namespace UI
{
    ImFont* MonoFont = nullptr;

    namespace
    {
        bool FileExists( const char* Path )
        {
            std::ifstream Stream( Path );
            return Stream.good( );
        }
    }

    void LoadFonts( )
    {
        ImGuiIO& Io = ImGui::GetIO( );
        const Settings::Configuration& Config = Settings::Get( );

        if ( FileExists( Config.UIFontPath.c_str( ) ) )
            Io.Fonts->AddFontFromFileTTF( Config.UIFontPath.c_str( ), Config.UIFontSize );
        else
            Io.Fonts->AddFontDefault( );

        MonoFont = nullptr;
        if ( FileExists( Config.MonoFontPath.c_str( ) ) )
            MonoFont = Io.Fonts->AddFontFromFileTTF( Config.MonoFontPath.c_str( ), Config.MonoFontSize );

        if ( !MonoFont )
            MonoFont = Io.Fonts->Fonts.empty( ) ? Io.Fonts->AddFontDefault( ) : Io.Fonts->Fonts[ 0 ];
    }

    void ApplyTheme( )
    {
        ImGuiStyle& Style = ImGui::GetStyle( );

        Style.WindowRounding = 0.0f;
        Style.ChildRounding = 10.0f;
        Style.FrameRounding = 8.0f;
        Style.PopupRounding = 10.0f;
        Style.GrabRounding = 8.0f;
        Style.TabRounding = 8.0f;
        Style.ScrollbarRounding = 10.0f;

        Style.WindowBorderSize = 0.0f;
        Style.ChildBorderSize = 1.0f;
        Style.FrameBorderSize = 1.0f;
        Style.PopupBorderSize = 1.0f;

        Style.WindowPadding = ImVec2( 12.0f, 12.0f );
        Style.FramePadding = ImVec2( 10.0f, 6.0f );
        Style.ItemSpacing = ImVec2( 10.0f, 8.0f );
        Style.IndentSpacing = 18.0f;
        Style.ScrollbarSize = 12.0f;

        auto White = []( float Alpha ) { return ImVec4( 1.0f, 1.0f, 1.0f, Alpha ); };
        ImVec4* Colors = Style.Colors;

        Colors[ ImGuiCol_Text ] = ImVec4( 0.93f, 0.93f, 0.95f, 1.00f );
        Colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.50f, 0.50f, 0.53f, 1.00f );
        Colors[ ImGuiCol_WindowBg ] = ImVec4( 0.05f, 0.05f, 0.06f, 1.00f );
        Colors[ ImGuiCol_ChildBg ] = White( 0.025f );
        Colors[ ImGuiCol_PopupBg ] = ImVec4( 0.07f, 0.07f, 0.08f, 0.96f );
        Colors[ ImGuiCol_Border ] = White( 0.10f );
        Colors[ ImGuiCol_BorderShadow ] = ImVec4( 0, 0, 0, 0 );
        Colors[ ImGuiCol_FrameBg ] = White( 0.05f );
        Colors[ ImGuiCol_FrameBgHovered ] = White( 0.09f );
        Colors[ ImGuiCol_FrameBgActive ] = White( 0.13f );
        Colors[ ImGuiCol_TitleBg ] = ImVec4( 0.04f, 0.04f, 0.05f, 1.00f );
        Colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.06f, 0.06f, 0.07f, 1.00f );
        Colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.07f, 0.07f, 0.08f, 1.00f );
        Colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0, 0, 0, 0 );
        Colors[ ImGuiCol_ScrollbarGrab ] = White( 0.14f );
        Colors[ ImGuiCol_ScrollbarGrabHovered ] = White( 0.22f );
        Colors[ ImGuiCol_ScrollbarGrabActive ] = White( 0.30f );
        Colors[ ImGuiCol_CheckMark ] = White( 0.90f );
        Colors[ ImGuiCol_SliderGrab ] = White( 0.55f );
        Colors[ ImGuiCol_SliderGrabActive ] = White( 0.80f );
        Colors[ ImGuiCol_Button ] = White( 0.07f );
        Colors[ ImGuiCol_ButtonHovered ] = White( 0.13f );
        Colors[ ImGuiCol_ButtonActive ] = White( 0.20f );
        Colors[ ImGuiCol_Header ] = White( 0.07f );
        Colors[ ImGuiCol_HeaderHovered ] = White( 0.12f );
        Colors[ ImGuiCol_HeaderActive ] = White( 0.18f );
        Colors[ ImGuiCol_Separator ] = White( 0.10f );
        Colors[ ImGuiCol_SeparatorHovered ] = White( 0.20f );
        Colors[ ImGuiCol_SeparatorActive ] = White( 0.30f );
        Colors[ ImGuiCol_ResizeGrip ] = White( 0.10f );
        Colors[ ImGuiCol_ResizeGripHovered ] = White( 0.20f );
        Colors[ ImGuiCol_ResizeGripActive ] = White( 0.30f );
        Colors[ ImGuiCol_Tab ] = White( 0.05f );
        Colors[ ImGuiCol_TabHovered ] = White( 0.14f );
        Colors[ ImGuiCol_TabSelected ] = White( 0.10f );
        Colors[ ImGuiCol_TabDimmed ] = White( 0.03f );
        Colors[ ImGuiCol_TabDimmedSelected ] = White( 0.07f );
        Colors[ ImGuiCol_TableHeaderBg ] = White( 0.04f );
        Colors[ ImGuiCol_TableBorderStrong ] = White( 0.10f );
        Colors[ ImGuiCol_TableBorderLight ] = White( 0.06f );
        Colors[ ImGuiCol_TextSelectedBg ] = White( 0.18f );
        Colors[ ImGuiCol_NavHighlight ] = White( 0.30f );

        Colors[ ImGuiCol_TextLink ] = ImVec4( 0.60f, 0.74f, 0.92f, 1.00f );
    }
}