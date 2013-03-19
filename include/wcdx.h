#ifndef WCDX_INCLUDED
#define WCDX_INCLUDED
#pragma once

#include <comdef.h>
#include <windef.h>

_COM_SMARTPTR_TYPEDEF(IDirect3D9, __uuidof(IDirect3D9));
_COM_SMARTPTR_TYPEDEF(IDirect3DDevice9, __uuidof(IDirect3DDevice9));
_COM_SMARTPTR_TYPEDEF(IDirect3DSurface9, __uuidof(IDirect3DSurface9));


#ifdef WCDX_EXPORTS
#define WCDXAPI __declspec(dllexport)
#else
#define WCDXAPI __declspec(dllimport)
#endif


typedef interface IDirect3D9                    IDirect3D9;
typedef interface IDirect3DDevice9              IDirect3DDevice9;


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
	IDirect3DSurface9Ptr buffer;
#pragma warning(pop)

	PALETTEENTRY palette[256];

	bool dirtyPalette;
	bool dirtyFrame;
};

#endif