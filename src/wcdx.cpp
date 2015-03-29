#include "common.h"
#include "wcdx.h"

#include <algorithm>
#include <iterator>
#include <limits>

#include <io.h>
#include <fcntl.h>

#include <Shlobj.h>
#include <strsafe.h>


using namespace std;

enum
{
	WM_APP_RENDER = WM_APP
};

static void ConvertTo(LONG& x, LONG& y, const SIZE& size);
static void ConvertFrom(LONG& x, LONG& y, const SIZE& size);
static HRESULT GetSavedGamePath(LPCWSTR subdir, LPWSTR path);
static HRESULT GetLocalAppDataPath(LPCWSTR subdir, LPWSTR path);

static bool CreateDirectoryRecursive(LPWSTR pathName);

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

Wcdx::Wcdx(LPCWSTR title, WNDPROC windowProc, bool fullScreen) : refCount(1), clientWindowProc(windowProc), frameStyle(WS_OVERLAPPEDWINDOW), frameExStyle(WS_EX_OVERLAPPEDWINDOW), fullScreen(false), dirty(false), mouseOver(false)
{
	frameWindow = ::CreateWindowEx(frameExStyle,
		reinterpret_cast<LPCWSTR>(FrameWindowClass()), title,
		frameStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, DllInstance, this);

	::GetWindowRect(frameWindow, &frameRect);

	// Force 4:3 aspect ratio.
	OnSizing(WMSZ_TOP, &frameRect);
	::MoveWindow(frameWindow, frameRect.left, frameRect.top, frameRect.right - frameRect.left, frameRect.bottom - frameRect.top, FALSE);

	RECT contentRect;
	GetContentRect(contentRect);
	contentWindow = ::CreateWindowEx(0, reinterpret_cast<LPCWSTR>(ContentWindowClass()), nullptr, WS_VISIBLE | WS_CHILD,
		contentRect.left, contentRect.top, contentRect.right - contentRect.left, contentRect.bottom - contentRect.top,
		frameWindow, nullptr, DllInstance, static_cast<IWcdx*>(this));

	d3d = ::Direct3DCreate9(D3D_SDK_VERSION);

	presentParams =
	{
		ContentWidth, ContentHeight, D3DFMT_UNKNOWN, 1,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_DISCARD, contentWindow, TRUE,
		FALSE, D3DFMT_UNKNOWN,
		D3DPRESENTFLAG_LOCKABLE_BACKBUFFER, 0, D3DPRESENT_INTERVAL_DEFAULT
	};

	HRESULT hr;
	if (FAILED(hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, frameWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentParams, &device)))
		_com_raise_error(hr);

    if (FAILED(hr = CreateIntermediateSurface()))
        _com_raise_error(hr);

	WcdxColor defColor = { 0, 0, 0, 0xFF };
	fill(begin(palette), end(palette), defColor);

	SetFullScreen(IsDebuggerPresent() ? false : fullScreen);
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
	::ShowWindow(frameWindow, visible ? SW_SHOW : SW_HIDE);
	if (visible)
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
	RECT rect = { x, y, LONG(x + width), LONG(y + height) };
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
    if (FAILED(hr = device->TestCooperativeLevel()))
    {
        if (hr != D3DERR_DEVICENOTRESET)
            return hr;

        surface = nullptr;
        if (FAILED(hr = device->Reset(&presentParams)))
            return hr;

        if (FAILED(hr = CreateIntermediateSurface()))
            return hr;
    }

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

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointToClient(POINT* point)
{
	if (point == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetClientRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	SIZE size = { viewRect.right, viewRect.bottom };
	ConvertTo(point->x, point->y, size);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointFromClient(POINT* point)
{
	if (point == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetClientRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	SIZE size = { viewRect.right, viewRect.bottom };
	ConvertFrom(point->x, point->y, size);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectToClient(RECT* rect)
{
	if (rect == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetClientRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	SIZE size = { viewRect.right, viewRect.bottom };
	ConvertTo(rect->left, rect->top, size);
	ConvertTo(rect->right, rect->bottom, size);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectFromClient(RECT* rect)
{
	if (rect == nullptr)
		return E_POINTER;

	RECT viewRect;
	if (!::GetClientRect(contentWindow, &viewRect))
		return HRESULT_FROM_WIN32(::GetLastError());

	SIZE size = { viewRect.right, viewRect.bottom };
	ConvertFrom(rect->left, rect->top, size);
	ConvertFrom(rect->right, rect->bottom, size);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::SavedGameOpen(const wchar_t* subdir, const wchar_t* filename, int oflag, int pmode, int* filedesc)
{
	if (subdir == nullptr || filename == nullptr || filedesc == nullptr)
		return E_POINTER;

	typedef HRESULT (* GamePathFn)(const wchar_t* subdir, wchar_t* path);
	GamePathFn pathFuncs[] =
	{
		GetSavedGamePath,
		GetLocalAppDataPath,
	};

	wchar_t path[_MAX_PATH];
	for (auto func : pathFuncs)
	{
		auto hr = func(subdir, path);
		if (SUCCEEDED(hr))
		{
			if ((oflag & _O_CREAT) != 0)
			{
				if (!CreateDirectoryRecursive(path))
					continue;
			}

			wchar_t* pathEnd;
			size_t remaining;
			if (FAILED(hr = StringCchCatEx(path, _MAX_PATH, L"\\", &pathEnd, &remaining, 0)))
				return hr;
			if (FAILED(hr = StringCchCopy(pathEnd, remaining, filename)))
				return hr;

			auto error = _wsopen_s(filedesc, path, oflag, _SH_DENYNO, pmode);
			if (*filedesc != -1)
				return S_OK;

			if (error != ENOENT)
				return E_FAIL;
		}
	}

	if ((oflag & _O_CREAT) == 0)
	{
		// If the savegame file exists, try to move or copy it into a better location.
		for (auto func : pathFuncs)
		{
			auto hr = func(subdir, path);
			if (SUCCEEDED(hr))
			{
				if (!CreateDirectoryRecursive(path))
					continue;

				wchar_t* pathEnd;
				size_t remaining;
				if (FAILED(StringCchCatEx(path, _MAX_PATH, L"\\", &pathEnd, &remaining, 0)))
					continue;
				if (FAILED(StringCchCopy(pathEnd, remaining, filename)))
					continue;

				if (::MoveFileEx(filename, path, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)
					|| ::CopyFile(filename, path, FALSE))
				{
					filename = path;
					break;
				}
			}
		}
	}

	_wsopen_s(filedesc, filename, oflag, _SH_DENYNO, pmode);
	if (*filedesc != -1)
		return S_OK;

	*filedesc = -1;
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::OpenFile(const unsigned char* filename, int oflag, int pmode, int* filedesc)
{
	if (filename == nullptr || filedesc == nullptr)
		return E_POINTER;

	return _sopen_s(filedesc, reinterpret_cast<const char*>(filename), oflag, _SH_DENYNO, pmode) == 0 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::CloseFile(int filedesc)
{
	return _close(filedesc) == 0 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::WriteFile(int filedesc, long offset, unsigned int size, const void* data)
{
	if (size > 0 && data == nullptr)
		return E_POINTER;

	if (_lseek(filedesc, offset, SEEK_SET) == -1)
		return E_FAIL;

	return _write(filedesc, data, size) != -1 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::ReadFile(int filedesc, long offset, unsigned int size, void* data)
{
	if (size > 0 && data == nullptr)
		return E_POINTER;

	if (_lseek(filedesc, offset, SEEK_SET) == -1)
		return E_FAIL;

	return _read(filedesc, data, size) != -1 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::SeekFile(int filedesc, long offset, int method, long* position)
{
	if (position == nullptr)
		return E_POINTER;

	*position = _lseek(filedesc, offset, method);
	return *position != -1 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::FileLength(int filedesc, long *length)
{
	if (length == nullptr)
		return E_POINTER;

	*length = _filelength(filedesc);
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
			break;
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

		case WM_SIZING:
			wcdx->OnSizing(wParam, reinterpret_cast<RECT*>(lParam));
			return TRUE;

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
		switch (message)
		{
		case WM_MOUSEMOVE:
			static_cast<Wcdx*>(wcdx)->OnContentMouseMove(wParam, LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOUSELEAVE:
			static_cast<Wcdx*>(wcdx)->OnContentMouseLeave();
			break;

		case WM_SYSKEYDOWN:
			if (static_cast<Wcdx*>(wcdx)->OnSysKeyDown(wParam, LOWORD(lParam), LOBYTE(HIWORD(lParam)), HIBYTE(HIWORD(lParam))))
				return 0;
			break;
		}
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

void Wcdx::OnSizing(DWORD windowEdge, RECT* dragRect)
{
	RECT client = { 0, 0, 0, 0 };
	::AdjustWindowRectEx(&client, frameStyle, FALSE, frameExStyle);
	client.left = dragRect->left - client.left;
	client.top = dragRect->top - client.top;
	client.right = dragRect->right - client.right;
	client.bottom = dragRect->bottom - client.bottom;

	auto width = client.right - client.left;
	auto height = client.bottom - client.top;

	bool adjustWidth;
	switch (windowEdge)
	{
	case WMSZ_LEFT:
	case WMSZ_RIGHT:
		adjustWidth = false;
		break;

	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		adjustWidth = true;
		break;

	default:
		adjustWidth = height > (3 * width) / 4;
		break;
	}

	if (adjustWidth)
	{
		width = (4 * height) / 3;
		auto delta = width - (client.right - client.left);
		switch (windowEdge)
		{
		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			dragRect->left -= delta / 2;
			dragRect->right += delta - (delta / 2);
			break;

		case WMSZ_TOPLEFT:
		case WMSZ_BOTTOMLEFT:
			dragRect->left -= delta;
			break;

		case WMSZ_TOPRIGHT:
		case WMSZ_BOTTOMRIGHT:
			dragRect->right += delta;
			break;
		}
	}
	else
	{
		height = (3 * width) / 4;
		auto delta = height - (client.bottom - client.top);
		switch (windowEdge)
		{
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
			dragRect->top -= delta / 2;
			dragRect->bottom += delta - (delta / 2);
			break;

		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
			dragRect->top -= delta;
			break;

		case WMSZ_BOTTOMLEFT:
		case WMSZ_BOTTOMRIGHT:
			dragRect->bottom += delta;
			break;
		}
	}
}

void Wcdx::OnRender()
{
	device->Present(nullptr, nullptr, nullptr, nullptr);
}

void Wcdx::OnContentMouseMove(DWORD keyState, SHORT x, SHORT y)
{
	if (!mouseOver)
	{
		mouseOver = true;
		TRACKMOUSEEVENT tme = 
		{
			sizeof(TRACKMOUSEEVENT),
			TME_LEAVE,
			contentWindow
		};
		::TrackMouseEvent(&tme);
		::ShowCursor(FALSE);
	}
}

void Wcdx::OnContentMouseLeave()
{
	mouseOver = false;
	::ShowCursor(TRUE);
}

HRESULT Wcdx::CreateIntermediateSurface()
{
    HRESULT hr;

    // Check back buffer format.  If this is already D3DFMT_X8R8G8B8, then we don't need an extra surface.
    IDirect3DSurface9Ptr backBuffer;
    if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        return hr;

    D3DSURFACE_DESC desc;
    if (FAILED(hr = backBuffer->GetDesc(&desc)))
        return hr;

    if (desc.Format != D3DFMT_X8R8G8B8)
        return hr = device->CreateOffscreenPlainSurface(ContentWidth, ContentHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &surface, nullptr);

    // No need for an intermediate surface.
    return S_FALSE;
}

void Wcdx::SetFullScreen(bool enabled)
{
	if (enabled == fullScreen)
		return;

	if (enabled)
	{
		::GetWindowRect(frameWindow, &frameRect);

		frameStyle = ::GetWindowLong(frameWindow, GWL_STYLE);
		::SetWindowLong(frameWindow, GWL_STYLE, WS_OVERLAPPED);
		frameExStyle = ::SetWindowLong(frameWindow, GWL_EXSTYLE, 0);

		HMONITOR monitor = ::MonitorFromWindow(frameWindow, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
		::GetMonitorInfo(monitor, &monitorInfo);

		::SetWindowPos(frameWindow, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);

        RECT contentRect;
        ::GetWindowRect(contentWindow, &contentRect);
        ::ClipCursor(&contentRect);

        fullScreen = true;
	}
	else
	{
		::SetLastError(0);
		DWORD style = ::SetWindowLong(frameWindow, GWL_STYLE, frameStyle);
		DWORD exStyle = ::SetWindowLong(frameWindow, GWL_EXSTYLE, frameExStyle);

		::SetWindowPos(frameWindow, HWND_TOP, frameRect.left, frameRect.top,
			frameRect.right - frameRect.left,
			frameRect.bottom - frameRect.top,
			SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);

        ::ClipCursor(nullptr);

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

void ConvertTo(LONG& x, LONG& y, const SIZE& size)
{
	x = ((x * size.cx) / Wcdx::ContentWidth);
	y = ((y * size.cy) / Wcdx::ContentHeight);
}

void ConvertFrom(LONG& x, LONG& y, const SIZE& size)
{
	x = (x * Wcdx::ContentWidth) / size.cx;
	y = (y * Wcdx::ContentHeight) / size.cy;
}

HRESULT GetSavedGamePath(LPCWSTR subdir, LPWSTR path)
{
	typedef HRESULT(STDAPICALLTYPE* GetKnownFolderPathFn)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);

	auto shellModule = ::GetModuleHandle(L"Shell32.dll");
	if (shellModule != nullptr)
	{
		auto GetKnownFolderPath = reinterpret_cast<GetKnownFolderPathFn>(::GetProcAddress(shellModule, "SHGetKnownFolderPath"));
		if (GetKnownFolderPath != nullptr)
		{
			// flags for Known Folder APIs
			enum { KF_FLAG_CREATE = 0x00008000, KF_FLAG_INIT = 0x00000800 };

			PWSTR folderPath;
			auto hr = GetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE | KF_FLAG_INIT, nullptr, &folderPath);
			if (FAILED(hr))
				return hr == E_INVALIDARG ? STG_E_PATHNOTFOUND : hr;

			at_scope_exit([&] { ::CoTaskMemFree(folderPath); });

			PWSTR pathEnd;
			size_t remaining;
			if (FAILED(hr = ::StringCchCopyEx(path, MAX_PATH, folderPath, &pathEnd, &remaining, 0)))
				return hr;
			if (FAILED(hr = ::StringCchCopyEx(pathEnd, remaining, L"\\", &pathEnd, &remaining, 0)))
				return hr;
			return ::StringCchCopy(pathEnd, remaining, subdir);
		}
	}

	return E_NOTIMPL;
}

HRESULT GetLocalAppDataPath(LPCWSTR subdir, LPWSTR path)
{
	auto hr = ::SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path);
	if (FAILED(hr))
		return hr;

	PWSTR pathEnd;
	size_t remaining;
	if (FAILED(hr = ::StringCchCatEx(path, MAX_PATH, L"\\", &pathEnd, &remaining, 0)))
		return hr;
	return ::StringCchCopy(pathEnd, remaining, subdir);
}

bool CreateDirectoryRecursive(LPWSTR pathName)
{
	if (::CreateDirectory(pathName, nullptr) || ::GetLastError() == ERROR_ALREADY_EXISTS)
		return true;

	if (::GetLastError() != ERROR_PATH_NOT_FOUND)
		return false;

	auto i = find(make_reverse_iterator(pathName + wcslen(pathName)), make_reverse_iterator(pathName), L'\\');
	if (i.base() == pathName)
		return false;

	*i = L'\0';
	auto result = CreateDirectoryRecursive(pathName);
	*i = L'\\';
	return result && ::CreateDirectory(pathName, nullptr);
}
