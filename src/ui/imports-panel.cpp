#include "ui/imports-panel.hpp"

#include "analysis/pe-parser.hpp"

#include "core/file-buffer.hpp"

#include "ui/theme.hpp"

#include <cstdio>

void ImportsPanel::Draw( const PEInfo& PortableExecutable, const FileBuffer& File )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    if ( !PortableExecutable.IsValid )
    {
        ImGui::TextDisabled( "Imports" );
        ImGui::Spacing( );
        ImGui::TextWrapped( "%s", PortableExecutable.Error.empty( ) ? "Not a PE file" : PortableExecutable.Error.c_str( ) );
        return;
    }

    if ( PortableExecutable.Modules.empty( ) )
    {
        ImGui::TextDisabled( "No imported modules" );
        return;
    }

    ImGui::SetNextItemWidth( -FLT_MIN );
    Filter.Draw( "##imports_filter" );
    if ( !Filter.IsActive( ) )
        ImGui::TextDisabled( "Filter modules / functions..." );
    ImGui::Spacing( );

    ImGui::BeginChild( "##imports_tree", ImVec2( 0.0f, 0.0f ) );

    char FunctionLabel[ 320 ];
    for ( const PEModule& Module : PortableExecutable.Modules )
    {
        const bool ModuleMatches = Filter.PassFilter( Module.Name.c_str( ) );
        bool AnyFunctionMatches = ModuleMatches;
        if ( !ModuleMatches )
        {
            for ( const PEImport& Import : Module.Functions )
            {
                if ( !Import.ByOrdinal && Filter.PassFilter( Import.Name.c_str( ) ) )
                {
                    AnyFunctionMatches = true;
                    break;
                }
            }
        }

        if ( !AnyFunctionMatches )
            continue;

        char Header[ 280 ];
        snprintf( Header, sizeof( Header ), "%s  (%zu)", Module.Name.c_str( ), Module.Functions.size( ) );

        if ( ImGui::TreeNode( Header ) )
        {
            ImGui::PushFont( UI::MonoFont );
            for ( const PEImport& Import : Module.Functions )
            {
                if ( Import.ByOrdinal )
                    snprintf( FunctionLabel, sizeof( FunctionLabel ), "Ordinal #%u", Import.Ordinal );
                else
                    snprintf( FunctionLabel, sizeof( FunctionLabel ), "%s", Import.Name.c_str( ) );

                if ( !ModuleMatches && !Import.ByOrdinal && !Filter.PassFilter( Import.Name.c_str( ) ) )
                    continue;

                ImGui::Selectable( FunctionLabel );
            }
            ImGui::PopFont( );
            ImGui::TreePop( );
        }
    }

    ImGui::EndChild( );
}