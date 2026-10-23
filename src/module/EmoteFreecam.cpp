#include "EmoteFreecam.hpp"
#include <cmath>
#include <algorithm>

namespace dynamicam {

// ── Static members ───────────────────────────────────────────────
float EmoteFreecam::s_yaw = 0.0f;
float EmoteFreecam::s_pitch = 15.0f;  // Slightly looking down at player's body

// ══════════════════════════════════════════════════════════════
// computeFreeCamPosition
// Returns an orbital camera position around basePos using s_yaw/s_pitch
// ══════════════════════════════════════════════════════════════

Vec3 EmoteFreecam::computeFreeCamPosition(const Vec3 &basePos, float) {
    float yawRad = s_yaw * static_cast<float>(M_PI / 180.0);
    float pitchRad = s_pitch * static_cast<float>(M_PI / 180.0);

    float cosPitch = std::cos(pitchRad);
    float sinYaw = std::sin(yawRad);
    float cosYaw = std::cos(yawRad);

    // Orbital offset (radius on XZ plane, height offset on Y)
    float radiusX = sinYaw * cosPitch * s_defaultRadius;
    float radiusZ = -cosYaw * cosPitch * s_defaultRadius;
    float radiusY = std::sin(pitchRad) * s_defaultRadius + s_defaultHeight;

    return Vec3{
        basePos.x + radiusX,
        basePos.y + radiusY,
        basePos.z + radiusZ
    };
}

// ══════════════════════════════════════════════════════════════
// getFreeCamYaw / getFreeCamPitch
// ══════════════════════════════════════════════════════════════

float EmoteFreecam::getFreeCamYaw() {
    return s_yaw;
}

float EmoteFreecam::getFreeCamPitch() {
    return s_pitch;
}

// ══════════════════════════════════════════════════════════════
// onTouchDrag
// Called each frame when the player is dragging their thumb during an emote.
// dxDelta = horizontal change in touch coordinates (pixels or joystick delta)
// dyDelta = vertical change in touch coordinates
// ══════════════════════════════════════════════════════════════

void EmoteFreecam::onTouchDrag(float dxDelta, float dyDelta) {
    float sensitivity = 0.4f;  // degrees per pixel/delta unit

    // Horizontal drag → changes yaw (rotate around Y axis)
    s_yaw -= dxDelta * sensitivity;

    // Vertical drag → changes pitch (tilt up/down)
    s_pitch += dyDelta * sensitivity;

    // Clamp pitch so we can't flip the camera upside-down
    if (s_pitch > 89.0f) s_pitch = 89.0f;
    if (s_pitch < -89.0f) s_pitch = -89.0f;
}

// ══════════════════════════════════════════════════════════════
// reset — restore default angles when not emoting
// ══════════════════════════════════════════════════════════════

void EmoteFreecam::reset() {
    s_yaw = 0.0f;
    s_pitch = 15.0f;
}

// ══════════════════════════════════════════════════════════════
// setAngles — override directly from external input source
// ══════════════════════════════════════════════════════════════

void EmoteFreecam::setAngles(float yawDeg, float pitchDeg) {
    s_yaw = yawDeg;
    s_pitch = std::clamp(pitchDeg, -89.0f, 89.0f);
}

} // namespace dynamicam
