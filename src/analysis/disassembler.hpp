#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct DisassembledInstruction
{
    uint64_t Address = 0;
    std::string Bytes;
    std::string Mnemonic;
    std::string Operands;

    std::vector< uint64_t > References;
};

std::vector< DisassembledInstruction > Disassemble( const uint8_t* Code, size_t Size, uint64_t Address, bool Is64Bit );