#include "PerspectiveSystem.hpp"
#include "Dynamicam.hpp"
#include "SignatureScan.hpp"
#include <cmath>
#include <cstring>

namespace dynamicam {

// ══════════════════════════════════════════════════════════════
// Per-frame entry point — called from hook_EGLSwapBuffers every frame
// ══════════════════════════════════════════════════════════════

void preRenderFrame(int screenWidth, int screenHeight) {
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    TransitionState &trans = getTransitionState();

    // Update smooth transition progress when switching perspectives
    if (trans.active && trans.progress < 1.0f) {
        float durationSec = 2.0f / config.smoothingSpeed;
        float frameTime = 1.0f / 60.0f;
        trans.progress += frameTime / durationSec;
        if (trans.progress >= 1.0f) {
            trans.progress = 1.0f;
            trans.active = false;
            dyn.setPerspective(trans.to);
        }
    }
}

// ══════════════════════════════════════════════════════════════
// Game-state accessors — resolve via dlsym first, then scanning
// Function pointers populated by initSignatureScanning() in RenderHooks
// ══════════════════════════════════════════════════════════════

// These match the shared slots in SignatureScan.cpp — we use them directly
extern void *s_scanned_GetPlayerPos;
extern void *s_scanned_GetCameraPos;
extern void *s_scanned_SetCameraPos;
extern void *s_scanned_GetCameraRot;
extern void *s_scanned_SetHeadVis;

// ---- getPlayerEyePosition: read player position + add eye height offset ----
Vec3 getPlayerEyePosition() {
    using Fn = Vec3 (*)();
    auto fn = reinterpret_cast<Fn>(s_scanned_GetPlayerPos);
    if (!fn) return Vec3{0.0f, 0.0f, 0.0f};

    Vec3 pos = fn();
    pos.y += 1.6f;   // eye level is ~1.6 units above feet
    return pos;
}

// ---- getPlayerLookDirection: convert camera yaw/pitch to unit direction vector ----
Vec3 getPlayerLookDirection() {
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(s_scanned_GetCameraRot);
    if (!fn) return Vec3{0.0f, 0.0f, -1.0f};

    float pitch = 0.0f, yaw = 0.0f;
    fn(&pitch, &yaw);

    float pitchRad = pitch * static_cast<float>(M_PI / 180.0);
    float yawRad = yaw * static_cast<float>(M_PI / 180.0);
    float cosPitch = std::cos(pitchRad);

    return Vec3{
        -std::sin(yawRad) * cosPitch,   // x
         std::sin(pitchRad),             // y (up/down)
        -std::cos(yawRad) * cosPitch     // z
    };
}

// ---- getCurrentCameraPosition: read current camera world coordinates ----
Vec3 getCurrentCameraPosition() {
    using Fn = void (*)(float *, float *, float *);
    auto fn = reinterpret_cast<Fn>(s_scanned_GetCameraPos);
    if (!fn) return Vec3{0.0f, 0.0f, 0.0f};

    float x, y, z;
    fn(&x, &y, &z);
    return Vec3{x, y, z};
}

// ---- setCameraPosition: write new camera world coordinates ----
void setCameraPosition(const Vec3 &pos) {
    using Fn = void (*)(float, float, float);
    auto fn = reinterpret_cast<Fn>(s_scanned_SetCameraPos);
    if (!fn) return;

    fn(pos.x, pos.y, pos.z);
}

// ---- setCameraRotation: write pitch and yaw in degrees ----
void setCameraRotation(float pitchDeg, float yawDeg) {
    using Fn = void (*)(float, float);
    auto fn = reinterpret_cast<Fn>(s_scanned_GetCameraRot);
    if (!fn) return;

    fn(pitchDeg, yawDeg);
}

// ---- getCameraYaw: read current yaw angle in degrees ----
float getCameraYaw() {
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(s_scanned_GetCameraRot);
    if (!fn) return 0.0f;

    float pitch, yaw;
    fn(&pitch, &yaw);
    return yaw;
}

// ---- getCameraPitch: read current pitch angle in degrees ----
float getCameraPitch() {
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(s_scanned_GetCameraRot);
    if (!fn) return 0.0f;

    float pitch, yaw;
    fn(&pitch, &yaw);
    return pitch;
}

// ---- getRaycastTarget: returns (0,0,0) if nothing targeted ----
// TODO: Hook into Minecraft hit-detection to return block center position
Vec3 getRaycastTarget() {
    return Vec3{0.0f, 0.0f, 0.0f};
}

// ---- setPlayerHeadVisible: toggle head bone mesh visibility ----
void setPlayerHeadVisible(bool visible) {
    using Fn = void (*)(bool);
    auto fn = reinterpret_cast<Fn>(s_scanned_SetHeadVis);
    if (!fn) return;

    fn(visible);
}

} // namespace dynamicam
