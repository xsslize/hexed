#include "ui/graph-panel.hpp"

#include "analysis/pe-parser.hpp"
#include "core/file-buffer.hpp"
#include "ui/theme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
    const float BlockPadding = 6.0f;
    const float VerticalGap = 42.0f;
    const float HorizontalGap = 32.0f;

    ImU32 EdgeColor( int Kind )
    {
        switch ( Kind )
        {
            case 1: return ImGui::GetColorU32( ImVec4( 0.45f, 0.78f, 0.50f, 0.95f ) ); // conditional taken
            case 2: return ImGui::GetColorU32( ImVec4( 0.85f, 0.47f, 0.43f, 0.95f ) ); // conditional fall-through
            default: return ImGui::GetColorU32( ImVec4( 0.55f, 0.62f, 0.72f, 0.90f ) ); // unconditional / fall-through
        }
    }
}

void GraphPanel::SetRoot( uint64_t Address )
{
    RootAddress = Address;
    HasRoot = true;
}

void GraphPanel::LayoutGraph( )
{
    if ( Graph.Blocks.empty( ) )
        return;

    ImGui::PushFont( UI::MonoFont );
    const float LineHeight = ImGui::GetTextLineHeight( );
    for ( CfgBlock& Block : Graph.Blocks )
    {
        float MaxWidth = 0.0f;
        for ( const std::string& Line : Block.Lines )
            MaxWidth = std::max( MaxWidth, ImGui::CalcTextSize( Line.c_str( ) ).x );
        Block.Width = MaxWidth + BlockPadding * 2.0f;
        Block.Height = static_cast< float >( Block.Lines.size( ) ) * LineHeight + BlockPadding * 2.0f;
    }
    ImGui::PopFont( );

    int MaxLayer = 0;
    for ( const CfgBlock& Block : Graph.Blocks )
        MaxLayer = std::max( MaxLayer, Block.Layer );

    std::vector< float > LayerHeight( MaxLayer + 1, 0.0f );
    for ( const CfgBlock& Block : Graph.Blocks )
        LayerHeight[ Block.Layer ] = std::max( LayerHeight[ Block.Layer ], Block.Height );

    std::vector< float > LayerY( MaxLayer + 1, 0.0f );
    for ( int Layer = 1; Layer <= MaxLayer; ++Layer )
        LayerY[ Layer ] = LayerY[ Layer - 1 ] + LayerHeight[ Layer - 1 ] + VerticalGap;

    std::vector< float > LayerX( MaxLayer + 1, 0.0f );
    for ( int Layer = 0; Layer <= MaxLayer; ++Layer )
    {
        for ( CfgBlock& Block : Graph.Blocks )
        {
            if ( Block.Layer != Layer )
                continue;
            Block.X = LayerX[ Layer ];
            Block.Y = LayerY[ Layer ];
            LayerX[ Layer ] += Block.Width + HorizontalGap;
        }
    }

    RecenterPending = true;
}

void GraphPanel::RebuildIfNeeded( const FileBuffer& File, const PEInfo& PortableExecutable )
{
    const std::vector< uint8_t >& Data = File.Bytes( );

    const uint32_t RootRva = static_cast< uint32_t >( RootAddress - PortableExecutable.ImageBase );
    int Section = -1;
    for ( int Index = 0; Index < static_cast< int >( PortableExecutable.Sections.size( ) ); ++Index )
    {
        const PESection& Candidate = PortableExecutable.Sections[ Index ];
        const uint32_t Span = Candidate.VirtualSize ? Candidate.VirtualSize : Candidate.RawDataSize;
        if ( RootRva >= Candidate.VirtualAddress && RootRva < Candidate.VirtualAddress + Span )
        {
            Section = Index;
            break;
        }
    }

    if ( Section < 0 )
    {
        Instructions.clear( );
        Graph = ControlFlowGraph( );
        LastRoot = RootAddress;
        return;
    }

    if ( Data.data( ) != LastData || Data.size( ) != LastSize || Section != LastSection )
    {
        LastData = Data.data( );
        LastSize = Data.size( );
        LastSection = Section;
        Is64BitView = PortableExecutable.Is64Bit;

        const PESection& CodeSection = PortableExecutable.Sections[ Section ];
        Instructions.clear( );

        size_t Begin = CodeSection.RawDataPointer;
        size_t Size = CodeSection.RawDataSize;
        if ( Begin < Data.size( ) )
        {
            if ( Begin + Size > Data.size( ) )
                Size = Data.size( ) - Begin;
            if ( Size > 0 )
            {
                const uint64_t Address = PortableExecutable.ImageBase + CodeSection.VirtualAddress;
                Instructions = Disassemble( Data.data( ) + Begin, Size, Address, PortableExecutable.Is64Bit );
            }
        }

        LastRoot = 0; // force a graph rebuild against the new listing
    }

    if ( RootAddress != LastRoot )
    {
        LastRoot = RootAddress;
        Graph = BuildControlFlowGraph( Instructions, RootAddress );
        Scale = 1.0f;
        LayoutGraph( );
    }
}

void GraphPanel::Draw( const FileBuffer& File, const PEInfo& PortableExecutable )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    if ( !PortableExecutable.IsValid || PortableExecutable.Sections.empty( ) )
    {
        ImGui::TextDisabled( "%s", PortableExecutable.IsValid ? "No sections to graph" : "Not a PE file" );
        return;
    }

    if ( !HasRoot )
        SetRoot( PortableExecutable.ImageBase + PortableExecutable.EntryPointRva );

    if ( ImGui::Button( "Graph from entry" ) )
        SetRoot( PortableExecutable.ImageBase + PortableExecutable.EntryPointRva );

    ImGui::SameLine( );
    if ( ImGui::Button( "Reset view" ) )
    {
        Scale = 1.0f;
        RecenterPending = true;
    }
    ImGui::SameLine( );

    char RootText[ 24 ];
    snprintf( RootText, sizeof( RootText ), Is64BitView ? "0x%016llX" : "0x%08llX",
                   static_cast< unsigned long long >( RootAddress ) );
    ImGui::TextDisabled( "Root %s  |  %zu Blocks%s  |  Drag: Pan  Wheel: Zoom", RootText, Graph.Blocks.size( ),
                         Graph.Truncated ? "  (truncated)" : "" );

    RebuildIfNeeded( File, PortableExecutable );
    ImGui::Separator( );

    const ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos( );
    ImVec2 CanvasSize = ImGui::GetContentRegionAvail( );
    CanvasSize.x = std::max( CanvasSize.x, 50.0f );
    CanvasSize.y = std::max( CanvasSize.y, 50.0f );

    ImGui::InvisibleButton( "##graph_canvas", CanvasSize, ImGuiButtonFlags_MouseButtonLeft );
    const bool Hovered = ImGui::IsItemHovered( );
    const bool Active = ImGui::IsItemActive( );
    ImGuiIO& Io = ImGui::GetIO( );

    if ( Active && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
    {
        Pan.x += Io.MouseDelta.x;
        Pan.y += Io.MouseDelta.y;
    }

    if ( Hovered && Io.MouseWheel != 0.0f )
    {
        const float OldScale = Scale;
        Scale *= ( Io.MouseWheel > 0.0f ) ? 1.1f : ( 1.0f / 1.1f );
        Scale = std::min( std::max( Scale, 0.2f ), 3.0f );

        const float MouseX = Io.MousePos.x - CanvasOrigin.x;
        const float MouseY = Io.MousePos.y - CanvasOrigin.y;
        Pan.x = MouseX - ( MouseX - Pan.x ) * ( Scale / OldScale );
        Pan.y = MouseY - ( MouseY - Pan.y ) * ( Scale / OldScale );
    }

    if ( RecenterPending && Graph.EntryBlock >= 0 && Graph.EntryBlock < static_cast< int >( Graph.Blocks.size( ) ) )
    {
        const CfgBlock& Entry = Graph.Blocks[ Graph.EntryBlock ];
        Pan.x = CanvasSize.x * 0.5f - ( Entry.X + Entry.Width * 0.5f ) * Scale;
        Pan.y = 30.0f - Entry.Y * Scale;
        RecenterPending = false;
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList( );
    const ImVec2 CanvasEnd = ImVec2( CanvasOrigin.x + CanvasSize.x, CanvasOrigin.y + CanvasSize.y );
    DrawList->PushClipRect( CanvasOrigin, CanvasEnd, true );
    DrawList->AddRectFilled( CanvasOrigin, CanvasEnd, ImGui::GetColorU32( ImVec4( 0.03f, 0.03f, 0.04f, 1.0f ) ) );

    if ( Graph.Blocks.empty( ) )
    {
        DrawList->AddText( ImVec2( CanvasOrigin.x + 14.0f, CanvasOrigin.y + 14.0f ), ImGui::GetColorU32( ImGuiCol_TextDisabled ), "No graph at this address" );
        DrawList->PopClipRect( );
        return;
    }

    auto ToScreen = [ & ]( float WorldX, float WorldY )
    {
        return ImVec2( CanvasOrigin.x + Pan.x + WorldX * Scale, CanvasOrigin.y + Pan.y + WorldY * Scale );
    };

    for ( const CfgBlock& Block : Graph.Blocks )
    {
        const ImVec2 From = ToScreen( Block.X + Block.Width * 0.5f, Block.Y + Block.Height );
        for ( const CfgEdge& Edge : Block.Successors )
        {
            if ( Edge.Target < 0 || Edge.Target >= static_cast< int >( Graph.Blocks.size( ) ) )
                continue;
            const CfgBlock& TargetBlock = Graph.Blocks[ Edge.Target ];
            const ImVec2 To = ToScreen( TargetBlock.X + TargetBlock.Width * 0.5f, TargetBlock.Y );
            const ImU32 Color = EdgeColor( Edge.Kind );

            const float Stub = 12.0f * Scale;
            ImVec2 Points[ 6 ];
            int PointCount = 0;
            if ( To.y >= From.y + Stub * 2.0f )
            {
                const float MiddleY = ( From.y + To.y ) * 0.5f;
                Points[ PointCount++ ] = From;
                Points[ PointCount++ ] = ImVec2( From.x, MiddleY );
                Points[ PointCount++ ] = ImVec2( To.x, MiddleY );
                Points[ PointCount++ ] = To;
            }
            else
            {
                const float DownY = From.y + Stub;
                const float UpY = To.y - Stub;
                const float LaneX = std::max( From.x, To.x ) + 50.0f * Scale;
                Points[ PointCount++ ] = From;
                Points[ PointCount++ ] = ImVec2( From.x, DownY );
                Points[ PointCount++ ] = ImVec2( LaneX, DownY );
                Points[ PointCount++ ] = ImVec2( LaneX, UpY );
                Points[ PointCount++ ] = ImVec2( To.x, UpY );
                Points[ PointCount++ ] = To;
            }

            DrawList->AddPolyline( Points, PointCount, Color, ImDrawFlags_None, 1.5f );

            const float Head = 5.0f * Scale;
            DrawList->AddTriangleFilled( To, ImVec2( To.x - Head * 0.7f, To.y - Head ), ImVec2( To.x + Head * 0.7f, To.y - Head ), Color );
        }
    }

    const ImU32 BlockBackground = ImGui::GetColorU32( ImVec4( 0.10f, 0.10f, 0.12f, 0.97f ) );
    const ImU32 BlockBorder = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.18f ) );
    const ImU32 EntryBorder = ImGui::GetColorU32( ImVec4( 0.55f, 0.80f, 0.72f, 0.90f ) );
    const ImU32 TextColor = ImGui::GetColorU32( ImVec4( 0.88f, 0.88f, 0.92f, 1.0f ) );
    const float FontSize = UI::MonoFont->FontSize * Scale;

    for ( int Index = 0; Index < static_cast< int >( Graph.Blocks.size( ) ); ++Index )
    {
        const CfgBlock& Block = Graph.Blocks[ Index ];
        const ImVec2 Min = ToScreen( Block.X, Block.Y );
        const ImVec2 Max = ToScreen( Block.X + Block.Width, Block.Y + Block.Height );

        DrawList->AddRectFilled( Min, Max, BlockBackground, 4.0f );
        const bool IsEntry = Index == Graph.EntryBlock;
        DrawList->AddRect( Min, Max, IsEntry ? EntryBorder : BlockBorder, 4.0f, 0, IsEntry ? 2.0f : 1.0f );

        float TextY = Min.y + BlockPadding * Scale;
        for ( const std::string& Line : Block.Lines )
        {
            DrawList->AddText( UI::MonoFont, FontSize, ImVec2( Min.x + BlockPadding * Scale, TextY ), TextColor,
                               Line.c_str( ) );
            TextY += FontSize;
        }
    }

    DrawList->PopClipRect( );
}