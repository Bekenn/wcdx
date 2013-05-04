#include "common.h"
#include "wcdx.h"

#include <algorithm>
#include <limits>


using namespace std;

WCDXAPI IWcdx* WcdxCreate(HWND window)
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

Wcdx::Wcdx(HWND window) : ref_count(1), d3d(::Direct3DCreate9(D3D_SDK_VERSION)), dirty(false)
{
	// Resize the window to be 4:3.
	RECT clientRect;
	if (!::GetClientRect(window, &clientRect))
		_com_raise_error(HRESULT_FROM_WIN32(::GetLastError()));

	LONG width = (4 * clientRect.bottom) / 3;
	LONG height = (3 * clientRect.right) / 4;
	if (width < clientRect.right)
	{
		clientRect.right = width;
		height = clientRect.bottom;
	}
	else
	{
		clientRect.bottom = height;
		width = clientRect.right;
	}

	RECT windowRect;
	if (!::GetWindowRect(window, &windowRect))
		_com_raise_error(HRESULT_FROM_WIN32(::GetLastError()));

	DWORD style = ::GetWindowLong(window, GWL_STYLE);
	DWORD exstyle = ::GetWindowLong(window, GWL_EXSTYLE);
	if (!::AdjustWindowRectEx(&clientRect, style, FALSE, exstyle))
		_com_raise_error(HRESULT_FROM_WIN32(::GetLastError()));

	LONG dx = ((windowRect.right - windowRect.left) - (clientRect.right - clientRect.left)) / 2;
	LONG dy = ((windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top)) / 2;
	if (!::MoveWindow(window, windowRect.left + dx, windowRect.top + dy, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, FALSE))
		_com_raise_error(HRESULT_FROM_WIN32(::GetLastError()));

	D3DPRESENT_PARAMETERS params =
	{
		320, 200, D3DFMT_UNKNOWN, 1,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_DISCARD, nullptr, TRUE,
		FALSE, D3DFMT_UNKNOWN,
		D3DPRESENTFLAG_LOCKABLE_BACKBUFFER, 0, D3DPRESENT_INTERVAL_DEFAULT
	};

	HRESULT hr;
	if (FAILED(hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
		_com_raise_error(hr);

	// Check back buffer format.  If this is already D3DFMT_X8R8G8B8, then we don't need an extra surface.
	IDirect3DSurface9Ptr backBuffer;
	if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
		_com_raise_error(hr);
	D3DSURFACE_DESC desc;
	if (FAILED(hr = backBuffer->GetDesc(&desc)))
		_com_raise_error(hr);
	if (desc.Format != D3DFMT_X8R8G8B8)
	{
		if (FAILED(hr = device->CreateOffscreenPlainSurface(320, 200, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &surface, nullptr)))
			_com_raise_error(hr);
	}

	WcdxColor defColor = { 0, 0, 0, 0xFF };
	fill(begin(palette), end(palette), defColor);
}

HRESULT STDMETHODCALLTYPE Wcdx::QueryInterface(REFIID riid, void** ppvObject)
{
	if (ppvObject == nullptr)
		return E_POINTER;

	if (IsEqualIID(riid, IID_IUnknown))
	{
		*reinterpret_cast<IUnknown**>(ppvObject) = this;
		++ref_count;
		return S_OK;
	}
	if (IsEqualIID(riid, IID_IWcdx))
	{
		*reinterpret_cast<IWcdx**>(ppvObject) = this;
		++ref_count;
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Wcdx::AddRef()
{
	return ++ref_count;
}

ULONG STDMETHODCALLTYPE Wcdx::Release()
{
	if (--ref_count == 0)
	{
		delete this;
		return 0;
	}

	return ref_count;
}

HRESULT STDMETHODCALLTYPE Wcdx::SetPalette(const WcdxColor entries[256])
{
	copy(entries, entries + 256, palette);
	dirty = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::UpdatePalette(UINT index, const WcdxColor* entry)
{
	palette[index] = *entry;
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

		IDirect3DSurface9Ptr backBuffer;
		if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
			return hr;

		IDirect3DSurface9Ptr& buffer = surface != nullptr ? surface : backBuffer;
		D3DLOCKED_RECT locked;
		RECT bounds = { 0, 0, 320, 200 };
		if (FAILED(hr = buffer->LockRect(&locked, &bounds, D3DLOCK_DISCARD)))
			return hr;
		const BYTE* src = framebuffer;
		WcdxColor* dest = static_cast<WcdxColor*>(locked.pBits);
		for (int row = 0; row < 200; ++row)
		{
			transform(src, src + 320, dest, [&](BYTE index)
			{
				return palette[index];
			});

			src += 320;
			dest += locked.Pitch / sizeof(*dest);
		}
		hr = buffer->UnlockRect();

		if (surface != nullptr)
		{
			if (FAILED(hr = device->StretchRect(surface, nullptr, backBuffer, nullptr, D3DTEXF_POINT)))
				return hr;
		}

		dirty = false;
	}

	if (FAILED(hr = device->Present(nullptr, nullptr, nullptr, nullptr)))
		return hr;

	return S_OK;
}
