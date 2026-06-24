#include "analysis/string-scanner.hpp"

#include <algorithm>

namespace
{
    // Hard cap so a huge file can't produce an unbounded result set.
    constexpr size_t MaximumStrings = 5'000'000;

    inline bool IsPrintableAscii( uint8_t Byte )
    {
        return Byte >= 0x20 && Byte <= 0x7E;
    }

}

const char* StringEncodingName( StringEncoding Encoding )
{
    switch ( Encoding )
    {
        case StringEncoding::Ascii7Bit: return "ASCII (7-bit)";
        case StringEncoding::CStyleAscii: return "C-style";
        case StringEncoding::Utf16: return "UTF-16";
        case StringEncoding::Utf32: return "UTF-32";
    }

    return "?";
}

std::vector< FoundString > ScanStrings( const std::vector< uint8_t >& Data, int MinimumLength )
{
    std::vector< FoundString > Results;

    if ( MinimumLength < 1 )
        MinimumLength = 1;

    const size_t Minimum = static_cast< size_t >( MinimumLength );
    const size_t Count = Data.size( );

    // Single-byte ASCII (strict 7-bit and C-style)
    for ( size_t Index = 0; Index < Count && Results.size( ) < MaximumStrings; )
    {
        if ( !IsPrintableAscii( Data[ Index ] ) )
        {
            ++Index;
            continue;
        }

        const size_t Start = Index;
        while ( Index < Count && IsPrintableAscii( Data[ Index ] ) )
            ++Index;

        const size_t Length = Index - Start;
        if ( Length >= Minimum )
        {
            const bool NulTerminated = ( Index < Count && Data[ Index ] == 0x00 );
            FoundString Found;
            Found.Offset = Start;
            Found.Encoding = NulTerminated ? StringEncoding::CStyleAscii : StringEncoding::Ascii7Bit;
            Found.Text.assign( reinterpret_cast< const char* >( &Data[ Start ] ), Length );
            Results.push_back( std::move( Found ) );
        }
    }

    // UTF-16LE
    for ( size_t Index = 0; Index + 1 < Count && Results.size( ) < MaximumStrings; )
    {
        if ( !( IsPrintableAscii( Data[ Index ] ) && Data[ Index + 1 ] == 0x00 ) )
        {
            ++Index;
            continue;
        }

        const size_t Start = Index;
        std::string Text;

        while ( Index + 1 < Count && IsPrintableAscii( Data[ Index ] ) && Data[ Index + 1 ] == 0x00 )
        {
            Text.push_back( static_cast< char >( Data[ Index ] ) );
            Index += 2;
        }

        if ( Text.size( ) >= Minimum )
        {
            FoundString Found;
            Found.Offset = Start;
            Found.Encoding = StringEncoding::Utf16;
            Found.Text = std::move( Text );
            Results.push_back( std::move( Found ) );
        }
    }

    // UTF-32LE
    for ( size_t Index = 0; Index + 3 < Count && Results.size( ) < MaximumStrings; )
    {
        if ( !( IsPrintableAscii( Data[ Index ] ) && Data[ Index + 1 ] == 0x00 && Data[ Index + 2 ] == 0x00 && Data[ Index + 3 ] == 0x00 ) )
        {
            ++Index;
            continue;
        }

        const size_t Start = Index;
        std::string Text;

        while ( Index + 3 < Count && IsPrintableAscii( Data[ Index ] ) && Data[ Index + 1 ] == 0x00 && Data[ Index + 2 ] == 0x00 && Data[ Index + 3 ] == 0x00 )
        {
            Text.push_back( static_cast< char >( Data[ Index ] ) );
            Index += 4;
        }

        if ( Text.size( ) >= Minimum )
        {
            FoundString Found;
            Found.Offset = Start;
            Found.Encoding = StringEncoding::Utf32;
            Found.Text = std::move( Text );
            Results.push_back( std::move( Found ) );
        }
    }

    std::sort( Results.begin( ), Results.end( ), []( const FoundString& Left, const FoundString& Right ) { return Left.Offset < Right.Offset; } );

    return Results;
}