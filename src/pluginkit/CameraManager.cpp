#include "CameraManager.hpp"
#include <cmath>

namespace dynamicam {

// First-person: camera sits at player eye position
// Offset slightly so you can see your body in front of you
Vec3 CameraManager::computeFirstPersonPosition(const Vec3 &playerPos) {
    // Eye height is about 1.6 units above feet
    return Vec3{
        playerPos.x,
        playerPos.y + 1.6f,   // Eye level
        playerPos.z
    };
}

// Second person back: behind player + slight sideways angle for cinematic feel
Vec3 CameraManager::computeSecondPersonBackPosition(
    const Vec3 &playerPos, const Vec3 &lookDir,
    float angleDeg, float distance) {
    
    float angleRad = angleDeg * static_cast<float>(M_PI / 180.0);
    
    // Look direction (forward from player)
    Vec3 forward = lookDir.normalized();
    
    // Right vector (perpendicular, on XZ plane)
    Vec3 right{-forward.z, 0.0f, forward.x};
    right = right.normalized();
    
    // Behind the player
    Vec3 behind = {-forward.x, 0.0f, -forward.z};
    behind = behind.normalized();
    
    // Blend: mostly behind, slightly to the side
    float blendCos = std::cos(angleRad);
    float blendSin = std::sin(angleRad);
    
    Vec3 offset = behind * (distance * blendCos) + right * (distance * blendSin);
    
    return Vec3{
        playerPos.x + offset.x,
        playerPos.y + 1.6f,
        playerPos.z + offset.z
    };
}

// Second person front: in front of player looking at their face
Vec3 CameraManager::computeSecondPersonFrontPosition(
    const Vec3 &playerPos, const Vec3 &lookDir,
    float angleDeg, float distance) {
    
    float angleRad = angleDeg * static_cast<float>(M_PI / 180.0);
    
    // Player's facing direction (we stand in front of them)
    Vec3 towardPlayer = lookDir.normalized();
    
    // Right vector
    Vec3 right{-towardPlayer.z, 0.0f, towardPlayer.x};
    right = right.normalized();
    
    // Blend: mostly in front, slightly to the side
    float blendCos = std::cos(angleRad);
    float blendSin = std::sin(angleRad);
    
    Vec3 offset = towardPlayer * (distance * blendCos) + right * (distance * blendSin);
    
    return Vec3{
        playerPos.x + offset.x,
        playerPos.y + 1.6f,
        playerPos.z + offset.z
    };
}

Vec3 CameraManager::directionTo(const Vec3 &from, const Vec3 &to) {
    return (to - from).normalized();
}

Vec3 CameraManager::blendDirection(const Vec3 &from, const Vec3 &to, float weight) {
    weight = std::clamp(weight, 0.0f, 1.0f);
    float x = from.x * (1.0f - weight) + to.x * weight;
    float y = from.y * (1.0f - weight) + to.y * weight;
    float z = from.z * (1.0f - weight) + to.z * weight;
    return Vec3{x, y, z}.normalized();
}

Vec3 CameraManager::lerpVec3(const Vec3 &a, const Vec3 &b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

float CameraManager::lerpAngle(float from, float to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float diff = to - from;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return from + diff * t;
}

float CameraManager::clampPitch(float pitch) {
    if (pitch > 90.0f) pitch = 90.0f;
    if (pitch < -90.0f) pitch = -90.0f;
    return pitch;
}

float CameraManager::normalizeYaw(float yaw) {
    while (yaw > 360.0f) yaw -= 360.0f;
    while (yaw < 0.0f) yaw += 360.0f;
    return yaw;
}

} // namespace dynamicam
