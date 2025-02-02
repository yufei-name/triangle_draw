#include <stdio.h>
#include "common.h"
#include "window_system.h"
#include <vulkan/vulkan_win32.h>

unsigned long long platform_param;

static ATOM win32WindowClass;
static HINSTANCE   win32Instance;
static HWND hWin;
static WNDCLASSEXW wc = { sizeof(wc) };

static const char* win32PlatformExtensionName[] = {
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

const char** get_platform_extension(unsigned int* platform_extension_num)
{
    if (platform_extension_num)
        *platform_extension_num = sizeof(win32PlatformExtensionName) / sizeof(win32PlatformExtensionName[0]);
    return win32PlatformExtensionName;
}
static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CLOSE: {
        // Prompt user to confirm exit
        int result = MessageBox(
            hWnd,
            L"Are you sure you want to exit?",
            L"Confirm Exit",
            MB_YESNO | MB_ICONQUESTION
        );

        if (result == IDYES) {
            DestroyWindow(hWnd); // Destroy the window if user confirms
        }
        return 0; // Do not pass WM_CLOSE to DefWindowProc
    }
    case WM_DESTROY:
        PostQuitMessage(0); // Post WM_QUIT to exit the message loop
        return 0;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}
int createNativeWindow(HINSTANCE hInstance, HWND *pHwind)
{
    DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION | WS_MAXIMIZEBOX | WS_THICKFRAME;
    DWORD exStyle = WS_EX_APPWINDOW;
    const char* source = "vulkan_triangle";
    
    HWND hWindow;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"Yufei";
    wc.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
        L"YUFEI_ICON", IMAGE_ICON,
        0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!wc.hIcon)
    {
        // No user-provided icon found, load default icon
        wc.hIcon = (HICON)LoadImageW(NULL,
            IDI_APPLICATION, IMAGE_ICON,
            0, 0, LR_DEFAULTSIZE | LR_SHARED);
    }

    win32WindowClass = RegisterClassExW(&wc);
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    WCHAR* wideTitle;
    int count;

    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    if (!count)
    {
        printf("Win32: Failed to convert string from UTF-8");
        return -1;
    }

    wideTitle = (WCHAR*)calloc(count, sizeof(WCHAR));

    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, wideTitle, count))
    {
        printf("Win32: Failed to convert string from UTF-8");
        free(wideTitle);
        return -1;
    }

    hWindow = CreateWindowExW(exStyle,
        MAKEINTATOM(win32WindowClass),
        wideTitle,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, // No parent window
        NULL, // No window menu
        hInstance,
        NULL);

    free(wideTitle);

    if (!hWindow)
    {
        printf("Win32: Failed to create window");
        return -1;
    }

    SetPropW(hWindow, L"GLFW", NULL);

    ChangeWindowMessageFilterEx(hWindow,WM_DROPFILES, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(hWindow,WM_COPYDATA, MSGFLT_ALLOW, NULL);
    //ChangeWindowMessageFilterEx(hWindow,WM_COPYGLOBALDATA, MSGFLT_ALLOW, NULL);

    WINDOWPLACEMENT wp = { sizeof(wp) };
    const HMONITOR mh = MonitorFromWindow(hWindow,MONITOR_DEFAULTTONEAREST);

    rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, GetDpiForWindow(hWindow));
    GetWindowPlacement(hWindow, &wp);
    OffsetRect(&rect,wp.rcNormalPosition.left - rect.left,wp.rcNormalPosition.top - rect.top);

    wp.rcNormalPosition = rect;
    wp.showCmd = SW_HIDE;
    SetWindowPlacement(hWindow, &wp);

    DragAcceptFiles(hWindow, TRUE);

    ShowWindow(hWindow, SW_SHOWNA);
    *pHwind = hWindow;
    return 0;
}
VkSurfaceKHR createWindowSurface(HINSTANCE hInstance, HWND hWindow, VkInstance vulkanInstance)
{
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
    memset(&surfaceCreateInfo, 0, sizeof(surfaceCreateInfo));
    VkSurfaceKHR surface;

    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hinstance = hInstance;
    surfaceCreateInfo.hwnd = hWindow;

    VkResult err = vkCreateWin32SurfaceKHR(vulkanInstance, &surfaceCreateInfo, NULL, &surface);
    if (err)
    {
        printf("Win32: Failed to create Vulkan surface: %d",err);
        return VK_NULL_HANDLE;
    }
    return surface;
}
int platform_initialization(VkInstance vkInst, VkSurfaceKHR* vkSurface)
{
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const WCHAR*)&platform_param,
        (HMODULE*)&win32Instance))
    {
        printf("Win32: Failed to retrieve own module handle");
        return -1;
    }
    createNativeWindow(win32Instance, &hWin);
    *vkSurface = createWindowSurface(win32Instance, hWin, vkInst);

    return 0;
}
int platform_deinitialization()
{
    DestroyWindow(hWin);
    return 0;
}

int platform_process_event(event_param_t* event_para)
{
    MSG msg;
    HWND handle;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            event_para->type = 1;
        }
        else if (msg.message == WM_DESTROY)
        {
            event_para->type = 1;
            PostQuitMessage(WM_QUIT);
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // HACK: Release modifier keys that the system did not emit KEYUP for
    // NOTE: Shift keys on Windows tend to "stick" when both are pressed as
    //       no key up message is generated by the first key release
    // NOTE: Windows key is not reported as released by the Win+V hotkey
    //       Other Win hotkeys are handled implicitly by _glfwInputWindowFocus
    //       because they change the input focus
    // NOTE: The other half of this is in the WM_*KEY* handler in windowProc
    /*handle = GetActiveWindow();
    if (handle)
    {
        window = GetPropW(handle, L"GLFW");
        if (window)
        {
            int i;
            const int keys[4][2] =
            {
                { VK_LSHIFT, GLFW_KEY_LEFT_SHIFT },
                { VK_RSHIFT, GLFW_KEY_RIGHT_SHIFT },
                { VK_LWIN, GLFW_KEY_LEFT_SUPER },
                { VK_RWIN, GLFW_KEY_RIGHT_SUPER }
            };

            for (i = 0; i < 4; i++)
            {
                const int vk = keys[i][0];
                const int key = keys[i][1];
                const int scancode = _glfw.win32.scancodes[key];

                if ((GetKeyState(vk) & 0x8000))
                    continue;
                if (window->keys[key] != GLFW_PRESS)
                    continue;

                _glfwInputKey(window, key, scancode, GLFW_RELEASE, getKeyMods());
            }
        }
    }

    window = _glfw.win32.disabledCursorWindow;
    if (window)
    {
        int width, height;
        _glfwGetWindowSizeWin32(window, &width, &height);

        // NOTE: Re-center the cursor only if it has moved since the last call,
        //       to avoid breaking glfwWaitEvents with WM_MOUSEMOVE
        if (window->win32.lastCursorPosX != width / 2 ||
            window->win32.lastCursorPosY != height / 2)
        {
            _glfwSetCursorPosWin32(window, width / 2, height / 2);
        }
    }*/
    return 0;
}