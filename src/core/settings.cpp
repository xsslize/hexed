#include "core/settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Settings
{
    Configuration& Get( )
    {
        static Configuration Instance;
        return Instance;
    }

    namespace
    {
        const char* ConfigDirectory = "configs";

        std::string SanitizeName( const std::string& Name )
        {
            std::string Result;
            for ( const char Character : Name )
            {
                if ( std::isalnum( static_cast< unsigned char >( Character ) ) || Character == '_' ||
                     Character == '-' || Character == ' ' || Character == '.' )
                    Result += Character;
            }

            while ( !Result.empty( ) && Result.front( ) == ' ' )
                Result.erase( Result.begin( ) );

            while ( !Result.empty( ) && Result.back( ) == ' ' )
                Result.pop_back( );

            return Result;
        }

        std::string ConfigPath( const std::string& Name )
        {
            return std::string( ConfigDirectory ) + "/" + Name + ".cfg";
        }

        float ParseFloat( const std::string& Value )
        {
            return static_cast< float >( std::atof( Value.c_str( ) ) );
        }

        ImVec2 ParseVec2( const std::string& Value )
        {
            float X = 0.0f, Y = 0.0f;
            sscanf( Value.c_str( ), "%f %f", &X, &Y );
            return ImVec2( X, Y );
        }

        void RememberLast( const std::string& Name )
        {
            std::ofstream Output( std::string( ConfigDirectory ) + "/_last.txt" );
            if ( Output )
                Output << Name << "\n";
        }
    }

    std::vector< std::string > ListConfigurations( )
    {
        std::vector< std::string > Names;
        std::error_code Error;
        for ( const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator( ConfigDirectory, Error ) )
        {
            if ( Entry.path( ).extension( ) == ".cfg" )
                Names.push_back( Entry.path( ).stem( ).string( ) );
        }

        std::sort( Names.begin( ), Names.end( ) );

        return Names;
    }

    bool SaveConfiguration( const std::string& Name )
    {
        const std::string Clean = SanitizeName( Name );
        if ( Clean.empty( ) )
            return false;

        std::error_code Error;
        std::filesystem::create_directories( ConfigDirectory, Error );

        std::ofstream Output( ConfigPath( Clean ) );
        if ( !Output )
            return false;

        const Configuration& Config = Get( );
        Output << "key.goto=" << static_cast< int >( Config.GoToAddress ) << "\n";
        Output << "key.xrefs=" << static_cast< int >( Config.Xrefs ) << "\n";
        Output << "key.back=" << static_cast< int >( Config.NavigateBack ) << "\n";
        Output << "font.ui=" << Config.UIFontPath << "\n";
        Output << "font.mono=" << Config.MonoFontPath << "\n";
        Output << "font.uisize=" << Config.UIFontSize << "\n";
        Output << "font.monosize=" << Config.MonoFontSize << "\n";

        const ImGuiStyle& Style = ImGui::GetStyle( );
        Output << "style.Alpha=" << Style.Alpha << "\n";
        Output << "style.WindowRounding=" << Style.WindowRounding << "\n";
        Output << "style.ChildRounding=" << Style.ChildRounding << "\n";
        Output << "style.FrameRounding=" << Style.FrameRounding << "\n";
        Output << "style.PopupRounding=" << Style.PopupRounding << "\n";
        Output << "style.GrabRounding=" << Style.GrabRounding << "\n";
        Output << "style.TabRounding=" << Style.TabRounding << "\n";
        Output << "style.ScrollbarRounding=" << Style.ScrollbarRounding << "\n";
        Output << "style.WindowBorderSize=" << Style.WindowBorderSize << "\n";
        Output << "style.ChildBorderSize=" << Style.ChildBorderSize << "\n";
        Output << "style.FrameBorderSize=" << Style.FrameBorderSize << "\n";
        Output << "style.PopupBorderSize=" << Style.PopupBorderSize << "\n";
        Output << "style.ScrollbarSize=" << Style.ScrollbarSize << "\n";
        Output << "style.GrabMinSize=" << Style.GrabMinSize << "\n";
        Output << "style.IndentSpacing=" << Style.IndentSpacing << "\n";
        Output << "style.WindowPadding=" << Style.WindowPadding.x << " " << Style.WindowPadding.y << "\n";
        Output << "style.FramePadding=" << Style.FramePadding.x << " " << Style.FramePadding.y << "\n";
        Output << "style.ItemSpacing=" << Style.ItemSpacing.x << " " << Style.ItemSpacing.y << "\n";
        Output << "style.ItemInnerSpacing=" << Style.ItemInnerSpacing.x << " " << Style.ItemInnerSpacing.y << "\n";
        Output << "style.CellPadding=" << Style.CellPadding.x << " " << Style.CellPadding.y << "\n";

        for ( int Index = 0; Index < ImGuiCol_COUNT; ++Index )
        {
            const ImVec4& Color = Style.Colors[ Index ];
            Output << "color." << Index << "=" << Color.x << " " << Color.y << " " << Color.z << " " << Color.w << "\n";
        }

        Output.close( );
        RememberLast( Clean );

        return true;
    }

    bool LoadConfiguration( const std::string& Name )
    {
        const std::string Clean = SanitizeName( Name );
        std::ifstream Input( ConfigPath( Clean ) );
        if ( !Input )
            return false;

        Configuration& Config = Get( );
        ImGuiStyle& Style = ImGui::GetStyle( );

        std::string Line;
        while ( std::getline( Input, Line ) )
        {
            if ( !Line.empty( ) && Line.back( ) == '\r' )
                Line.pop_back( );

            const std::size_t Equals = Line.find( '=' );
            if ( Equals == std::string::npos )
                continue;

            const std::string Key = Line.substr( 0, Equals );
            const std::string Value = Line.substr( Equals + 1 );

            if ( Key == "key.goto" )
                Config.GoToAddress = static_cast< ImGuiKey >( atoi( Value.c_str( ) ) );
            else if ( Key == "key.xrefs" )
                Config.Xrefs = static_cast< ImGuiKey >( atoi( Value.c_str( ) ) );
            else if ( Key == "key.back" )
                Config.NavigateBack = static_cast< ImGuiKey >( atoi( Value.c_str( ) ) );
            else if ( Key == "font.ui" )
                Config.UIFontPath = Value;
            else if ( Key == "font.mono" )
                Config.MonoFontPath = Value;
            else if ( Key == "font.uisize" )
                Config.UIFontSize = ParseFloat( Value );
            else if ( Key == "font.monosize" )
                Config.MonoFontSize = ParseFloat( Value );
            else if ( Key.rfind( "color.", 0 ) == 0 )
            {
                const int Index = atoi( Key.c_str( ) + 6 );
                if ( Index >= 0 && Index < ImGuiCol_COUNT )
                {
                    ImVec4& Color = Style.Colors[ Index ];
                    sscanf( Value.c_str( ), "%f %f %f %f", &Color.x, &Color.y, &Color.z, &Color.w );
                }
            }
            else if ( Key.rfind( "style.", 0 ) == 0 )
            {
                const std::string Field = Key.substr( 6 );
                if ( Field == "Alpha" ) Style.Alpha = ParseFloat( Value );
                else if ( Field == "WindowRounding" ) Style.WindowRounding = ParseFloat( Value );
                else if ( Field == "ChildRounding" ) Style.ChildRounding = ParseFloat( Value );
                else if ( Field == "FrameRounding" ) Style.FrameRounding = ParseFloat( Value );
                else if ( Field == "PopupRounding" ) Style.PopupRounding = ParseFloat( Value );
                else if ( Field == "GrabRounding" ) Style.GrabRounding = ParseFloat( Value );
                else if ( Field == "TabRounding" ) Style.TabRounding = ParseFloat( Value );
                else if ( Field == "ScrollbarRounding" ) Style.ScrollbarRounding = ParseFloat( Value );
                else if ( Field == "WindowBorderSize" ) Style.WindowBorderSize = ParseFloat( Value );
                else if ( Field == "ChildBorderSize" ) Style.ChildBorderSize = ParseFloat( Value );
                else if ( Field == "FrameBorderSize" ) Style.FrameBorderSize = ParseFloat( Value );
                else if ( Field == "PopupBorderSize" ) Style.PopupBorderSize = ParseFloat( Value );
                else if ( Field == "ScrollbarSize" ) Style.ScrollbarSize = ParseFloat( Value );
                else if ( Field == "GrabMinSize" ) Style.GrabMinSize = ParseFloat( Value );
                else if ( Field == "IndentSpacing" ) Style.IndentSpacing = ParseFloat( Value );
                else if ( Field == "WindowPadding" ) Style.WindowPadding = ParseVec2( Value );
                else if ( Field == "FramePadding" ) Style.FramePadding = ParseVec2( Value );
                else if ( Field == "ItemSpacing" ) Style.ItemSpacing = ParseVec2( Value );
                else if ( Field == "ItemInnerSpacing" ) Style.ItemInnerSpacing = ParseVec2( Value );
                else if ( Field == "CellPadding" ) Style.CellPadding = ParseVec2( Value );
            }
        }

        Config.FontsDirty = true; // rebuild fonts with the loaded paths / sizes
        RememberLast( Clean );

        return true;
    }

    bool DeleteConfiguration( const std::string& Name )
    {
        std::error_code Error;
        return std::filesystem::remove( ConfigPath( SanitizeName( Name ) ), Error );
    }

    void LoadLastConfiguration( )
    {
        std::ifstream Input( std::string( ConfigDirectory ) + "/_last.txt" );
        if ( !Input )
            return;

        std::string Name;
        std::getline( Input, Name );

        if ( !Name.empty( ) && Name.back( ) == '\r' )
            Name.pop_back( );

        if ( !Name.empty( ) )
            LoadConfiguration( Name );
    }
}