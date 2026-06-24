#pragma once

#include "analysis/string-scanner.hpp"

#include "imgui.h"

#include <cstddef>
#include <string>
#include <vector>

class FileBuffer;

class StringsPanel
{
  public:
    void Draw( const FileBuffer& File );
    bool ConsumeJumpRequest( size_t& OutOffset );

    bool ConsumeXrefRequest( size_t& OutOffset );
  private:
    void RescanIfNeeded( const FileBuffer& File );
    void RebuildFilteredIndicesIfNeeded( );
    bool PassesTypeToggles( StringEncoding Encoding ) const;

    std::vector< FoundString > Results;
    std::vector< int > FilteredIndices;

    int MinimumLength = 4;
    char SearchText[ 128 ] = { 0 };
    bool ShowAscii7Bit = true;
    bool ShowCStyleAscii = true;
    bool ShowUtf16 = true;
    bool ShowUtf32 = true;

    const void* LastScannedData = nullptr;
    size_t LastScannedSize = 0;
    int LastScannedMinimum = -1;
    std::string LastSearchText;
    bool LastShowAscii7Bit = true;
    bool LastShowCStyleAscii = true;
    bool LastShowUtf16 = true;
    bool LastShowUtf32 = true;
    bool FilteredDirty = true;

    int SelectedResultIndex = -1;

    bool HasJumpRequest = false;
    size_t RequestedJumpOffset = 0;

    bool HasXrefRequest = false;
    size_t RequestedXrefOffset = 0;
};