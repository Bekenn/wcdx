#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

#include <iwcdx.h>

#include <comdef.h>
#include <d3d9.h>


class Wcdx : public IWcdx
{
public:
	Wcdx(HWND window);
private:
	Wcdx(const Wcdx&);
	Wcdx& operator = (const Wcdx&);

public:
	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;

	// IWcdx
    HRESULT STDMETHODCALLTYPE SetFullScreen(BOOL enabled) override;
	HRESULT STDMETHODCALLTYPE SetPalette(const WcdxColor entries[256]) override;
	HRESULT STDMETHODCALLTYPE UpdatePalette(UINT index, const WcdxColor* entry) override;
    HRESULT STDMETHODCALLTYPE UpdateFrame(INT x, INT y, UINT width, UINT height, UINT pitch, const byte* bits) override;
    HRESULT STDMETHODCALLTYPE Present() override;

private:
	ULONG ref_count;
	HWND window;
	RECT windowRect;
	DWORD windowStyle;
	DWORD windowExStyle;

	// Suppress warnings about IDirect3D9Ptr not being exported -- they're not
	// directly usable by clients, so it doesn't matter.
#pragma warning(push)
#pragma warning(disable:4251)
	IDirect3D9Ptr d3d;
	IDirect3DDevice9Ptr device;
	IDirect3DSurface9Ptr surface;
#pragma warning(pop)

	WcdxColor palette[256];
	BYTE framebuffer[320 * 200];
	bool dirty;
	bool fullScreen;
};

#endif