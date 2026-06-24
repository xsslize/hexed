#include "ui/exports-panel.hpp"

#include "analysis/pe-parser.hpp"
#include "core/file-buffer.hpp"
#include "ui/theme.hpp"

#include <cstdio>

void ExportsPanel::RebuildFilteredIndicesIfNeeded( const PEInfo& PortableExecutable )
{
    if ( PortableExecutable.Exports.data( ) != LastExports || PortableExecutable.Exports.size( ) != LastExportCount )
    {
        LastExports = PortableExecutable.Exports.data( );
        LastExportCount = PortableExecutable.Exports.size( );
        SelectedRow = -1;
        FilteredDirty = true;
    }

    if ( LastFilterText != Filter.InputBuf )
    {
        LastFilterText = Filter.InputBuf;
        FilteredDirty = true;
    }

    if ( !FilteredDirty )
        return;

    FilteredIndices.clear( );
    FilteredIndices.reserve( PortableExecutable.Exports.size( ) );
    for ( int Index = 0; Index < static_cast< int >( PortableExecutable.Exports.size( ) ); ++Index )
    {
        const PEExport& Export = PortableExecutable.Exports[ Index ];
        const char* DisplayName = Export.Name.empty( ) ? "(ordinal)" : Export.Name.c_str( );
        if ( Filter.PassFilter( DisplayName ) || Filter.PassFilter( Export.Forwarder.c_str( ) ) )
            FilteredIndices.push_back( Index );
    }

    FilteredDirty = false;
}

void ExportsPanel::Draw( const PEInfo& PortableExecutable, const FileBuffer& File )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    if ( !PortableExecutable.IsValid )
    {
        ImGui::TextDisabled( "Not a PE file" );
        return;
    }

    if ( PortableExecutable.Exports.empty( ) )
    {
        ImGui::TextDisabled( "This file exports nothing" );
        return;
    }

    ImGui::Text( "Export module: %s", PortableExecutable.ExportModuleName.empty( ) ? "<unnamed>" : PortableExecutable.ExportModuleName.c_str( ) );

    ImGui::SetNextItemWidth( -FLT_MIN );
    Filter.Draw( "##exports_filter" );

    RebuildFilteredIndicesIfNeeded( PortableExecutable );

    ImGui::TextDisabled( "%zu / %zu exports  (double-click: view in disassembly)", FilteredIndices.size( ), PortableExecutable.Exports.size( ) );
    ImGui::Spacing( );

    const ImGuiTableFlags TableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;

    if ( !ImGui::BeginTable( "##exports_table", 3, TableFlags ) )
        return;

    ImGui::TableSetupColumn( "Ordinal", ImGuiTableColumnFlags_WidthFixed, 72.0f );
    ImGui::TableSetupColumn( "RVA", ImGuiTableColumnFlags_WidthFixed, 100.0f );
    ImGui::TableSetupColumn( "Name / Forwarder", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableHeadersRow( );

    ImGui::PushFont( UI::MonoFont );

    ImGuiListClipper Clipper;
    Clipper.Begin( static_cast< int >( FilteredIndices.size( ) ) );
    while ( Clipper.Step( ) )
    {
        for ( int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row )
        {
            const int ExportIndex = FilteredIndices[ Row ];
            const PEExport& Export = PortableExecutable.Exports[ ExportIndex ];

            ImGui::TableNextRow( );
            ImGui::TableNextColumn( );

            char OrdinalText[ 16 ];
            snprintf( OrdinalText, sizeof( OrdinalText ), "%u", Export.Ordinal );

            ImGui::PushID( ExportIndex );
            if ( ImGui::Selectable( OrdinalText, ExportIndex == SelectedRow, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) )
            {
                SelectedRow = ExportIndex;
                if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && Export.Forwarder.empty( ) && Export.FunctionRva != 0 )
                {
                    HasGotoRequest = true;
                    RequestedGotoAddress = PortableExecutable.ImageBase + Export.FunctionRva;
                }
            }
            ImGui::PopID( );

            ImGui::TableNextColumn( );
            ImGui::Text( "0x%08X", Export.FunctionRva );

            ImGui::TableNextColumn( );
            const char* DisplayName = Export.Name.empty( ) ? "(ordinal only)" : Export.Name.c_str( );
            if ( !Export.Forwarder.empty( ) )
                ImGui::Text( "%s  ->  %s", DisplayName, Export.Forwarder.c_str( ) );
            else
                ImGui::TextUnformatted( DisplayName );
        }
    }

    ImGui::PopFont( );
    ImGui::EndTable( );
}

bool ExportsPanel::ConsumeGotoRequest( uint64_t& OutAddress )
{
    if ( !HasGotoRequest )
        return false;

    OutAddress = RequestedGotoAddress;
    HasGotoRequest = false;
    return true;
}