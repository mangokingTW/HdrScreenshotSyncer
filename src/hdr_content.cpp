#include "hdr_content.h"

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <DirectXPackedVector.h>

#include <cstdint>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace hdr {
namespace {

// A pixel counts as HDR when its brightest channel exceeds SDR white by this
// margin; the frame counts as HDR when at least this fraction of sampled pixels
// do. SDR content in an HDR framebuffer tops out at SDR white, so the margin
// separates it cleanly from HDR highlights. Both are tunable after real testing.
constexpr float kAboveSdrWhite = 1.10f;       // 10% brighter than SDR white
constexpr double kHdrPixelFraction = 0.0002;  // 0.02% of sampled pixels
constexpr float kDefaultSdrWhiteScrgb = 2.5f;  // 200 nits / 80 (scRGB 1.0 = 80 nits)
constexpr float kBlackFrameMax = 0.01f;        // below this the frame is all black
constexpr UINT kMaxSamplesPerAxis = 512;       // cap the scan cost on large windows

// Captured desktop-duplication state, kept alive between polls and rebuilt when
// the foreground window moves to another output or access is lost.
struct Capture {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutput5> output5;  // kept so duplication can be re-armed cheaply
    ComPtr<IDXGIOutputDuplication> dupl;
    ComPtr<ID3D11Texture2D> staging;
    RECT outputRect{};      // desktop coordinates of the duplicated output
    UINT stagingW = 0;
    UINT stagingH = 0;
    wchar_t gdiName[32]{};  // \\.\DISPLAYn of the duplicated output

    void reset() {
        staging.Reset();
        dupl.Reset();
        output5.Reset();
        context.Reset();
        device.Reset();
        outputRect = RECT{};
        stagingW = 0;
        stagingH = 0;
        gdiName[0] = L'\0';
    }
};

Capture g_cap;
std::optional<bool> g_last;
HWND g_lastFg = nullptr;

// Re-arm the existing duplication so the next AcquireNextFrame returns the
// current desktop even when nothing is animating (its first frame after
// DuplicateOutput1 is always the current image). Used when the foreground window
// changes but stays on the same output.
bool rearm_duplication() {
    if (!g_cap.output5 || !g_cap.device) {
        return false;
    }
    g_cap.dupl.Reset();
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT};
    return SUCCEEDED(g_cap.output5->DuplicateOutput1(g_cap.device.Get(), 0, ARRAYSIZE(formats),
                                                     formats, &g_cap.dupl));
}

void set_status(ScanDiag* diag, const wchar_t* status) {
    if (diag) {
        diag->status = status;
    }
}

// Whether the window belongs to Snipping Tool. While it is foreground the user
// is mid-capture and its translucent overlay isn't the content being shot, so we
// must not re-evaluate off it -- we freeze the decision the real content set.
bool is_snipping_tool(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return false;
    }
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }
    wchar_t path[MAX_PATH];
    DWORD size = ARRAYSIZE(path);
    bool match = false;
    if (QueryFullProcessImageNameW(proc, 0, path, &size)) {
        const wchar_t* base = path;
        for (const wchar_t* p = path; *p; ++p) {
            if (*p == L'\\' || *p == L'/') {
                base = p + 1;
            }
        }
        match = lstrcmpiW(base, L"SnippingTool.exe") == 0;
    }
    CloseHandle(proc);
    return match;
}

// SDR white for the given display in scRGB units (1.0 == 80 nits). Falls back to
// a 200-nit default if the level can't be read.
float sdr_white_scrgb(const wchar_t* gdiDeviceName) {
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        return kDefaultSdrWhiteScrgb;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(),
                           nullptr) != ERROR_SUCCESS) {
        return kDefaultSdrWhiteScrgb;
    }
    paths.resize(pathCount);

    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME src{};
        src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        src.header.size = sizeof(src);
        src.header.adapterId = path.sourceInfo.adapterId;
        src.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS) {
            continue;
        }
        if (lstrcmpiW(src.viewGdiDeviceName, gdiDeviceName) != 0) {
            continue;
        }

        DISPLAYCONFIG_SDR_WHITE_LEVEL white{};
        white.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
        white.header.size = sizeof(white);
        white.header.adapterId = path.targetInfo.adapterId;
        white.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS && white.SDRWhiteLevel > 0) {
            return static_cast<float>(white.SDRWhiteLevel) / 1000.0f;
        }
        return kDefaultSdrWhiteScrgb;
    }
    return kDefaultSdrWhiteScrgb;
}

// Build a D3D11 device + output duplication for the output whose desktop rect
// matches monRect. Requests an FP16 (scRGB) frame so HDR displays come back in
// the wide-range format we scan. On failure, records the step and HRESULT.
bool recreate_for_monitor(const RECT& monRect, const wchar_t* gdiName, ScanDiag* diag) {
    g_cap.reset();
    auto fail = [&](const wchar_t* step, HRESULT h) {
        if (diag) {
            diag->status = step;
            diag->hr = static_cast<long>(h);
        }
        return false;
    };

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return fail(L"init-factory", hr);
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT ai = 0; factory->EnumAdapters1(ai, &adapter) != DXGI_ERROR_NOT_FOUND; ++ai) {
        ComPtr<IDXGIOutput> output;
        for (UINT oi = 0; adapter->EnumOutputs(oi, &output) != DXGI_ERROR_NOT_FOUND; ++oi) {
            DXGI_OUTPUT_DESC desc{};
            if (FAILED(output->GetDesc(&desc)) ||
                std::memcmp(&desc.DesktopCoordinates, &monRect, sizeof(RECT)) != 0) {
                output.Reset();
                continue;
            }

            ComPtr<IDXGIOutput5> output5;
            hr = output.As(&output5);
            if (FAILED(hr)) {
                return fail(L"init-output5", hr);
            }

            const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
            D3D_FEATURE_LEVEL got{};
            hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, levels,
                                   ARRAYSIZE(levels), D3D11_SDK_VERSION, &g_cap.device, &got,
                                   &g_cap.context);
            if (FAILED(hr)) {
                g_cap.reset();
                return fail(L"init-d3d11", hr);
            }

            const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT};
            hr = output5->DuplicateOutput1(g_cap.device.Get(), 0, ARRAYSIZE(formats), formats,
                                           &g_cap.dupl);
            if (FAILED(hr)) {
                g_cap.reset();
                return fail(L"init-duplicate", hr);
            }

            g_cap.output5 = output5;
            g_cap.outputRect = desc.DesktopCoordinates;
            if (!lstrcpynW(g_cap.gdiName, gdiName, ARRAYSIZE(g_cap.gdiName))) {
                g_cap.gdiName[0] = L'\0';
            }
            return true;
        }
        adapter.Reset();
    }
    return fail(L"init-no-output", 0);
}

} // namespace

std::optional<bool> foreground_has_hdr_content(ScanDiag* diag, unsigned long acquireTimeoutMs) {
    HWND fg = GetForegroundWindow();
    if (!fg) {
        set_status(diag, L"no-foreground");
        return g_last;
    }

    if (is_snipping_tool(fg)) {
        // Mid-capture: hold the last content decision so the corrector doesn't
        // flip based on Snipping Tool's own overlay right as the shot is taken.
        set_status(diag, L"snip-foreground");
        return g_last;
    }

    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi)) {
        set_status(diag, L"monitor-info-failed");
        return g_last;
    }

    bool recreated = false;
    if (!g_cap.dupl || std::memcmp(&g_cap.outputRect, &mi.rcMonitor, sizeof(RECT)) != 0) {
        if (!recreate_for_monitor(mi.rcMonitor, mi.szDevice, diag)) {
            return g_last;  // recreate_for_monitor set the step/HRESULT in diag
        }
        recreated = true;
    }
    if (!recreated && fg != g_lastFg) {
        // Foreground window changed on the same output: re-arm so we get a fresh
        // frame to scan even if the new window isn't animating.
        rearm_duplication();
    }
    g_lastFg = fg;
    if (!g_cap.dupl) {
        set_status(diag, L"no-duplication");
        return g_last;
    }

    ComPtr<IDXGIResource> res;
    DXGI_OUTDUPL_FRAME_INFO info{};
    HRESULT hr = g_cap.dupl->AcquireNextFrame(acquireTimeoutMs, &info, &res);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        set_status(diag, L"acquire-timeout");
        return g_last;  // screen unchanged since last scan -- previous decision stands
    }
    if (hr == DXGI_ERROR_ACCESS_LOST) {
        g_cap.reset();
        set_status(diag, L"access-lost");
        return g_last;
    }
    if (FAILED(hr)) {
        set_status(diag, L"acquire-failed");
        return g_last;
    }

    ComPtr<ID3D11Texture2D> frame;
    if (FAILED(res.As(&frame))) {
        g_cap.dupl->ReleaseFrame();
        set_status(diag, L"resource-qi-failed");
        return g_last;
    }

    D3D11_TEXTURE2D_DESC fd{};
    frame->GetDesc(&fd);
    if (fd.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
        // Not an FP16 frame => this output isn't in HDR, so its content is SDR.
        g_cap.dupl->ReleaseFrame();
        set_status(diag, L"sdr-output");
        g_last = false;
        return false;
    }

    // Scan the foreground window's rectangle, clamped to this output; fall back
    // to the whole output if the rect can't be had.
    RECT scan = g_cap.outputRect;
    RECT fr{};
    if (GetWindowRect(fg, &fr)) {
        RECT inter{};
        if (IntersectRect(&inter, &fr, &g_cap.outputRect)) {
            scan = inter;
        }
    }
    const LONG lx = scan.left - g_cap.outputRect.left;
    const LONG ly = scan.top - g_cap.outputRect.top;
    if (lx < 0 || ly < 0 || scan.right <= scan.left || scan.bottom <= scan.top) {
        g_cap.dupl->ReleaseFrame();
        set_status(diag, L"rect-invalid");
        return g_last;
    }
    UINT sw = static_cast<UINT>(scan.right - scan.left);
    UINT sh = static_cast<UINT>(scan.bottom - scan.top);
    if (static_cast<UINT>(lx) + sw > fd.Width) {
        sw = fd.Width - static_cast<UINT>(lx);
    }
    if (static_cast<UINT>(ly) + sh > fd.Height) {
        sh = fd.Height - static_cast<UINT>(ly);
    }
    if (sw == 0 || sh == 0) {
        g_cap.dupl->ReleaseFrame();
        set_status(diag, L"rect-empty");
        return g_last;
    }

    if (!g_cap.staging || g_cap.stagingW != sw || g_cap.stagingH != sh) {
        g_cap.staging.Reset();
        D3D11_TEXTURE2D_DESC sd{};
        sd.Width = sw;
        sd.Height = sh;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(g_cap.device->CreateTexture2D(&sd, nullptr, &g_cap.staging))) {
            g_cap.dupl->ReleaseFrame();
            set_status(diag, L"staging-failed");
            return g_last;
        }
        g_cap.stagingW = sw;
        g_cap.stagingH = sh;
    }

    D3D11_BOX box{};
    box.left = static_cast<UINT>(lx);
    box.top = static_cast<UINT>(ly);
    box.front = 0;
    box.right = box.left + sw;
    box.bottom = box.top + sh;
    box.back = 1;
    g_cap.context->CopySubresourceRegion(g_cap.staging.Get(), 0, 0, 0, 0, frame.Get(), 0, &box);
    g_cap.dupl->ReleaseFrame();

    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(g_cap.context->Map(g_cap.staging.Get(), 0, D3D11_MAP_READ, 0, &map))) {
        set_status(diag, L"map-failed");
        return g_last;
    }

    const float sdrWhite = sdr_white_scrgb(g_cap.gdiName);
    const float threshold = sdrWhite * kAboveSdrWhite;
    const UINT strideX = sw > kMaxSamplesPerAxis ? sw / kMaxSamplesPerAxis : 1;
    const UINT strideY = sh > kMaxSamplesPerAxis ? sh / kMaxSamplesPerAxis : 1;
    uint64_t sampled = 0;
    uint64_t hot = 0;
    float maxChannel = 0.0f;
    const auto* base = static_cast<const uint8_t*>(map.pData);
    for (UINT y = 0; y < sh; y += strideY) {
        const auto* row =
            reinterpret_cast<const DirectX::PackedVector::HALF*>(base + static_cast<size_t>(y) * map.RowPitch);
        for (UINT x = 0; x < sw; x += strideX) {
            const DirectX::PackedVector::HALF* px = row + static_cast<size_t>(x) * 4;
            const float r = DirectX::PackedVector::XMConvertHalfToFloat(px[0]);
            const float g = DirectX::PackedVector::XMConvertHalfToFloat(px[1]);
            const float b = DirectX::PackedVector::XMConvertHalfToFloat(px[2]);
            const float m = r > g ? (r > b ? r : b) : (g > b ? g : b);
            if (m > maxChannel) {
                maxChannel = m;
            }
            ++sampled;
            if (m > threshold) {
                ++hot;
            }
        }
    }
    g_cap.context->Unmap(g_cap.staging.Get(), 0);

    if (sampled == 0) {
        set_status(diag, L"no-samples");
        return g_last;
    }
    const double hotFraction = static_cast<double>(hot) / static_cast<double>(sampled);
    if (diag) {
        diag->status = L"ok";
        diag->sdrWhite = sdrWhite;
        diag->threshold = threshold;
        diag->maxChannel = maxChannel;
        diag->hotFraction = hotFraction;
    }

    // An all-black capture is an artifact of a focus change / alt-tab overlay,
    // not real content -- keep the last decision instead of forcing SDR (which
    // would flicker the setting off mid-game).
    if (maxChannel <= kBlackFrameMax) {
        set_status(diag, L"black-frame");
        return g_last;
    }

    const bool isHdr = hotFraction > kHdrPixelFraction;
    g_last = isHdr;
    return isHdr;
}

} // namespace hdr
