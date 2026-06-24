#pragma once

#include <cstddef>
#include <string>

class FileBuffer;

class HexView
{
  public:
    void Draw( const FileBuffer& File );
    void GoTo( size_t Offset );
  private:
    void FindNext( const FileBuffer& File );

    int BytesPerRow = 16;
    int GotoAddress = 0;
    bool GotoPending = false;

    char SearchText[ 128 ] = { 0 };
    bool SearchHexMode = true;
    std::string LastSearchText;
    bool LastHexMode = true;
    bool HasMatch = false;
    size_t MatchOffset = 0;
    size_t MatchLength = 0;
};