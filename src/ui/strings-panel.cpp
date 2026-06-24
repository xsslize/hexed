#include "ui/strings-panel.hpp"

#include "core/file-buffer.hpp"
#include "core/settings.hpp"

#include "ui/theme.hpp"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace
{
    std::string ToLower( const char* Text )
    {
        std::string Result( Text );
        for ( char& Character : Result )
            Character = static_cast< char >( std::tolower( static_cast< unsigned char >( Character ) ) );
        return Result;
    }

    void DrawHighlighted( const std::string& Text, const std::string& LowerQuery )
    {
        if ( LowerQuery.empty( ) )
        {
            ImGui::TextUnformatted( Text.c_str( ) );
            return;
        }

        const std::string LowerText = ToLower( Text.c_str( ) );
        const ImVec4 DimColor = ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled );

        bool First = true;
        auto DrawSegment = [ & ]( size_t Begin, size_t End, bool Highlight )
        {
            if ( End <= Begin )
                return;

            if ( !First )
                ImGui::SameLine( 0.0f, 0.0f );

            First = false;

            if ( !Highlight )
                ImGui::PushStyleColor( ImGuiCol_Text, DimColor );

            ImGui::TextUnformatted( Text.c_str( ) + Begin, Text.c_str( ) + End );
            if ( !Highlight )
                ImGui::PopStyleColor( );
        };

        size_t Cursor = 0;
        while ( Cursor < Text.size( ) )
        {
            const size_t Match = LowerText.find( LowerQuery, Cursor );
            if ( Match == std::string::npos )
            {
                DrawSegment( Cursor, Text.size( ), false );
                break;
            }

            DrawSegment( Cursor, Match, false );
            DrawSegment( Match, Match + LowerQuery.size( ), true );

            Cursor = Match + LowerQuery.size( );
        }
    }
}

void StringsPanel::RescanIfNeeded( const FileBuffer& File )
{
    const std::vector< uint8_t >& Data = File.Bytes( );
    if ( Data.data( ) == LastScannedData && Data.size( ) == LastScannedSize && MinimumLength == LastScannedMinimum )
        return;

    Results = ScanStrings( Data, MinimumLength );
    LastScannedData = Data.data( );
    LastScannedSize = Data.size( );
    LastScannedMinimum = MinimumLength;
    FilteredDirty = true;
    SelectedResultIndex = -1;
}

bool StringsPanel::PassesTypeToggles( StringEncoding Encoding ) const
{
    switch ( Encoding )
    {
        case StringEncoding::Ascii7Bit: return ShowAscii7Bit;
        case StringEncoding::CStyleAscii: return ShowCStyleAscii;
        case StringEncoding::Utf16: return ShowUtf16;
        case StringEncoding::Utf32: return ShowUtf32;
    }

    return true;
}

void StringsPanel::RebuildFilteredIndicesIfNeeded( )
{
    if ( LastSearchText != SearchText )
    {
        LastSearchText = SearchText;
        FilteredDirty = true;
    }

    if ( LastShowAscii7Bit != ShowAscii7Bit || LastShowCStyleAscii != ShowCStyleAscii || LastShowUtf16 != ShowUtf16 || LastShowUtf32 != ShowUtf32 )
    {
        LastShowAscii7Bit = ShowAscii7Bit;
        LastShowCStyleAscii = ShowCStyleAscii;
        LastShowUtf16 = ShowUtf16;
        LastShowUtf32 = ShowUtf32;
        FilteredDirty = true;
    }

    if ( !FilteredDirty )
        return;

    FilteredIndices.clear( );
    FilteredIndices.reserve( Results.size( ) );

    const std::string LowerQuery = ToLower( SearchText );

    for ( int Index = 0; Index < static_cast< int >( Results.size( ) ); ++Index )
    {
        const FoundString& String = Results[ Index ];
        if ( !PassesTypeToggles( String.Encoding ) )
            continue;

        if ( !LowerQuery.empty( ) && ToLower( String.Text.c_str( ) ).find( LowerQuery ) == std::string::npos )
            continue;

        FilteredIndices.push_back( Index );
    }

    FilteredDirty = false;
}

void StringsPanel::Draw( const FileBuffer& File )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    RescanIfNeeded( File );

    ImGui::SetNextItemWidth( 140.0f );
    ImGui::InputInt( "Min length", &MinimumLength, 1, 4 );

    if ( MinimumLength < 1 )
        MinimumLength = 1;
    if ( MinimumLength > 64 )
        MinimumLength = 64;

    ImGui::SameLine( );
    ImGui::Checkbox( "ASCII", &ShowAscii7Bit );
    ImGui::SameLine( );
    ImGui::Checkbox( "C-style", &ShowCStyleAscii );
    ImGui::SameLine( );
    ImGui::Checkbox( "UTF-16", &ShowUtf16 );
    ImGui::SameLine( );
    ImGui::Checkbox( "UTF-32", &ShowUtf32 );

    ImGui::SetNextItemWidth( -FLT_MIN );
    ImGui::InputTextWithHint( "##strings_search", "Search strings ( Case-Insensitive )", SearchText, sizeof( SearchText ) );

    RebuildFilteredIndicesIfNeeded( );

    const std::string LowerQuery = ToLower( SearchText );

    ImGui::TextDisabled( "%zu / %zu Strings  (Double-Click: Hex  |  X: Xrefs in disassembly)", FilteredIndices.size( ),
                         Results.size( ) );
    ImGui::Spacing( );

    const ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;

    if ( ImGui::BeginTable( "##strings_table", 3, TableFlags ) )
    {
        ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 96.0f );
        ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 84.0f );
        ImGui::TableSetupColumn( "String", ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableHeadersRow( );

        ImGui::PushFont( UI::MonoFont );

        ImGuiListClipper Clipper;
        Clipper.Begin( static_cast< int >( FilteredIndices.size( ) ) );
        while ( Clipper.Step( ) )
        {
            for ( int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row )
            {
                const int ResultIndex = FilteredIndices[ Row ];
                const FoundString& String = Results[ ResultIndex ];

                ImGui::TableNextRow( );
                ImGui::TableNextColumn( );

                char OffsetText[ 32 ];
                snprintf( OffsetText, sizeof( OffsetText ), "0x%08zX", String.Offset );

                ImGui::PushID( ResultIndex );
                if ( ImGui::Selectable( OffsetText, ResultIndex == SelectedResultIndex,
                                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) )
                {
                    SelectedResultIndex = ResultIndex;
                    if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
                    {
                        HasJumpRequest = true;
                        RequestedJumpOffset = String.Offset;
                    }
                }
                ImGui::PopID( );

                ImGui::TableNextColumn( );
                ImGui::TextUnformatted( StringEncodingName( String.Encoding ) );

                ImGui::TableNextColumn( );
                DrawHighlighted( String.Text, LowerQuery );
            }
        }

        ImGui::PopFont( );
        ImGui::EndTable( );
    }

    if ( !ImGui::IsAnyItemActive( ) && ImGui::IsKeyPressed( Settings::Get( ).Xrefs, false ) && SelectedResultIndex >= 0 &&
         SelectedResultIndex < static_cast< int >( Results.size( ) ) )
    {
        HasXrefRequest = true;
        RequestedXrefOffset = Results[ SelectedResultIndex ].Offset;
    }
}

bool StringsPanel::ConsumeJumpRequest( size_t& OutOffset )
{
    if ( !HasJumpRequest )
        return false;

    OutOffset = RequestedJumpOffset;
    HasJumpRequest = false;

    return true;
}

bool StringsPanel::ConsumeXrefRequest( size_t& OutOffset )
{
    if ( !HasXrefRequest )
        return false;

    OutOffset = RequestedXrefOffset;
    HasXrefRequest = false;

    return true;
}