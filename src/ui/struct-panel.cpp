#include "ui/struct-panel.hpp"

#include "core/file-buffer.hpp"
#include "ui/theme.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    struct StructType
    {
        const char* Name;
        int Size; // bytes; 0 means an array sized by StructField::ArrayLength
    };

    const StructType StructTypes[ ] =
    {
        { "int8", 1 }, { "uint8", 1 }, { "int16", 2 }, { "uint16", 2 }, { "int32", 4 }, { "uint32", 4 },
        { "int64", 8 }, { "uint64", 8 }, { "float", 4 }, { "double", 8 }, { "char[]", 0 }, { "bytes[]", 0 }
    };

    const int StructTypeCount = static_cast< int >( sizeof( StructTypes ) / sizeof( StructTypes[ 0 ] ) );

    const char* StructTypeNames[ ] =
    {
        "int8", "uint8", "int16", "uint16", "int32", "uint32",
        "int64", "uint64", "float", "double", "char[]", "bytes[]"
    };

    int FieldSize( const StructField& Field )
    {
        const StructType& Type = StructTypes[ Field.Type ];
        return Type.Size > 0 ? Type.Size : Field.ArrayLength;
    }

    std::string FormatValue( const std::vector< uint8_t >& Data, size_t Offset, const StructField& Field )
    {
        const int Size = FieldSize( Field );

        // Overflow-safe bounds check: a huge base offset must not wrap around.
        if ( Offset > Data.size( ) || static_cast< size_t >( Size ) > Data.size( ) - Offset )
            return "<out of range>";

        const uint8_t* Pointer = Data.data( ) + Offset;
        char Buffer[ 256 ];
        switch ( Field.Type )
        {
            case 0: { int8_t V; memcpy( &V, Pointer, 1 ); snprintf( Buffer, sizeof( Buffer ), "%d  (0x%02X)", V, static_cast< uint8_t >( V ) ); return Buffer; }
            case 1: { uint8_t V; memcpy( &V, Pointer, 1 ); snprintf( Buffer, sizeof( Buffer ), "%u  (0x%02X)", V, V ); return Buffer; }
            case 2: { int16_t V; memcpy( &V, Pointer, 2 ); snprintf( Buffer, sizeof( Buffer ), "%d  (0x%04X)", V, static_cast< uint16_t >( V ) ); return Buffer; }
            case 3: { uint16_t V; memcpy( &V, Pointer, 2 ); snprintf( Buffer, sizeof( Buffer ), "%u  (0x%04X)", V, V ); return Buffer; }
            case 4: { int32_t V; memcpy( &V, Pointer, 4 ); snprintf( Buffer, sizeof( Buffer ), "%d  (0x%08X)", V, static_cast< uint32_t >( V ) ); return Buffer; }
            case 5: { uint32_t V; memcpy( &V, Pointer, 4 ); snprintf( Buffer, sizeof( Buffer ), "%u  (0x%08X)", V, V ); return Buffer; }
            case 6: { int64_t V; memcpy( &V, Pointer, 8 ); snprintf( Buffer, sizeof( Buffer ), "%lld  (0x%016llX)", static_cast< long long >( V ), static_cast< unsigned long long >( V ) ); return Buffer; }
            case 7: { uint64_t V; memcpy( &V, Pointer, 8 ); snprintf( Buffer, sizeof( Buffer ), "%llu  (0x%016llX)", static_cast< unsigned long long >( V ), static_cast< unsigned long long >( V ) ); return Buffer; }
            case 8: { float V; memcpy( &V, Pointer, 4 ); snprintf( Buffer, sizeof( Buffer ), "%g", V ); return Buffer; }
            case 9: { double V; memcpy( &V, Pointer, 8 ); snprintf( Buffer, sizeof( Buffer ), "%g", V ); return Buffer; }
            case 10:
            {
                std::string Text = "\"";
                for ( int Index = 0; Index < Size; ++Index )
                {
                    const char Character = static_cast< char >( Pointer[ Index ] );
                    Text += ( Character >= 0x20 && Character < 0x7F ) ? Character : '.';
                }

                Text += "\"";

                return Text;
            }
            case 11:
            {
                std::string Text;
                char ByteText[ 4 ];
                for ( int Index = 0; Index < Size && Index < 64; ++Index )
                {
                    snprintf( ByteText, sizeof( ByteText ), "%02X ", Pointer[ Index ] );
                    Text += ByteText;
                }

                if ( Size > 64 )
                    Text += "...";

                return Text;
            }
        }

        return "?";
    }
}

void StructPanel::Draw( const FileBuffer& File )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    ImGui::SetNextItemWidth( 160.0f );
    ImGui::InputTextWithHint( "Base offset (Hex)", "0", BaseOffsetText, sizeof( BaseOffsetText ),
                              ImGuiInputTextFlags_CharsHexadecimal );
    ImGui::SameLine( );
    if ( ImGui::Button( "+ Add field" ) )
        Fields.emplace_back( );

    ImGui::Spacing( );

    if ( Fields.empty( ) )
    {
        ImGui::TextDisabled( "Add fields to overlay a structure on the bytes at the base offset" );
        return;
    }

    int RemoveIndex = -1;
    ImGui::BeginChild( "##struct_fields", ImVec2( 0.0f, ImGui::GetContentRegionAvail( ).y * 0.45f ),
                       ImGuiChildFlags_Borders );
    for ( int Index = 0; Index < static_cast< int >( Fields.size( ) ); ++Index )
    {
        StructField& Field = Fields[ Index ];
        ImGui::PushID( Index );

        ImGui::SetNextItemWidth( 110.0f );
        ImGui::Combo( "##type", &Field.Type, StructTypeNames, StructTypeCount );

        ImGui::SameLine( );
        if ( StructTypes[ Field.Type ].Size == 0 )
        {
            ImGui::SetNextItemWidth( 80.0f );
            ImGui::InputInt( "##length", &Field.ArrayLength, 0, 0 );
            if ( Field.ArrayLength < 1 )
                Field.ArrayLength = 1;
            if ( Field.ArrayLength > 4096 )
                Field.ArrayLength = 4096;
            ImGui::SameLine( );
        }

        ImGui::SetNextItemWidth( 180.0f );
        ImGui::InputTextWithHint( "##name", "field name", Field.Name, sizeof( Field.Name ) );

        ImGui::SameLine( );
        if ( ImGui::Button( "Remove" ) )
            RemoveIndex = Index;

        ImGui::PopID( );
    }
    ImGui::EndChild( );

    if ( RemoveIndex >= 0 )
        Fields.erase( Fields.begin( ) + RemoveIndex );

    ImGui::Separator( );

    const size_t Base = static_cast< size_t >( strtoull( BaseOffsetText, nullptr, 16 ) );
    const std::vector< uint8_t >& Data = File.Bytes( );

    const ImGuiTableFlags TableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

    if ( !ImGui::BeginTable( "##struct_values", 4, TableFlags ) )
        return;

    ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 110.0f );
    ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthFixed, 160.0f );
    ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 90.0f );
    ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableHeadersRow( );

    ImGui::PushFont( UI::MonoFont );

    size_t Running = Base;
    for ( int Index = 0; Index < static_cast< int >( Fields.size( ) ); ++Index )
    {
        const StructField& Field = Fields[ Index ];

        ImGui::TableNextRow( );
        ImGui::TableNextColumn( );
        ImGui::Text( "0x%zX", Running );

        ImGui::TableNextColumn( );
        if ( Field.Name[ 0 ] != '\0' )
            ImGui::TextUnformatted( Field.Name );
        else
            ImGui::TextDisabled( "(field %d)", Index );

        ImGui::TableNextColumn( );
        if ( StructTypes[ Field.Type ].Size == 0 )
            ImGui::Text( "%s%d", StructTypes[ Field.Type ].Name, Field.ArrayLength );
        else
            ImGui::TextUnformatted( StructTypes[ Field.Type ].Name );

        ImGui::TableNextColumn( );
        ImGui::TextUnformatted( FormatValue( Data, Running, Field ).c_str( ) );

        Running += static_cast< size_t >( FieldSize( Field ) );
    }

    ImGui::PopFont( );
    ImGui::EndTable( );
}