#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>

struct PEInfo;
class FileBuffer;

class ExportsPanel
{
  public:
    void Draw( const PEInfo& PortableExecutable, const FileBuffer& File );
    bool ConsumeGotoRequest( uint64_t& OutAddress );
  private:
    void RebuildFilteredIndicesIfNeeded( const PEInfo& PortableExecutable );

    ImGuiTextFilter Filter;
    std::vector< int > FilteredIndices;
    std::string LastFilterText;
    const void* LastExports = nullptr;
    size_t LastExportCount = 0;
    bool FilteredDirty = true;
    int SelectedRow = -1;

    bool HasGotoRequest = false;
    uint64_t RequestedGotoAddress = 0;
};