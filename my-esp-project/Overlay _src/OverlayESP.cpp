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

// 绘制模式：0=点，1=字母
const int DRAW_MODE = 0;  // 可切换为0测试点绘制

void GetGameClientRect(HWND hwnd, RECT* clientRect, POINT* clientScreenPos) {
    // 获取客户区在窗口内的坐标（相对窗口左上角）
    GetClientRect(hwnd, clientRect);
    // 将客户区左上角（0,0）转换为屏幕绝对坐标（含窗口边框偏移）
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
        return 1;  // 禁用默认背景擦除，避免闪烁

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, TRUE);  // 触发重绘
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 清除残影（用透明色填充）
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &clientRect, clearBrush);
        DeleteObject(clearBrush);

        // 设置绘制颜色（红色）
        SetTextColor(hdc, RGB(255, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // 获取游戏客户区在屏幕上的位置（用于坐标转换）
        RECT gameClientRect;
        POINT gameClientPos;
        if (targetHwnd) {
            GetGameClientRect(targetHwnd, &gameClientRect, &gameClientPos);
        }

        for (auto& box : enemyBoxes) {
            // 脚部坐标
            int xFoot = static_cast<int>(box.footX - gameClientPos.x);
            int yFoot = static_cast<int>(box.footY - gameClientPos.y);

            // 头部坐标
            int xHead = static_cast<int>(box.headX - gameClientPos.x);
            int yHead = static_cast<int>(box.headY - gameClientPos.y);

            if (xFoot < 0 || xFoot >= 1024 || yFoot < 0 || yFoot >= 768) continue;

            // 算出中心点和高宽
            int boxHeight = yFoot - yHead;
            int boxWidth = static_cast<int>(boxHeight / 2.0f);  // 宽高比大约1:2
            int xLeft = xFoot - boxWidth / 2;
            int xRight = xFoot + boxWidth / 2;

            // 画红框
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
            RECT gameClientRect;  // 游戏客户区大小（1024×768）
            POINT gameClientPos;  // 游戏客户区在屏幕上的左上角坐标（含边框偏移）
            GetGameClientRect(hwnd, &gameClientRect, &gameClientPos);

            // Overlay窗口大小 = 游戏客户区大小（1024×768）
            int clientWidth = gameClientRect.right - gameClientRect.left;
            int clientHeight = gameClientRect.bottom - gameClientRect.top;

            // Overlay位置 = 游戏客户区左上角（精确覆盖客户区，排除边框）
            MoveWindow(overlayHwnd,
                gameClientPos.x,  // 客户区左上角X（屏幕坐标）
                gameClientPos.y,  // 客户区左上角Y（屏幕坐标）
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

    // 设置透明色为黑色（绘制黑色会被忽略）
    SetLayeredWindowAttributes(overlayHwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(overlayHwnd, &margins);

    ShowWindow(overlayHwnd, SW_SHOW);

    // 初始化窗口位置
    targetHwnd = FindWindowA(NULL, TARGET_WINDOW_NAME);
    if (targetHwnd)
    {
        GetWindowRect(targetHwnd, &gameRect);
        MoveWindow(overlayHwnd, gameRect.left, gameRect.top,
            gameRect.right - gameRect.left,
            gameRect.bottom - gameRect.top, TRUE);
    }

    // 开启定时器触发重绘（30FPS）
    SetTimer(overlayHwnd, 1, 33, NULL);

    // 启动后台线程
    std::thread(UpdateOverlayPos).detach();
    std::thread(UpdateESPFromFile).detach();

    // 消息循环
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}