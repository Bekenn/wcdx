#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

#include "resource.h"

#include "platform.h"

#include <iwcdx.h>

#include <comdef.h>
#include <d3d9.h>


class Wcdx : public IWcdx
{
public:
    static const LONG ContentWidth = 320;
    static const LONG ContentHeight = 200;

public:
    Wcdx(LPCWSTR title, WNDPROC windowProc, bool fullScreen);
    Wcdx(const Wcdx&) = delete;
    Wcdx& operator = (const Wcdx&) = delete;
    ~Wcdx();

public:
    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IWcdx
    HRESULT STDMETHODCALLTYPE SetVisible(BOOL visible) override;
    HRESULT STDMETHODCALLTYPE SetPalette(const WcdxColor entries[256]) override;
    HRESULT STDMETHODCALLTYPE UpdatePalette(UINT index, const WcdxColor* entry) override;
    HRESULT STDMETHODCALLTYPE UpdateFrame(INT x, INT y, UINT width, UINT height, UINT pitch, const byte* bits) override;
    HRESULT STDMETHODCALLTYPE Present() override;

    HRESULT STDMETHODCALLTYPE IsFullScreen() override;
    HRESULT STDMETHODCALLTYPE ConvertPointToClient(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertPointFromClient(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertRectToClient(RECT* rect) override;
    HRESULT STDMETHODCALLTYPE ConvertRectFromClient(RECT* rect) override;

    HRESULT STDMETHODCALLTYPE SavedGameOpen(const wchar_t* subdir, const wchar_t* filename, int oflag, int pmode, int* filedesc) override;
    HRESULT STDMETHODCALLTYPE OpenFile(const char* filename, int oflag, int pmode, int* filedesc) override;
    HRESULT STDMETHODCALLTYPE CloseFile(int filedesc) override;
    HRESULT STDMETHODCALLTYPE WriteFile(int filedesc, long offset, unsigned int size, const void* data) override;
    HRESULT STDMETHODCALLTYPE ReadFile(int filedesc, long offset, unsigned int size, void* data) override;
    HRESULT STDMETHODCALLTYPE SeekFile(int filedesc, long offset, int method, long* position) override;
    HRESULT STDMETHODCALLTYPE FileLength(int filedesc, long *length) override;

    HRESULT STDMETHODCALLTYPE ConvertPointToScreen(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertPointFromScreen(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertRectToScreen(RECT* rect) override;
    HRESULT STDMETHODCALLTYPE ConvertRectFromScreen(RECT* rect) override;

    HRESULT STDMETHODCALLTYPE QueryValue(const wchar_t* keyname, const wchar_t* valuename, void* data, DWORD* size) override;
    HRESULT STDMETHODCALLTYPE SetValue(const wchar_t* keyname, const wchar_t* valuename, DWORD type, const void* data, DWORD size) override;

private:
    static ATOM FrameWindowClass();
    static LRESULT CALLBACK FrameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnSize(DWORD resizeType, WORD clientWidth, WORD clientHeight);
    void OnActivate(WORD state, BOOL minimized, HWND other);
    void OnWindowPosChanged(WINDOWPOS* windowPos);
    void OnNCDestroy();
    bool OnNCLButtonDblClk(int hittest, POINTS position);
    bool OnSysKeyDown(DWORD vkey, WORD repeatCount, BYTE scode, BYTE flags);
    bool OnSysCommand(WORD type, SHORT x, SHORT y);
    void OnSizing(DWORD windowEdge, RECT* dragRect);
    void OnRender();

    HRESULT UpdateMonitor(UINT& adapter);
    HRESULT RecreateDevice(UINT adapter);
    HRESULT ResetDevice();
    HRESULT CreateIntermediateSurface();
    void SetFullScreen(bool enabled);
    RECT GetContentRect(RECT clientRect);
    void ConfineCursor();

private:
    ULONG _refCount;
    SmartResource<HWND> _window;
    HMONITOR _monitor;
    WNDPROC _clientWindowProc;
    DWORD _frameStyle;
    DWORD _frameExStyle;
    RECT _frameRect;

    IDirect3D9Ptr _d3d;
    IDirect3DDevice9Ptr _device;
    IDirect3DSurface9Ptr _surface;

    D3DPRESENT_PARAMETERS _presentParams;

    WcdxColor _palette[256];
    BYTE _framebuffer[ContentWidth * ContentHeight];

    bool _fullScreen;
    bool _dirty;
    bool _sizeChanged;
};

#endif