#pragma once

#include "CameraManager.hpp"

namespace dynamicam {

// HotItemRender — ensures held-item animations play correctly at any camera angle.
// In second-person view the item swing animation would look wrong if it assumed
// a fixed first-person hand position. This module compensates by adjusting the
// transform sent to Minecraft's item renderer.

class HotItemRender {
public:
    // Call every frame before Minecraft renders the held item.
    // Pass current camera position so we can compute offset from it.
    static void onPreItemRender(const Vec3 &cameraPos);

    // Call after Minecraft finishes rendering the held item.
    static void onPostItemRender();

    // Update the bob/swing timing each frame (independent of render calls).
    static void updateAnimation(float deltaTime);

private:
    // Elapsed time used for the swinging/bobbing animation
    static float s_itemBobTimer;

    // Swing progress [0..1] updated when player attacks or breaks blocks
    static float s_itemSwingProgress;
};

} // namespace dynamicam
