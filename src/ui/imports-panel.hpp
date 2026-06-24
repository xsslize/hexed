#pragma once

#include "imgui.h"

struct PEInfo;
class FileBuffer;

class ImportsPanel
{
  public:
    void Draw( const PEInfo& PortableExecutable, const FileBuffer& File );
  private:
    ImGuiTextFilter Filter;
};