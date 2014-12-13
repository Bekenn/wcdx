#include "common.h"
#include "../res/resources.h"

#include <iwcdx.h>

#include <d3d9.h>
#include <Shlwapi.h>
#include <comdef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <assert.h>

// GDI+ headers use unqualified min and max (probably expecting the Windows macros)
using std::min;
using std::max;
#pragma warning(push)
#pragma warning(disable: 4458) // VS2015 Preview warns about hidden names in GDI+ headers
#include <gdiplus.h>
#pragma warning(pop)


using namespace Gdiplus;
using namespace std;

enum
{
	WM_APP_RENDER = WM_APP
};

static unique_ptr<Bitmap> LoadPng(LPCWSTR resource);
static void ShowImage(IWcdx* wcdx, Bitmap& image);

static void OnDestroy(HWND window);
static void OnShowWindow(HWND window, bool show, DWORD reason);
static void OnLButtonUp(HWND window, WORD x, WORD y, DWORD flags);
static void OnRender(HWND window);
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static vector<unique_ptr<Bitmap>>* Images;
static size_t ImageIndex = 0;

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

		vector<unique_ptr<Bitmap>> images;
		for (int n = 0; n < 10; ++n)
			images.push_back(LoadPng(MAKEINTRESOURCE(IDPNG_SCREEN0 + n)));
		Images = &images;

		if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
			return EXIT_FAILURE;
		IWcdx* wcdx = WcdxCreate(L"Wcdx Test", WindowProc, TRUE);

		ShowImage(wcdx, *images[ImageIndex]);
		wcdx->SetVisible(TRUE);

		MSG message;
		BOOL result;
		while ((result = ::GetMessage(&message, nullptr, 0, 0)) != 0)
		{
			if (result == -1)
				return EXIT_FAILURE;
			::DispatchMessage(&message);
		}

		wcdx->Release();
		::CoUninitialize();
		return message.wParam;
	}
	catch (const _com_error& error)
	{
		::MessageBox(nullptr, error.ErrorMessage(), L"COM Error", MB_ICONERROR | MB_OK);
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

void ShowImage(IWcdx* wcdx, Bitmap& image)
{
	vector<BYTE> paletteData(image.GetPaletteSize());
	ColorPalette& palette = *reinterpret_cast<ColorPalette*>(paletteData.data());
	image.GetPalette(&palette, paletteData.size());

	assert(palette.Count == 256);
	wcdx->SetPalette(reinterpret_cast<WcdxColor*>(palette.Entries));

	Rect imageRect(0, 0, image.GetWidth(), image.GetHeight());
	BitmapData bits;
	image.LockBits(&imageRect, 0, image.GetPixelFormat(), &bits);
	at_scope_exit([&]{ image.UnlockBits(&bits); });

	RECT updateRect = { 0, 0, LONG(image.GetWidth()), LONG(image.GetHeight()) };
	wcdx->UpdateFrame(updateRect.left, updateRect.top, updateRect.right - updateRect.left, updateRect.bottom - updateRect.top, bits.Stride, reinterpret_cast<byte*>(bits.Scan0));
}

void OnDestroy(HWND window)
{
	::PostQuitMessage(EXIT_SUCCESS);
}

void OnShowWindow(HWND window, bool show, DWORD reason)
{
	if (show)
		::PostMessage(window, WM_APP_RENDER, 0, 0);
}

void OnLButtonUp(HWND window, WORD x, WORD y, DWORD flags)
{
	IWcdx* wcdx = reinterpret_cast<IWcdx*>(::GetWindowLongPtr(window, GWLP_USERDATA));
	if (++ImageIndex == Images->size())
		ImageIndex = 0;

	ShowImage(wcdx, *(*Images)[ImageIndex]);
	::PostMessage(window, WM_APP_RENDER, 0, 0);
}

void OnRender(HWND window)
{
	if (!::IsWindowVisible(window))
		return;

	IWcdx* wcdx = reinterpret_cast<IWcdx*>(::GetWindowLongPtr(window, GWLP_USERDATA));
	wcdx->Present();

//	::PostMessage(window, WM_APP_RENDER, 0, 0);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
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
