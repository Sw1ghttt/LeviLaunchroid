#include "PerspectiveSystem.hpp"
#include "Dynamicam.hpp"
#include <cmath>
#include <cstring>

namespace dynamicam {

// ── Single instance of transition state for smooth perspective switching ─
TransitionState &getTransitionState() {
    static TransitionState s_state;
    return s_state;
}

// ══════════════════════════════════════════════════════════════
// Per-frame entry point
// ══════════════════════════════════════════════════════════════

void preRenderFrame(int screenWidth, int screenHeight) {
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    TransitionState &trans = getTransitionState();

    // Update smooth transition progress
    if (trans.active && trans.progress < 1.0f) {
        float durationSec = 2.0f / config.smoothingSpeed;  // ~1-200s depending on slider
        float frameTime = 1.0f / 60.0f;  // assume ~60fps
        trans.progress += frameTime / durationSec;
        if (trans.progress >= 1.0f) {
            trans.progress = 1.0f;
            trans.active = false;
            dyn.setPerspective(trans.to);
        }
    }
}

// ══════════════════════════════════════════════════════════════
// Game-state accessors — resolved via dlsym + signature scanning
// ══════════════════════════════════════════════════════════════

static void *g_mcpeLib = nullptr;

// Cached function pointers (populated in initSignatureScanning)
static void *g_fnGetPlayerPos = nullptr;
static void *g_fnGetCameraPos = nullptr;
static void *g_fnSetCameraPos = nullptr;
static void *g_fnGetCameraRot = nullptr;
static void *g_fnSetCameraRot = nullptr;
static void *g_fnSetHeadVis   = nullptr;

// ---- getPlayerEyePosition ----
Vec3 getPlayerEyePosition() {
    if (!g_mcpeLib || !g_fnGetPlayerPos) return Vec3{0.0f, 0.0f, 0.0f};

    // Signature: __ZN10PlayerEntity10getPositionEv  -> Vec3 PlayerEntity::getPosition()
    using Fn = Vec3 (*)();
    auto fn = reinterpret_cast<Fn>(g_fnGetPlayerPos);
    Vec3 pos = fn();

    // Add eye height offset (~1.6 units above feet)
    pos.y += 1.6f;
    return pos;
}

// ---- getPlayerLookDirection ----
Vec3 getPlayerLookDirection() {
    if (!g_mcpeLib || !g_fnGetCameraRot) return Vec3{0.0f, 0.0f, -1.0f};

    float pitch = 0.0f, yaw = 0.0f;
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(g_fnGetCameraRot);
    fn(&pitch, &yaw);

    // Convert spherical coordinates → unit direction vector
    float pitchRad = pitch * M_PI / 180.0f;
    float yawRad = yaw * M_PI / 180.0f;
    float cosPitch = std::cos(pitchRad);

    return Vec3{
        -std::sin(yawRad) * cosPitch,   // x (forward-back on X axis)
        std::sin(pitchRad),              // y (up-down)
        -std::cos(yawRad) * cosPitch     // z (forward-back on Z axis)
    };
}

// ---- getCurrentCameraPosition ----
Vec3 getCurrentCameraPosition() {
    if (!g_mcpeLib || !g_fnGetCameraPos) return Vec3{0.0f, 0.0f, 0.0f};

    float x = 0.0f, y = 0.0f, z = 0.0f;
    using Fn = void (*)(float *, float *, float *);
    auto fn = reinterpret_cast<Fn>(g_fnGetCameraPos);
    fn(&x, &y, &z);
    return Vec3{x, y, z};
}

// ---- setCameraPosition ----
void setCameraPosition(const Vec3 &pos) {
    if (!g_mcpeLib || !g_fnSetCameraPos) return;

    using Fn = void (*)(float, float, float);
    auto fn = reinterpret_cast<Fn>(g_fnSetCameraPos);
    fn(pos.x, pos.y, pos.z);
}

// ---- setCameraRotation ----
void setCameraRotation(float pitchDeg, float yawDeg) {
    if (!g_mcpeLib || !g_fnSetCameraRot) return;

    using Fn = void (*)(float, float);
    auto fn = reinterpret_cast<Fn>(g_fnSetCameraRot);
    fn(pitchDeg, yawDeg);
}

// ---- getCameraYaw ----
float getCameraYaw() {
    if (!g_mcpeLib || !g_fnGetCameraRot) return 0.0f;

    float pitch = 0.0f, yaw = 0.0f;
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(g_fnGetCameraRot);
    fn(&pitch, &yaw);
    return yaw;
}

// ---- getCameraPitch ----
float getCameraPitch() {
    if (!g_mcpeLib || !g_fnGetCameraRot) return 0.0f;

    float pitch = 0.0f, yaw = 0.0f;
    using Fn = void (*)(float *, float *);
    auto fn = reinterpret_cast<Fn>(g_fnGetCameraRot);
    fn(&pitch, &yaw);
    return pitch;
}

// ---- getRaycastTarget ----
Vec3 getRaycastTarget() {
    // TODO: Implement by reading Minecraft hit-detection result
    // This requires hooking into the raycast system in libminecraftpe.so
    // For now returns zero — aim tracking is disabled until implemented
    return Vec3{0.0f, 0.0f, 0.0f};
}

// ---- setPlayerHeadVisible ----
void setPlayerHeadVisible(bool visible) {
    if (!g_mcpeLib || !g_fnSetHeadVis) return;

    using Fn = void (*)(bool);
    auto fn = reinterpret_cast<Fn>(g_fnSetHeadVis);
    fn(visible);
}

} // namespace dynamicam
