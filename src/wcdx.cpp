#include "common.h"
#include "wcdx.h"

#include <algorithm>
#include <limits>


using namespace std;

enum
{
	WM_APP_RENDER = WM_APP
};

static void ConvertTo(LONG& x, LONG& y, const RECT& rect);
static void ConvertFrom(LONG& x, LONG& y, const RECT& rect);

WCDXAPI IWcdx* WcdxCreate(LPCWSTR windowTitle, WNDPROC windowProc, BOOL fullScreen)
{
	try
	{
		return new Wcdx(windowTitle, windowProc, fullScreen != FALSE);
	}
	catch (const _com_error&)
	{
		return nullptr;
	}
}

Wcdx::Wcdx(LPCWSTR title, WNDPROC windowProc, bool fullScreen) : refCount(1), clientWindowProc(windowProc), frameStyle(WS_OVERLAPPEDWINDOW), frameExStyle(WS_EX_OVERLAPPEDWINDOW), fullScreen(false), dirty(false)
{
	frameWindow = ::CreateWindowEx(frameExStyle,
		reinterpret_cast<LPCWSTR>(FrameWindowClass()), title,
		frameStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, DllInstance, this);

	::GetWindowRect(frameWindow, &frameRect);

	RECT contentRect;
	GetContentRect(contentRect);
	contentWindow = ::CreateWindowEx(0, reinterpret_cast<LPCWSTR>(ContentWindowClass()), nullptr, WS_VISIBLE | WS_CHILD,
		contentRect.left, contentRect.top, contentRect.right - contentRect.left, contentRect.bottom - contentRect.top,
		frameWindow, nullptr, DllInstance, static_cast<IWcdx*>(this));

	d3d = ::Direct3DCreate9(D3D_SDK_VERSION);

	D3DPRESENT_PARAMETERS params =
	{
		ContentWidth, ContentHeight, D3DFMT_UNKNOWN, 1,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_DISCARD, contentWindow, TRUE,
		FALSE, D3DFMT_UNKNOWN,
		D3DPRESENTFLAG_LOCKABLE_BACKBUFFER, 0, D3DPRESENT_INTERVAL_DEFAULT
	};

	HRESULT hr;
	if (FAILED(hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, frameWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
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
		if (FAILED(hr = device->CreateOffscreenPlainSurface(ContentWidth, ContentHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &surface, nullptr)))
			_com_raise_error(hr);
	}

	WcdxColor defColor = { 0, 0, 0, 0xFF };
	fill(begin(palette), end(palette), defColor);

	SetFullScreen(fullScreen);
}

Wcdx::~Wcdx()
{
	if (frameWindow != nullptr)
		::DestroyWindow(frameWindow);
}

HRESULT STDMETHODCALLTYPE Wcdx::QueryInterface(REFIID riid, void** ppvObject)
{
	if (ppvObject == nullptr)
		return E_POINTER;

	if (IsEqualIID(riid, IID_IUnknown))
	{
		*reinterpret_cast<IUnknown**>(ppvObject) = this;
		++refCount;
		return S_OK;
	}
	if (IsEqualIID(riid, IID_IWcdx))
	{
		*reinterpret_cast<IWcdx**>(ppvObject) = this;
		++refCount;
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Wcdx::AddRef()
{
	return ++refCount;
}

ULONG STDMETHODCALLTYPE Wcdx::Release()
{
	if (--refCount == 0)
	{
		delete this;
		return 0;
	}

	return refCount;
}

HRESULT STDMETHODCALLTYPE Wcdx::SetVisible(BOOL visible)
{
	if (!::ShowWindow(frameWindow, visible ? SW_SHOW : SW_HIDE))
		return HRESULT_FROM_WIN32(::GetLastError());
	::PostMessage(frameWindow, WM_APP_RENDER, 0, 0);
	return S_OK;
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
		min(rect.right, LONG(ContentWidth)),
		min(rect.bottom, LONG(ContentHeight))
	};

	const BYTE* src = static_cast<const BYTE*>(bits);
	BYTE* dest = framebuffer + clipped.left + (ContentWidth * clipped.top);
	width = clipped.right - clipped.left;
	for (height = clipped.bottom - clipped.top; height-- > 0; )
	{
		copy(src, src + width, dest);
		src += pitch;
		dest += ContentWidth;
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
		RECT bounds = { 0, 0, ContentWidth, ContentHeight };
		if (FAILED(hr = buffer->LockRect(&locked, &bounds, D3DLOCK_DISCARD)))
			return hr;
		const BYTE* src = framebuffer;
		WcdxColor* dest = static_cast<WcdxColor*>(locked.pBits);
		for (int row = 0; row < ContentHeight; ++row)
		{
			transform(src, src + ContentWidth, dest, [&](BYTE index)
			{
				return palette[index];
			});

			src += ContentWidth;
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

HRESULT STDMETHODCALLTYPE Wcdx::IsFullScreen()
{
	return fullScreen ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointToScreen(POINT* point)
{
	if (point == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetWindowRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	ConvertTo(point->x, point->y, viewRect);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointFromScreen(POINT* point)
{
	if (point == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetWindowRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	ConvertFrom(point->x, point->y, viewRect);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectToScreen(RECT* rect)
{
	if (rect == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetWindowRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	ConvertTo(rect->left, rect->top, viewRect);
	ConvertTo(rect->right, rect->bottom, viewRect);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectFromScreen(RECT* rect)
{
	if (rect == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetWindowRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	ConvertFrom(rect->left, rect->top, viewRect);
	ConvertFrom(rect->right, rect->bottom, viewRect);
	return S_OK;
}

ATOM Wcdx::FrameWindowClass()
{
	static ATOM windowClass = 0;
	if (windowClass == 0)
	{
		WNDCLASSEX wc =
		{
			sizeof(WNDCLASSEX),
			CS_HREDRAW | CS_VREDRAW,
			FrameWindowProc,
			0,
			0,
			DllInstance,
			nullptr,
			::LoadCursor(nullptr, IDC_ARROW),
			::CreateSolidBrush(RGB(0, 0, 0)),
			nullptr,
			L"Wcdx Frame Window",
			nullptr
		};

		windowClass = ::RegisterClassEx(&wc);
	}

	return windowClass;
}

ATOM Wcdx::ContentWindowClass()
{
	static ATOM windowClass = 0;
	if (windowClass == 0)
	{
		WNDCLASSEX wc =
		{
			sizeof(WNDCLASSEX),
			0,
			ContentWindowProc,
			0,
			0,
			DllInstance,
			nullptr,
			::LoadCursor(nullptr, IDC_ARROW),
			nullptr,
			nullptr,
			L"Wcdx Content Window",
			nullptr
		};

		windowClass = ::RegisterClassEx(&wc);
	}

	return windowClass;
}

LRESULT CALLBACK Wcdx::FrameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Wcdx* wcdx = reinterpret_cast<Wcdx*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (wcdx == nullptr)
	{
		switch (message)
		{
		case WM_NCCREATE:
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
			return TRUE;
		}
	}
	else
	{
		switch (message)
		{
		case WM_SIZE:
			wcdx->OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_ACTIVATE:
			wcdx->OnActivate(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
			return 0;

		case WM_NCDESTROY:
			wcdx->OnNCDestroy();
			return 0;

		case WM_SYSKEYDOWN:
			if (wcdx->OnSysKeyDown(wParam, LOWORD(lParam), LOBYTE(HIWORD(lParam)), HIBYTE(HIWORD(lParam))))
				return 0;
			break;

		case WM_APP_RENDER:
			wcdx->OnRender();
			break;
		}
	}

	return ::DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK Wcdx::ContentWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	IWcdx* wcdx = reinterpret_cast<IWcdx*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (wcdx == nullptr)
	{
		switch (message)
		{
		case WM_NCCREATE:
			wcdx = static_cast<IWcdx*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wcdx));
			return static_cast<Wcdx*>(wcdx)->clientWindowProc(hwnd, message, wParam, lParam);
		}
	}
	else
	{
		return static_cast<Wcdx*>(wcdx)->clientWindowProc(hwnd, message, wParam, lParam);
	}

	return ::DefWindowProc(hwnd, message, wParam, lParam);
}

void Wcdx::OnSize(DWORD resizeType, WORD clientWidth, WORD clientHeight)
{
	RECT contentRect;
	GetContentRect(contentRect);
	::MoveWindow(contentWindow, contentRect.left, contentRect.top, contentRect.right - contentRect.left, contentRect.bottom - contentRect.top, FALSE);
	::PostMessage(frameWindow, WM_APP_RENDER, 0, 0);
}

void Wcdx::OnActivate(WORD state, BOOL minimized, HWND other)
{
	if (state != WA_INACTIVE)
		::SetFocus(contentWindow);
}

void Wcdx::OnNCDestroy()
{
	frameWindow = nullptr;
	contentWindow = nullptr;
}

bool Wcdx::OnSysKeyDown(DWORD vkey, WORD repeatCount, BYTE scode, BYTE flags)
{
	if ((vkey == VK_RETURN) && ((flags & 0x60) == 0x20))
	{
		SetFullScreen(!fullScreen);
		return true;
	}

	return false;
}

void Wcdx::OnRender()
{
	device->Present(nullptr, nullptr, nullptr, nullptr);
}

void Wcdx::SetFullScreen(bool enabled)
{
	if (enabled == fullScreen)
		return;

	if (enabled)
	{
		::GetWindowRect(frameWindow, &frameRect);
#if 0
		if (!::GetWindowRect(window, &windowRect))
			return HRESULT_FROM_WIN32(::GetLastError());
#endif

		::SetLastError(0);
		frameStyle = ::GetWindowLong(frameWindow, GWL_STYLE);
		::SetWindowLong(frameWindow, GWL_STYLE, WS_OVERLAPPED);
#if 0
		if ((windowStyle == 0) && (::GetLastError() != 0))
			return HRESULT_FROM_WIN32(::GetLastError());
#endif

		frameExStyle = ::SetWindowLong(frameWindow, GWL_EXSTYLE, 0);
#if 0
		if ((windowExStyle == 0) && (::GetLastError() != 0))
		{
			::SetWindowLong(window, GWL_STYLE, windowStyle);
			return HRESULT_FROM_WIN32(::GetLastError());
		}
#endif

		HMONITOR monitor = ::MonitorFromWindow(frameWindow, MONITOR_DEFAULTTONEAREST);
#if 0
		if (monitor == nullptr)
			return HRESULT_FROM_WIN32(::GetLastError());
#endif

		MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
		::GetMonitorInfo(monitor, &monitorInfo);
#if 0
		if (!::GetMonitorInfo(monitor, &monitorInfo))
			return HRESULT_FROM_WIN32(::GetLastError());
#endif

		::SetWindowPos(frameWindow, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);
#if 0
		if (!::SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW))
		{
			::SetWindowLong(window, GWL_EXSTYLE, windowExStyle);
			::SetWindowLong(window, GWL_STYLE, windowStyle);
			return HRESULT_FROM_WIN32(::GetLastError());
		}
#endif
		fullScreen = true;
	}
	else
	{
		::SetLastError(0);
		DWORD style = ::SetWindowLong(frameWindow, GWL_STYLE, frameStyle);
#if 0
		if ((style == 0) && (::GetLastError() != 0))
			return HRESULT_FROM_WIN32(::GetLastError());
#endif

		DWORD exStyle = ::SetWindowLong(frameWindow, GWL_EXSTYLE, frameExStyle);
#if 0
		if ((exStyle == 0) && (::GetLastError() != 0))
		{
			::SetWindowLong(window, GWL_STYLE, style);
			return HRESULT_FROM_WIN32(::GetLastError());
		}
#endif

		::SetWindowPos(frameWindow, HWND_TOP, frameRect.left, frameRect.top,
			frameRect.right - frameRect.left,
			frameRect.bottom - frameRect.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);
#if 0
		if (!::SetWindowPos(window, HWND_TOP, windowRect.left, windowRect.top,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW))
		{
			::SetWindowLong(window, GWL_STYLE, style);
			::SetWindowLong(window, GWL_EXSTYLE, exStyle);
			return HRESULT_FROM_WIN32(::GetLastError());
		}
#endif
		fullScreen = false;
	}

	::PostMessage(frameWindow, WM_APP_RENDER, 0, 0);
}

void Wcdx::GetContentRect(RECT& contentRect)
{
	::GetClientRect(frameWindow, &contentRect);

	LONG width = (4 * contentRect.bottom) / 3;
	LONG height = (3 * contentRect.right) / 4;
	if (width < contentRect.right)
	{
		contentRect.left = (contentRect.right - width) / 2;
		contentRect.right = contentRect.left + width;
		height = contentRect.bottom;
	}
	else
	{
		contentRect.top = (contentRect.bottom - height) / 2;
		contentRect.bottom = contentRect.top + height;
		width = contentRect.right;
	}
}

void ConvertTo(LONG& x, LONG& y, const RECT& rect)
{
	x = ((x * (rect.right - rect.left)) / Wcdx::ContentWidth) + rect.left;
	y = ((y * (rect.bottom - rect.top)) / Wcdx::ContentHeight) + rect.top;
}

void ConvertFrom(LONG& x, LONG& y, const RECT& rect)
{
	x = ((x - rect.left) * Wcdx::ContentWidth) / (rect.right - rect.left);
	y = ((y - rect.top) * Wcdx::ContentHeight) / (rect.bottom - rect.top);
}
