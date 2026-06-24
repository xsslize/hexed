#include "analysis/pe-parser.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace
{
    // Bounds-checked little-endian read of a trivially-copyable value.
    template < typename ValueType > bool ReadValue( const std::vector< uint8_t >& Data, size_t Offset, ValueType& Output )
    {
        if ( Offset > Data.size( ) || sizeof( ValueType ) > Data.size( ) - Offset )
            return false;
        memcpy( &Output, Data.data( ) + Offset, sizeof( ValueType ) );
        return true;
    }

    // Reads a NUL-terminated ASCII string at a file offset, capped for safety.
    std::string ReadCString( const std::vector< uint8_t >& Data, size_t Offset, size_t MaximumLength = 512 )
    {
        std::string Result;
        for ( size_t Index = 0; Index < MaximumLength && Offset + Index < Data.size( ); ++Index )
        {
            const char Character = static_cast< char >( Data[ Offset + Index ] );
            if ( Character == '\0' )
                break;
            Result.push_back( Character );
        }
        return Result;
    }

    constexpr uint16_t MagicPE32 = 0x010B;
    constexpr uint16_t MagicPE32Plus = 0x020B;

    // Hard caps so a crafted/corrupt file can't make us spin or allocate forever.
    constexpr size_t MaximumModules = 4096;
    constexpr size_t MaximumFunctionsPerModule = 65536;
    constexpr uint32_t MaximumExports = 65536;

    constexpr size_t BadOffset = std::numeric_limits< size_t >::max( );
}

size_t PEInfo::TotalImports( ) const
{
    size_t Total = 0;
    for ( const PEModule& Module : Modules )
        Total += Module.Functions.size( );
    return Total;
}

const char* PEInfo::MachineName( ) const
{
    switch ( Machine )
    {
        case 0x014c: return "x86 (i386)";
        case 0x8664: return "x64 (AMD64)";
        case 0x01c0: return "ARM";
        case 0xaa64: return "ARM64";
        case 0x0200: return "IA64";
        default: return "unknown";
    }
}

bool PESection::IsExecutable( ) const
{
    // IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE
    return ( Characteristics & ( 0x20000000 | 0x00000020 ) ) != 0;
}

PEInfo ParsePE( const std::vector< uint8_t >& Data )
{
    PEInfo Info;

    uint16_t DosMagic = 0;
    if ( !ReadValue( Data, 0, DosMagic ) || DosMagic != 0x5A4D )
    {
        Info.Error = "Not a PE file (missing 'MZ' signature)";
        return Info;
    }

    uint32_t NewHeaderOffset = 0;
    if ( !ReadValue( Data, 0x3C, NewHeaderOffset ) )
    {
        Info.Error = "Truncated DOS header";
        return Info;
    }

    uint32_t PESignature = 0;
    if ( !ReadValue( Data, NewHeaderOffset, PESignature ) || PESignature != 0x00004550 )
    {
        Info.Error = "Invalid PE signature";
        return Info;
    }

    const size_t FileHeaderOffset = static_cast<  size_t  >( NewHeaderOffset ) + 4;
    uint16_t NumberOfSections = 0, SizeOfOptionalHeader = 0;
    ReadValue( Data, FileHeaderOffset + 0, Info.Machine );
    ReadValue( Data, FileHeaderOffset + 2, NumberOfSections );
    ReadValue( Data, FileHeaderOffset + 16, SizeOfOptionalHeader );
    Info.SectionCount = NumberOfSections;

    const size_t OptionalHeaderOffset = FileHeaderOffset + 20;
    uint16_t OptionalMagic = 0;
    if ( !ReadValue( Data, OptionalHeaderOffset, OptionalMagic ) )
    {
        Info.Error = "Truncated optional header";
        return Info;
    }

    size_t DataDirectoryOffset = 0;
    if ( OptionalMagic == MagicPE32 )
    {
        Info.Is64Bit = false;
        uint32_t ImageBase32 = 0;
        ReadValue( Data, OptionalHeaderOffset + 28, ImageBase32 );
        Info.ImageBase = ImageBase32;
        DataDirectoryOffset = OptionalHeaderOffset + 96;
    }
    else if ( OptionalMagic == MagicPE32Plus )
    {
        Info.Is64Bit = true;
        ReadValue( Data, OptionalHeaderOffset + 24, Info.ImageBase );
        DataDirectoryOffset = OptionalHeaderOffset + 112;
    }
    else
    {
        Info.Error = "Unknown optional header magic";
        return Info;
    }

    ReadValue( Data, OptionalHeaderOffset + 16, Info.EntryPointRva );

    uint32_t ImportDirectoryRva = 0;
    ReadValue( Data, DataDirectoryOffset + 1 * 8, ImportDirectoryRva );

    const size_t SectionTableOffset = OptionalHeaderOffset + SizeOfOptionalHeader;
    Info.Sections.reserve( NumberOfSections );

    for ( uint16_t Index = 0; Index < NumberOfSections; ++Index )
    {
        const size_t EntryOffset = SectionTableOffset + static_cast< size_t >( Index ) * 40;

        char NameBuffer[ 9 ] = { 0 };
        for ( int Character = 0; Character < 8; ++Character )
        {
            uint8_t Byte = 0;
            if ( !ReadValue( Data, EntryOffset + Character, Byte ) )
                break;
            NameBuffer[ Character ] = static_cast< char >( Byte );
        }

        PESection CurrentSection;
        CurrentSection.Name = NameBuffer;
        if ( !ReadValue( Data, EntryOffset + 8, CurrentSection.VirtualSize ) ||
             !ReadValue( Data, EntryOffset + 12, CurrentSection.VirtualAddress ) ||
             !ReadValue( Data, EntryOffset + 16, CurrentSection.RawDataSize ) ||
             !ReadValue( Data, EntryOffset + 20, CurrentSection.RawDataPointer ) ||
             !ReadValue( Data, EntryOffset + 36, CurrentSection.Characteristics ) )
            break;

        Info.Sections.push_back( CurrentSection );
    }

    auto RvaToFileOffset = [ & ]( uint32_t Rva ) -> size_t
    {
        for ( const PESection& CurrentSection : Info.Sections )
        {
            const uint32_t Span = CurrentSection.VirtualSize ? CurrentSection.VirtualSize : CurrentSection.RawDataSize;
            if ( Rva >= CurrentSection.VirtualAddress && Rva < CurrentSection.VirtualAddress + Span )
                return static_cast< size_t >( CurrentSection.RawDataPointer ) +
                       ( Rva - CurrentSection.VirtualAddress );
        }

        return BadOffset;
    };

    uint32_t ExportDirectoryRva = 0, ExportDirectorySize = 0;
    ReadValue( Data, DataDirectoryOffset + 0, ExportDirectoryRva );
    ReadValue( Data, DataDirectoryOffset + 4, ExportDirectorySize );

    const size_t ExportOffset = ExportDirectoryRva ? RvaToFileOffset( ExportDirectoryRva ) : BadOffset;
    if ( ExportOffset != BadOffset )
    {
        uint32_t ModuleNameRva = 0, OrdinalBase = 0, NumberOfFunctions = 0, NumberOfNames = 0;
        uint32_t AddressOfFunctions = 0, AddressOfNames = 0, AddressOfNameOrdinals = 0;
        ReadValue( Data, ExportOffset + 12, ModuleNameRva );
        ReadValue( Data, ExportOffset + 16, OrdinalBase );
        ReadValue( Data, ExportOffset + 20, NumberOfFunctions );
        ReadValue( Data, ExportOffset + 24, NumberOfNames );
        ReadValue( Data, ExportOffset + 28, AddressOfFunctions );
        ReadValue( Data, ExportOffset + 32, AddressOfNames );
        ReadValue( Data, ExportOffset + 36, AddressOfNameOrdinals );

        const size_t ModuleNameOffset = RvaToFileOffset( ModuleNameRva );
        if ( ModuleNameOffset != BadOffset )
            Info.ExportModuleName = ReadCString( Data, ModuleNameOffset );

        std::unordered_map< uint16_t, std::string > NameBySlot;
        const size_t NamesOffset = RvaToFileOffset( AddressOfNames );
        const size_t NameOrdinalsOffset = RvaToFileOffset( AddressOfNameOrdinals );
        if ( NamesOffset != BadOffset && NameOrdinalsOffset != BadOffset )
        {
            const uint32_t NameCount = std::min( NumberOfNames, MaximumExports );
            for ( uint32_t Index = 0; Index < NameCount; ++Index )
            {
                uint32_t NameRva = 0;
                uint16_t Slot = 0;
                if ( !ReadValue( Data, NamesOffset + Index * 4, NameRva ) ||
                     !ReadValue( Data, NameOrdinalsOffset + Index * 2, Slot ) )
                    break;
                const size_t NameOffset = RvaToFileOffset( NameRva );
                if ( NameOffset != BadOffset )
                    NameBySlot[ Slot ] = ReadCString( Data, NameOffset );
            }
        }

        const size_t FunctionsOffset = RvaToFileOffset( AddressOfFunctions );
        if ( FunctionsOffset != BadOffset )
        {
            const uint32_t FunctionCount = std::min( NumberOfFunctions, MaximumExports );
            for ( uint32_t Index = 0; Index < FunctionCount; ++Index )
            {
                uint32_t FunctionRva = 0;
                if ( !ReadValue( Data, FunctionsOffset + Index * 4, FunctionRva ) )
                    break;

                if ( FunctionRva == 0 )
                    continue; // unused ordinal slot

                PEExport Export;
                Export.Ordinal = static_cast< uint16_t >( OrdinalBase + Index );
                Export.FunctionRva = FunctionRva;

                const auto NameLookup = NameBySlot.find( static_cast< uint16_t >( Index ) );
                if ( NameLookup != NameBySlot.end( ) )
                    Export.Name = NameLookup->second;
                else
                    Export.ByOrdinalOnly = true;

                if ( FunctionRva >= ExportDirectoryRva && FunctionRva < ExportDirectoryRva + ExportDirectorySize )
                {
                    const size_t ForwarderOffset = RvaToFileOffset( FunctionRva );
                    if ( ForwarderOffset != BadOffset )
                        Export.Forwarder = ReadCString( Data, ForwarderOffset );
                }

                Info.Exports.push_back( std::move( Export ) );
            }
        }
    }

    if ( ImportDirectoryRva == 0 )
    {
        Info.IsValid = true;
        return Info;
    }

    const size_t DescriptorTableOffset = RvaToFileOffset( ImportDirectoryRva );
    if ( DescriptorTableOffset == BadOffset )
    {
        Info.IsValid = true;
        Info.Error = "Import directory RVA could not be mapped to a section";
        return Info;
    }

    const uint64_t OrdinalFlag = Info.Is64Bit ? 0x8000000000000000ULL : 0x0000000080000000ULL;

    for ( size_t ModuleIndex = 0; ModuleIndex < MaximumModules; ++ModuleIndex )
    {
        const size_t DescriptorOffset = DescriptorTableOffset + ModuleIndex * 20;
        uint32_t OriginalFirstThunk = 0, NameRva = 0, FirstThunk = 0;
        if ( !ReadValue( Data, DescriptorOffset + 0, OriginalFirstThunk ) ||
             !ReadValue( Data, DescriptorOffset + 12, NameRva ) ||
             !ReadValue( Data, DescriptorOffset + 16, FirstThunk ) )
            break;

        if ( OriginalFirstThunk == 0 && NameRva == 0 && FirstThunk == 0 )
            break;

        PEModule Module;
        const size_t NameOffset = RvaToFileOffset( NameRva );
        Module.Name = ( NameOffset == BadOffset ) ? "<unmapped name>" : ReadCString( Data, NameOffset );

        const uint32_t ThunkRva = OriginalFirstThunk ? OriginalFirstThunk : FirstThunk;
        const size_t ThunkOffset = RvaToFileOffset( ThunkRva );

        if ( ThunkOffset != BadOffset )
        {
            for ( size_t FunctionIndex = 0; FunctionIndex < MaximumFunctionsPerModule; ++FunctionIndex )
            {
                uint64_t ThunkEntry = 0;
                if ( Info.Is64Bit )
                {
                    if ( !ReadValue( Data, ThunkOffset + FunctionIndex * 8, ThunkEntry ) )
                        break;
                }
                else
                {
                    uint32_t ThunkEntry32 = 0;
                    if ( !ReadValue( Data, ThunkOffset + FunctionIndex * 4, ThunkEntry32 ) )
                        break;

                    ThunkEntry = ThunkEntry32;
                }

                if ( ThunkEntry == 0 )
                    break;

                PEImport Import;
                if ( ThunkEntry & OrdinalFlag )
                {
                    Import.ByOrdinal = true;
                    Import.Ordinal = static_cast< uint16_t >( ThunkEntry & 0xFFFF );
                }
                else
                {
                    const uint32_t HintNameRva = static_cast< uint32_t >( ThunkEntry & 0x7FFFFFFF );
                    const size_t HintNameOffset = RvaToFileOffset( HintNameRva );
                    Import.Name = ( HintNameOffset == BadOffset )
                                      ? "<unmapped name>"
                                      : ReadCString( Data, HintNameOffset + 2 );
                }

                Module.Functions.push_back( std::move( Import ) );
            }
        }

        Info.Modules.push_back( std::move( Module ) );
    }

    Info.IsValid = true;

    return Info;
}