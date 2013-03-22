#include "common.h"
#include "../res/resources.h"

#include <wcdx.h>

#include <DxErr.h>


enum
{
	WM_APP_RENDER = WM_APP
};

static bool OnCreate(HWND window, const CREATESTRUCT& create);
static void OnDestroy(HWND window);
static void OnShowWindow(HWND window, bool show, DWORD reason);
static void OnRender(HWND window);
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCommandLine, int nCmdShow)
{
	try
	{
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
		::SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&wcdx));

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

void OnRender(HWND window)
{
	if (!::IsWindowVisible(window))
		return;

	Wcdx& wcdx = *reinterpret_cast<Wcdx*>(::GetWindowLongPtr(window, GWLP_USERDATA));
	wcdx.Present();

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

	case WM_APP_RENDER:
		OnRender(hWnd);
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}
