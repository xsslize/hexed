#pragma once

#include "imgui.h"

#include <vector>

class FileBuffer;

struct StructField
{
    int Type = 4;
    int ArrayLength = 8;
    char Name[ 64 ] = { 0 };
};

class StructPanel
{
  public:
    void Draw( const FileBuffer& File );

  private:
    char BaseOffsetText[ 24 ] = "0";
    std::vector< StructField > Fields;
};