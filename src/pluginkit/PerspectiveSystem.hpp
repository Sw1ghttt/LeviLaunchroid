#pragma once

// PerspectiveSystem — per-frame camera transition state management
// and external game-state queries for RenderHooks.

#include "CameraManager.hpp"
#include <cstdint>

namespace dynamicam {

// ── Per-frame callback invoked from hook_EGLSwapBuffers ────────
void preRenderFrame(int screenWidth, int screenHeight);

// ── Camera / player accessor functions ─────────────────────────
// These resolve at runtime via dlsym into libminecraftpe.so or
// signature-scanned ARM64 patterns.

// Get player eye/world position
Vec3 getPlayerEyePosition();

// Get normalized look direction (from camera yaw/pitch)
Vec3 getPlayerLookDirection();

// Get current camera position in world space
Vec3 getCurrentCameraPosition();

// Set camera position in world space
void setCameraPosition(const Vec3 &pos);

// Set camera pitch (up/down) and yaw (left/right) in degrees
void setCameraRotation(float pitchDeg, float yawDeg);

// Get individual camera angles in degrees
float getCameraYaw();
float getCameraPitch();

// Get raycast target block position (0,0,0 = nothing targeted)
Vec3 getRaycastTarget();

// Hide/show the player's head bone/mesh
void setPlayerHeadVisible(bool visible);

// ── Internal smooth-transition state ───────────────────────────
struct TransitionState {
    bool active = false;
    float progress = 1.0f;      // 0.0 → 1.0 over transition duration
    PerspectiveMode from = PerspectiveMode::FIRST_PERSON;
    PerspectiveMode to = PerspectiveMode::FIRST_PERSON;
};

TransitionState &getTransitionState();

} // namespace dynamicam
