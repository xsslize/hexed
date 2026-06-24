#include "analysis/control-flow-graph.hpp"

#include <cstdio>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr size_t MaximumFunctionInstructions = 20000;

    bool IsReturn( const std::string& Mnemonic )
    {
        return Mnemonic.compare( 0, 3, "ret" ) == 0 || Mnemonic == "iret" || Mnemonic == "iretd" || Mnemonic == "iretq";
    }

    bool IsJump( const std::string& Mnemonic )
    {
        return !Mnemonic.empty( ) && Mnemonic[ 0 ] == 'j';
    }

    bool IsConditionalJump( const std::string& Mnemonic )
    {
        return IsJump( Mnemonic ) && Mnemonic != "jmp";
    }
}

ControlFlowGraph BuildControlFlowGraph( const std::vector< DisassembledInstruction >& Instructions, uint64_t StartAddress )
{
    ControlFlowGraph Graph;
    const int Count = static_cast< int >( Instructions.size( ) );
    if ( Count == 0 )
        return Graph;

    std::unordered_map< uint64_t, int > AddressToIndex;
    AddressToIndex.reserve( Count );
    for ( int Index = 0; Index < Count; ++Index )
        AddressToIndex[ Instructions[ Index ].Address ] = Index;

    const auto StartLookup = AddressToIndex.find( StartAddress );
    if ( StartLookup == AddressToIndex.end( ) )
        return Graph;
    const int StartIndex = StartLookup->second;

    auto BranchTargetIndex = [ & ]( const DisassembledInstruction& Instruction ) -> int
    {
        for ( const uint64_t Reference : Instruction.References )
        {
            const auto Lookup = AddressToIndex.find( Reference );
            if ( Lookup != AddressToIndex.end( ) )
                return Lookup->second;
        }
        return -1;
    };

    std::unordered_set< int > Reachable;
    std::vector< int > Stack = { StartIndex };
    while ( !Stack.empty( ) )
    {
        int Index = Stack.back( );
        Stack.pop_back( );
        while ( Index >= 0 && Index < Count && Reachable.count( Index ) == 0 )
        {
            if ( Reachable.size( ) >= MaximumFunctionInstructions )
            {
                Graph.Truncated = true;
                break;
            }
            Reachable.insert( Index );

            const DisassembledInstruction& Instruction = Instructions[ Index ];
            if ( IsReturn( Instruction.Mnemonic ) )
                break;
            if ( IsJump( Instruction.Mnemonic ) )
            {
                const int Target = BranchTargetIndex( Instruction );
                if ( Target >= 0 )
                    Stack.push_back( Target );
                if ( IsConditionalJump( Instruction.Mnemonic ) && Index + 1 < Count )
                    Stack.push_back( Index + 1 );
                break;
            }
            ++Index;
        }
    }
    if ( Reachable.empty( ) )
        return Graph;

    std::set< int > Leaders;
    Leaders.insert( StartIndex );
    for ( const int Index : Reachable )
    {
        const DisassembledInstruction& Instruction = Instructions[ Index ];
        if ( !IsJump( Instruction.Mnemonic ) && !IsReturn( Instruction.Mnemonic ) )
            continue;

        if ( IsJump( Instruction.Mnemonic ) )
        {
            const int Target = BranchTargetIndex( Instruction );
            if ( Target >= 0 && Reachable.count( Target ) > 0 )
                Leaders.insert( Target );
        }
        if ( Index + 1 < Count && Reachable.count( Index + 1 ) > 0 )
            Leaders.insert( Index + 1 );
    }

    const std::vector< int > LeaderList( Leaders.begin( ), Leaders.end( ) );
    std::unordered_map< int, int > LeaderToBlock;
    for ( int BlockIndex = 0; BlockIndex < static_cast< int >( LeaderList.size( ) ); ++BlockIndex )
        LeaderToBlock[ LeaderList[ BlockIndex ] ] = BlockIndex;

    Graph.Blocks.resize( LeaderList.size( ) );
    for ( int BlockIndex = 0; BlockIndex < static_cast< int >( LeaderList.size( ) ); ++BlockIndex )
    {
        CfgBlock& Block = Graph.Blocks[ BlockIndex ];
        int Index = LeaderList[ BlockIndex ];
        Block.StartAddress = Instructions[ Index ].Address;

        while ( Index < Count && Reachable.count( Index ) > 0 )
        {
            const DisassembledInstruction& Instruction = Instructions[ Index ];

            char Line[ 256 ];
            snprintf( Line, sizeof( Line ), "0x%llX  %s%s%s",
                           static_cast< unsigned long long >( Instruction.Address ), Instruction.Mnemonic.c_str( ),
                           Instruction.Operands.empty( ) ? "" : " ", Instruction.Operands.c_str( ) );

            Block.Lines.push_back( Line );

            if ( IsReturn( Instruction.Mnemonic ) )
                break;

            if ( IsJump( Instruction.Mnemonic ) )
            {
                const int Target = BranchTargetIndex( Instruction );
                if ( Target >= 0 && LeaderToBlock.count( Target ) > 0 )
                    Block.Successors.push_back( { LeaderToBlock[ Target ], IsConditionalJump( Instruction.Mnemonic ) ? 1 : 0 } );
                if ( IsConditionalJump( Instruction.Mnemonic ) && Index + 1 < Count && LeaderToBlock.count( Index + 1 ) > 0 )
                    Block.Successors.push_back( { LeaderToBlock[ Index + 1 ], 2 } );
                break;
            }

            const int Next = Index + 1;
            if ( Next >= Count || Reachable.count( Next ) == 0 )
                break;

            if ( LeaderToBlock.count( Next ) > 0 )
            {
                Block.Successors.push_back( { LeaderToBlock[ Next ], 0 } );
                break;
            }

            Index = Next;
        }
    }

    Graph.EntryBlock = LeaderToBlock.count( StartIndex ) > 0 ? LeaderToBlock[ StartIndex ] : 0;

    for ( CfgBlock& Block : Graph.Blocks )
        Block.Layer = -1;

    std::queue< int > Queue;
    Graph.Blocks[ Graph.EntryBlock ].Layer = 0;
    Queue.push( Graph.EntryBlock );
    while ( !Queue.empty( ) )
    {
        const int BlockIndex = Queue.front( );
        Queue.pop( );
        for ( const CfgEdge& Edge : Graph.Blocks[ BlockIndex ].Successors )
        {
            if ( Graph.Blocks[ Edge.Target ].Layer < 0 )
            {
                Graph.Blocks[ Edge.Target ].Layer = Graph.Blocks[ BlockIndex ].Layer + 1;
                Queue.push( Edge.Target );
            }
        }
    }

    for ( CfgBlock& Block : Graph.Blocks )
    {
        if ( Block.Layer < 0 )
            Block.Layer = 0;
    }

    return Graph;
}