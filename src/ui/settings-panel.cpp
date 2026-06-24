#include "ui/settings-panel.hpp"

#include "core/settings.hpp"
#include "ui/theme.hpp"

#include "imgui.h"

#include <cstdio>
#include <string>

namespace
{
    struct FontChoice
    {
        const char* Name;
        const char* Path;
    };

    const FontChoice FontChoices[] =
    {
        { "Segoe UI", "C:\\Windows\\Fonts\\segoeui.ttf" },
        { "Consolas", "C:\\Windows\\Fonts\\consola.ttf" },
        { "Cascadia Code", "C:\\Windows\\Fonts\\CascadiaCode.ttf" },
        { "Cascadia Mono", "C:\\Windows\\Fonts\\CascadiaMono.ttf" },
        { "Courier New", "C:\\Windows\\Fonts\\cour.ttf" },
        { "Lucida Console", "C:\\Windows\\Fonts\\lucon.ttf" },
        { "Tahoma", "C:\\Windows\\Fonts\\tahoma.ttf" },
        { "Verdana", "C:\\Windows\\Fonts\\verdana.ttf" },
        { "Arial", "C:\\Windows\\Fonts\\arial.ttf" }
    };

    const int FontChoiceCount = static_cast< int >( sizeof( FontChoices ) / sizeof( FontChoices[ 0 ] ) );

    int FontIndexForPath( const std::string& Path )
    {
        for ( int Index = 0; Index < FontChoiceCount; ++Index )
            if ( Path == FontChoices[ Index ].Path )
                return Index;
        return -1;
    }

    ImGuiKey CaptureBindableKey( )
    {
        for ( ImGuiKey Key = ImGuiKey_A; Key <= ImGuiKey_Z; Key = static_cast< ImGuiKey >( Key + 1 ) )
        {
            if ( ImGui::IsKeyPressed( Key, false ) )
                return Key;
        }

        for ( ImGuiKey Key = ImGuiKey_0; Key <= ImGuiKey_9; Key = static_cast< ImGuiKey >( Key + 1 ) )
        {
            if ( ImGui::IsKeyPressed( Key, false ) )
                return Key;
        }

        for ( ImGuiKey Key = ImGuiKey_F1; Key <= ImGuiKey_F12; Key = static_cast< ImGuiKey >( Key + 1 ) )
        {
            if ( ImGui::IsKeyPressed( Key, false ) )
                return Key;
        }

        const ImGuiKey Specials[ ] =
        {
            ImGuiKey_Space, ImGuiKey_Enter, ImGuiKey_Escape, ImGuiKey_Tab,
            ImGuiKey_Home, ImGuiKey_End, ImGuiKey_PageUp, ImGuiKey_PageDown,
            ImGuiKey_Insert, ImGuiKey_Delete, ImGuiKey_LeftArrow,
            ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow
        };

        for ( const ImGuiKey Key : Specials )
            if ( ImGui::IsKeyPressed( Key, false ) )
                return Key;

        return ImGuiKey_None;
    }
}

void SettingsPanel::DrawConfigurations( )
{
    if ( !ImGui::CollapsingHeader( "Configurations", ImGuiTreeNodeFlags_DefaultOpen ) )
        return;

    if ( ConfigListDirty )
    {
        ConfigList = Settings::ListConfigurations( );
        ConfigListDirty = false;
    }

    ImGui::TextDisabled( "Saved configurations:" );
    ImGui::BeginChild( "##config_list", ImVec2( 0.0f, 110.0f ), ImGuiChildFlags_Borders );
    if ( ConfigList.empty( ) )
        ImGui::TextDisabled( "(None yet)" );

    for ( const std::string& Name : ConfigList )
    {
        if ( ImGui::Selectable( Name.c_str( ), Name == SelectedConfig ) )
        {
            SelectedConfig = Name;
            snprintf( ConfigNameBuffer, sizeof( ConfigNameBuffer ), "%s", Name.c_str( ) );
        }
    }
    ImGui::EndChild( );

    ImGui::SetNextItemWidth( 240.0f );
    ImGui::InputTextWithHint( "##config_name", "Configuration name...", ConfigNameBuffer, sizeof( ConfigNameBuffer ) );

    ImGui::SameLine( );
    if ( ImGui::Button( "Save" ) && ConfigNameBuffer[ 0 ] != '\0' )
    {
        if ( Settings::SaveConfiguration( ConfigNameBuffer ) )
        {
            SelectedConfig = ConfigNameBuffer;
            ConfigListDirty = true;
        }
    }

    ImGui::SameLine( );
    ImGui::BeginDisabled( SelectedConfig.empty( ) );
    if ( ImGui::Button( "Load" ) )
        Settings::LoadConfiguration( SelectedConfig );
    ImGui::SameLine( );
    if ( ImGui::Button( "Delete" ) )
    {
        Settings::DeleteConfiguration( SelectedConfig );
        SelectedConfig.clear( );
        ConfigNameBuffer[ 0 ] = '\0';
        ConfigListDirty = true;
    }
    ImGui::EndDisabled( );

    ImGui::Spacing( );
    ImGui::TextDisabled( "Saves keybindings, fonts, and the theme. The last used one loads on startup" );
}

void SettingsPanel::Draw( )
{
    Settings::Configuration& Config = Settings::Get( );

    ImGui::BeginChild( "##settings_scroll", ImVec2( 0.0f, 0.0f ) );

    DrawConfigurations( );

    if ( ImGui::CollapsingHeader( "Keybindings", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        struct KeyBind
        {
            const char* Label;
            ImGuiKey* Key;
        };

        KeyBind Binds[ ] =
        {
            { "Go to address", &Config.GoToAddress },
            { "Cross-references", &Config.Xrefs },
            { "Navigate back", &Config.NavigateBack }
        };

        const int BindCount = static_cast< int >( sizeof( Binds ) / sizeof( Binds[ 0 ] ) );
        for ( int Index = 0; Index < BindCount; ++Index )
        {
            ImGui::PushID( Index );
            ImGui::TextUnformatted( Binds[ Index ].Label );
            ImGui::SameLine( 200.0f );

            const bool Capturing = CapturingAction == Index;
            const char* ButtonLabel = Capturing ? "Press a key..." : ImGui::GetKeyName( *Binds[ Index ].Key );
            if ( ImGui::Button( ButtonLabel, ImVec2( 180.0f, 0.0f ) ) )
                CapturingAction = Capturing ? -1 : Index;

            ImGui::PopID( );
        }

        if ( CapturingAction >= 0 && CapturingAction < BindCount )
        {
            const ImGuiKey Pressed = CaptureBindableKey( );
            if ( Pressed != ImGuiKey_None )
            {
                *Binds[ CapturingAction ].Key = Pressed;
                CapturingAction = -1;
            }
        }

        ImGui::Spacing( );
        ImGui::TextDisabled( "Click a binding, then press the new key. (File > Open stays Ctrl+O)" );
    }

    if ( ImGui::CollapsingHeader( "Fonts", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        const char* FontNames[ 16 ];
        for ( int Index = 0; Index < FontChoiceCount; ++Index )
            FontNames[ Index ] = FontChoices[ Index ].Name;

        int UIIndex = FontIndexForPath( Config.UIFontPath );
        int MonoIndex = FontIndexForPath( Config.MonoFontPath );

        ImGui::SetNextItemWidth( 220.0f );
        if ( ImGui::Combo( "UI font", &UIIndex, FontNames, FontChoiceCount ) && UIIndex >= 0 )
            Config.UIFontPath = FontChoices[ UIIndex ].Path;

        ImGui::SetNextItemWidth( 220.0f );
        if ( ImGui::Combo( "Monospace font", &MonoIndex, FontNames, FontChoiceCount ) && MonoIndex >= 0 )
            Config.MonoFontPath = FontChoices[ MonoIndex ].Path;

        ImGui::SetNextItemWidth( 220.0f );
        ImGui::SliderFloat( "UI size", &Config.UIFontSize, 11.0f, 28.0f, "%.0f px" );
        ImGui::SetNextItemWidth( 220.0f );
        ImGui::SliderFloat( "Monospace size", &Config.MonoFontSize, 11.0f, 24.0f, "%.0f px" );

        if ( ImGui::Button( "Apply fonts" ) )
            Config.FontsDirty = true;
        ImGui::SameLine( );
        ImGui::TextDisabled( "rebuilds the font atlas" );
    }

    if ( ImGui::CollapsingHeader( "Theme & style", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        if ( ImGui::Button( "Reset to default theme" ) )
            UI::ApplyTheme( );
        ImGui::Spacing( );
        ImGui::TextDisabled( "Full color / rounding / spacing editor:" );
        ImGui::ShowStyleEditor( );
    }

    ImGui::EndChild( );
}