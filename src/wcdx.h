#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

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
	~Wcdx();
private:
	Wcdx(const Wcdx&);
	Wcdx& operator = (const Wcdx&);

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

private:
	static ATOM FrameWindowClass();
	static ATOM ContentWindowClass();
	static LRESULT CALLBACK FrameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ContentWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	void OnSize(DWORD resizeType, WORD clientWidth, WORD clientHeight);
	void OnActivate(WORD state, BOOL minimized, HWND other);
	void OnNCDestroy();
	bool OnSysKeyDown(DWORD vkey, WORD repeatCount, BYTE scode, BYTE flags);
	void OnRender();

	void SetFullScreen(bool enabled);
	void GetContentRect(RECT& contentRect);

private:
	ULONG refCount;
	HWND frameWindow;
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

	WcdxColor palette[256];
	BYTE framebuffer[ContentWidth * ContentHeight];

	bool fullScreen;
	bool dirty;
};

#endif