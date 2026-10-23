#include "HotItemRender.hpp"
#include <cmath>

namespace dynamicam {

// ── Static members ───────────────────────────────────────────
float HotItemRender::s_itemBobTimer = 0.0f;
float HotItemRender::s_itemSwingProgress = 0.0f;

// ════════════════════════════════════════════════════════════
// onPreItemRender
// Called just before Minecraft renders what's in the player's hand.
// Adjusts the item transform so it appears correctly from any camera angle.
// ════════════════════════════════════════════════════════════

void HotItemRender::onPreItemRender(const Vec3 &cameraPos) {
    // The bob/wobble animation that makes held items gently sway as you walk.
    // We keep running this regardless of perspective so the timing is correct.
    s_itemBobTimer += 0.016f;  // ~60 fps increment

    float bobAmount = std::sin(s_itemBobTimer * 4.5f) * 0.05f;  // subtle wobble
    float swingAmount = s_itemSwingProgress * 1.2f;  // amplify during attack

    // TODO: Apply these offsets to the game's item-render transformation matrix.
    // This needs a dlsym'd address for the Minecraft render-item function:
    //   __ZN13ItemRenderer11renderItemE...
    // For now we store the values for external use if needed.
    (void)cameraPos;
    (void)bobAmount;
    (void)singAmount;
}

// ════════════════════════════════════════════════════════════
// onPostItemRender
// Restore any GL state that was modified during item rendering.
// ════════════════════════════════════════════════════════════

void HotItemRender::onPostItemRender() {
    // Restore depth test and cull face if they were temporarily disabled
    // by hook_playerModelRender. Most clean hooks don't need this.
}

// ════════════════════════════════════════════════════════════
// updateAnimation
// Advance the swing/bob timer each frame. Call from initHooks thread or main loop.
// ════════════════════════════════════════════════════════════

void HotItemRender::updateAnimation(float deltaTime) {
    s_itemBobTimer += deltaTime;

    // Decay swing progress back to zero over time
    if (s_itemSwingProgress > 0.0f) {
        s_itemSwingProgress -= deltaTime * 3.0f;
        if (s_itemSwingProgress < 0.0f) {
            s_itemSwingProgress = 0.0f;
        }
    }
}

} // namespace dynamicam
