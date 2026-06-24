#include "app/app.hpp"

#include "imgui.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <commdlg.h>

bool App::Render( )
{
    DrawMenuBar( );

    const ImGuiIO& Io = ImGui::GetIO( );
    if ( Io.KeyCtrl && ImGui::IsKeyPressed( ImGuiKey_O, false ) )
        OpenFileDialog( );

    const ImGuiViewport* Viewport = ImGui::GetMainViewport( );
    ImGui::SetNextWindowPos( Viewport->WorkPos );
    ImGui::SetNextWindowSize( Viewport->WorkSize );

    const ImGuiWindowFlags RootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
    ImGui::Begin( "##root", nullptr, RootFlags );

    DrawSummaryBar( );

    if ( ImGui::BeginTabBar( "##main_tabs" ) )
    {
        if ( ImGui::BeginTabItem( "Imports" ) )
        {
            ImportsPanelWidget.Draw( PortableExecutableInfo, File );
            ImGui::EndTabItem( );
        }
        if ( ImGui::BeginTabItem( "Exports" ) )
        {
            ExportsPanelWidget.Draw( PortableExecutableInfo, File );
            ImGui::EndTabItem( );
        }
        if ( ImGui::BeginTabItem( "Strings" ) )
        {
            StringsPanelWidget.Draw( File );
            ImGui::EndTabItem( );
        }

        const ImGuiTabItemFlags HexTabFlags = WantHexTab ? ImGuiTabItemFlags_SetSelected : 0;
        WantHexTab = false;
        if ( ImGui::BeginTabItem( "Hex", nullptr, HexTabFlags ) )
        {
            HexViewWidget.Draw( File );
            ImGui::EndTabItem( );
        }

        const ImGuiTabItemFlags DisasmTabFlags = WantDisasmTab ? ImGuiTabItemFlags_SetSelected : 0;
        WantDisasmTab = false;
        if ( ImGui::BeginTabItem( "Disassembly", nullptr, DisasmTabFlags ) )
        {
            DisassemblyPanelWidget.Draw( File, PortableExecutableInfo );
            ImGui::EndTabItem( );
        }

        const ImGuiTabItemFlags GraphTabFlags = WantGraphTab ? ImGuiTabItemFlags_SetSelected : 0;
        WantGraphTab = false;
        if ( ImGui::BeginTabItem( "Graph", nullptr, GraphTabFlags ) )
        {
            GraphPanelWidget.Draw( File, PortableExecutableInfo );
            ImGui::EndTabItem( );
        }

        if ( ImGui::BeginTabItem( "Struct" ) )
        {
            StructPanelWidget.Draw( File );
            ImGui::EndTabItem( );
        }

        if ( ImGui::BeginTabItem( "Settings" ) )
        {
            SettingsPanelWidget.Draw( );
            ImGui::EndTabItem( );
        }

        ImGui::EndTabBar( );
    }

    ImGui::End( );
    ImGui::PopStyleVar( 2 );

    size_t JumpOffset = 0;
    if ( StringsPanelWidget.ConsumeJumpRequest( JumpOffset ) )
    {
        HexViewWidget.GoTo( JumpOffset );
        WantHexTab = true;
    }

    size_t XrefOffset = 0;
    if ( StringsPanelWidget.ConsumeXrefRequest( XrefOffset ) )
    {
        uint64_t VirtualAddress = 0;
        if ( FileOffsetToVirtualAddress( XrefOffset, VirtualAddress ) )
        {
            DisassemblyPanelWidget.ShowXrefsForAddress( VirtualAddress );
            WantDisasmTab = true;
        }
    }

    uint64_t GraphAddress = 0;
    if ( DisassemblyPanelWidget.ConsumeGraphRequest( GraphAddress ) )
    {
        GraphPanelWidget.SetRoot( GraphAddress );
        WantGraphTab = true;
    }

    uint64_t ExportAddress = 0;
    if ( ExportsPanelWidget.ConsumeGotoRequest( ExportAddress ) )
    {
        DisassemblyPanelWidget.GoToAddress( ExportAddress );
        WantDisasmTab = true;
    }

    return IsRunning;
}

void App::DrawSummaryBar( )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded - File > Open (Ctrl+O), or drag a file onto the window" );
        ImGui::Separator( );
        return;
    }

    if ( !PortableExecutableInfo.IsValid )
    {
        ImGui::TextDisabled( "%s  (%zu bytes)",
                             PortableExecutableInfo.Error.empty( ) ? "Not a PE file" : PortableExecutableInfo.Error.c_str( ),
                             File.Size( ) );
        ImGui::Separator( );
        return;
    }

    const PEInfo& PortableExecutable = PortableExecutableInfo;
    /*ImGui::TextUnformatted( File.Path( ).c_str( ) );
    ImGui::SameLine( );*/
    ImGui::TextDisabled(
        "%s  |  %s  |  Base 0x%llX  |  Entry 0x%08X  |  %u Sections  |  %zu Imports / %zu Modules  |  %zu Exports",
        PortableExecutable.MachineName( ), PortableExecutable.Is64Bit ? "PE32+" : "PE32",
        static_cast< unsigned long long >( PortableExecutable.ImageBase ), PortableExecutable.EntryPointRva,
        PortableExecutable.SectionCount, PortableExecutable.TotalImports( ), PortableExecutable.Modules.size( ),
        PortableExecutable.Exports.size( ) );
    ImGui::Separator( );
}

bool App::FileOffsetToVirtualAddress( size_t Offset, uint64_t& VirtualAddress ) const
{
    for ( const PESection& Section : PortableExecutableInfo.Sections )
    {
        const size_t SectionEnd = static_cast< size_t >( Section.RawDataPointer ) + Section.RawDataSize;
        if ( Offset >= Section.RawDataPointer && Offset < SectionEnd )
        {
            VirtualAddress = PortableExecutableInfo.ImageBase + Section.VirtualAddress + ( Offset - Section.RawDataPointer );
            return true;
        }
    }

    return false;
}

void App::DrawMenuBar( )
{
    if ( ImGui::BeginMainMenuBar( ) )
    {
        if ( ImGui::BeginMenu( "File" ) )
        {
            if ( ImGui::MenuItem( "Open...", "Ctrl+O" ) )
                OpenFileDialog( );
            if ( ImGui::MenuItem( "Close", nullptr, false, !File.Empty( ) ) )
            {
                File.Clear( );
                PortableExecutableInfo = PEInfo{};
            }
            ImGui::Separator( );
            if ( ImGui::MenuItem( "Exit", "Alt+F4" ) )
                IsRunning = false;
            ImGui::EndMenu( );
        }
        ImGui::EndMainMenuBar( );
    }
}

void App::OnFileDropped( const std::string& Path )
{
    LoadFile( Path );
}

void App::LoadFile( const std::string& Path )
{
    if ( File.Load( Path ) )
        PortableExecutableInfo = ParsePE( File.Bytes( ) );
}

void App::OpenFileDialog( )
{
    char Path[ MAX_PATH ] = { 0 };

    OPENFILENAMEA OpenFileName = { 0 };
    OpenFileName.lStructSize = sizeof( OpenFileName );
    OpenFileName.hwndOwner = nullptr;
    OpenFileName.lpstrFilter = "All files\0*.*\0Executables\0*.exe;*.dll;*.sys\0";
    OpenFileName.lpstrFile = Path;
    OpenFileName.nMaxFile = sizeof( Path );
    OpenFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if ( GetOpenFileNameA( &OpenFileName ) )
        LoadFile( Path );
}