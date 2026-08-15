#pragma once

// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

// ----------------------------------------------------------------------------
// Defines
// ----------------------------------------------------------------------------

using Microsoft::WRL::ComPtr;

// ----------------------------------------------------------------------------
// Classes
// ----------------------------------------------------------------------------

// Captures a single monitor's contents via the DXGI Desktop Duplication API
// and exposes it as a CPU-side BGRA8 buffer.
class DesktopDuplicator
{
public:

    // States
    bool Init(int monitorIndex);
    void Shutdown();

    // Blocks up to timeoutMs waiting for a new frame.
    // Returns true and fills outBuffer if a new frame arrived.
    // Returns false (buffer untouched) on timeout - just keep drawing the last frame.
    bool GetFrame(std::vector<uint8_t>& outBuffer, UINT timeoutMs = 16);

    // Getters
    RECT GetDesktopRect() const { return desktopRect; }
    UINT Width()          const { return width; }
    UINT Height()         const { return height; }

private:

    ComPtr<ID3D11Device>           device;
    ComPtr<ID3D11DeviceContext>    context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D>        stagingTexture;

    UINT width = 0;
    UINT height = 0;

    RECT desktopRect = {};

};
