#pragma once

#include "analysis/disassembler.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct CfgEdge
{
    int Target = -1;
    int Kind = 0; // 0 = unconditional / fall-through, 1 = conditional taken, 2 = conditional fall-through
};

struct CfgBlock
{
    uint64_t StartAddress = 0;
    std::vector< std::string > Lines;
    std::vector< CfgEdge > Successors;
    int Layer = 0;

    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

struct ControlFlowGraph
{
    std::vector< CfgBlock > Blocks;
    int EntryBlock = -1;
    bool Truncated = false;
};

ControlFlowGraph BuildControlFlowGraph( const std::vector< DisassembledInstruction >& Instructions, uint64_t StartAddress );