#pragma once

#include <cstdint>
#include <string>
#include <vector>

class FileBuffer
{
  public:
    bool Load( const std::string& Path );

    void Clear( );

    bool Empty( ) const
    {
        return ByteData.empty( );
    }

    size_t Size( ) const
    {
        return ByteData.size( );
    }

    const std::vector< uint8_t >& Bytes( ) const
    {
        return ByteData;
    }

    const std::string& Path( ) const
    {
        return FilePath;
    }

  private:
    std::vector< uint8_t > ByteData;
    std::string FilePath;
};