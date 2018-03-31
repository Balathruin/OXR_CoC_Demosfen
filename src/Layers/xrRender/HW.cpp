#include "stdafx.h"

#include "Layers/xrRender/HW.h"
#include "xrEngine/XR_IOConsole.h"
#include "xrCore/xr_token.h"

#ifndef _EDITOR
void fill_vid_mode_list(CHW* _hw);
void free_vid_mode_list();

void fill_render_mode_list();
void free_render_mode_list();
#else
void fill_vid_mode_list(CHW* _hw) {}
void free_vid_mode_list() {}
void fill_render_mode_list() {}
void free_render_mode_list() {}
#endif

CHW HW;

CHW::CHW()
{

}

CHW::~CHW()
{

}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
void CHW::CreateD3D()
{
    hD3D = XRay::LoadModule(GEnv.isDedicatedServer ? "xrD3D9-Null" : "d3d9.dll");
    R_ASSERT2(hD3D->exist(), "Can't find 'd3d9.dll'\nPlease install latest version of DirectX before running this program");

    typedef IDirect3D9* WINAPI _Direct3DCreate9(UINT SDKVersion);
    auto createD3D = (_Direct3DCreate9*)hD3D->getProcAddress("Direct3DCreate9");
    R_ASSERT(createD3D);
    pD3D = createD3D(D3D_SDK_VERSION);
    R_ASSERT2(pD3D, "Please install DirectX 9.0c");
}

void CHW::DestroyD3D()
{
    _SHOW_REF("refCount:pD3D", pD3D);
    _RELEASE(pD3D);
}

void CHW::CreateDevice(HWND m_hWnd, bool move_window)
{
    m_move_window = move_window;
    CreateD3D();

    if (strstr(Core.Params, "-wnd_mode"))
        psDeviceFlags.set(rsFullscreen, false);

    bool bWindowed = !psDeviceFlags.is(rsFullscreen);

#ifndef _EDITOR
    if (GEnv.isDedicatedServer)
        bWindowed = true;
#endif

    m_DriverType = Caps.bForceGPU_REF ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL;

    DevAdapter = D3DADAPTER_DEFAULT;

    // Display the name of video board
    D3DADAPTER_IDENTIFIER9 adapterID;
    R_CHK(pD3D->GetAdapterIdentifier(DevAdapter, 0, &adapterID));
    Msg("* GPU [vendor:%X]-[device:%X]: %s", adapterID.VendorId, adapterID.DeviceId, adapterID.Description);

    u16 drv_Product    = HIWORD(adapterID.DriverVersion.HighPart);
    u16 drv_Version    = LOWORD(adapterID.DriverVersion.HighPart);
    u16 drv_SubVersion = HIWORD(adapterID.DriverVersion.LowPart);
    u16 drv_Build      = LOWORD(adapterID.DriverVersion.LowPart);
    Msg("* GPU driver: %d.%d.%d.%d", u32(drv_Product), u32(drv_Version), u32(drv_SubVersion), u32(drv_Build));

    Caps.id_vendor = adapterID.VendorId;
    Caps.id_device = adapterID.DeviceId;

    // Retreive windowed mode
    D3DDISPLAYMODE mWindowed;
    R_CHK(pD3D->GetAdapterDisplayMode(DevAdapter, &mWindowed));

    // Select back-buffer & depth-stencil format
    D3DFORMAT& fTarget = Caps.fTarget;
    D3DFORMAT& fDepth = Caps.fDepth;
    if (bWindowed)
    {
        fTarget = mWindowed.Format;
        R_CHK(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, TRUE));
        fDepth = selectDepthStencil(fTarget);
    }
    else
    {
        switch (psCurrentBPP)
        {
        case 32:
            fTarget = D3DFMT_X8R8G8B8;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_A8R8G8B8;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_R8G8B8;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_UNKNOWN;
            break;
        case 16:
        default:
            fTarget = D3DFMT_R5G6B5;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_X1R5G5B5;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_X4R4G4B4;
            if (SUCCEEDED(pD3D->CheckDeviceType(DevAdapter, m_DriverType, fTarget, fTarget, FALSE)))
                break;
            fTarget = D3DFMT_UNKNOWN;
            break;
        }
        fDepth = selectDepthStencil(fTarget);
    }

    if ((D3DFMT_UNKNOWN == fTarget) || (D3DFMT_UNKNOWN == fTarget))
    {
        Msg("Failed to initialize graphics hardware.\n"
            "Please try to restart the game.\n"
            "Can not find matching format for back buffer.");
        FlushLog();
        MessageBox(nullptr, "Failed to initialize graphics hardware.\nPlease try to restart the game.", "Error!",
            MB_OK | MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 0);
    }

    // Set up the presentation parameters
    D3DPRESENT_PARAMETERS& P = DevPP;
    ZeroMemory(&P, sizeof(P));

#ifndef _EDITOR
    selectResolution(P.BackBufferWidth, P.BackBufferHeight, bWindowed);
#endif
    // Back buffer
    //. P.BackBufferWidth       = dwWidth;
    //. P.BackBufferHeight      = dwHeight;
    P.BackBufferFormat = fTarget;
    P.BackBufferCount = 1;

    // Multisample
    P.MultiSampleType = D3DMULTISAMPLE_NONE;
    P.MultiSampleQuality = 0;

    // Windoze
    P.SwapEffect = bWindowed ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;
    P.hDeviceWindow = m_hWnd;
    P.Windowed = bWindowed;

    // Depth/stencil
    P.EnableAutoDepthStencil = TRUE;
    P.AutoDepthStencilFormat = fDepth;
    P.Flags = 0; //. D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;

    // Refresh rate
    if (bWindowed)
    {
        P.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        P.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    }
    else
    {
        P.PresentationInterval = selectPresentInterval(); // Vsync (R1\R2)
        P.FullScreen_RefreshRateInHz = selectRefresh(P.BackBufferWidth, P.BackBufferHeight, fTarget);
    }

    // Create the device
    u32 GPU = selectGPU();
    HRESULT R = HW.pD3D->CreateDevice(DevAdapter, m_DriverType, m_hWnd,
        GPU | D3DCREATE_MULTITHREADED, //. ? locks at present
        &P, &pDevice);

    if (FAILED(R))
    {
        R = HW.pD3D->CreateDevice(DevAdapter, m_DriverType, m_hWnd,
            GPU | D3DCREATE_MULTITHREADED, //. ? locks at present
            &P, &pDevice);
    }
    if (D3DERR_DEVICELOST == R)
    {
        // Fatal error! Cannot create rendering device AT STARTUP !!!
        Msg("Failed to initialize graphics hardware.\n"
            "Please try to restart the game.\n"
            "CreateDevice returned 0x%08x(D3DERR_DEVICELOST)", R);
        FlushLog();
        MessageBox(nullptr, "Failed to initialize graphics hardware.\nPlease try to restart the game.", "Error!",
            MB_OK | MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 0);
    };

    _SHOW_REF("* CREATE: DeviceREF:", HW.pDevice);
    switch (GPU)
    {
    case D3DCREATE_SOFTWARE_VERTEXPROCESSING:                        Log("* Vertex Processor: SOFTWARE"     ); break;
    case D3DCREATE_MIXED_VERTEXPROCESSING:                           Log("* Vertex Processor: MIXED"        ); break;
    case D3DCREATE_HARDWARE_VERTEXPROCESSING:                        Log("* Vertex Processor: HARDWARE"     ); break;
    case D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE: Log("* Vertex Processor: PURE HARDWARE"); break;
    }

// Capture misc data
#ifdef DEBUG
    R_CHK(pDevice->CreateStateBlock(D3DSBT_ALL, &dwDebugSB));
#endif
    R_CHK(pDevice->GetRenderTarget(0, &pBaseRT));
    R_CHK(pDevice->GetDepthStencilSurface(&pBaseZB));
    u32 memory = pDevice->GetAvailableTextureMem();
    Msg("*   Texture memory: %d M", memory / (1024 * 1024));
    Msg("*        DDI-level: %2.1f", float(D3DXGetDriverLevel(pDevice)) / 100.f);
#ifndef _EDITOR
    updateWindowProps(m_hWnd);
    fill_vid_mode_list(this);
#endif
}

void CHW::DestroyDevice()
{
    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _RELEASE(pBaseZB);

    _SHOW_REF("refCount:pBaseRT", pBaseRT);
    _RELEASE(pBaseRT);
#ifdef DEBUG
    _SHOW_REF("refCount:dwDebugSB", dwDebugSB);
    _RELEASE(dwDebugSB);
#endif
    _SHOW_REF("DeviceREF:", HW.pDevice);
    _RELEASE(HW.pDevice);

    DestroyD3D();

#ifndef _EDITOR
    free_vid_mode_list();
#endif
}

//////////////////////////////////////////////////////////////////////
// Resetting device
//////////////////////////////////////////////////////////////////////
void CHW::Reset(HWND hwnd)
{
#ifdef DEBUG
    _RELEASE(dwDebugSB);
#endif
    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _SHOW_REF("refCount:pBaseRT", pBaseRT);
    _RELEASE(pBaseZB);
    _RELEASE(pBaseRT);

#ifndef _EDITOR
    bool bWindowed = true;
    if (strstr(Core.Params, "-wnd_mode"))
        psDeviceFlags.set(rsFullscreen, false);

    if (!GEnv.isDedicatedServer)
        bWindowed = !psDeviceFlags.is(rsFullscreen);

    selectResolution(DevPP.BackBufferWidth, DevPP.BackBufferHeight, bWindowed);
    // Windoze
    DevPP.SwapEffect = bWindowed ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;
    DevPP.Windowed = bWindowed;
    if (!bWindowed)
    {
        DevPP.PresentationInterval = selectPresentInterval(); // Vsync (R1\R2)
        DevPP.FullScreen_RefreshRateInHz = selectRefresh(DevPP.BackBufferWidth, DevPP.BackBufferHeight, Caps.fTarget);
    }
    else
    {
        DevPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        DevPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    }
#endif

    while (true)
    {
        HRESULT _hr = HW.pDevice->Reset(&DevPP);
        if (SUCCEEDED(_hr))
            break;
        Msg("! ERROR: [%dx%d]: %s", DevPP.BackBufferWidth, DevPP.BackBufferHeight, xrDebug::ErrorToString(_hr));
        Sleep(100);
    }
    R_CHK(pDevice->GetRenderTarget(0, &pBaseRT));
    R_CHK(pDevice->GetDepthStencilSurface(&pBaseZB));
#ifdef DEBUG
    R_CHK(pDevice->CreateStateBlock(D3DSBT_ALL, &dwDebugSB));
#endif
#ifndef _EDITOR
    updateWindowProps(hwnd);
    ShowWindow(hwnd, SW_SHOWNORMAL);
#endif
}

D3DFORMAT CHW::selectDepthStencil(D3DFORMAT fTarget)
{
    // R2 hack
#pragma todo("R2 need to specify depth format")
    if (GEnv.CurrentRenderer == 2)
        return D3DFMT_D24S8;

    // R1 usual
    static D3DFORMAT fDS_Try1[6] = { D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D32, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D15S1 };

    D3DFORMAT* fDS_Try = fDS_Try1;
    int fDS_Cnt = 6;

    for (int it = 0; it < fDS_Cnt; it++)
    {
        if (SUCCEEDED(pD3D->CheckDeviceFormat(
            DevAdapter, m_DriverType, fTarget, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, fDS_Try[it])))
        {
            if (SUCCEEDED(pD3D->CheckDepthStencilMatch(DevAdapter, m_DriverType, fTarget, fTarget, fDS_Try[it])))
            {
                return fDS_Try[it];
            }
        }
    }
    return D3DFMT_UNKNOWN;
}

void CHW::selectResolution(u32& dwWidth, u32& dwHeight, BOOL bWindowed)
{
    fill_vid_mode_list(this);
#ifndef _EDITOR
    if (GEnv.isDedicatedServer)
    {
        dwWidth = 640;
        dwHeight = 480;
    }
    else
#endif
    {
        if (bWindowed)
        {
            dwWidth = psCurrentVidMode[0];
            dwHeight = psCurrentVidMode[1];
        }
        else // check
        {
#ifndef _EDITOR
            string64 buff;
            xr_sprintf(buff, sizeof(buff), "%dx%d", psCurrentVidMode[0], psCurrentVidMode[1]);

            if (_ParseItem(buff, GEnv.vid_mode_token) == u32(-1)) // not found
            { // select safe
                xr_sprintf(buff, sizeof(buff), "vid_mode %s", GEnv.vid_mode_token[0].name);
                Console->Execute(buff);
            }

            dwWidth = psCurrentVidMode[0];
            dwHeight = psCurrentVidMode[1];
#endif
        }
    }
}

u32 CHW::selectPresentInterval()
{
    D3DCAPS9 caps;
    pD3D->GetDeviceCaps(DevAdapter, m_DriverType, &caps);

    if (!psDeviceFlags.test(rsVSync))
    {
        if (caps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
            return D3DPRESENT_INTERVAL_IMMEDIATE;
        if (caps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
            return D3DPRESENT_INTERVAL_ONE;
    }
    return D3DPRESENT_INTERVAL_DEFAULT;
}

u32 CHW::selectGPU()
{
#if RENDER == R_R1
    BOOL isIntelGMA = FALSE;

    if (Caps.id_vendor == 0x8086)
    { // Intel

#define GMA_SL_SIZE 43

        DWORD IntelGMA_SoftList[GMA_SL_SIZE] = {0x2782, 0x2582, 0x2792, 0x2592, 0x2772, 0x2776, 0x27A2, 0x27A6, 0x27AE,
            0x2982, 0x2983, 0x2992, 0x2993, 0x29A2, 0x29A3, 0x2972, 0x2973, 0x2A02, 0x2A03, 0x2A12, 0x2A13, 0x29C2,
            0x29C3, 0x29B2, 0x29B3, 0x29D2, 0x29D3,

            0x2A42, 0x2A43, 0x2E02, 0x2E03, 0x2E12, 0x2E13, 0x2E22, 0x2E23, 0x2E32, 0x2E33, 0x2E42, 0x2E43, 0x2E92,
            0x2E93, 0x0042, 0x0046};

        for (int idx = 0; idx < GMA_SL_SIZE; ++idx)
            if (IntelGMA_SoftList[idx] == Caps.id_device)
            {
                isIntelGMA = TRUE;
                break;
            }
    }

    if (isIntelGMA)
        switch (ps_r1_SoftwareSkinning)
        {
        case 0:
            Msg("* Enabling software skinning");
            ps_r1_SoftwareSkinning = 1;
            break;
        case 1: Msg("* Using software skinning"); break;
        case 2:
            Msg("* WARNING: Using hardware skinning");
            Msg("*   setting 'r1_software_skinning' to '1' may improve performance");
            break;
        }
    else if (ps_r1_SoftwareSkinning == 1)
    {
        Msg("* WARNING: Using software skinning");
        Msg("*   setting 'r1_software_skinning' to '0' should improve performance");
    }

#endif // RENDER == R_R1

    if (Caps.bForceGPU_SW)
        return D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    D3DCAPS9 caps;
    pD3D->GetDeviceCaps(DevAdapter, m_DriverType, &caps);

    if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
    {
        if (Caps.bForceGPU_NonPure)
            return D3DCREATE_HARDWARE_VERTEXPROCESSING;
        else
        {
            if (caps.DevCaps & D3DDEVCAPS_PUREDEVICE)
                return D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
            else
                return D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
        // return D3DCREATE_MIXED_VERTEXPROCESSING;
    }
    else
        return D3DCREATE_SOFTWARE_VERTEXPROCESSING;
}

u32 CHW::selectRefresh(u32 dwWidth, u32 dwHeight, D3DFORMAT fmt)
{
    if (psDeviceFlags.is(rsRefresh60hz))
    {
        return D3DPRESENT_RATE_DEFAULT;
    }
    else
    {
        u32 selected = D3DPRESENT_RATE_DEFAULT;
        u32 count = pD3D->GetAdapterModeCount(DevAdapter, fmt);
        for (u32 I = 0; I < count; I++)
        {
            D3DDISPLAYMODE Mode;
            pD3D->EnumAdapterModes(DevAdapter, fmt, I, &Mode);
            if (Mode.Width == dwWidth && Mode.Height == dwHeight)
            {
                //if (Mode.RefreshRate > selected)
                //    selected = Mode.RefreshRate;
                if (Mode.RefreshRate <= maxRefreshRate && Mode.RefreshRate>selected)
                    selected = Mode.RefreshRate;  //ECO_RENDER modif.
            }
        }
        return selected;
    }
}

BOOL CHW::support(D3DFORMAT fmt, DWORD type, DWORD usage)
{
    HRESULT hr = pD3D->CheckDeviceFormat(DevAdapter, m_DriverType, Caps.fTarget, usage, (D3DRESOURCETYPE)type, fmt);
    if (FAILED(hr))
        return FALSE;
    else
        return TRUE;
}

void CHW::updateWindowProps(HWND m_hWnd)
{
    if (strstr(Core.Params, "-wnd_mode"))
        psDeviceFlags.set(rsFullscreen, false);

    bool bWindowed = !psDeviceFlags.is(rsFullscreen);

#ifndef _EDITOR
    if (GEnv.isDedicatedServer)
        bWindowed = true;
#endif

    u32 dwWindowStyle = 0;
    // Set window properties depending on what mode were in.
    if (bWindowed)
    {
        if (m_move_window)
        {
            const bool drawBorders = strstr(Core.Params, "-draw_borders");
            dwWindowStyle = WS_VISIBLE;
            if (drawBorders)
                dwWindowStyle |= WS_BORDER | WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX;
            SetWindowLong(m_hWnd, GWL_STYLE, dwWindowStyle);
            // When moving from fullscreen to windowed mode, it is important to
            // adjust the window size after recreating the device rather than
            // beforehand to ensure that you get the window size you want.  For
            // example, when switching from 640x480 fullscreen to windowed with
            // a 1000x600 window on a 1024x768 desktop, it is impossible to set
            // the window size to 1000x600 until after the display mode has
            // changed to 1024x768, because windows cannot be larger than the
            // desktop.

            RECT m_rcWindowBounds;
            float fYOffset = 0.f;
            bool centerScreen = false;
            if (GEnv.isDedicatedServer || strstr(Core.Params, "-center_screen"))
                centerScreen = true;

            if (centerScreen)
            {
                RECT DesktopRect;

                GetClientRect(GetDesktopWindow(), &DesktopRect);

                SetRect(&m_rcWindowBounds,
                    (DesktopRect.right - DevPP.BackBufferWidth) / 2,
                    (DesktopRect.bottom - DevPP.BackBufferHeight) / 2,
                    (DesktopRect.right + DevPP.BackBufferWidth) / 2,
                    (DesktopRect.bottom + DevPP.BackBufferHeight) / 2);
            }
            else
            {
                if (drawBorders)
                    fYOffset = GetSystemMetrics(SM_CYCAPTION); // size of the window title bar
                SetRect(&m_rcWindowBounds, 0, 0, DevPP.BackBufferWidth, DevPP.BackBufferHeight);
            };

            AdjustWindowRect(&m_rcWindowBounds, dwWindowStyle, FALSE);

            SetWindowPos(m_hWnd, HWND_NOTOPMOST,
                m_rcWindowBounds.left, m_rcWindowBounds.top + fYOffset,
                m_rcWindowBounds.right - m_rcWindowBounds.left,
                m_rcWindowBounds.bottom - m_rcWindowBounds.top,
                SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_DRAWFRAME);
        }
    }
    else
    {
        SetWindowLong(m_hWnd, GWL_STYLE, dwWindowStyle = (WS_POPUP | WS_VISIBLE));
        SetWindowLong(m_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);
    }

#ifndef _EDITOR
    if (!GEnv.isDedicatedServer)
    {
        ShowCursor(FALSE);
        SetForegroundWindow(m_hWnd);
    }
#endif
}

struct _uniq_mode
{
    _uniq_mode(LPCSTR v) : _val(v) {}
    LPCSTR _val;
    bool operator()(LPCSTR _other) { return !xr_stricmp(_val, _other); }
};

#ifndef _EDITOR

void free_vid_mode_list()
{
    for (int i = 0; GEnv.vid_mode_token[i].name; i++)
    {
        xr_free(GEnv.vid_mode_token[i].name);
    }
    xr_free(GEnv.vid_mode_token);
    GEnv.vid_mode_token = nullptr;
}

void fill_vid_mode_list(CHW* _hw)
{
    if (GEnv.vid_mode_token != nullptr)
        return;
    xr_vector<LPCSTR> _tmp;
    xr_vector<D3DDISPLAYMODE> modes;

    // Get the number of display modes available
    UINT cnt = _hw->pD3D->GetAdapterModeCount(_hw->DevAdapter, _hw->Caps.fTarget);

    // Get the list of display modes
    modes.resize(cnt);
    for (auto i = 0; i < cnt; ++i)
        _hw->pD3D->EnumAdapterModes(_hw->DevAdapter, _hw->Caps.fTarget, i, &modes[i]);

    for (auto &i : modes)
    {
        string32 str;

        if (i.Width < 800)
            continue;

        xr_sprintf(str, sizeof(str), "%dx%d", i.Width, i.Height);

        if (_tmp.end() != std::find_if(_tmp.begin(), _tmp.end(), _uniq_mode(str)))
            continue;

        _tmp.push_back(nullptr);
        _tmp.back() = xr_strdup(str);
    }

    u32 _cnt = _tmp.size() + 1;

    GEnv.vid_mode_token = xr_alloc<xr_token>(_cnt);

    GEnv.vid_mode_token[_cnt - 1].id = -1;
    GEnv.vid_mode_token[_cnt - 1].name = nullptr;

#ifdef DEBUG
    Msg("Available video modes[%d]:", _tmp.size());
#endif // DEBUG
    for (auto i = 0; i < _tmp.size(); ++i)
    {
        GEnv.vid_mode_token[i].id = i;
        GEnv.vid_mode_token[i].name = _tmp[i];
#ifdef DEBUG
        Msg("[%s]", _tmp[i]);
#endif // DEBUG
    }
}
#endif
