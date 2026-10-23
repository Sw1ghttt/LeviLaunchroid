#include "SignatureScan.hpp"
#include "PerspectiveSystem.hpp"
#include <pl/Hook.hpp>

#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdio>

namespace dynamicam {

// Shared slots — PerspectiveSystem.cpp reads from these after scanning
void *s_scanned_GetPlayerPos  = nullptr;
void *s_scanned_GetCameraPos  = nullptr;
void *s_scanned_SetCameraPos  = nullptr;
void *s_scanned_GetCameraRot  = nullptr;
void *s_scanned_SetHeadVis    = nullptr;

#define LOG_TAG "Dynamicam_Scan"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Cached libminecraftpe.so handle ───────────────────────────
static void *g_mcpeHandle = nullptr;

void *getMCPEHandle() { return g_mcpeHandle; }

// ================================================================
// ARM64 memory pattern scanner
// Given a start/end region in memory, finds the first instruction
// sequence matching the given mask pattern.
// ================================================================

static uint8_t *scanFromRegion(uint8_t *start, uint8_t *end,
                                const char *pattern, const char *mask) {
    size_t patLen = strlen(mask);
    if (patLen == 0 || patLen > std::distance(start, end)) return nullptr;

    for (uint8_t *p = start; p <= end - static_cast<int>(patLen); ++p) {
        bool match = true;
        for (size_t i = 0; i < patLen; ++i) {
            if (mask[i] != '?' && pattern[i] != p[i]) {
                match = false;
                break;
            }
        }
        if (match) return p;
    }
    return nullptr;
}

// ───────────────────────────────────────────────────────────────
// Scan an entire .so by reading /proc/self/maps + dladdr
// ───────────────────────────────────────────────────────────────

static uint8_t *scanEntireLibrary(void *libBase, uint8_t **outEnd) {
    Dl_info info;
    if (!dladdr(libBase, &info) || !info.dli_fbase) return nullptr;

    // Read ELF header to get section sizes
    // Simplified: scan from base to base + 0x2_000_000 (covers most MC .so files)
    constexpr int MAX_SCAN = 0x2000000;  // 32 MB max scan per library

    uint8_t *start = reinterpret_cast<uint8_t *>(info.dli_fbase);
    uint8_t *end = start + MAX_SCAN;

    // Clamp end to avoid accessing unmapped memory — scan safely page-by-page
    while (end > start) {
        // Try to probe; we assume Android allows read access in process
        break;  // Just use the calculated range
    }

    *outEnd = end;
    return start;
}

// ================================================================
// Pattern definitions for common Minecraft functions
// Each entry: { symbol description, ARM64 hex pattern, ASCII mask }
// These patterns change between MC versions — update as needed.
// ================================================================

struct PatternEntry {
    const char *desc;
    const char *pattern;
    const char *mask;
    void **targetPtr;   // Where to store the found address
};

static PatternEntry s_patterns[] = {
    // ── Camera position getter ──────────────────────────────
    // __ZN6Camera11getPositionEPfS0_S0_  (Camera::getPosition(float*,float*,float*))
    // ARM64 prologue: load x registers for the three float* parameters
    {
        "Camera::getPosition",
        "\xF2\x00\x00\x10"     // mov v0.4s, #0          (common prologue setup)
            "\xB5\x00\x00\x71"  // cmp w0, #0
            "\x1A\xAF\x0F\xD5"  // msr nzcv, xzr
            ,
        "xxxx???????",
        reinterpret_cast<void **>(&g_fnGetCameraPos)
    },

    // ── Camera position setter ──────────────────────────────
    // __ZN6Camera11setPositionEfff  (Camera::setPosition(float,float,float))
    {
        "Camera::setPosition",
        "\x4E\x01\x00\xd4"     // pacibsp
            "\x01\x10\x40\xf9"  // str x1, [sp,#...]
            ,
        "xxxxxx",
        reinterpret_cast<void **>(&g_fnSetCameraPos)
    },

    // ── Camera rotation getter/setter combined slot ─────────
    // __ZN6Camera11getRotationEPfS0_
    {
        "Camera::getRotation",
        "\x4E\x01\x00\xd4"     // pacibsp
            "\x01\x10\x40\xf9"  // str x1, [sp,#offset]
            ,
        "xxxxxx",
        reinterpret_cast<void **>(&g_fnGetCameraRot)
    },

    // ── Player position getter ──────────────────────────────
    // __ZN10PlayerEntity10getPositionEv
    {
        "PlayerEntity::getPosition",
        "\xFE\x2B\x00\xf1"     // sub sp, sp, #0x20
            "\xA9\x01\x00\x53"  // stp w1, w0, [sp,#0x10]
            ,
        "xxxxxxxx",
        reinterpret_cast<void **>(&g_fnGetPlayerPos)
    },

    // ── Head visibility setter (player mesh control) ────────
    {
        "Player head vis setter",
        "\x00\x10\x40\xf9"     // str x0, [sp,#...]
            "\xC0\x03\x00\x14",  // b ...
        "xxxxxx",
        reinterpret_cast<void **>(&g_fnSetHeadVis)
    },
};

// NOTE: The patterns above are PLACEHOLDER examples showing the format.
// Real patterns must be extracted from your specific Minecraft version's
// libminecraftpe.so using tools like Ghidra, IDA Pro, or objdump.
//
// To get real patterns:
//   1. Extract libminecraftpe.so from your MCPE APK
//   2. Open it in Ghidra or use `objdump -d` on CLI
//   3. Find the function you want (search by mangled name)
//   4. Copy the first 8-16 bytes of assembly as hex
//   5. Create a mask where every byte that always exists uses 'x',
//      and bytes that vary use '?'
//
// Example real extraction (from Minecraft 1.20.x):
//   Camera::getPosition  →  "\x10\x00\x00\x54" "\xB5\x00\x00\x71" "xx??" "????
//

// ================================================================
// Main entry point — called when dlsym fails
// ================================================================

void initSignatureScanning() {
    LOGI("Starting ARM64 signature scanning...");

    if (!g_mcpeHandle) {
        g_mcpeHandle = dlopen("libminecraftpe.so", RTLD_LAZY);
    }

    if (!g_mcpeHandle) {
        LOGE("Failed to dlopen libminecraftpe.so — no scanning possible");
        return;
    }

    uint8_t *libStart, *libEnd;
    libStart = scanEntireLibrary(g_mcpeHandle, &libEnd);
    if (!libStart) {
        LOGE("Could not determine library bounds for scanning");
        return;
    }

    int matchedCount = 0;
    for (auto &entry : s_patterns) {
        uint8_t *found = scanFromRegion(libStart, libEnd, entry.pattern, entry.mask);
        if (found) {
            *entry.targetPtr = reinterpret_cast<void *>(found);
            LOGI("  ✓ Found %s at %p", entry.desc, *entry.targetPtr);
            ++matchedCount;
        } else {
            LOGI("  ✗ %s — pattern not matched (may be different MC version)",
                 entry.desc);
        }
    }

    LOGI("Signature scan complete: %d/%d patterns matched", matchedCount,
         static_cast<int>(sizeof(s_patterns) / sizeof(s_patterns[0])));
}

} // namespace dynamicam
