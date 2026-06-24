#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PEImport
{
    std::string Name;
    bool ByOrdinal = false;
    uint16_t Ordinal = 0;
};

struct PEModule
{
    std::string Name;
    std::vector< PEImport > Functions;
};

struct PEExport
{
    std::string Name;
    std::string Forwarder;
    uint16_t Ordinal = 0;
    uint32_t FunctionRva = 0;
    bool ByOrdinalOnly = false;
};

struct PESection
{
    std::string Name;
    uint32_t VirtualAddress = 0;
    uint32_t VirtualSize = 0;
    uint32_t RawDataPointer = 0;
    uint32_t RawDataSize = 0;
    uint32_t Characteristics = 0;

    bool IsExecutable( ) const;
};

struct PEInfo
{
    bool IsValid = false;
    std::string Error;

    bool Is64Bit = false;
    uint16_t Machine = 0;
    uint64_t ImageBase = 0;
    uint32_t EntryPointRva = 0;
    uint16_t SectionCount = 0;

    std::vector< PEModule > Modules;
    std::vector< PESection > Sections;
    std::vector< PEExport > Exports;
    std::string ExportModuleName;

    std::size_t TotalImports( ) const;
    const char* MachineName( ) const;
};

PEInfo ParsePE( const std::vector< uint8_t >& Data );