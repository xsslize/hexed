#include "ui/hex-view.hpp"

#include "core/file-buffer.hpp"

#include "ui/theme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

namespace
{
    constexpr char HexDigits[] = "0123456789ABCDEF";

    inline char PrintableAscii( uint8_t Byte )
    {
        return ( Byte >= 0x20 && Byte < 0x7F ) ? static_cast< char >( Byte ) : '.';
    }

    std::vector< int > BuildPattern( const char* Text, bool HexMode )
    {
        std::vector< int > Pattern;
        if ( !HexMode )
        {
            for ( const char* Cursor = Text; *Cursor != '\0'; ++Cursor )
                Pattern.push_back( static_cast< unsigned char >( *Cursor ) );

            return Pattern;
        }

        std::vector< int > Nibbles;
        for ( const char* Cursor = Text; *Cursor != '\0'; ++Cursor )
        {
            const char Character = *Cursor;
            if ( Character == ' ' || Character == '\t' )
                continue;
            if ( Character == '?' )
            {
                Nibbles.push_back( -1 );
            }
            else if ( isxdigit( static_cast< unsigned char >( Character ) ) )
            {
                const int Value = ( Character >= '0' && Character <= '9' ) ? Character - '0' : tolower( static_cast< unsigned char >( Character ) ) - 'a' + 10;
                Nibbles.push_back( Value );
            }
            else
            {
                return { }; // invalid character
            }
        }

        for ( size_t Index = 0; Index + 1 < Nibbles.size( ); Index += 2 )
        {
            const int High = Nibbles[ Index ];
            const int Low = Nibbles[ Index + 1 ];
            Pattern.push_back( ( High < 0 || Low < 0 ) ? -1 : High * 16 + Low );
        }

        return Pattern;
    }
}

void HexView::GoTo( size_t Offset )
{
    GotoAddress = static_cast< int >( Offset );
    GotoPending = true;
}

void HexView::FindNext( const FileBuffer& File )
{
    const std::vector< int > Pattern = BuildPattern( SearchText, SearchHexMode );
    HasMatch = false;
    if ( Pattern.empty( ) )
        return;

    const std::vector< uint8_t >& Data = File.Bytes( );
    const size_t Size = Data.size( );
    const size_t Length = Pattern.size( );
    if ( Length > Size )
        return;

    const size_t Start = MatchLength > 0 ? MatchOffset + 1 : 0;
    for ( size_t Step = 0; Step < Size; ++Step )
    {
        const size_t Offset = ( Start + Step ) % Size;
        if ( Offset + Length > Size )
            continue;

        bool Matched = true;
        for ( size_t Index = 0; Index < Length; ++Index )
        {
            if ( Pattern[ Index ] >= 0 && Data[ Offset + Index ] != static_cast< uint8_t >( Pattern[ Index ] ) )
            {
                Matched = false;
                break;
            }
        }

        if ( Matched )
        {
            HasMatch = true;
            MatchOffset = Offset;
            MatchLength = Length;
            GoTo( Offset );
            return;
        }
    }
}

void HexView::Draw( const FileBuffer& File )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded. Use File > Open (Ctrl+O)" );
        return;
    }

    const std::vector< uint8_t >& Data = File.Bytes( );
    const size_t Size = Data.size( );
    const int Columns = BytesPerRow;

    ImGui::SetNextItemWidth( 120.0f );
    ImGui::InputInt( "Bytes/row", &BytesPerRow, 8, 8 );
    if ( BytesPerRow < 4 )
        BytesPerRow = 4;
    if ( BytesPerRow > 64 )
        BytesPerRow = 64;

    ImGui::SameLine( );
    ImGui::SetNextItemWidth( 140.0f );
    if ( ImGui::InputInt( "Go to (hex)", &GotoAddress, 0, 0, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue ) )
        GotoPending = true;

    if ( GotoAddress < 0 )
        GotoAddress = 0;

    /*ImGui::SameLine( );
    ImGui::TextDisabled( "%s  (%zu bytes)", File.Path( ).c_str( ), Size );*/

    if ( LastSearchText != SearchText || LastHexMode != SearchHexMode )
    {
        LastSearchText = SearchText;
        LastHexMode = SearchHexMode;
        HasMatch = false;
        MatchLength = 0;
    }

    ImGui::SetNextItemWidth( 260.0f );
    const bool Submitted = ImGui::InputTextWithHint( "##hex_search", SearchHexMode ? "Hex pattern, e.g. 48 89 ?? 5C" : "Text to find", SearchText,
        sizeof( SearchText ), ImGuiInputTextFlags_EnterReturnsTrue );
    ImGui::SameLine( );
    ImGui::Checkbox( "Hex", &SearchHexMode );
    ImGui::SameLine( );
    const bool FindClicked = ImGui::Button( "Find Next" );
    ImGui::SameLine( );
    if ( HasMatch )
        ImGui::TextDisabled( "match @ 0x%zX  (%zu bytes)", MatchOffset, MatchLength );
    else if ( SearchText[ 0 ] != '\0' )
        ImGui::TextDisabled( "no match" );

    if ( Submitted || FindClicked )
        FindNext( File );

    ImGui::Separator( );

    ImGui::BeginChild( "##hex_scroll", ImVec2( 0.0f, 0.0f ), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar );
    ImGui::PushFont( UI::MonoFont );

    const int TotalRows = static_cast< int >( ( Size + Columns - 1 ) / Columns );
    const float LineHeight = ImGui::GetTextLineHeight( );
    const float CharWidth = ImGui::CalcTextSize( "0" ).x;

    if ( GotoPending )
    {
        const int TargetRow = GotoAddress / Columns;
        ImGui::SetScrollY( TargetRow * LineHeight );
        GotoPending = false;
    }

    char Line[ 1024 ];

    ImGuiListClipper Clipper;
    Clipper.Begin( TotalRows, LineHeight );
    while ( Clipper.Step( ) )
    {
        for ( int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row )
        {
            const size_t Address = static_cast< size_t >( Row ) * Columns;
            int Position = snprintf( Line, sizeof( Line ), "%08zX:  ", Address );
            for ( int Column = 0; Column < Columns && Position + 3 < static_cast< int >( sizeof( Line ) ); ++Column )
            {
                const size_t Index = Address + Column;
                if ( Index < Size )
                {
                    const uint8_t Byte = Data[ Index ];
                    Line[ Position++ ] = HexDigits[ Byte >> 4 ];
                    Line[ Position++ ] = HexDigits[ Byte & 0x0F ];
                }
                else
                {
                    Line[ Position++ ] = ' ';
                    Line[ Position++ ] = ' ';
                }

                Line[ Position++ ] = ' ';
            }

            if ( Position + 2 < static_cast< int >( sizeof( Line ) ) )
            {
                Line[ Position++ ] = ' ';
                Line[ Position++ ] = '|';
            }

            for ( int Column = 0; Column < Columns && Position + 1 < static_cast< int >( sizeof( Line ) ); ++Column )
            {
                const size_t Index = Address + Column;
                Line[ Position++ ] = ( Index < Size ) ? PrintableAscii( Data[ Index ] ) : ' ';
            }

            if ( Position < static_cast< int >( sizeof( Line ) ) - 1 )
                Line[ Position++ ] = '|';

            Line[ Position ] = '\0';

            const ImVec2 RowPos = ImGui::GetCursorScreenPos( );
            if ( HasMatch && MatchOffset < Address + Columns && MatchOffset + MatchLength > Address )
            {
                const size_t First = MatchOffset > Address ? MatchOffset - Address : 0;
                const size_t LastExclusive = std::min( Address + Columns, MatchOffset + MatchLength ) - Address;
                ImDrawList* DrawList = ImGui::GetWindowDrawList( );
                const ImU32 Highlight = ImGui::GetColorU32( ImVec4( 0.95f, 0.75f, 0.30f, 0.30f ) );
                for ( size_t Column = First; Column < LastExclusive; ++Column )
                {
                    const float HexX = RowPos.x + static_cast< float >( 11 + Column * 3 ) * CharWidth;
                    DrawList->AddRectFilled( ImVec2( HexX, RowPos.y ), ImVec2( HexX + 2.0f * CharWidth, RowPos.y + LineHeight ), Highlight );

                    const float AsciiX = RowPos.x + static_cast< float >( 11 + Columns * 3 + 2 + Column ) * CharWidth;
                    DrawList->AddRectFilled( ImVec2( AsciiX, RowPos.y ), ImVec2( AsciiX + CharWidth, RowPos.y + LineHeight ), Highlight );
                }
            }

            ImGui::TextUnformatted( Line );
        }
    }

    ImGui::PopFont( );
    ImGui::EndChild( );
}