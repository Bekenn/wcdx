#include "common.h"
#include "wcdx.h"

#include <algorithm>
#include <limits>


using namespace std;

IWcdx* CreateWcdx(HWND window)
{
	try
	{
		return new Wcdx(window);
	}
	catch (const _com_error&)
	{
		return nullptr;
	}
}

Wcdx::Wcdx(HWND window) : d3d(::Direct3DCreate9(D3D_SDK_VERSION)), dirty(false)
{
	D3DPRESENT_PARAMETERS params =
	{
		1024, 768, D3DFMT_UNKNOWN, 1,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_DISCARD, nullptr, TRUE,
		FALSE, D3DFMT_UNKNOWN,
		D3DPRESENTFLAG_LOCKABLE_BACKBUFFER, 0, D3DPRESENT_INTERVAL_DEFAULT
	};

	HRESULT hr;
	if (FAILED(hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
		_com_raise_error(hr);
	if (FAILED(hr = device->CreateOffscreenPlainSurface(320, 200, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &surface, nullptr)))
		_com_raise_error(hr);

	RGBQUAD defColor = { 0, 0, 0, 0xFF };
	fill(begin(palette), end(palette), defColor);
}

HRESULT STDMETHODCALLTYPE Wcdx::QueryInterface(REFIID riid, void** ppvObject)
{
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Wcdx::AddRef()
{
	return 0;
}

ULONG STDMETHODCALLTYPE Wcdx::Release()
{
	return 0;
}

HRESULT STDMETHODCALLTYPE Wcdx::SetPalette(const PALETTEENTRY entries[256])
{
	transform(entries, entries + 256, palette, [](PALETTEENTRY pe)
	{
		RGBQUAD color = { pe.peRed, pe.peGreen, pe.peBlue, 0xFF };
		return color;
	});
	dirty = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::UpdatePalette(UINT index, const PALETTEENTRY* entry)
{
	palette[index].rgbRed = entry->peRed;
	palette[index].rgbGreen = entry->peGreen;
	palette[index].rgbBlue = entry->peBlue;
	palette[index].rgbReserved = 0xFF;
	dirty = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::UpdateFrame(INT x, INT y, UINT width, UINT height, UINT pitch, const byte* bits)
{
	RECT rect = { x, y, x + width, y + height };
	RECT clipped =
	{
		max(rect.left, LONG(0)),
		max(rect.top, LONG(0)),
		min(rect.right, LONG(320)),
		min(rect.bottom, LONG(200))
	};

	const BYTE* src = static_cast<const BYTE*>(bits);
	BYTE* dest = framebuffer + clipped.left + (320 * clipped.top);
	width = clipped.right - clipped.left;
	for (height = clipped.bottom - clipped.top; height-- > 0; )
	{
		copy(src, src + width, dest);
		src += pitch;
		dest += 320;
	}
	dirty = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::Present()
{
	if (!dirty)
		return S_OK;

	HRESULT hr;
	if (FAILED(hr = device->BeginScene()))
		return hr;

	{
		at_scope_exit([&]{ device->EndScene(); });

		D3DLOCKED_RECT locked;
		RECT bounds = { 0, 0, 320, 200 };
		if (FAILED(hr = surface->LockRect(&locked, &bounds, D3DLOCK_DISCARD)))
			return hr;
		const BYTE* src = framebuffer;
		RGBQUAD* dest = static_cast<RGBQUAD*>(locked.pBits);
		for (int row = 0; row < 200; ++row)
		{
			transform(src, src + 320, dest, [&](BYTE index)
			{
				return palette[index];
			});

			src += 320;
			dest += locked.Pitch / sizeof(*dest);
		}
		hr = surface->UnlockRect();

		IDirect3DSurface9Ptr backBuffer;
		if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
			return hr;
		if (FAILED(hr = device->StretchRect(surface, nullptr, backBuffer, nullptr, D3DTEXF_NONE)))
			return hr;
		dirty = false;
	}

	if (FAILED(hr = device->Present(nullptr, nullptr, nullptr, nullptr)))
		return hr;

	return S_OK;
}
