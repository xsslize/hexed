#include "ui/disassembly-panel.hpp"

#include "analysis/pe-parser.hpp"
#include "analysis/string-scanner.hpp"
#include "core/file-buffer.hpp"
#include "core/settings.hpp"
#include "ui/theme.hpp"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>

namespace
{
    // Syntax-highlight tints — restrained accents that sit on the dark theme
    const ImVec4 ColorMnemonic( 0.86f, 0.87f, 0.92f, 1.0f );
    const ImVec4 ColorControlFlow( 0.93f, 0.69f, 0.42f, 1.0f );
    const ImVec4 ColorRegister( 0.55f, 0.80f, 0.72f, 1.0f );
    const ImVec4 ColorImmediate( 0.83f, 0.71f, 0.52f, 1.0f );
    const ImVec4 ColorKeyword( 0.60f, 0.62f, 0.72f, 1.0f );
    const ImVec4 ColorPunctuation( 0.55f, 0.55f, 0.58f, 1.0f );

    bool IsWordChar( char Character )
    {
        return std::isalnum( static_cast< unsigned char >( Character ) ) || Character == '_';
    }

    std::string ToLower( const std::string& Text )
    {
        std::string Result = Text;
        for ( char& Character : Result )
            Character = static_cast< char >( std::tolower( static_cast< unsigned char >( Character ) ) );
        return Result;
    }

    bool IsAllDigits( const std::string& Text )
    {
        if ( Text.empty( ) )
            return false;

        for ( const char Character : Text )
        {
            if ( !isdigit( static_cast< unsigned char >( Character ) ) )
                return false;
        }

        return true;
    }

    bool IsKeywordToken( const std::string& Low )
    {
        static const std::unordered_set< std::string > Keywords =
        {
            "ptr", "byte", "word", "dword", "qword", "xmmword",
            "ymmword", "zmmword", "tbyte", "fword", "short", "near", "far"
        };

        return Keywords.count( Low ) > 0;
    }

    bool IsRegisterToken( const std::string& Low )
    {
        static const std::unordered_set< std::string > Registers =
        {
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "rip",
            "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip",
            "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
            "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh", "sil", "dil", "bpl", "spl",
            "cs", "ds", "es", "fs", "gs", "ss",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        };

        if ( Registers.count( Low ) > 0 )
            return true;

        if ( Low.size( ) >= 2 && Low[ 0 ] == 'r' )
        {
            size_t Cursor = 1;
            while ( Cursor < Low.size( ) && isdigit( static_cast< unsigned char >( Low[ Cursor ] ) ) )
                ++Cursor;

            if ( Cursor > 1 )
            {
                const int Number = atoi( Low.substr( 1, Cursor - 1 ).c_str( ) );
                const std::string Suffix = Low.substr( Cursor );
                if ( Number >= 8 && Number <= 15 && ( Suffix.empty( ) || Suffix == "b" || Suffix == "w" || Suffix == "d" ) )
                    return true;
            }
        }

        if ( Low.size( ) >= 4 && ( Low.compare( 0, 3, "xmm" ) == 0 || Low.compare( 0, 3, "ymm" ) == 0 || Low.compare( 0, 3, "zmm" ) == 0 ) )
        {
            for ( size_t Index = 3; Index < Low.size( ); ++Index )
            {
                if ( !isdigit( static_cast< unsigned char >( Low[ Index ] ) ) )
                    return false;
            }

            return true;
        }

        return false;
    }

    bool IsControlFlowMnemonic( const std::string& Mnemonic )
    {
        if ( Mnemonic.empty( ) )
            return false;

        if ( Mnemonic[ 0 ] == 'j' )
            return true;

        return Mnemonic == "call" || Mnemonic == "ret" || Mnemonic == "retn" || Mnemonic == "retf" ||
               Mnemonic == "loop" || Mnemonic == "loope" || Mnemonic == "loopne" || Mnemonic == "iret" ||
               Mnemonic == "iretd" || Mnemonic == "iretq" || Mnemonic == "syscall" || Mnemonic == "sysret";
    }

    int DefaultSectionIndex( const PEInfo& PortableExecutable )
    {
        const int Count = static_cast< int >( PortableExecutable.Sections.size( ) );
        for ( int Index = 0; Index < Count; ++Index )
        {
            const PESection& Section = PortableExecutable.Sections[ Index ];
            const uint32_t Span = Section.VirtualSize ? Section.VirtualSize : Section.RawDataSize;
            if ( PortableExecutable.EntryPointRva >= Section.VirtualAddress && PortableExecutable.EntryPointRva < Section.VirtualAddress + Span )
                return Index;
        }

        for ( int Index = 0; Index < Count; ++Index )
        {
            if ( PortableExecutable.Sections[ Index ].IsExecutable( ) )
                return Index;
        }

        return Count > 0 ? 0 : -1;
    }
}

void DisassemblyPanel::FormatAddress( char* Buffer, size_t BufferSize, uint64_t Address ) const
{
    snprintf( Buffer, BufferSize, Is64BitView ? "0x%016llX" : "0x%08llX", static_cast< unsigned long long >( Address ) );
}

int DisassemblyPanel::RowForAddress( uint64_t Address ) const
{
    if ( Instructions.empty( ) || Address < LowestAddress || Address > HighestAddress )
        return -1;

    int Low = 0;
    int High = static_cast< int >( Instructions.size( ) ) - 1;
    int Result = -1;
    while ( Low <= High )
    {
        const int Mid = ( Low + High ) / 2;
        if ( Instructions[ Mid ].Address <= Address )
        {
            Result = Mid;
            Low = Mid + 1;
        }
        else
        {
            High = Mid - 1;
        }
    }

    return Result;
}

void DisassemblyPanel::BuildIndices( )
{
    AddressToRow.clear( );
    Xrefs.clear( );
    JumpEdges.clear( );

    if ( Instructions.empty( ) )
    {
        LowestAddress = 0;
        HighestAddress = 0;
        return;
    }

    AddressToRow.reserve( Instructions.size( ) );
    for ( int Row = 0; Row < static_cast< int >( Instructions.size( ) ); ++Row )
        AddressToRow[ Instructions[ Row ].Address ] = Row;

    LowestAddress = Instructions.front( ).Address;
    HighestAddress = Instructions.back( ).Address;

    for ( int Row = 0; Row < static_cast< int >( Instructions.size( ) ); ++Row )
    {
        const DisassembledInstruction& Instruction = Instructions[ Row ];

        for ( const uint64_t Reference : Instruction.References )
        {
            const int TargetRow = RowForAddress( Reference );
            const uint64_t Key = TargetRow >= 0 ? Instructions[ TargetRow ].Address : Reference;
            Xrefs[ Key ].push_back( Instruction.Address );
        }

        if ( !Instruction.Mnemonic.empty( ) && Instruction.Mnemonic[ 0 ] == 'j' )
        {
            for ( const uint64_t Reference : Instruction.References )
            {
                const int TargetRow = RowForAddress( Reference );
                if ( TargetRow >= 0 )
                {
                    JumpEdges.emplace_back( Row, TargetRow );
                    break;
                }
            }
        }
    }
}

void DisassemblyPanel::NavigateTo( uint64_t Address, bool RecordHistory )
{
    const int Row = RowForAddress( Address );
    if ( Row < 0 )
        return;

    if ( RecordHistory && SelectedRow >= 0 && SelectedRow < static_cast< int >( Instructions.size( ) ) )
        History.push_back( Instructions[ SelectedRow ].Address );

    SelectedRow = Row;
    ScrollToRow = Row;
}

void DisassemblyPanel::NavigateBack( )
{
    if ( History.empty( ) )
        return;

    const uint64_t Address = History.back( );
    History.pop_back( );
    NavigateTo( Address, false );
}

void DisassemblyPanel::ShowXrefsForAddress( uint64_t Address )
{
    XrefTarget = Address;
    OpenXrefsPopup = true;
    PendingCodeSection = true;
}

bool DisassemblyPanel::ConsumeGraphRequest( uint64_t& OutAddress )
{
    if ( !HasGraphRequest )
        return false;

    OutAddress = RequestedGraphAddress;
    HasGraphRequest = false;
    return true;
}

void DisassemblyPanel::GoToAddress( uint64_t Address )
{
    PendingGoTo = true;
    PendingGoToAddress = Address;
}

void DisassemblyPanel::BuildStringAnnotations( const FileBuffer& File, const PEInfo& PortableExecutable )
{
    const std::vector< uint8_t >& Data = File.Bytes( );
    if ( Data.data( ) == LastStringData && Data.size( ) == LastStringSize )
        return;

    LastStringData = Data.data( );
    LastStringSize = Data.size( );
    StringByAddress.clear( );

    const std::vector< FoundString > Strings = ScanStrings( Data, 4 );
    for ( const FoundString& String : Strings )
    {
        uint64_t VirtualAddress = 0;
        bool Mapped = false;
        for ( const PESection& Section : PortableExecutable.Sections )
        {
            const size_t End = static_cast< size_t >( Section.RawDataPointer ) + Section.RawDataSize;
            if ( String.Offset >= Section.RawDataPointer && String.Offset < End )
            {
                VirtualAddress = PortableExecutable.ImageBase + Section.VirtualAddress + ( String.Offset - Section.RawDataPointer );
                Mapped = true;
                break;
            }
        }

        if ( !Mapped )
            continue;

        std::string Text = String.Text;
        if ( Text.size( ) > 48 )
            Text = Text.substr( 0, 48 ) + "...";

        StringByAddress.emplace( VirtualAddress, std::move( Text ) );
    }
}

const std::string* DisassemblyPanel::StringAnnotation( const DisassembledInstruction& Instruction ) const
{
    for ( const uint64_t Reference : Instruction.References )
    {
        const auto Lookup = StringByAddress.find( Reference );
        if ( Lookup != StringByAddress.end( ) )
            return &Lookup->second;
    }

    return nullptr;
}

void DisassemblyPanel::FindNext( )
{
    if ( Instructions.empty( ) || SearchText[ 0 ] == '\0' )
        return;

    const std::string Query = ToLower( SearchText );
    const int Count = static_cast< int >( Instructions.size( ) );
    const int Start = SelectedRow >= 0 ? SelectedRow : -1;

    for ( int Step = 1; Step <= Count; ++Step )
    {
        const int Row = ( ( Start + Step ) % Count + Count ) % Count;
        const DisassembledInstruction& Instruction = Instructions[ Row ];
        const std::string Haystack = ToLower( Instruction.Mnemonic + " " + Instruction.Operands );
        if ( Haystack.find( Query ) != std::string::npos )
        {
            NavigateTo( Instruction.Address, true );
            return;
        }
    }
}

void DisassemblyPanel::RescanIfNeeded( const FileBuffer& File, const PEInfo& PortableExecutable )
{
    const std::vector< uint8_t >& Data = File.Bytes( );
    if ( Data.data( ) == LastScannedData && Data.size( ) == LastScannedSize && SelectedSection == LastScannedSection )
        return;

    LastScannedData = Data.data( );
    LastScannedSize = Data.size( );
    LastScannedSection = SelectedSection;
    Is64BitView = PortableExecutable.Is64Bit;

    Instructions.clear( );
    History.clear( );
    SelectedRow = -1;
    ScrollToRow = -1;

    if ( SelectedSection >= 0 && SelectedSection < static_cast< int >( PortableExecutable.Sections.size( ) ) )
    {
        const PESection& Section = PortableExecutable.Sections[ SelectedSection ];

        const size_t Begin = Section.RawDataPointer;
        size_t Size = Section.RawDataSize;
        if ( Begin < Data.size( ) )
        {
            if ( Begin + Size > Data.size( ) )
                Size = Data.size( ) - Begin;

            if ( Size > 0 )
            {
                const uint64_t Address = PortableExecutable.ImageBase + Section.VirtualAddress;
                Instructions = Disassemble( Data.data( ) + Begin, Size, Address, PortableExecutable.Is64Bit );
            }
        }
    }

    BuildIndices( );

    const uint64_t EntryAddress = PortableExecutable.ImageBase + PortableExecutable.EntryPointRva;
    const int EntryRow = RowForAddress( EntryAddress );
    SelectedRow = EntryRow >= 0 ? EntryRow : ( Instructions.empty( ) ? -1 : 0 );
    ScrollToRow = SelectedRow;
}

void DisassemblyPanel::DrawToolbar( const PEInfo& PortableExecutable )
{
    const int SectionCount = static_cast< int >( PortableExecutable.Sections.size( ) );
    const char* Preview = ( SelectedSection >= 0 && SelectedSection < SectionCount ) ? PortableExecutable.Sections[ SelectedSection ].Name.c_str( ) : "";

    ImGui::SetNextItemWidth( 200.0f );
    if ( ImGui::BeginCombo( "Section", Preview ) )
    {
        for ( int Index = 0; Index < SectionCount; ++Index )
        {
            const PESection& Section = PortableExecutable.Sections[ Index ];
            char Label[ 64 ];
            snprintf( Label, sizeof( Label ), "%-8s%s", Section.Name.c_str( ), Section.IsExecutable( ) ? "  (Executable)" : "" );
            if ( ImGui::Selectable( Label, Index == SelectedSection ) )
                SelectedSection = Index;
        }
        ImGui::EndCombo( );
    }

    ImGui::SameLine( );
    ImGui::SetNextItemWidth( 220.0f );
    if ( ImGui::InputTextWithHint( "##disasm_search", "Find ( Enter = Next )...", SearchText, sizeof( SearchText ), ImGuiInputTextFlags_EnterReturnsTrue ) )
        FindNext( );

    ImGui::SameLine( );
    if ( ImGui::Button( "Go (G)" ) )
        OpenGoToPopup = true;

    ImGui::SameLine( );
    ImGui::BeginDisabled( History.empty( ) );
    if ( ImGui::Button( "Back (Esc)" ) )
        NavigateBack( );
    ImGui::EndDisabled( );

    ImGui::SameLine( );
    ImGui::BeginDisabled( SelectedRow < 0 );
    if ( ImGui::Button( "Graph (Here)" ) && SelectedRow >= 0 )
    {
        HasGraphRequest = true;
        RequestedGraphAddress = Instructions[ SelectedRow ].Address;
    }
    ImGui::EndDisabled( );

    ImGui::SameLine( );
    ImGui::TextDisabled( "%zu instr (%s) | X: Xrefs click address to follow", Instructions.size( ), Is64BitView ? "x86-64" : "x86" );
}

void DisassemblyPanel::DrawOperands( const std::string& Operands )
{
    bool First = true;
    int LinkId = 0;

    auto Emit = [ & ]( const std::string& Piece, const ImVec4* Color )
    {
        if ( Piece.empty( ) )
            return;

        if ( !First )
            ImGui::SameLine( 0.0f, 0.0f );

        First = false;

        if ( Color != nullptr )
            ImGui::PushStyleColor( ImGuiCol_Text, *Color );

        ImGui::TextUnformatted( Piece.c_str( ) );

        if ( Color != nullptr )
            ImGui::PopStyleColor( );
    };

    size_t Index = 0;
    while ( Index < Operands.size( ) )
    {
        if ( !IsWordChar( Operands[ Index ] ) )
        {
            size_t End = Index;
            while ( End < Operands.size( ) && !IsWordChar( Operands[ End ] ) )
                ++End;
            Emit( Operands.substr( Index, End - Index ), &ColorPunctuation );
            Index = End;
            continue;
        }

        size_t End = Index;
        while ( End < Operands.size( ) && IsWordChar( Operands[ End ] ) )
            ++End;

        const std::string Token = Operands.substr( Index, End - Index );
        const std::string Low = ToLower( Token );
        Index = End;

        if ( Low.size( ) > 2 && Low[ 0 ] == '0' && Low[ 1 ] == 'x' )
        {
            const uint64_t Value = std::strtoull( Token.c_str( ) + 2, nullptr, 16 );
            if ( Value >= LowestAddress && Value <= HighestAddress )
            {
                if ( !First )
                    ImGui::SameLine( 0.0f, 0.0f );
                First = false;
                ImGui::PushID( LinkId++ );
                if ( ImGui::TextLink( Token.c_str( ) ) )
                    NavigateTo( Value, true );
                ImGui::PopID( );
            }
            else
            {
                Emit( Token, &ColorImmediate );
            }
        }
        else if ( IsAllDigits( Low ) )
        {
            Emit( Token, &ColorImmediate );
        }
        else if ( IsKeywordToken( Low ) )
        {
            Emit( Token, &ColorKeyword );
        }
        else if ( IsRegisterToken( Low ) )
        {
            Emit( Token, &ColorRegister );
        }
        else
        {
            Emit( Token, nullptr );
        }
    }
}

void DisassemblyPanel::DrawListing( )
{
    const ImGuiTableFlags TableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;

    if ( !ImGui::BeginTable( "##disasm_table", 5, TableFlags ) )
        return;

    ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize |
                                     ImGuiTableColumnFlags_NoHeaderLabel,
                             34.0f );
    ImGui::TableSetupColumn( "Address", ImGuiTableColumnFlags_WidthFixed, 150.0f );
    ImGui::TableSetupColumn( "Bytes", ImGuiTableColumnFlags_WidthFixed, 200.0f );
    ImGui::TableSetupColumn( "Mnemonic", ImGuiTableColumnFlags_WidthFixed, 80.0f );
    ImGui::TableSetupColumn( "Operands", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableHeadersRow( );

    ImGui::PushFont( UI::MonoFont );

    const ImU32 SelectedColor = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.10f ) );
    const float LineHeight = ImGui::GetTextLineHeight( );

    std::unordered_map< int, float > RowCenterY;
    float GutterLeft = 0.0f, GutterRight = 0.0f;
    bool GutterCaptured = false;

    ImGuiListClipper Clipper;
    Clipper.Begin( static_cast< int >( Instructions.size( ) ) );
    if ( ScrollToRow >= 0 )
        Clipper.IncludeItemByIndex( ScrollToRow );

    while ( Clipper.Step( ) )
    {
        for ( int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row )
        {
            const DisassembledInstruction& Instruction = Instructions[ Row ];

            ImGui::TableNextRow( );
            if ( Row == SelectedRow )
                ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, SelectedColor );

            ImGui::PushID( Row );

            ImGui::TableNextColumn( );
            const ImVec2 GutterCursor = ImGui::GetCursorScreenPos( );
            if ( !GutterCaptured )
            {
                GutterLeft = GutterCursor.x;
                GutterRight = GutterCursor.x + ImGui::GetContentRegionAvail( ).x;
                GutterCaptured = true;
            }
            RowCenterY[ Row ] = GutterCursor.y + LineHeight * 0.5f;

            ImGui::TableNextColumn( );
            char AddressText[ 24 ];
            FormatAddress( AddressText, sizeof( AddressText ), Instruction.Address );
            if ( ImGui::Selectable( AddressText, Row == SelectedRow ) )
                SelectedRow = Row;

            ImGui::TableNextColumn( );
            ImGui::TextUnformatted( Instruction.Bytes.c_str( ) );

            ImGui::TableNextColumn( );
            const ImVec4& MnemonicColor =
                IsControlFlowMnemonic( Instruction.Mnemonic ) ? ColorControlFlow : ColorMnemonic;
            ImGui::PushStyleColor( ImGuiCol_Text, MnemonicColor );
            ImGui::TextUnformatted( Instruction.Mnemonic.c_str( ) );
            ImGui::PopStyleColor( );

            ImGui::TableNextColumn( );
            DrawOperands( Instruction.Operands );
            if ( const std::string* Annotation = StringAnnotation( Instruction ) )
            {
                ImGui::SameLine( 0.0f, 0.0f );
                ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.52f, 0.62f, 0.46f, 1.0f ) );
                ImGui::Text( "  ; \"%s\"", Annotation->c_str( ) );
                ImGui::PopStyleColor( );
            }

            if ( Row == ScrollToRow )
            {
                ImGui::SetScrollHereY( 0.3f );
                ScrollToRow = -1;
            }

            ImGui::PopID( );
        }
    }

    ImGui::PopFont( );
    ImGui::EndTable( );

    if ( GutterCaptured && !JumpEdges.empty( ) )
    {
        const ImVec2 ClipMin = ImGui::GetItemRectMin( );
        const ImVec2 ClipMax = ImGui::GetItemRectMax( );
        const ImU32 ForwardColor = ImGui::GetColorU32( ImVec4( 0.55f, 0.62f, 0.72f, 0.85f ) );
        const ImU32 BackwardColor = ImGui::GetColorU32( ImVec4( 0.90f, 0.66f, 0.40f, 0.85f ) );

        ImDrawList* DrawList = ImGui::GetWindowDrawList( );
        DrawList->PushClipRect( ClipMin, ClipMax, true );

        int Lane = 0;
        for ( const std::pair< int, int >& Edge : JumpEdges )
        {
            const auto Source = RowCenterY.find( Edge.first );
            const auto Target = RowCenterY.find( Edge.second );
            if ( Source == RowCenterY.end( ) || Target == RowCenterY.end( ) )
                continue;

            const float SourceY = Source->second;
            const float TargetY = Target->second;
            const bool Backward = Edge.second < Edge.first;
            const ImU32 Color = Backward ? BackwardColor : ForwardColor;
            const float LaneX = GutterRight - 6.0f - static_cast< float >( Lane % 4 ) * 5.0f;
            ++Lane;

            DrawList->AddLine( ImVec2( GutterRight, SourceY ), ImVec2( LaneX, SourceY ), Color );
            DrawList->AddLine( ImVec2( LaneX, SourceY ), ImVec2( LaneX, TargetY ), Color );
            DrawList->AddLine( ImVec2( LaneX, TargetY ), ImVec2( GutterRight - 4.0f, TargetY ), Color );
            DrawList->AddTriangleFilled( ImVec2( GutterRight, TargetY ), ImVec2( GutterRight - 5.0f, TargetY - 3.0f ),
                                         ImVec2( GutterRight - 5.0f, TargetY + 3.0f ), Color );
        }

        DrawList->PopClipRect( );
    }
}

void DisassemblyPanel::HandleShortcuts( )
{
    if ( ImGui::IsAnyItemActive( ) )
        return;

    const Settings::Configuration& Config = Settings::Get( );

    if ( ImGui::IsKeyPressed( Config.GoToAddress, false ) )
        OpenGoToPopup = true;

    if ( ImGui::IsKeyPressed( Config.Xrefs, false ) && SelectedRow >= 0 )
    {
        XrefTarget = Instructions[ SelectedRow ].Address;
        OpenXrefsPopup = true;
    }

    if ( ImGui::IsKeyPressed( Config.NavigateBack, false ) && !History.empty( ) )
        NavigateBack( );
}

void DisassemblyPanel::DrawGoToPopup( )
{
    if ( OpenGoToPopup )
    {
        ImGui::OpenPopup( "Go to address" );
        OpenGoToPopup = false;
    }

    if ( !ImGui::BeginPopupModal( "Go to address", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;

    ImGui::TextUnformatted( "Address (hex):" );
    if ( ImGui::IsWindowAppearing( ) )
        ImGui::SetKeyboardFocusHere( );

    const bool Submitted = ImGui::InputTextWithHint( "##goto", "140001000", GoToText, sizeof( GoToText ),
                                                     ImGuiInputTextFlags_EnterReturnsTrue |
                                                         ImGuiInputTextFlags_CharsHexadecimal );

    ImGui::Separator( );
    if ( ImGui::Button( "Go", ImVec2( 90.0f, 0.0f ) ) || Submitted )
    {
        const uint64_t Target = std::strtoull( GoToText, nullptr, 16 );
        NavigateTo( Target, true );
        ImGui::CloseCurrentPopup( );
    }
    ImGui::SameLine( );
    if ( ImGui::Button( "Cancel", ImVec2( 90.0f, 0.0f ) ) )
        ImGui::CloseCurrentPopup( );

    ImGui::EndPopup( );
}

void DisassemblyPanel::DrawXrefsPopup( )
{
    if ( OpenXrefsPopup )
    {
        ImGui::OpenPopup( "Xrefs" );
        OpenXrefsPopup = false;
    }

    if ( !ImGui::BeginPopup( "Xrefs" ) )
        return;

    char TargetText[ 24 ];
    FormatAddress( TargetText, sizeof( TargetText ), XrefTarget );
    ImGui::Text( "References to %s", TargetText );
    ImGui::Separator( );

    const auto Entry = Xrefs.find( XrefTarget );
    if ( Entry == Xrefs.end( ) || Entry->second.empty( ) )
    {
        ImGui::TextDisabled( "No references found" );
        ImGui::EndPopup( );
        return;
    }

    ImGui::PushFont( UI::MonoFont );
    for ( const uint64_t Source : Entry->second )
    {
        const int Row = RowForAddress( Source );

        char AddressText[ 24 ];
        FormatAddress( AddressText, sizeof( AddressText ), Source );

        char Label[ 160 ];
        if ( Row >= 0 )
            snprintf( Label, sizeof( Label ), "%s  %s %s", AddressText, Instructions[ Row ].Mnemonic.c_str( ), Instructions[ Row ].Operands.c_str( ) );
        else
            snprintf( Label, sizeof( Label ), "%s", AddressText );

        ImGui::PushID( static_cast< int >( Source ) );
        if ( ImGui::Selectable( Label ) )
        {
            NavigateTo( Source, true );
            ImGui::CloseCurrentPopup( );
        }
        ImGui::PopID( );
    }
    ImGui::PopFont( );

    ImGui::EndPopup( );
}

void DisassemblyPanel::Draw( const FileBuffer& File, const PEInfo& PortableExecutable )
{
    if ( File.Empty( ) )
    {
        ImGui::TextDisabled( "No file loaded" );
        return;
    }

    if ( !PortableExecutable.IsValid || PortableExecutable.Sections.empty( ) )
    {
        ImGui::TextDisabled( "%s", PortableExecutable.IsValid ? "No sections to disassemble" : "Not a PE file" );
        return;
    }

    if ( File.Bytes( ).data( ) != LastScannedData || File.Bytes( ).size( ) != LastScannedSize )
        SelectedSection = DefaultSectionIndex( PortableExecutable );

    if ( PendingCodeSection )
    {
        SelectedSection = DefaultSectionIndex( PortableExecutable );
        PendingCodeSection = false;
    }

    DrawToolbar( PortableExecutable );
    RescanIfNeeded( File, PortableExecutable );
    BuildStringAnnotations( File, PortableExecutable );

    if ( PendingGoTo )
    {
        NavigateTo( PendingGoToAddress, true );
        PendingGoTo = false;
    }

    ImGui::Separator( );

    DrawListing( );

    HandleShortcuts( );
    DrawGoToPopup( );
    DrawXrefsPopup( );
}