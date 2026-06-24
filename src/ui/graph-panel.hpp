#pragma once

#include "analysis/control-flow-graph.hpp"
#include "analysis/disassembler.hpp"

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct PEInfo;
class FileBuffer;

class GraphPanel
{
  public:
    void Draw( const FileBuffer& File, const PEInfo& PortableExecutable );
    void SetRoot( uint64_t Address );
  private:
    void RebuildIfNeeded( const FileBuffer& File, const PEInfo& PortableExecutable );
    void LayoutGraph( );

    ControlFlowGraph Graph;
    std::vector< DisassembledInstruction > Instructions;
    uint64_t RootAddress = 0;
    bool HasRoot = false;
    bool Is64BitView = false;

    ImVec2 Pan = ImVec2( 0.0f, 0.0f );
    float Scale = 1.0f;
    bool RecenterPending = false;

    const void* LastData = nullptr;
    size_t LastSize = 0;
    int LastSection = -2;
    uint64_t LastRoot = 1; // non-zero so the first real root forces a build
};