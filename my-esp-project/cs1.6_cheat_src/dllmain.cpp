#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdbool.h>
#include "pch.h"
#include <stdio.h>
#include <math.h>
#include <vector>

// ==== Vector and Matrix Structures ====
typedef struct {
    float x, y, z;
} Vector3;

typedef struct {
    float x, y;
} Vector2;

// Row-major 4x4 matrix (stored by rows: each row has 4 elements, total 4 rows)
typedef struct {
    float m[16]; // Row-major: m[row * 4 + column]
} Matrix4x4;

// Window rectangle structure
typedef struct {
    int left, top, right, bottom;
} Rect;

// ==== Global State ====
bool lockHp = true;
bool lockAmmo = true;

// ==== Corrected WorldToScreen Function ====
bool WorldToScreen(Vector3 worldPos, Matrix4x4 viewMatrix, RECT windowRect, Vector3* screenPos) {
    float viewWidth = (windowRect.right - windowRect.left) / 2.0f;
    float viewHeight = (windowRect.bottom - windowRect.top) / 2.0f;

    float cameraZ = viewMatrix.m[2] * worldPos.x + viewMatrix.m[6] * worldPos.y + viewMatrix.m[10] * worldPos.z + viewMatrix.m[14];
    if (cameraZ < 0.01f) return false;

    float scale = 1.0f / cameraZ;

    float screenX = viewWidth + (viewMatrix.m[0] * worldPos.x + viewMatrix.m[4] * worldPos.y + viewMatrix.m[8] * worldPos.z + viewMatrix.m[12]) * scale * viewWidth;
    float screenY = viewHeight - (viewMatrix.m[1] * worldPos.x + viewMatrix.m[5] * worldPos.y + viewMatrix.m[9] * worldPos.z + viewMatrix.m[13]) * scale * viewHeight;

    screenPos->x = windowRect.left + screenX;
    screenPos->y = windowRect.top + screenY;
    screenPos->z = cameraZ;

    return true;
}

// ==== Write ESP Data to File (Visible Enemies Only) ====
void WriteESPDataToFile(Vector3* foots, Vector3* heads, float* distances, int count) {
    FILE* file = NULL;
    if (fopen_s(&file, "C:\\Users\\Public\\esp_data.json", "w") != 0 || !file) return;

    fprintf(file, "[\n");
    for (int i = 0; i < count; i++) {
        fprintf(file,
            "  {\"x\": %.1f, \"y\": %.1f, \"distance\": %.1f, \"head\": {\"x\": %.1f, \"y\": %.1f}}%s\n",
            foots[i].x, foots[i].y, distances[i], heads[i].x, heads[i].y,
            (i < count - 1) ? "," : "");
    }
    fprintf(file, "]\n");
    fclose(file);
}

void ESPScan() {
    // Find the game window
    HWND hwnd = FindWindowA(NULL, "Counter-Strike");
    if (!hwnd) {
        OutputDebugStringA("Game window not found\n");
        return;
    }

    // Get window client area screen coordinates
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    POINT topLeft = { clientRect.left, clientRect.top };
    POINT bottomRight = { clientRect.right, clientRect.bottom };
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);
    RECT windowRect = {
        topLeft.x,
        topLeft.y,
        bottomRight.x,
        bottomRight.y
    };

    // Get module base address
    DWORD moduleBase = (DWORD)GetModuleHandleA("cstrike.exe");
    if (!moduleBase) {
        OutputDebugStringA("Failed to get module base\n");
        return;
    }

    // Get base address
    DWORD base = *(DWORD*)(moduleBase + 0x1117C64);
    if (!base) {
        OutputDebugStringA("Failed to get base address\n");
        return;
    }

    // Containers for multiple entities
    const int MAX_ENTITIES = 32; // Maximum number of entities
    std::vector<Vector3> foots;
    std::vector<Vector3> heads;
    std::vector<float> distances;

    // Get local player coordinates (for distance calculation)
    DWORD localPlayerBase = *(DWORD*)(base + 0x4B9C);
    int myTeam = -1;
    float localX = 0.0f, localY = 0.0f, localZ = 0.0f;
    if (localPlayerBase) {
        localX = *(float*)(localPlayerBase + 0x88);
        localY = *(float*)(localPlayerBase + 0x8C);
        localZ = *(float*)(localPlayerBase + 0x90);
    }

    // Iterate over all entities (additive offset)
    DWORD currentOffset = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        // Calculate current entity base address
        DWORD entityBase = *(DWORD*)(base + 0x4B9C + currentOffset);
        if (!entityBase || IsBadReadPtr((void*)entityBase, 0x200)) {
            currentOffset += 0x5110;
            continue;
        }

        /* Optional: Skip local player (uncomment to include self)
        if (localPlayerBase && entityBase == localPlayerBase) {
            currentOffset += 0x5110;
            continue;
        }*/

        // ==== Team Check ====
        DWORD teamPtr = *(DWORD*)(entityBase + 0x7C);
        if (!teamPtr) {
            currentOffset += 0x5110;
            continue;
        }
        int entityTeam = *(int*)(teamPtr + 0x1C8);
        if (entityTeam == 0 || entityTeam == myTeam) {
            currentOffset += 0x5110;
            continue;
        }

        // Get entity world coordinates
        float x = *(float*)(entityBase + 0x88);
        float y = *(float*)(entityBase + 0x8C);
        float z = *(float*)(entityBase + 0x90);

        // Get view matrix
        float* matrixRaw = (float*)0x02C20100;
        if (IsBadReadPtr(matrixRaw, sizeof(float) * 16)) {
            currentOffset += 0x5110;
            continue;
        }
        Matrix4x4 viewMatrix;
        memcpy(viewMatrix.m, matrixRaw, sizeof(float) * 16);

        // Calculate foot and head screen positions
        Vector3 worldFoot = { x, y, z - 40.0f };
        Vector3 worldHead = { x, y, z + 30.0f };
        Vector3 screenFoot, screenHead;

        bool footOk = WorldToScreen(worldFoot, viewMatrix, windowRect, &screenFoot);
        bool headOk = WorldToScreen(worldHead, viewMatrix, windowRect, &screenHead);
        if (!footOk || !headOk) {
            currentOffset += 0x5110;
            continue;
        }

        // Calculate distance from local player (if valid)
        float distance = 0.0f;
        if (localPlayerBase) {
            distance = sqrtf(
                (x - localX) * (x - localX) +
                (y - localY) * (y - localY) +
                (z - localZ) * (z - localZ)
            );
        }

        // Save to containers
        foots.push_back(screenFoot);
        heads.push_back(screenHead);
        distances.push_back(distance);

        // Increment offset to next entity
        currentOffset += 0x5110;
    }

    // Write ESP data to file
    WriteESPDataToFile(
        foots.data(),
        heads.data(),
        distances.data(),
        foots.size()
    );
}

// ==== Health Lock Function ====
void LockHealth() {
    static int lastHp = 100;
    static bool firstTime = true;

    DWORD moduleBase = (DWORD)GetModuleHandleA("cstrike.exe");
    if (!moduleBase) return;

    DWORD base = *(DWORD*)(moduleBase + 0x1117C64);
    if (!base) return;

    DWORD ptr1 = *(DWORD*)(base + 0x4B9C);
    if (!ptr1) return;

    DWORD ptr2 = *(DWORD*)(ptr1 + 0x7C);
    if (!ptr2) return;

    DWORD ptr3 = *(DWORD*)(ptr2 + 0x04);
    if (!ptr3) return;

    int* hpAddr = (int*)(ptr3 + 0x160);
    if (IsBadReadPtr(hpAddr, sizeof(int))) return;

    int currentHp = *hpAddr;
    if (firstTime) {
        lastHp = currentHp;
        firstTime = false;
        return;
    }
    if (currentHp < lastHp && currentHp > 0) {
        *hpAddr = lastHp;
    }
    else {
        lastHp = currentHp;
    }
}

// ==== Ammo Lock Function ====
void ammoLock() {
    static bool firstTime = true;
    static int maxAmmo = 0;

    DWORD moduleBase = (DWORD)GetModuleHandleA("cstrike.exe");
    if (!moduleBase)return;

    DWORD base = *(DWORD*)(moduleBase + 0x1117C64);
    if (!base) return;

    DWORD ptr1 = *(DWORD*)(base + 0x4B9C);
    if (!ptr1) return;

    DWORD ptr2 = *(DWORD*)(ptr1 + 0x7C);
    if (!ptr2) return;

    DWORD ptr3 = *(DWORD*)(ptr2 + 0x5F0);
    if (!ptr3)return;

    int* ammoAdr = (int*)(ptr3 + 0xCC);
    if (IsBadReadPtr(ammoAdr, sizeof(int))) return;

    int currentAmmo = *ammoAdr;
    if (currentAmmo > 0 && firstTime) {
        maxAmmo = currentAmmo;
        firstTime = false;
    }

    if (maxAmmo > 0 && currentAmmo < maxAmmo) {
        *ammoAdr = maxAmmo;
    }
}

// ==== Main Thread ====
DWORD WINAPI MainThread(LPVOID lpParam) {
    OutputDebugStringA("Main thread started\n");
    MessageBoxA(NULL, "DLL injected successfully", "Info", MB_OK);
    while (1) {
        if (lockHp) LockHealth();
        if (lockAmmo) ammoLock();
        ESPScan();
        Sleep(30);
    }
    return 0;
}

// ==== DLL Entry Point ====
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    return TRUE;
}
