#include <windows.h>
#include <dwmapi.h>
#include <vector>
#include <thread>
#include <fstream>
#include <string>
#include <sstream>
#include <json/json.h>
#include <algorithm>
#include <iostream>

#pragma comment(lib, "dwmapi.lib")

struct ESPBox {
    float footX, footY;
    float headX, headY;
    float distance;
};

std::vector<ESPBox> enemyBoxes;

const char* TARGET_WINDOW_NAME = "Counter-Strike";

HWND targetHwnd = nullptr;
HWND overlayHwnd = nullptr;
RECT gameRect = { 0 };

// Draw mode: 0 = box only, 1 = letters (not used currently)
const int DRAW_MODE = 0;  // You can switch to 0 to test box drawing

void GetGameClientRect(HWND hwnd, RECT* clientRect, POINT* clientScreenPos) {
    // Get the client area coordinates relative to the window's top-left corner
    GetClientRect(hwnd, clientRect);
    // Convert the client area origin (0,0) to screen coordinates (accounts for window border)
    POINT clientOrigin = { 0, 0 };
    ClientToScreen(hwnd, &clientOrigin);
    clientScreenPos->x = clientOrigin.x;
    clientScreenPos->y = clientOrigin.y;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;  // Disable default background erase to avoid flickering

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, TRUE);  // Trigger repaint
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Clear ghosting (fill with transparent color)
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &clientRect, clearBrush);
        DeleteObject(clearBrush);

        // Set draw color (red)
        SetTextColor(hdc, RGB(255, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // Get game client area position on screen (for coordinate conversion)
        RECT gameClientRect;
        POINT gameClientPos;
        if (targetHwnd) {
            GetGameClientRect(targetHwnd, &gameClientRect, &gameClientPos);
        }

        for (auto& box : enemyBoxes) {
            // Foot position
            int xFoot = static_cast<int>(box.footX - gameClientPos.x);
            int yFoot = static_cast<int>(box.footY - gameClientPos.y);

            // Head position
            int xHead = static_cast<int>(box.headX - gameClientPos.x);
            int yHead = static_cast<int>(box.headY - gameClientPos.y);

            if (xFoot < 0 || xFoot >= 1024 || yFoot < 0 || yFoot >= 768) continue;

            // Calculate center and box dimensions
            int boxHeight = yFoot - yHead;
            int boxWidth = static_cast<int>(boxHeight / 2.0f);  // Approximate 1:2 aspect ratio
            int xLeft = xFoot - boxWidth / 2;
            int xRight = xFoot + boxWidth / 2;

            // Draw red box
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            SelectObject(hdc, pen);
            HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            SelectObject(hdc, nullBrush);

            Rectangle(hdc, xLeft, yHead, xRight, yFoot);

            DeleteObject(pen);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void UpdateOverlayPos() {
    while (true) {
        HWND hwnd = FindWindowA(NULL, TARGET_WINDOW_NAME);
        if (hwnd) {
            targetHwnd = hwnd;
            RECT gameClientRect;  // Game client area size (e.g., 1024×768)
            POINT gameClientPos;  // Top-left screen position of game client area
            GetGameClientRect(hwnd, &gameClientRect, &gameClientPos);

            // Overlay size = game client area size
            int clientWidth = gameClientRect.right - gameClientRect.left;
            int clientHeight = gameClientRect.bottom - gameClientRect.top;

            // Overlay position = top-left of client area (excludes window borders)
            MoveWindow(overlayHwnd,
                gameClientPos.x,  // X screen coordinate
                gameClientPos.y,  // Y screen coordinate
                clientWidth,      // 1024
                clientHeight,     // 768
                TRUE);
        }
        Sleep(60);  // ~30 FPS
    }
}

void UpdateESPFromFile()
{
    while (true)
    {
        std::ifstream file(R"(C:\Users\Public\esp_data.json)");
        if (file)
        {
            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;

            Json::Value root;
            std::string errs;
            bool ok = Json::parseFromStream(builder, file, &root, &errs);
            file.close();

            if (!ok) {
                std::cerr << "JSON parse error: " << errs << std::endl;
                Sleep(100);
                continue;
            }

            std::vector<ESPBox> newBoxes;
            for (const auto& e : root)
            {
                if (!e.isMember("x") || !e.isMember("y") || !e.isMember("head")) continue;
                if (!e["head"].isMember("x") || !e["head"].isMember("y")) continue;

                ESPBox box;
                box.footX = e["x"].asFloat();
                box.footY = e["y"].asFloat();
                box.headX = e["head"]["x"].asFloat();
                box.headY = e["head"]["y"].asFloat();
                box.distance = e.get("distance", 0.0f).asFloat();

                newBoxes.push_back(box);
            }

            enemyBoxes = newBoxes;
        }

        Sleep(100);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    const char CLASS_NAME[] = "OverlayESPClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    overlayHwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        CLASS_NAME, "",
        WS_POPUP,
        0, 0, 800, 600,
        NULL, NULL, hInstance, NULL);

    // Set transparent color to black (black will not be rendered)
    SetLayeredWindowAttributes(overlayHwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(overlayHwnd, &margins);

    ShowWindow(overlayHwnd, SW_SHOW);

    // Initialize window position
    targetHwnd = FindWindowA(NULL, TARGET_WINDOW_NAME);
    if (targetHwnd)
    {
        GetWindowRect(targetHwnd, &gameRect);
        MoveWindow(overlayHwnd, gameRect.left, gameRect.top,
            gameRect.right - gameRect.left,
            gameRect.bottom - gameRect.top, TRUE);
    }

    // Start timer to trigger repaint (~30FPS)
    SetTimer(overlayHwnd, 1, 33, NULL);

    // Launch background threads
    std::thread(UpdateOverlayPos).detach();
    std::thread(UpdateESPFromFile).detach();

    // Message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
