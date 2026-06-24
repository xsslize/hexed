#pragma once

#include "analysis/disassembler.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct PEInfo;
class FileBuffer;

class DisassemblyPanel
{
  public:
    void Draw( const FileBuffer& File, const PEInfo& PortableExecutable );
    void ShowXrefsForAddress( uint64_t Address );

    bool ConsumeGraphRequest( uint64_t& OutAddress );

    void GoToAddress( uint64_t Address );
  private:
    void RescanIfNeeded( const FileBuffer& File, const PEInfo& PortableExecutable );
    void BuildStringAnnotations( const FileBuffer& File, const PEInfo& PortableExecutable );

    const std::string* StringAnnotation( const DisassembledInstruction& Instruction ) const;

    void BuildIndices( );

    int RowForAddress( uint64_t Address ) const;

    void NavigateTo( uint64_t Address, bool RecordHistory );
    void NavigateBack( );
    void FindNext( );

    void DrawToolbar( const PEInfo& PortableExecutable );
    void DrawOperands( const std::string& Operands );
    void DrawListing( );
    void HandleShortcuts( );
    void DrawGoToPopup( );
    void DrawXrefsPopup( );

    void FormatAddress( char* Buffer, size_t BufferSize, uint64_t Address ) const;

    std::vector< DisassembledInstruction > Instructions;
    int SelectedSection = -1;
    bool Is64BitView = false;

    std::unordered_map< uint64_t, int > AddressToRow;
    std::unordered_map< uint64_t, std::vector< uint64_t > > Xrefs;
    std::vector< std::pair< int, int > > JumpEdges;
    uint64_t LowestAddress = 0;
    uint64_t HighestAddress = 0;

    int SelectedRow = -1;
    int ScrollToRow = -1;
    std::vector< uint64_t > History;

    char GoToText[ 32 ] = { 0 };
    char SearchText[ 128 ] = { 0 };
    bool OpenGoToPopup = false;
    bool OpenXrefsPopup = false;
    uint64_t XrefTarget = 0;
    bool PendingCodeSection = false;

    bool HasGraphRequest = false;
    uint64_t RequestedGraphAddress = 0;

    bool PendingGoTo = false;
    uint64_t PendingGoToAddress = 0;

    std::unordered_map< uint64_t, std::string > StringByAddress;
    const void* LastStringData = nullptr;
    size_t LastStringSize = 0;

    const void* LastScannedData = nullptr;
    size_t LastScannedSize = 0;
    int LastScannedSection = -2;
};