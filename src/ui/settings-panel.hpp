#pragma once

#include <string>
#include <vector>

class SettingsPanel
{
  public:
    void Draw( );
  private:
    void DrawConfigurations( );

    int CapturingAction = -1;

    char ConfigNameBuffer[ 64 ] = { 0 };
    std::string SelectedConfig;
    std::vector< std::string > ConfigList;
    bool ConfigListDirty = true;
};