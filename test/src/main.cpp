#include "common.h"
#include "../res/resources.h"

#include <iwcdx.h>

#include <DxErr.h>
#include <Shlwapi.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <assert.h>

using std::min;
using std::max;
#include <gdiplus.h>


using namespace Gdiplus;
using namespace std;

enum
{
	WM_APP_RENDER = WM_APP
};

struct WindowData
{
	Wcdx& wcdx;
	vector<unique_ptr<Bitmap>> images;
	size_t imageIndex;
};

static unique_ptr<Bitmap> LoadPng(LPCWSTR resource);
static void ShowImage(HWND window, Bitmap& image);

static bool OnCreate(HWND window, const CREATESTRUCT& create);
static void OnDestroy(HWND window);
static void OnShowWindow(HWND window, bool show, DWORD reason);
static void OnLButtonUp(HWND window, WORD x, WORD y, DWORD flags);
static void OnRender(HWND window);
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCommandLine, int nCmdShow)
{
	try
	{
		ULONG_PTR gdiplusToken;
		GdiplusStartupInput gdiStartupInput(nullptr, TRUE, TRUE);
		GdiplusStartupOutput gdiStartupOutput;
		::GdiplusStartup(&gdiplusToken, &gdiStartupInput, &gdiStartupOutput);
		at_scope_exit([&]{ ::GdiplusShutdown(gdiplusToken); });
		ULONG_PTR gdiplusHookToken;
		gdiStartupOutput.NotificationHook(&gdiplusHookToken);
		at_scope_exit([&]{ gdiStartupOutput.NotificationUnhook(gdiplusHookToken); });

		WNDCLASSEX wc =
		{
			sizeof(WNDCLASSEX),
			0,
			WindowProc,
			0, 0,
			hInstance,
			nullptr,
			::LoadCursor(nullptr, IDC_ARROW),
			reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
			nullptr,
			L"Wcdx Test Window",
			nullptr
		};

		ATOM wcAtom = ::RegisterClassEx(&wc);

		HWND window = ::CreateWindowEx(0, MAKEINTATOM(wcAtom),
			L"Wcdx Test", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
			CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
			nullptr, nullptr, hInstance, nullptr);
		Wcdx wcdx(window);
		WindowData windowData = { wcdx };
		::SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&windowData));

		for (int n = 0; n < 10; ++n)
			windowData.images.push_back(LoadPng(MAKEINTRESOURCE(IDPNG_SCREEN0 + n)));

		ShowImage(window, *windowData.images[windowData.imageIndex]);

		::ShowWindow(window, nCmdShow);

		MSG message;
		BOOL result;
		while ((result = ::GetMessage(&message, nullptr, 0, 0)) != 0)
		{
			if (result == -1)
				return EXIT_FAILURE;
			::DispatchMessage(&message);
		}
		return message.wParam;
	}
	catch (const _com_error& error)
	{
		LPCWSTR errorMessage = HRESULT_FACILITY(error.Error()) == _FACD3D ? DXGetErrorDescription(error.Error()) : error.ErrorMessage();
		::MessageBox(nullptr, errorMessage, L"COM Error", MB_ICONERROR | MB_OK);
	}

	return EXIT_FAILURE;
}

unique_ptr<Bitmap> LoadPng(LPCWSTR resource)
{
	HRSRC resourceHandle = ::FindResource(nullptr, resource, L"PNG");
	HGLOBAL resourceGlobal = ::LoadResource(nullptr, resourceHandle);
	LPVOID resourceData = ::LockResource(resourceGlobal);
	DWORD resourceSize = ::SizeofResource(nullptr, resourceHandle);

	IStream* stream = ::SHCreateMemStream(static_cast<const BYTE*>(resourceData), resourceSize);
	if (stream != nullptr)
	{
		at_scope_exit([&]{ stream->Release(); });
		return unique_ptr<Bitmap>(new Bitmap(stream));
	}

	return nullptr;
}

void ShowImage(HWND window, Bitmap& image)
{
	WindowData& windowData = *reinterpret_cast<WindowData*>(::GetWindowLongPtr(window, GWLP_USERDATA));

	vector<BYTE> paletteData(image.GetPaletteSize());
	ColorPalette& palette = *reinterpret_cast<ColorPalette*>(paletteData.data());
	image.GetPalette(&palette, paletteData.size());

	assert(palette.Count == 256);
	windowData.wcdx.SetPalette(reinterpret_cast<PALETTEENTRY*>(palette.Entries));

	Rect imageRect(0, 0, image.GetWidth(), image.GetHeight());
	BitmapData bits;
	image.LockBits(&imageRect, 0, image.GetPixelFormat(), &bits);
	at_scope_exit([&]{ image.UnlockBits(&bits); });

	RECT updateRect = { 0, 0, image.GetWidth(), image.GetHeight() };
	windowData.wcdx.UpdateFrame(bits.Scan0, updateRect, bits.Stride);
}

bool OnCreate(HWND window, const CREATESTRUCT& create)
{
	if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
		return false;
	return true;
}

void OnDestroy(HWND window)
{
	::CoUninitialize();
	::PostQuitMessage(EXIT_SUCCESS);
}

void OnShowWindow(HWND window, bool show, DWORD reason)
{
	if (show)
		::PostMessage(window, WM_APP_RENDER, 0, 0);
}

void OnLButtonUp(HWND window, WORD x, WORD y, DWORD flags)
{
	WindowData& windowData = *reinterpret_cast<WindowData*>(::GetWindowLongPtr(window, GWLP_USERDATA));
	if (++windowData.imageIndex == windowData.images.size())
		windowData.imageIndex = 0;

	ShowImage(window, *windowData.images[windowData.imageIndex]);
	::PostMessage(window, WM_APP_RENDER, 0, 0);
}

void OnRender(HWND window)
{
	if (!::IsWindowVisible(window))
		return;

	WindowData& windowData = *reinterpret_cast<WindowData*>(::GetWindowLongPtr(window, GWLP_USERDATA));
	windowData.wcdx.Present();

//	::PostMessage(window, WM_APP_RENDER, 0, 0);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NCCREATE:
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		break;

	case WM_CREATE:
		if (!OnCreate(hWnd, *reinterpret_cast<LPCREATESTRUCT>(lParam)))
			return -1;
		break;

	case WM_DESTROY:
		OnDestroy(hWnd);
		break;

	case WM_SHOWWINDOW:
		OnShowWindow(hWnd, wParam != FALSE, lParam);
		break;

	case WM_LBUTTONUP:
		OnLButtonUp(hWnd, LOWORD(lParam), HIWORD(lParam), wParam);
		break;

	case WM_APP_RENDER:
		OnRender(hWnd);
		break;
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}
