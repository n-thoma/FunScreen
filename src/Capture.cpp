#include "Capture.h"
#include <dxgi1_2.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

bool DesktopDuplicator::Init(int monitorIndex)
{
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        device.GetAddressOf(), &featureLevel, context.GetAddressOf());

    if (FAILED(hr))
    {
        printf("D3D11CreateDevice failed: 0x%08lx\n", hr);
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(adapter.GetAddressOf());

    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(monitorIndex, output.GetAddressOf());
    if (FAILED(hr)) { printf("EnumOutputs(%d) failed - does that monitor index exist?\n", monitorIndex); return false; }

    DXGI_OUTPUT_DESC outDesc;
    output->GetDesc(&outDesc);
    desktopRect = outDesc.DesktopCoordinates;
    width  = outDesc.DesktopCoordinates.right  - outDesc.DesktopCoordinates.left;
    height = outDesc.DesktopCoordinates.bottom - outDesc.DesktopCoordinates.top;

    ComPtr<IDXGIOutput1> output1;
    output.As(&output1);

    hr = output1->DuplicateOutput(device.Get(), duplication.GetAddressOf());
    if (FAILED(hr))
    {
        printf("DuplicateOutput failed: 0x%08lx (another app may already be duplicating, "
               "or you're on a remote/RDP session which doesn't support this)\n", hr);
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
    if (FAILED(hr))
    {
        printf("CreateTexture2D (staging) failed: 0x%08lx\n", hr);
        return false;
    }

    return true;
}

bool DesktopDuplicator::GetFrame(std::vector<uint8_t>& outBuffer, UINT timeoutMs)
{
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = duplication->AcquireNextFrame(timeoutMs, &frameInfo, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    {
        return false; // no new frame yet so the caller should keep last texture
    }
    if (FAILED(hr))
    {
        printf("AcquireNextFrame failed: 0x%08lx\n", hr);
        return false;
    }

    ComPtr<ID3D11Texture2D> frameTexture;
    desktopResource.As(&frameTexture);

    context->CopyResource(stagingTexture.Get(), frameTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        outBuffer.resize((size_t)width * height * 4);
        uint8_t* dst = outBuffer.data();
        const uint8_t* src = (const uint8_t*)mapped.pData;
        for (UINT y = 0; y < height; ++y)
        {
            memcpy(dst + (size_t)y * width * 4, src + (size_t)y * mapped.RowPitch, (size_t)width * 4);
        }
        context->Unmap(stagingTexture.Get(), 0);
    }

    duplication->ReleaseFrame();
    return SUCCEEDED(hr);
}

void DesktopDuplicator::Shutdown()
{
    duplication.Reset();
    stagingTexture.Reset();
    context.Reset();
    device.Reset();
}
