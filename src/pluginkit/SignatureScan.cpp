#include "SignatureScan.hpp"
#include <pl/Hook.hpp>

#include <android/log.h>
#include <dlfcn.h>
#include <cstring>

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

static void *g_mcpeHandle = nullptr;

void *getMCPEHandle() { return g_mcpeHandle; }

// ================================================================
// ARM64 memory pattern scanner
// Scans a memory region for instructions matching the given hex pattern
// and ASCII mask (? = wildcard, x = match exact byte)
// ================================================================

static uint8_t *scanFromRegion(uint8_t *start, uint8_t *end,
                                const char *pattern, const char *mask) {
    size_t patLen = strlen(mask);
    if (patLen == 0 || static_cast<size_t>(end - start) < patLen) return nullptr;

    for (uint8_t *p = start; p <= end - static_cast<int>(patLen); ++p) {
        bool match = true;
        for (size_t i = 0; i < patLen; ++i) {
            if (mask[i] != '?' && pattern[i] != static_cast<char>(p[i])) {
                match = false;
                break;
            }
        }
        if (match) return p;
    }
    return nullptr;
}

// Scan entire library range (base + up to 32MB)
static uint8_t *scanEntireLibrary(void *libBase, uint8_t **outEnd) {
    Dl_info info;
    if (!dladdr(libBase, &info) || !info.dli_fbase) return nullptr;

    uint8_t *start = reinterpret_cast<uint8_t *>(info.dli_fbase);
    constexpr int MAX_SCAN = 0x2000000;  // 32 MB covers most MC .so files
    *outEnd = start + MAX_SCAN;
    return start;
}

// ================================================================
// Pattern database — one entry per function we need to find
// FORMAT: { description, HEX bytes, MASK chars, target pointer }
//
// IMPORTANT: These patterns are extracted from your specific MC version!
// To get patterns:
//   1. Extract libminecraftpe.so from your MCPE APK
//   2. Open in Ghidra or run objdump -d on CLI
//   3. Find the function by mangled name
//   4. Copy first 8-16 bytes as hex → create mask (x/fixed, ?/don't care)
// ================================================================

struct PatternEntry {
    const char *desc;
    const char *bytes;
    const char *mask;
    void **targetPtr;
};

static PatternEntry s_patterns[] = {
    // ── Camera::getPosition(float*,float*,float*) ──────────────
    // Typical prologue: alloc stack, save registers
    {
        "Camera::getPosition",
        "\xF2\x3F\xFF\xD1\x10\x00\x00\x10",  // sub sp,sp,#... ; mov v0.4s,#0
        "xxxxxxxx",
        &s_scanned_GetCameraPos
    },

    // ── Camera::setPosition(float,float,float) ─────────────────
    {
        "Camera::setPosition",
        "\xFE\x2B\x00\xf1\xA9\x01\x00\x53",  // sub sp,#... ; stp w0,w1,[sp,...]
        "xxxxxxxx",
        &s_scanned_SetCameraPos
    },

    // ── Camera::getRotation(float*,float*) ─────────────────────
    {
        "Camera::getRotation",
        "\xF6\x3B\xFF\xD1\x10\x00\x00\x10",  // sub sp,#... ; mov v0.4s,#0
        "xxxxxxxx",
        &s_scanned_GetCameraRot
    },

    // ── PlayerEntity::getPosition() ────────────────────────────
    {
        "PlayerEntity::getPosition",
        "\xFE\x43\x00\xd4\xF6\x3B\xFF\xD1",  // br x16? ; sub sp,#...
        "xxxxxxxx",
        &s_scanned_GetPlayerPos
    },

    // ── Player head visibility setter (bone/mesh toggle) ───────
    // This is usually inside Player::render or a dedicated bone-controller
    // Placeholder — you'll need to find the exact call that toggles mesh visibility
    {
        "Player head vis setter",
        "\x00\x10\x40\xF9\xC0\x03\x00\x14",  // str x0,[sp,...] ; b ...
        "xxxxxxxx",
        &s_scanned_SetHeadVis
    },
};

// ================================================================
// Main entry point — called from RenderHooks::initHooks() when dlsym fails
// ================================================================

void initSignatureScanning() {
    LOGI("Starting ARM64 signature scanning...");

    if (!g_mcpeHandle) {
        g_mcpeHandle = dlopen("libminecraftpe.so", RTLD_LAZY);
    }

    if (!g_mcpeHandle) {
        LOGE("Failed to dlopen libminecraftpe.so");
        return;
    }

    uint8_t *libStart = nullptr, *libEnd = nullptr;
    libStart = scanEntireLibrary(g_mcpeHandle, &libEnd);
    if (!libStart) {
        LOGE("Could not determine library bounds");
        return;
    }

    int matchedCount = 0;
    for (const auto &entry : s_patterns) {
        uint8_t *found = scanFromRegion(libStart, libEnd, entry.bytes, entry.mask);
        if (found) {
            *entry.targetPtr = reinterpret_cast<void *>(found);
            LOGI("  ✓ %s at %p", entry.desc, found);
            ++matchedCount;
        } else {
            LOGI("  ✗ %s — pattern not found (may be wrong MC version)", entry.desc);
        }
    }

    LOGI("Scan complete: %d/%d patterns matched", matchedCount,
         static_cast<int>(sizeof(s_patterns) / sizeof(s_patterns[0])));
}

} // namespace dynamicam
