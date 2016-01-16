#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

#include "window.h"

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
	HRESULT STDMETHODCALLTYPE OpenFile(const unsigned char* filename, int oflag, int pmode, int* filedesc) override;
	HRESULT STDMETHODCALLTYPE CloseFile(int filedesc) override;
	HRESULT STDMETHODCALLTYPE WriteFile(int filedesc, long offset, unsigned int size, const void* data) override;
	HRESULT STDMETHODCALLTYPE ReadFile(int filedesc, long offset, unsigned int size, void* data) override;
	HRESULT STDMETHODCALLTYPE SeekFile(int filedesc, long offset, int method, long* position) override;
	HRESULT STDMETHODCALLTYPE FileLength(int filedesc, long *length) override;

    HRESULT STDMETHODCALLTYPE ConvertPointToScreen(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertPointFromScreen(POINT* point) override;
    HRESULT STDMETHODCALLTYPE ConvertRectToScreen(RECT* rect) override;
    HRESULT STDMETHODCALLTYPE ConvertRectFromScreen(RECT* rect) override;

private:
	static ATOM FrameWindowClass();
	static ATOM ContentWindowClass();
	static LRESULT CALLBACK FrameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ContentWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	void OnSize(DWORD resizeType, WORD clientWidth, WORD clientHeight);
	void OnActivate(WORD state, BOOL minimized, HWND other);
	void OnNCDestroy();
	bool OnSysKeyDown(DWORD vkey, WORD repeatCount, BYTE scode, BYTE flags);
    bool OnSysCommand(WORD type, SHORT x, SHORT y);
	void OnSizing(DWORD windowEdge, RECT* dragRect);
	void OnRender();

	void OnContentMouseMove(DWORD keyState, SHORT x, SHORT y);
	void OnContentMouseLeave();

    HRESULT CreateIntermediateSurface();
	void SetFullScreen(bool enabled);
	void GetContentRect(RECT& contentRect);
    void ConfineCursor();

private:
	ULONG refCount;
	SmartWindow frameWindow;
	HWND contentWindow;
	WNDPROC clientWindowProc;
	DWORD frameStyle;
	DWORD frameExStyle;
	RECT frameRect;

	// Suppress warnings about IDirect3D9Ptr not being exported -- they're not
	// directly usable by clients, so it doesn't matter.
#pragma warning(push)
#pragma warning(disable:4251)
	IDirect3D9Ptr d3d;
	IDirect3DDevice9Ptr device;
	IDirect3DSurface9Ptr surface;
#pragma warning(pop)

    D3DCAPS9 deviceCaps;
    D3DPRESENT_PARAMETERS presentParams;

	WcdxColor palette[256];
	BYTE framebuffer[ContentWidth * ContentHeight];

	bool fullScreen;
	bool dirty;
	bool mouseOver;
};

#endif