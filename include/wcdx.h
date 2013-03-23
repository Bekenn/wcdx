#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

#include <comdef.h>
#include <d3d9.h>


#ifdef WCDX_EXPORTS
#define WCDXAPI __declspec(dllexport)
#else
#define WCDXAPI __declspec(dllimport)
#endif


class WCDXAPI Wcdx
{
public:
	Wcdx(HWND window);

public:
	void SetPalette(const PALETTEENTRY entries[256]);
	void UpdatePalette(UINT index, const PALETTEENTRY& entry);
	void UpdateFrame(const void* bits, const RECT& rect, UINT pitch);
	void Present();

private:
	// Suppress warnings about IDirect3D9Ptr not being exported -- they're not
	// directly usable by clients, so it doesn't matter.
#pragma warning(push)
#pragma warning(disable:4251)
	IDirect3D9Ptr d3d;
	IDirect3DDevice9Ptr device;
	IDirect3DSurface9Ptr surface;
#pragma warning(pop)

	RGBQUAD palette[256];
	BYTE framebuffer[320 * 200];
	bool dirty;
};

#endif