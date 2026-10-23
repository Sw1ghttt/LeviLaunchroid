#pragma once

#include <cmath>
#include <vector>

namespace dynamicam {

struct Vec3 {
    float x, y, z;

    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float l = length();
        return l > 0.0001f ? *this * (1.0f / l) : Vec3{0.0f, 0.0f, 0.0f};
    }
};

class CameraManager {
public:
    // First person: camera at eye level, slightly forward from head center
    static Vec3 computeFirstPersonPosition(const Vec3 &playerPos);

    // Second person back: behind player with slight angle offset
    static Vec3 computeSecondPersonBackPosition(
        const Vec3 &playerPos, const Vec3 &lookDir,
        float angleDeg, float distance);

    // Second person front: in front of player looking at face
    static Vec3 computeSecondPersonFrontPosition(
        const Vec3 &playerPos, const Vec3 &lookDir,
        float angleDeg, float distance);

    // Direction from A to B
    static Vec3 directionTo(const Vec3 &from, const Vec3 &to);

    // Blend between two directions based on weight (0-1)
    static Vec3 blendDirection(const Vec3 &from, const Vec3 &to, float weight);

    // Smooth vector interpolation (lerp)
    static Vec3 lerpVec3(const Vec3 &a, const Vec3 &b, float t);

    // Angle lerp with wrapping support (-180..180)
    static float lerpAngle(float from, float to, float t);

    // Clamp pitch to valid range
    static float clampPitch(float pitch);

    // Normalize yaw to 0..360
    static float normalizeYaw(float yaw);
};

} // namespace dynamicam
