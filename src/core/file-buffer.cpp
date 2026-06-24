#include "core/file-buffer.hpp"

#include <fstream>

bool FileBuffer::Load( const std::string& Path )
{
    std::ifstream Input( Path, std::ios::binary | std::ios::ate );
    if ( !Input )
        return false;

    const std::streamoff FileSize = Input.tellg( );
    if ( FileSize < 0 )
        return false;

    Input.seekg( 0, std::ios::beg );

    std::vector< uint8_t > Data( static_cast<  size_t  >( FileSize ) );
    if ( FileSize > 0 && !Input.read( reinterpret_cast<  char*  >( Data.data( ) ), FileSize ) )
        return false;

    ByteData = std::move( Data );
    FilePath = Path;

    return true;
}

void FileBuffer::Clear( )
{
    ByteData.clear( );
    FilePath.clear( );
}