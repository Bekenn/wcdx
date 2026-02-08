#include "wcdx.h"

#include <image/image.h>
#include <image/resources.h>

#include <stdext/array_view.h>
#include <stdext/scope_guard.h>
#include <stdext/file.h>
#include <stdext/format.h>
#include <stdext/multi.h>
#include <stdext/utility.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <random>
#include <system_error>

#include <io.h>
#include <fcntl.h>

#include <Shlobj.h>
#include <strsafe.h>


namespace
{
    enum
    {
        WM_APP_RENDER = WM_APP
    };

    POINT ConvertTo(POINT point, RECT rect);
    POINT ConvertFrom(POINT point, RECT rect);
    HRESULT GetSavedGamePath(LPCWSTR subdir, LPWSTR path);
    HRESULT GetLocalAppDataPath(LPCWSTR subdir, LPWSTR path);

    bool CreateDirectoryRecursive(LPWSTR pathName);

    std::independent_bits_engine<std::mt19937, 1, unsigned int> RandomBit(std::random_device{}());
}

WCDXAPI IWcdx* WcdxCreate(LPCWSTR windowTitle, WNDPROC windowProc, BOOL _fullScreen)
{
    try
    {
        return new Wcdx(windowTitle, windowProc, _fullScreen != FALSE);
    }
    catch (const _com_error&)
    {
        return nullptr;
    }
}

Wcdx::Wcdx(LPCWSTR title, WNDPROC windowProc, bool _fullScreen)
    : _refCount(1), _monitor(nullptr), _clientWindowProc(windowProc), _frameStyle(WS_OVERLAPPEDWINDOW), _frameExStyle(WS_EX_OVERLAPPEDWINDOW)
    , _fullScreen(false), _dirty(false), _sizeChanged(false)
#if DEBUG_SCREENSHOTS
    , _screenshotFrameCounter(0), _screenshotIndex(0), _screenshotFileIndex(0)
#endif
{
    // Create the window.
    auto hwnd = ::CreateWindowEx(_frameExStyle,
        reinterpret_cast<LPCWSTR>(FrameWindowClass()), title,
        _frameStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, DllInstance, this);
    if (hwnd == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
    _window.Reset(hwnd, &::DestroyWindow);

    // Force 4:3 aspect ratio.
    ::GetWindowRect(_window, &_frameRect);
    OnSizing(WMSZ_TOP, &_frameRect);
    ::MoveWindow(_window, _frameRect.left, _frameRect.top, _frameRect.right - _frameRect.left, _frameRect.bottom - _frameRect.top, FALSE);

    // Initialize D3D.
    _d3d = ::Direct3DCreate9(D3D_SDK_VERSION);

    // Find the adapter corresponding to the window.
    HRESULT hr;
    UINT adapter;
    if (FAILED(hr = UpdateMonitor(adapter)))
        _com_raise_error(hr);

    if (FAILED(hr = RecreateDevice(adapter)))
        _com_raise_error(hr);

    WcdxColor defColor = { 0, 0, 0, 0xFF };
    std::fill_n(_palette, std::size(_palette), defColor);

    SetFullScreen(IsDebuggerPresent() ? false : _fullScreen);

#if DEBUG_SCREENSHOTS
    auto res = ::FindResource(DllInstance, MAKEINTRESOURCE(RESOURCE_ID_WC1PAL), RT_RCDATA);
    if (res == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
    auto resp = ::LoadResource(DllInstance, res);
    _screenshotPalette = static_cast<const std::byte*>(::LockResource(resp)) + 0x30;
    _screenshotPaletteSize = 0x300;
#endif
}

Wcdx::~Wcdx() = default;

HRESULT STDMETHODCALLTYPE Wcdx::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == nullptr)
        return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown))
    {
        *reinterpret_cast<IUnknown**>(ppvObject) = this;
        ++_refCount;
        return S_OK;
    }
    if (IsEqualIID(riid, IID_IWcdx))
    {
        *reinterpret_cast<IWcdx**>(ppvObject) = this;
        ++_refCount;
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Wcdx::AddRef()
{
    return ++_refCount;
}

ULONG STDMETHODCALLTYPE Wcdx::Release()
{
    if (--_refCount == 0)
    {
        delete this;
        return 0;
    }

    return _refCount;
}

HRESULT STDMETHODCALLTYPE Wcdx::SetVisible(BOOL visible)
{
    ::ShowWindow(_window, visible ? SW_SHOW : SW_HIDE);
    if (visible)
        ::PostMessage(_window, WM_APP_RENDER, 0, 0);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::SetPalette(const WcdxColor entries[256])
{
    std::copy_n(entries, 256, _palette);
    _dirty = true;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::UpdatePalette(UINT index, const WcdxColor* entry)
{
    _palette[index] = *entry;
    _dirty = true;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::UpdateFrame(INT x, INT y, UINT width, UINT height, UINT pitch, const byte* bits)
{
    RECT rect = { x, y, LONG(x + width), LONG(y + height) };
    RECT clipped =
    {
        std::max(rect.left, LONG(0)),
        std::max(rect.top, LONG(0)),
        std::min(rect.right, LONG(ContentWidth)),
        std::min(rect.bottom, LONG(ContentHeight))
    };

    auto src = reinterpret_cast<const std::byte*>(bits);
    auto dest = _framebuffer + clipped.left + (ContentWidth * clipped.top);
    width = clipped.right - clipped.left;
    for (height = clipped.bottom - clipped.top; height-- > 0; )
    {
        std::copy_n(src, width, dest);
        src += pitch;
        dest += ContentWidth;
    }
    _dirty = true;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::Present()
{
    HRESULT hr;
    if (FAILED(hr = _device->TestCooperativeLevel()))
    {
        if (hr != D3DERR_DEVICENOTRESET)
            return hr;

        if (FAILED(hr = ResetDevice()))
            return hr;
    }

    RECT clientRect;
    ::GetClientRect(_window, &clientRect);

    if (FAILED(hr = _device->BeginScene()))
        return hr;
    {
        at_scope_exit([&]{ _device->EndScene(); });

        if (_dirty)
        {
            D3DLOCKED_RECT locked;
            RECT bounds = { 0, 0, ContentWidth, ContentHeight };
            if (FAILED(hr = _surface->LockRect(&locked, &bounds, D3DLOCK_DISCARD)))
                return hr;
            {
                at_scope_exit([&]{ _surface->UnlockRect(); });

                const std::byte* src = _framebuffer;
                auto dest = static_cast<WcdxColor*>(locked.pBits);
                for (int row = 0; row < ContentHeight; ++row)
                {
                    std::transform(src, src + ContentWidth, dest, [&](std::byte index)
                    {
                        return _palette[uint8_t(index)];
                    });

                    src += ContentWidth;
                    dest += locked.Pitch / sizeof(*dest);
                }
            }
        }

#if DEBUG_SCREENSHOTS
        std::copy_n(_framebuffer, std::size(_framebuffer), _screenshotBuffers[_screenshotIndex]);
        if (++_screenshotIndex == std::size(_screenshotBuffers))
            _screenshotIndex = 0;

        if (_screenshotFrameCounter != 0 && --_screenshotFrameCounter == 0)
        {
            for (size_t n = 0; n != std::size(_screenshotBuffers); ++n)
            {
                auto index = (_screenshotIndex + n) % std::size(_screenshotBuffers);
                auto& buffer = _screenshotBuffers[index];
                stdext::array_view<const std::byte> paletteView(_screenshotPalette, _screenshotPaletteSize);
                stdext::memory_input_stream bufferStream(buffer, sizeof(buffer));
                stdext::file_output_stream file(stdext::format_string("screenshot${0:03}.png", _screenshotFileIndex).c_str(), stdext::utf8_path_encoding());
                wcdx::image::write_image({ ContentWidth, ContentHeight }, paletteView, bufferStream, file);
                ++_screenshotFileIndex;
            }
        }
#endif

        RECT activeRect = GetContentRect(clientRect);

            if (activeRect.right - activeRect.left < clientRect.right - clientRect.left)
            {
                D3DRECT bars[] =
                {
                    { clientRect.left, clientRect.top, activeRect.left, activeRect.bottom },
                    { activeRect.right, activeRect.top, clientRect.right, clientRect.bottom }
                };
                if (FAILED(hr = _device->Clear(2, bars, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0)))
                    return hr;
            }
            else if (activeRect.bottom - activeRect.top < clientRect.bottom - clientRect.top)
            {
                D3DRECT bars[] =
                {
                    { clientRect.left, clientRect.top, activeRect.right, activeRect.top },
                    { activeRect.left, activeRect.bottom, clientRect.right, clientRect.bottom }
                };
                if (FAILED(hr = _device->Clear(2, bars, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0)))
                    return hr;
            }

        if (_dirty || _sizeChanged)
        {
            IDirect3DSurface9Ptr backBuffer;
            if (FAILED(hr = _device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
                return hr;
            if (FAILED(hr = _device->StretchRect(_surface, nullptr, backBuffer, &activeRect, D3DTEXF_POINT)))
                return hr;
            _dirty = false;
            _sizeChanged = false;
        }
    }

    if (FAILED(hr = _device->Present(&clientRect, nullptr, nullptr, nullptr)))
        return hr;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::IsFullScreen()
{
    return _fullScreen ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointToClient(POINT* point)
{
    if (point == nullptr)
        return E_POINTER;

    RECT viewRect;
    if (!::GetClientRect(_window, &viewRect))
        return HRESULT_FROM_WIN32(::GetLastError());

    *point = ConvertTo(*point, GetContentRect(viewRect));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointFromClient(POINT* point)
{
    if (point == nullptr)
        return E_POINTER;

    RECT viewRect;
    if (!::GetClientRect(_window, &viewRect))
        return HRESULT_FROM_WIN32(::GetLastError());

    *point = ConvertFrom(*point, GetContentRect(viewRect));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectToClient(RECT* rect)
{
    if (rect == nullptr)
        return E_POINTER;

    RECT viewRect;
    if (!::GetClientRect(_window, &viewRect))
        return HRESULT_FROM_WIN32(::GetLastError());

    auto topLeft = ConvertTo({ rect->left, rect->top }, GetContentRect(viewRect));
    auto bottomRight = ConvertTo({ rect->right, rect->bottom }, GetContentRect(viewRect));
    *rect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectFromClient(RECT* rect)
{
    if (rect == nullptr)
        return E_POINTER;

    RECT viewRect;
    if (!::GetClientRect(_window, &viewRect))
        return HRESULT_FROM_WIN32(::GetLastError());

    auto topLeft = ConvertFrom({ rect->left, rect->top }, GetContentRect(viewRect));
    auto bottomRight = ConvertFrom({ rect->right, rect->bottom }, GetContentRect(viewRect));
    *rect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
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
                    ::DeleteFile(filename);
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

HRESULT STDMETHODCALLTYPE Wcdx::OpenFile(const char* filename, int oflag, int pmode, int* filedesc)
{
    if (filename == nullptr || filedesc == nullptr)
        return E_POINTER;

    return _sopen_s(filedesc, filename, oflag, _SH_DENYNO, pmode) == 0 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::CloseFile(int filedesc)
{
    return _close(filedesc) == 0 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::WriteFile(int filedesc, long offset, unsigned int size, const void* data)
{
    if (size > 0 && data == nullptr)
        return E_POINTER;

    if (offset != -1 && _lseek(filedesc, offset, SEEK_SET) == -1)
        return E_FAIL;

    return _write(filedesc, data, size) != -1 ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Wcdx::ReadFile(int filedesc, long offset, unsigned int size, void* data)
{
    if (size > 0 && data == nullptr)
        return E_POINTER;

    if (offset != -1 && _lseek(filedesc, offset, SEEK_SET) == -1)
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

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointToScreen(POINT* point)
{
    HRESULT hr;
    if (FAILED(hr = ConvertPointToClient(point)))
        return hr;
    ClientToScreen(_window, point);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertPointFromScreen(POINT* point)
{
    if (point == nullptr)
        return E_POINTER;

    ScreenToClient(_window, point);
    return ConvertPointFromClient(point);
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectToScreen(RECT* rect)
{
    HRESULT hr;
    if (FAILED(hr = ConvertRectToClient(rect)))
        return E_POINTER;
    ClientToScreen(_window, reinterpret_cast<POINT*>(&rect->left));
    ClientToScreen(_window, reinterpret_cast<POINT*>(&rect->right));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Wcdx::ConvertRectFromScreen(RECT* rect)
{
    if (rect == nullptr)
        return E_POINTER;

    ScreenToClient(_window, reinterpret_cast<POINT*>(&rect->left));
    ScreenToClient(_window, reinterpret_cast<POINT*>(&rect->right));
    return ConvertRectFromClient(rect);
}

HRESULT STDMETHODCALLTYPE Wcdx::QueryValue(const wchar_t* keyname, const wchar_t* valuename, void* data, DWORD* size)
{
    HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };

    for (auto root : roots)
    {
        HKEY key;
        auto error = ::RegOpenKeyEx(root, keyname, 0, KEY_QUERY_VALUE, &key);
        if (error != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(error);
        at_scope_exit([&]{ ::RegCloseKey(key); });

        error = ::RegQueryValueEx(key, valuename, nullptr, nullptr, static_cast<BYTE*>(data), size);
        if (error != ERROR_FILE_NOT_FOUND)
            return HRESULT_FROM_WIN32(error);
    }

    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

HRESULT STDMETHODCALLTYPE Wcdx::SetValue(const wchar_t* keyname, const wchar_t* valuename, DWORD type, const void* data, DWORD size)
{
    HKEY key;
    auto error = ::RegCreateKeyEx(HKEY_CURRENT_USER, keyname, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (error != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(error);
    at_scope_exit([&]{ ::RegCloseKey(key); });

    error = ::RegSetValueEx(key, valuename, 0, type, static_cast<const BYTE*>(data), size);
    return HRESULT_FROM_WIN32(error);
}

HRESULT STDMETHODCALLTYPE Wcdx::FillSnow(byte color_index, INT x, INT y, UINT width, UINT height, UINT pitch, byte* pixels)
{
    for (UINT h = 0; h != height; ++h)
    {
        auto p = pixels + (y * pitch) + x;
        for (UINT w = 0; w != width; ++w)
            *p++ = byte(RandomBit() * color_index);
        ++y;
    }

    return S_OK;
}

ATOM Wcdx::FrameWindowClass()
{
    static const ATOM windowClass = []
    {
        // Create an empty cursor.
        BYTE and_mask = 0xFF;
        BYTE xor_mask = 0;
        auto hcursor = ::CreateCursor(DllInstance, 0, 0, 1, 1, &and_mask, &xor_mask);
        if (hcursor == nullptr)
            throw std::system_error(::GetLastError(), std::system_category());

        WNDCLASSEX wc =
        {
            sizeof(WNDCLASSEX),
            0,
            FrameWindowProc,
            0,
            0,
            DllInstance,
            nullptr,
            hcursor,
            nullptr,
            nullptr,
            L"Wcdx Frame Window",
            nullptr
        };

        return ::RegisterClassEx(&wc);
    }();

    return windowClass;
}

LRESULT CALLBACK Wcdx::FrameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto wcdx = reinterpret_cast<Wcdx*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (wcdx == nullptr)
    {
        switch (message)
        {
        case WM_NCCREATE:
            // The client window procedure is an ANSI window procedure, so we need to mangle it
            // appropriately.  We can do that by passing it through SetWindowLongPtr.
            wcdx = static_cast<Wcdx*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
            auto wndproc = ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, LONG_PTR(wcdx->_clientWindowProc));
            wcdx->_clientWindowProc = WNDPROC(::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, wndproc));
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wcdx));
            return ::CallWindowProc(wcdx->_clientWindowProc, hwnd, message, wParam, lParam);
        }
    }
    else
    {
        switch (message)
        {
        case WM_SIZE:
            wcdx->OnSize(DWORD(wParam), LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_ACTIVATE:
            wcdx->OnActivate(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
            return 0;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_WINDOWPOSCHANGED:
            wcdx->OnWindowPosChanged(reinterpret_cast<WINDOWPOS*>(lParam));
            break;

        case WM_NCDESTROY:
            wcdx->OnNCDestroy();
            return 0;

        case WM_NCLBUTTONDBLCLK:
            if (wcdx->OnNCLButtonDblClk(int(wParam), *reinterpret_cast<POINTS*>(&lParam)))
                return 0;
            break;

        case WM_SYSCHAR:
            if (wcdx->OnSysChar(DWORD(wParam), LOWORD(lParam), HIWORD(lParam)))
                return 0;
            break;

        case WM_SYSCOMMAND:
            if (wcdx->OnSysCommand(WORD(wParam), LOWORD(lParam), HIWORD(lParam)))
                return 0;
            break;

        case WM_SIZING:
            wcdx->OnSizing(DWORD(wParam), reinterpret_cast<RECT*>(lParam));
            return TRUE;

        case WM_APP_RENDER:
            wcdx->OnRender();
            break;
        }
        return ::CallWindowProc(wcdx->_clientWindowProc, hwnd, message, wParam, lParam);
    }

    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

void Wcdx::OnSize(DWORD resizeType, WORD clientWidth, WORD clientHeight)
{
    stdext::discard(resizeType, clientWidth, clientHeight);

    _sizeChanged = true;
    ::PostMessage(_window, WM_APP_RENDER, 0, 0);
}

void Wcdx::OnActivate(WORD state, BOOL minimized, HWND other)
{
    stdext::discard(minimized, other);

    if (state != WA_INACTIVE)
        ConfineCursor();
}

void Wcdx::OnWindowPosChanged(WINDOWPOS* windowPos)
{
    if ((windowPos->flags & SWP_HIDEWINDOW) != 0 || _d3d == nullptr)
        return;

    HRESULT hr;
    UINT adapter;
    if (FAILED(hr = UpdateMonitor(adapter)))
        return;

    D3DDEVICE_CREATION_PARAMETERS parameters;
    if (FAILED(hr = _device->GetCreationParameters(&parameters)) || parameters.AdapterOrdinal != adapter)
        RecreateDevice(adapter);
}

void Wcdx::OnNCDestroy()
{
    _window.Invalidate();
}

bool Wcdx::OnNCLButtonDblClk(int hittest, POINTS position)
{
    stdext::discard(position);

    if (hittest != HTCAPTION)
        return false;

    // Windows should already be doing this, but doesn't appear to.
    ::SendMessage(_window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    return true;
}

bool Wcdx::OnSysChar(DWORD vkey, WORD repeatCount, WORD flags)
{
    if (vkey == VK_RETURN && ((flags & (KF_REPEAT | KF_ALTDOWN)) == KF_ALTDOWN))
    {
        SetFullScreen(!_fullScreen);
        return true;
    }

    return false;
}

bool Wcdx::OnSysCommand(WORD type, SHORT x, SHORT y)
{
    stdext::discard(x, y);

    if (type == SC_MAXIMIZE)
    {
        SetFullScreen(true);
        return true;
    }

    return false;
}

void Wcdx::OnSizing(DWORD windowEdge, RECT* dragRect)
{
    RECT client = { 0, 0, 0, 0 };
    ::AdjustWindowRectEx(&client, _frameStyle, FALSE, _frameExStyle);
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
    Present();
}

HRESULT Wcdx::UpdateMonitor(UINT& adapter)
{
    adapter = D3DADAPTER_DEFAULT;

    HRESULT hr;
    _monitor = ::MonitorFromWindow(_window, MONITOR_DEFAULTTONULL);
    for (UINT n = 0, count = _d3d->GetAdapterCount(); n < count; ++n)
    {
        D3DDISPLAYMODE currentMode;
        if (FAILED(hr = _d3d->GetAdapterDisplayMode(n, &currentMode)))
            return hr;

        // Require hardware acceleration.
        hr = _d3d->CheckDeviceType(n, D3DDEVTYPE_HAL, currentMode.Format, currentMode.Format, TRUE);
        if (hr == D3DERR_NOTAVAILABLE)
            continue;
        if (FAILED(hr))
            return hr;

        D3DCAPS9 caps;
        hr = _d3d->GetDeviceCaps(n, D3DDEVTYPE_HAL, &caps);
        if (hr == D3DERR_NOTAVAILABLE)
            continue;
        if (FAILED(hr))
            return hr;

        // Select the adapter that's already displaying the window.
        if (_d3d->GetAdapterMonitor(n) == _monitor)
        {
            adapter = n;
            break;
        }
    }

    return S_OK;
}

HRESULT Wcdx::RecreateDevice(UINT adapter)
{
    HRESULT hr;
    D3DDISPLAYMODE currentMode;
    if (FAILED(hr = _d3d->GetAdapterDisplayMode(adapter, &currentMode)))
        return hr;

    _presentParams =
    {
        currentMode.Width, currentMode.Height, currentMode.Format, 1,
        D3DMULTISAMPLE_NONE, 0,
        D3DSWAPEFFECT_COPY, _window, TRUE,
        FALSE, D3DFMT_UNKNOWN,
        0, 0, D3DPRESENT_INTERVAL_DEFAULT
    };

    if (FAILED(hr = _d3d->CreateDevice(adapter, D3DDEVTYPE_HAL, _window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &_presentParams, &_device)))
        return hr;

    if (FAILED(hr = CreateIntermediateSurface()))
        return hr;

    return S_OK;
}

HRESULT Wcdx::ResetDevice()
{
    HRESULT hr;
    _surface = nullptr;
    if (FAILED(hr = _device->Reset(&_presentParams)))
        return hr;

    if (FAILED(hr = CreateIntermediateSurface()))
        return hr;

    return S_OK;
}

HRESULT Wcdx::CreateIntermediateSurface()
{
    _dirty = true;
    return _device->CreateOffscreenPlainSurface(ContentWidth, ContentHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &_surface, nullptr);
}

void Wcdx::SetFullScreen(bool enabled)
{
    if (enabled == _fullScreen)
        return;

    if (enabled)
    {
        ::GetWindowRect(_window, &_frameRect);

        _frameStyle = ::SetWindowLong(_window, GWL_STYLE, WS_OVERLAPPED);
        _frameExStyle = ::SetWindowLong(_window, GWL_EXSTYLE, 0);

        HMONITOR monitor = ::MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        ::GetMonitorInfo(monitor, &monitorInfo);

        ::SetWindowPos(_window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);

        _fullScreen = true;
    }
    else
    {
        ::SetLastError(0);
        ::SetWindowLong(_window, GWL_STYLE, _frameStyle);
        ::SetWindowLong(_window, GWL_EXSTYLE, _frameExStyle);

        ::SetWindowPos(_window, HWND_TOP, _frameRect.left, _frameRect.top,
            _frameRect.right - _frameRect.left,
            _frameRect.bottom - _frameRect.top,
            SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW);

        _fullScreen = false;
    }

    ConfineCursor();
    ::PostMessage(_window, WM_APP_RENDER, 0, 0);
}

RECT Wcdx::GetContentRect(RECT clientRect)
{
    auto width = (4 * clientRect.bottom) / 3;
    auto height = (3 * clientRect.right) / 4;
    if (width < clientRect.right)
    {
        clientRect.left = (clientRect.right - width) / 2;
        clientRect.right = clientRect.left + width;
    }
    else
    {
        clientRect.top = (clientRect.bottom - height) / 2;
        clientRect.bottom = clientRect.top + height;
    }

    return clientRect;
}

void Wcdx::ConfineCursor()
{
    if (_fullScreen)
    {
        RECT contentRect;
        ::GetClientRect(_window, &contentRect);
        contentRect = GetContentRect(contentRect);

        POINT topLeft = { contentRect.left, contentRect.top };
        POINT bottomRight = { contentRect.right, contentRect.bottom };
        ::ClientToScreen(_window, &topLeft);
        ::ClientToScreen(_window, &bottomRight);

        contentRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
        ::ClipCursor(&contentRect);
    }
    else
        ::ClipCursor(nullptr);
}

namespace
{
    POINT ConvertTo(POINT point, RECT rect)
    {
        return
        {
            rect.left + ((point.x * (rect.right - rect.left)) / Wcdx::ContentWidth),
            rect.top + ((point.y * (rect.bottom - rect.top)) / Wcdx::ContentHeight)
        };
    }

    POINT ConvertFrom(POINT point, RECT rect)
    {
        return
        {
            ((point.x - rect.left) * Wcdx::ContentWidth) / (rect.right - rect.left),
            ((point.y - rect.top) * Wcdx::ContentHeight) / (rect.bottom - rect.top)
        };
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

                at_scope_exit([&]{ ::CoTaskMemFree(folderPath); });

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

        auto i = std::find(std::make_reverse_iterator(pathName + wcslen(pathName)), std::make_reverse_iterator(pathName), L'\\');
        if (i.base() == pathName)
            return false;

        *i = L'\0';
        auto result = CreateDirectoryRecursive(pathName);
        *i = L'\\';
        return result && ::CreateDirectory(pathName, nullptr);
    }
}
