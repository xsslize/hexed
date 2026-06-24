#pragma once

#include "analysis/pe-parser.hpp"

#include "core/file-buffer.hpp"

#include "ui/disassembly-panel.hpp"
#include "ui/exports-panel.hpp"
#include "ui/graph-panel.hpp"
#include "ui/hex-view.hpp"
#include "ui/imports-panel.hpp"
#include "ui/settings-panel.hpp"
#include "ui/strings-panel.hpp"
#include "ui/struct-panel.hpp"

#include <string>

class App
{
  public:
    bool Render( );
    void OnFileDropped( const std::string& Path );
  private:
    void DrawMenuBar( );
    void DrawSummaryBar( );
    void OpenFileDialog( );
    void LoadFile( const std::string& Path );
    bool FileOffsetToVirtualAddress( size_t Offset, uint64_t& VirtualAddress ) const;

    FileBuffer File;
    PEInfo PortableExecutableInfo;
    HexView HexViewWidget;
    ImportsPanel ImportsPanelWidget;
    ExportsPanel ExportsPanelWidget;
    StringsPanel StringsPanelWidget;
    DisassemblyPanel DisassemblyPanelWidget;
    GraphPanel GraphPanelWidget;
    StructPanel StructPanelWidget;
    SettingsPanel SettingsPanelWidget;
    bool IsRunning = true;
    bool WantHexTab = false;
    bool WantDisasmTab = false;
    bool WantGraphTab = false;
};