#include "Aimbot.h"
#include <windows.h>
#include <tlhelp32.h>
#include <cmath>
#include <iostream>

// ============================================================
//  OFFSETS ACTUALIZADOS (de tu cs2-dumper)
// ============================================================
#define OFFSET_dwLocalPlayerPawn 0x2341698      // client.dll + dwLocalPlayerPawn
#define OFFSET_dwEntityList 0x24E76A0           // client.dll + dwEntityList
#define OFFSET_m_iCrosshairId 0x10C             // CBasePlayerPawn -> m_iCrosshairId (si no funciona, busca en el dumper)
#define OFFSET_m_iHealth 0x63C                  // CBasePlayerPawn -> m_iHealth
#define OFFSET_m_iTeamNum 0x3EB                 // CBasePlayerPawn -> m_iTeamNum

// ============================================================
//  AIMBOT - VARIABLES
// ============================================================
float aimbotFOV = 150.0f;
float aimbotSmoothness = 5.0f;
bool aimbotEnabled = false;

// ============================================================
//  TRIGGERBOT - VARIABLES
// ============================================================
bool triggerbotEnabled = false;
int triggerbotDelay = 50;

// ============================================================
//  FUNCIONES DEL AIMBOT
// ============================================================
constexpr float PI = 3.14159265358979323846f;
constexpr float RAD2DEG = 180.0f / PI;

Vector3A CalculateAngle(const Vector3A& src, const Vector3A& dst) {
    Vector3A angles;
    Vector3A delta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
    float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);
    angles.x = std::atan2(-delta.z, hyp) * RAD2DEG;
    angles.y = std::atan2(delta.y, delta.x) * RAD2DEG;
    angles.z = 0.0f;
    if (angles.x > 89.0f) angles.x = 89.0f;
    if (angles.x < -89.0f) angles.x = -89.0f;
    while (angles.y > 180.0f) angles.y -= 360.0f;
    while (angles.y < -180.0f) angles.y += 360.0f;
    return angles;
}

void SmoothAngle(Vector3A& currentAngle, const Vector3A& targetAngle, float smooth) {
    Vector3A delta = { targetAngle.x - currentAngle.x, targetAngle.y - currentAngle.y, targetAngle.z - currentAngle.z };
    if (delta.y > 180) delta.y -= 360;
    if (delta.y < -180) delta.y += 360;
    currentAngle.x += delta.x / (smooth + 1.0f);
    currentAngle.y += delta.y / (smooth + 1.0f);
}

float GetFOVRadius() {
    return aimbotFOV;
}

// ============================================================
//  TRIGGERBOT - INICIALIZACIÓN (variables estáticas internas)
// ============================================================
static HANDLE g_hProcess = NULL;
static DWORD g_client = 0;
static bool g_initialized = false;
static bool g_firstErrorShown = false;  // Para no spamear el error

void InitializeTriggerbot() {
    if (g_initialized) return;

    HWND hwnd = FindWindowA(NULL, "Counter-Strike 2");
    if (!hwnd) {
        printf("[Triggerbot] ERROR: No se encontró la ventana de CS2\n");
        return;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        printf("[Triggerbot] ERROR: No se pudo obtener el PID de CS2\n");
        return;
    }

    g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!g_hProcess) {
        printf("[Triggerbot] ERROR: No se pudo abrir el proceso (error %d)\n", GetLastError());
        return;
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        printf("[Triggerbot] ERROR: No se pudo crear snapshot de módulos\n");
        CloseHandle(g_hProcess);
        g_hProcess = NULL;
        return;
    }

    MODULEENTRY32W me32 = { sizeof(MODULEENTRY32W) };
    if (Module32FirstW(snap, &me32)) {
        do {
            if (wcscmp(me32.szModule, L"client.dll") == 0) {
                g_client = (DWORD)me32.modBaseAddr;
                break;
            }
        } while (Module32NextW(snap, &me32));
    }
    CloseHandle(snap);

    if (g_client == 0) {
        printf("[Triggerbot] ERROR: No se encontró client.dll en el proceso\n");
        CloseHandle(g_hProcess);
        g_hProcess = NULL;
        return;
    }

    g_initialized = true;
    g_firstErrorShown = false;  // Reiniciar el flag de error
    printf("[Triggerbot] Inicializado correctamente: handle=0x%p, client=0x%X\n", g_hProcess, g_client);
}

// ============================================================
//  TRIGGERBOT - FUNCIÓN PRINCIPAL (CORREGIDA)
// ============================================================
void Triggerbot() {
    if (!triggerbotEnabled) return;

    if (!g_initialized) {
        InitializeTriggerbot();
        if (!g_initialized) return;
        printf("[Triggerbot] Inicializado: handle=0x%p, client=0x%X\n", g_hProcess, g_client);
    }

    DWORD localPlayer = 0;
    SIZE_T bytesRead = 0;

    ReadProcessMemory(g_hProcess, (LPCVOID)(g_client + OFFSET_dwLocalPlayerPawn), &localPlayer, sizeof(DWORD), &bytesRead);
    if (localPlayer == 0) {
        // Mostrar error solo una vez para no saturar la consola
        if (!g_firstErrorShown) {
            printf("[Triggerbot] ERROR: No se pudo leer localPlayer (error %d). Verifica offsets.\n", GetLastError());
            g_firstErrorShown = true;
        }
        return;
    }

    int localTeam = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(localPlayer + OFFSET_m_iTeamNum), &localTeam, sizeof(int), &bytesRead);
    if (localTeam == -1) return;

    int crosshairId = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(localPlayer + OFFSET_m_iCrosshairId), &crosshairId, sizeof(int), &bytesRead);
    if (crosshairId <= 0 || crosshairId > 64) return;

    DWORD entityList = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(g_client + OFFSET_dwEntityList), &entityList, sizeof(DWORD), &bytesRead);
    if (entityList == 0) return;

    DWORD entity = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(entityList + (crosshairId - 1) * 0x10), &entity, sizeof(DWORD), &bytesRead);
    if (entity == 0) return;

    int health = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(entity + OFFSET_m_iHealth), &health, sizeof(int), &bytesRead);
    if (health <= 0) return;

    int team = 0;
    ReadProcessMemory(g_hProcess, (LPCVOID)(entity + OFFSET_m_iTeamNum), &team, sizeof(int), &bytesRead);
    if (team == localTeam) return;

    // Disparar
    printf("[Triggerbot] ¡DISPARANDO! (crosshairId=%d, health=%d)\n", crosshairId, health);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(triggerbotDelay);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}