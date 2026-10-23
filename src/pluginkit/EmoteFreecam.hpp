#pragma once

#include "CameraManager.hpp"

namespace dynamicam {

// EmoteFreecam — orbiting camera that works independently of emote animations.
// When a player triggers an emote, this module lets the camera freely rotate
// around the player without constraining it to follow the emote animation.

class EmoteFreecam {
public:
    // Compute orbital camera position around basePos
    static Vec3 computeFreeCamPosition(const Vec3 &basePos, float deltaTime);

    // Get current free-cam angles (degrees)
    static float getFreeCamYaw();
    static float getFreeCamPitch();

    // Called when player drags thumb/touches screen to look around during an emote
    static void onTouchDrag(float dxDelta, float dyDelta);

    // Reset to default view (used when starting/stopping emotes)
    static void reset();

    // Set manual angle overrides (useful for keyboard/mouse freecam)
    static void setAngles(float yawDeg, float pitchDeg);

private:
    // Current orbital angles in degrees
    static float s_yaw;
    static float s_pitch;

    // Orbital radius (distance from player center)
    static constexpr float s_defaultRadius = 5.0f;

    // Base height offset above player feet
    static constexpr float s_defaultHeight = 1.6f;
};

} // namespace dynamicam
