#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace Settings
{
    struct Configuration
    {
        ImGuiKey GoToAddress = ImGuiKey_G;
        ImGuiKey Xrefs = ImGuiKey_X;
        ImGuiKey NavigateBack = ImGuiKey_Escape;

        std::string UIFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        std::string MonoFontPath = "C:\\Windows\\Fonts\\consola.ttf";
        float UIFontSize = 18.0f;
        float MonoFontSize = 15.0f;
        bool FontsDirty = false;
    };

    Configuration& Get( );

    std::vector< std::string > ListConfigurations( );

    bool SaveConfiguration( const std::string& Name );
    bool LoadConfiguration( const std::string& Name );
    bool DeleteConfiguration( const std::string& Name );

    void LoadLastConfiguration( );
}