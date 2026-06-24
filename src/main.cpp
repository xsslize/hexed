#include "app/app.hpp"
#include "core/settings.hpp"
#include "ui/theme.hpp"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <shellapi.h>
#include <tchar.h>

static ID3D11Device* D3DDevice = nullptr;
static ID3D11DeviceContext* D3DDeviceContext = nullptr;
static IDXGISwapChain* SwapChain = nullptr;
static ID3D11RenderTargetView* MainRenderTargetView = nullptr;

static App* GApplication = nullptr;

bool CreateDeviceD3D( HWND WindowHandle );
void CleanupDeviceD3D( );
void CreateRenderTarget( );
void CleanupRenderTarget( );

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam );
LRESULT WINAPI WndProc( HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam );

int WINAPI wWinMain( HINSTANCE Instance, HINSTANCE, LPWSTR, int )
{
    WNDCLASSEXW WindowClass = { sizeof( WindowClass ), CS_CLASSDC, WndProc, 0L, 0L, Instance, nullptr, nullptr, nullptr, nullptr, L"Hexed", nullptr };
    RegisterClassExW( &WindowClass );
    HWND WindowHandle = CreateWindowW( WindowClass.lpszClassName, L"Hexed", WS_OVERLAPPEDWINDOW, 100,
                                         100, 1100, 720, nullptr, nullptr, WindowClass.hInstance, nullptr );

    if ( !CreateDeviceD3D( WindowHandle ) )
    {
        CleanupDeviceD3D( );
        UnregisterClassW( WindowClass.lpszClassName, WindowClass.hInstance );
        return 1;
    }

    DragAcceptFiles( WindowHandle, TRUE );
    ShowWindow( WindowHandle, SW_SHOWDEFAULT );
    UpdateWindow( WindowHandle );

    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImGuiIO& Io = ImGui::GetIO( );
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    Io.IniFilename = nullptr;

    UI::ApplyTheme( );
    // Re-apply the last-used configuration (keybinds + theme + font choices) so
    // it persists across launches, then build fonts with those choices.
    Settings::LoadLastConfiguration( );
    Settings::Get( ).FontsDirty = false;
    UI::LoadFonts( );

    ImGui_ImplWin32_Init( WindowHandle );
    ImGui_ImplDX11_Init( D3DDevice, D3DDeviceContext );

    App Application;
    GApplication = &Application;
    const ImVec4 ClearColor = ImVec4( 0.03f, 0.03f, 0.04f, 1.0f );

    bool Done = false;
    while ( !Done )
    {
        MSG Message;
        while ( PeekMessage( &Message, nullptr, 0U, 0U, PM_REMOVE ) )
        {
            TranslateMessage( &Message );
            DispatchMessage( &Message );
            if ( Message.message == WM_QUIT )
                Done = true;
        }

        if ( Done )
            break;

        // Rebuild the font atlas between frames when the Settings tab requests it.
        if ( Settings::Get( ).FontsDirty )
        {
            ImGui::GetIO( ).Fonts->Clear( );
            UI::LoadFonts( );
            ImGui_ImplDX11_InvalidateDeviceObjects( ); // texture is recreated in the next NewFrame
            Settings::Get( ).FontsDirty = false;
        }

        ImGui_ImplDX11_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        if ( !Application.Render( ) )
            Done = true;

        ImGui::Render( );

        const float ClearColorArray[ 4 ] = { ClearColor.x * ClearColor.w, ClearColor.y * ClearColor.w, ClearColor.z * ClearColor.w, ClearColor.w };

        D3DDeviceContext->OMSetRenderTargets( 1, &MainRenderTargetView, nullptr );
        D3DDeviceContext->ClearRenderTargetView( MainRenderTargetView, ClearColorArray );
        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

        SwapChain->Present( 1, 0 );
    }

    GApplication = nullptr;

    ImGui_ImplDX11_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
    ImGui::DestroyContext( );

    CleanupDeviceD3D( );
    DestroyWindow( WindowHandle );
    UnregisterClassW( WindowClass.lpszClassName, WindowClass.hInstance );

    return 0;
}

bool CreateDeviceD3D( HWND WindowHandle )
{
    DXGI_SWAP_CHAIN_DESC SwapChainDescription = { };
    SwapChainDescription.BufferCount = 2;
    SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
    SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDescription.OutputWindow = WindowHandle;
    SwapChainDescription.SampleDesc.Count = 1;
    SwapChainDescription.Windowed = TRUE;
    SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT CreationFlags = 0;
    D3D_FEATURE_LEVEL FeatureLevel;
    const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if ( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreationFlags, FeatureLevels, 2,
                                        D3D11_SDK_VERSION, &SwapChainDescription, &SwapChain, &D3DDevice, &FeatureLevel,
                                        &D3DDeviceContext ) != S_OK )
    {
        return false;
    }

    CreateRenderTarget( );

    return true;
}

void CleanupDeviceD3D( )
{
    CleanupRenderTarget( );

    if ( SwapChain )
    {
        SwapChain->Release( );
        SwapChain = nullptr;
    }

    if ( D3DDeviceContext )
    {
        D3DDeviceContext->Release( );
        D3DDeviceContext = nullptr;
    }

    if ( D3DDevice )
    {
        D3DDevice->Release( );
        D3DDevice = nullptr;
    }
}

void CreateRenderTarget( )
{
    ID3D11Texture2D* BackBuffer = nullptr;
    SwapChain->GetBuffer( 0, IID_PPV_ARGS( &BackBuffer ) );
    D3DDevice->CreateRenderTargetView( BackBuffer, nullptr, &MainRenderTargetView );
    BackBuffer->Release( );
}

void CleanupRenderTarget( )
{
    if ( MainRenderTargetView )
    {
        MainRenderTargetView->Release( );
        MainRenderTargetView = nullptr;
    }
}

LRESULT WINAPI WndProc( HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam )
{
    if ( ImGui_ImplWin32_WndProcHandler( WindowHandle, Message, WParam, LParam ) )
        return true;

    switch ( Message )
    {
        case WM_DROPFILES:
        {
            HDROP DropHandle = reinterpret_cast< HDROP >( WParam );
            char DroppedPath[ MAX_PATH ] = { 0 };
            if ( DragQueryFileA( DropHandle, 0, DroppedPath, MAX_PATH ) && GApplication )
                GApplication->OnFileDropped( DroppedPath );

            DragFinish( DropHandle );

            return 0;
        }
        case WM_SIZE:
        {
            if ( D3DDevice && WParam != SIZE_MINIMIZED )
            {
                CleanupRenderTarget( );
                SwapChain->ResizeBuffers( 0, static_cast<  UINT  >( LOWORD( LParam ) ),
                                          static_cast<  UINT  >( HIWORD( LParam ) ), DXGI_FORMAT_UNKNOWN, 0 );
                CreateRenderTarget( );
            }
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            if ( ( WParam & 0xfff0 ) == SC_KEYMENU ) // disable ALT application menu
                return 0;

            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
            return 0;
        }
    }

    return DefWindowProcW( WindowHandle, Message, WParam, LParam );
}