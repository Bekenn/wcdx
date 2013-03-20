#include "common.h"
#include <wcdx.h>


static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCommandLine, int nCmdShow)
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

	HWND window = ::CreateWindowEx(0, MAKEINTATOM(wcAtom), L"Wcdx Test", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX), CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, hInstance, nullptr);
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

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		::PostQuitMessage(EXIT_SUCCESS);
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}
