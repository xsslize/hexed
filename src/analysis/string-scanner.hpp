#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class StringEncoding
{
    Ascii7Bit,
    CStyleAscii,
    Utf16,
    Utf32,
};

const char* StringEncodingName( StringEncoding Encoding );

struct FoundString
{
    size_t Offset = 0;
    StringEncoding Encoding = StringEncoding::Ascii7Bit;
    std::string Text;
};

std::vector< FoundString > ScanStrings( const std::vector< uint8_t >& Data, int MinimumLength );