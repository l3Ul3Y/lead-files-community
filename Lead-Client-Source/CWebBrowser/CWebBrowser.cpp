#include "CWebBrowser.h"
#include <windows.h>
#include <objbase.h> // Required for COM interfaces if WIN32_LEAN_AND_MEAN is used
#include <string>
#include <queue>
#include <atomic>
#include "webview2/WebView2.h"
#include <webview2/com.h>
#include <wrl.h>

using namespace Microsoft::WRL;

#pragma comment(lib, "WebView2Loader.lib")

static wil::com_ptr<ICoreWebView2Controller> g_controller;
static wil::com_ptr<ICoreWebView2> g_webview;
static HWND g_hParentWnd = NULL;
static RECT g_rcWebBrowser = { 0 };
static bool g_bVisible = false;
static bool g_bInitialized = false;
static std::string g_pendingUrl;

void InitializeWebView(HWND hWnd);

extern "C" {

int WebBrowser_Startup(HINSTANCE hInstance)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    return 1;
}

void WebBrowser_Cleanup()
{
    if (g_controller)
    {
        g_controller->Close();
        g_controller = nullptr;
    }
    g_webview = nullptr;
    CoUninitialize();
}

void WebBrowser_Destroy()
{
    WebBrowser_Cleanup();
}

int WebBrowser_Show(HWND parent, const char* addr, const RECT* rcWebBrowser)
{
    g_hParentWnd = parent;
    if (rcWebBrowser)
        g_rcWebBrowser = *rcWebBrowser;

    g_bVisible = true;

    if (!g_controller)
    {
        g_pendingUrl = addr ? addr : "";
        InitializeWebView(parent);
    }
    else
    {
        g_controller->put_IsVisible(TRUE);
        g_controller->put_Bounds(g_rcWebBrowser);
        
        if (addr && *addr)
        {
            int len = MultiByteToWideChar(CP_UTF8, 0, addr, -1, NULL, 0);
            if (len > 0)
            {
                std::wstring wstr(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, addr, -1, &wstr[0], len);
                g_webview->Navigate(wstr.c_str());
            }
        }
    }
    return 1;
}

void WebBrowser_Hide()
{
    g_bVisible = false;
    if (g_controller)
    {
        g_controller->put_IsVisible(FALSE);
    }
}

void WebBrowser_Move(const RECT* rcWebBrowser)
{
    if (rcWebBrowser)
        g_rcWebBrowser = *rcWebBrowser;

    if (g_controller)
    {
        g_controller->put_Bounds(g_rcWebBrowser);
    }
}

int WebBrowser_IsVisible()
{
    return g_bVisible ? 1 : 0;
}

const RECT& WebBrowser_GetRect()
{
    return g_rcWebBrowser;
}

}

void InitializeWebView(HWND hWnd)
{
    if (g_bInitialized) return;
    g_bInitialized = true;

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                     g_bInitialized = false;
                     return result;
                }
                
                env->CreateCoreWebView2Controller(hWnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) {
                                g_bInitialized = false;
                                return result;
                            }

                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            g_controller->put_Bounds(g_rcWebBrowser);
                            g_controller->put_IsVisible(g_bVisible ? TRUE : FALSE);

                            if (!g_pendingUrl.empty())
                            {
                                int len = MultiByteToWideChar(CP_UTF8, 0, g_pendingUrl.c_str(), -1, NULL, 0);
                                if (len > 0)
                                {
                                    std::wstring wstr(len, 0);
                                    MultiByteToWideChar(CP_UTF8, 0, g_pendingUrl.c_str(), -1, &wstr[0], len);
                                    g_webview->Navigate(wstr.c_str());
                                }
                                g_pendingUrl.clear();
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}
