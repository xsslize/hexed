#include "analysis/disassembler.hpp"

#include <capstone/capstone.h>
#include <capstone/x86.h>

#include <cstdio>

namespace
{
    constexpr size_t MaximumInstructions = 5'000'000;
}

std::vector< DisassembledInstruction > Disassemble( const uint8_t* Code, size_t Size, uint64_t Address, bool Is64Bit )
{
    std::vector< DisassembledInstruction > Result;
    if ( Code == nullptr || Size == 0 )
        return Result;

    csh Handle;
    const cs_mode Mode = Is64Bit ? CS_MODE_64 : CS_MODE_32;
    if ( cs_open( CS_ARCH_X86, Mode, &Handle ) != CS_ERR_OK )
        return Result;

    cs_option( Handle, CS_OPT_DETAIL, CS_OPT_ON );

    cs_insn* Instructions = nullptr;
    const size_t Count = cs_disasm( Handle, Code, Size, Address, MaximumInstructions, &Instructions );

    Result.reserve( Count );
    for ( size_t Index = 0; Index < Count; ++Index )
    {
        const cs_insn& Instruction = Instructions[ Index ];

        DisassembledInstruction Decoded;
        Decoded.Address = Instruction.address;
        Decoded.Mnemonic = Instruction.mnemonic;
        Decoded.Operands = Instruction.op_str;

        char ByteBuffer[ 4 ];
        for ( uint16_t ByteIndex = 0; ByteIndex < Instruction.size; ++ByteIndex )
        {
            snprintf( ByteBuffer, sizeof( ByteBuffer ), "%02X ", Instruction.bytes[ ByteIndex ] );
            Decoded.Bytes += ByteBuffer;
        }

        if ( Instruction.detail != nullptr )
        {
            const cs_x86& X86 = Instruction.detail->x86;
            for ( uint8_t OperandIndex = 0; OperandIndex < X86.op_count; ++OperandIndex )
            {
                const cs_x86_op& Operand = X86.operands[ OperandIndex ];
                if ( Operand.type == X86_OP_IMM )
                {
                    Decoded.References.push_back( static_cast< uint64_t >( Operand.imm ) );
                }
                else if ( Operand.type == X86_OP_MEM )
                {
                    if ( Operand.mem.base == X86_REG_RIP )
                        Decoded.References.push_back( Instruction.address + Instruction.size + static_cast< uint64_t >( Operand.mem.disp ) );
                    else if ( Operand.mem.base == X86_REG_INVALID && Operand.mem.index == X86_REG_INVALID && Operand.mem.disp != 0 )
                        Decoded.References.push_back( static_cast< uint64_t >( Operand.mem.disp ) );
                }
            }
        }

        Result.push_back( std::move( Decoded ) );
    }

    if ( Instructions != nullptr )
        cs_free( Instructions, Count );

    cs_close( &Handle );

    return Result;
}