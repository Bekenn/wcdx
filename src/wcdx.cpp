#include "common.h"
#include <wcdx.h>

#include <algorithm>
#include <limits>


using namespace std;

Wcdx::Wcdx(HWND window) : d3d(::Direct3DCreate9(D3D_SDK_VERSION)), dirtyPalette(false), dirtyFrame(false)
{
	D3DPRESENT_PARAMETERS params =
	{
		1024, 768, D3DFMT_UNKNOWN, 1,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_COPY, nullptr, TRUE,
		FALSE, D3DFMT_UNKNOWN,
		D3DPRESENTFLAG_LOCKABLE_BACKBUFFER, 0, D3DPRESENT_INTERVAL_DEFAULT
	};

	HRESULT hr;
	if (FAILED(hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
		_com_raise_error(hr);

	//	device->CreateRenderTarget(320, 200, D3DFMT_P8, D3DMULTISAMPLE_NONE, 0, TRUE, &buffer, nullptr);
	if (FAILED(hr = device->CreateOffscreenPlainSurface(320, 200, D3DFMT_P8, D3DPOOL_DEFAULT, &buffer, nullptr)))
		_com_raise_error(hr);
}

void Wcdx::SetPalette(const PALETTEENTRY entries[256])
{
	copy(entries, entries + 256, palette);
	for (auto& entry : palette)
		entry.peFlags = numeric_limits<decltype(entry.peFlags)>::max();
	dirtyPalette = true;
}

void Wcdx::UpdatePalette(UINT index, const PALETTEENTRY& entry)
{
	palette[index] = entry;
	palette[index].peFlags = numeric_limits<decltype(entry.peFlags)>::max();
	dirtyPalette = true;
}

void Wcdx::UpdateFrame(const void* bits, const RECT& rect, UINT pitch)
{
	D3DLOCKED_RECT lockedRect;
	buffer->LockRect(&lockedRect, &rect, D3DLOCK_DISCARD);
	const BYTE* src = static_cast<const BYTE*>(bits);
	BYTE* dest = static_cast<BYTE*>(lockedRect.pBits);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	while (height-- > 0)
	{
		copy(src, src + width, dest);
		src += pitch;
		dest += lockedRect.Pitch;
	};

	buffer->UnlockRect();
	dirtyFrame = true;
}

void Wcdx::Present()
{
	if (!dirtyPalette && !dirtyFrame)
		return;

	device->BeginScene();

	if (dirtyPalette)
	{
		device->SetPaletteEntries(0, palette);
		dirtyPalette = false;
	}

	if (dirtyFrame)
	{
		IDirect3DSurface9Ptr backBuffer;
		device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
		device->StretchRect(buffer, nullptr, backBuffer, nullptr, D3DTEXF_NONE);
		dirtyFrame = false;
	}

	device->EndScene();

	device->Present(nullptr, nullptr, nullptr, nullptr);
}
