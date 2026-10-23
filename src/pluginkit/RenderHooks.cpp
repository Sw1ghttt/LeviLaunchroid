#include "RenderHooks.hpp"
#include "Dynamicam.hpp"
#include "CameraManager.hpp"
#include "PerspectiveSystem.hpp"
#include "EmoteFreecam.hpp"
#include "HotItemRender.hpp"
#include "SignatureScan.hpp"

#include <pl/Hook.hpp>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cmath>
#include <cstring>

namespace dynamicam {

#define LOG_TAG "Dynamicam"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- Stored original function pointers ---
FnEGLSwapBuffers_t g_origEGLSwapBuffers = nullptr;

static FnCameraRender_t s_origCameraRender = nullptr;
static FnPlayerModelRender_t s_origPlayerModelRender = nullptr;

static bool s_gameActive = false;
static int s_screenWidth = 0;
static int s_screenHeight = 0;

// ================================================================
// EGL Swap Buffers Hook
// Intercepted every frame; used as our per-frame entry point
// ================================================================

int hook_EGLSwapBuffers(void *display, void *surface) {
    if (!g_origEGLSwapBuffers) {
        // If we couldn't find the original, just return success to avoid crash
        return 1;
    }

    // Only process frames when an OpenGL context is active
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) {
        return g_origEGLSwapBuffers(display, surface);
    }

    // Read back buffer dimensions — skip tiny pre-render surfaces
    EGLint w = 0, h = 0;
    eglQuerySurface(static_cast<EGLDisplay>(display), static_cast<EGLSurface>(surface),
                    EGL_WIDTH, &w);
    eglQuerySurface(static_cast<EGLDisplay>(display), static_cast<EGLSurface>(surface),
                    EGL_HEIGHT, &h);

    if (w < 400 || h < 400) {
        return g_origEGLSwapBuffers(display, surface);
    }

    s_screenWidth = static_cast<int>(w);
    s_screenHeight = static_cast<int>(h);
    s_gameActive = true;

    // Per-frame updates — run before the game renders its own frame
    PerspectiveSystem::preRenderFrame(s_screenWidth, s_screenHeight);

    return g_origEGLSwapBuffers(display, surface);
}

// ================================================================
// Camera Render Hook
// Modifies camera position / rotation each frame based on current perspective
// ================================================================

void hook_cameraRender(CameraCtx *camera, float deltaTime) {
    if (!s_gameActive) {
        if (s_origCameraRender) {
            s_origCameraRender(camera, deltaTime);
        }
        return;
    }

    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();

    // Skip modification if first person body is disabled and mode is vanilla 1P
    if (!config.firstPersonEnabled && dyn.getCurrentPerspective() == PerspectiveMode::FIRST_PERSON) {
        if (s_origCameraRender) {
            s_origCameraRender(camera, deltaTime);
        }
        return;
    }

    // ── Get current player/camera state from Minecraft ──────────────
    Vec3 eyePos = getPlayerEyePosition();
    Vec3 lookDir = getPlayerLookDirection();
    Vec3 currentCamPos = getCurrentCameraPosition();
    float currentYaw = getCameraYaw();
    float currentPitch = getCameraPitch();

    // ── Calculate target camera position ───────────────────────────
    Vec3 targetPos = currentCamPos;
    float targetYaw = currentYaw;
    float targetPitch = currentPitch;

    if (dyn.isEmoting() && config.freecamDuringEmote) {
        // Freecam orbit during emotes
        targetPos = EmoteFreecam::computeFreeCamPosition(eyePos, deltaTime);
        targetYaw = EmoteFreecam::getFreeCamYaw();
        targetPitch = EmoteFreecam::getFreeCamPitch();
    } else {
        switch (dyn.getCurrentPerspective()) {
            case PerspectiveMode::FIRST_PERSON: {
                targetPos = CameraManager::computeFirstPersonPosition(eyePos);
                // Apply user-configurable offsets
                targetPos.x += config.cameraOffsetX;
                targetPos.y += config.cameraOffsetY;
                targetPos.z += config.cameraOffsetZ;
                targetYaw = std::atan2(lookDir.x, lookDir.z);
                targetPitch = std::asin(std::clamp(lookDir.y, -1.0f, 1.0f));
                break;
            }

            case PerspectiveMode::SECOND_PERSON_BACK: {
                targetPos = CameraManager::computeSecondPersonBackPosition(
                    eyePos, lookDir,
                    config.secondPersonBackAngle,
                    config.secondPersonBackDistance);
                // Look at the player
                Vec3 lookAtTarget = eyePos;
                targetYaw = std::atan2(
                    -(lookAtTarget.x - targetPos.x),
                    -(lookAtTarget.z - targetPos.z));
                targetPitch = std::asin(std::clamp(
                    (lookAtTarget.y - targetPos.y) / targetPos.distanceTo(lookAtTarget),
                    -1.0f, 1.0f));
                break;
            }

            case PerspectiveMode::SECOND_PERSON_FRONT: {
                targetPos = CameraManager::computeSecondPersonFrontPosition(
                    eyePos, lookDir,
                    config.secondPersonFrontAngle,
                    config.secondPersonFrontDistance);
                // Look at the player
                Vec3 lookAtTarget = eyePos;
                targetYaw = std::atan2(
                    -(lookAtTarget.x - targetPos.x),
                    -(lookAtTarget.z - targetPos.z));
                targetPitch = std::asin(std::clamp(
                    (lookAtTarget.y - targetPos.y) / targetPos.distanceTo(lookAtTarget),
                    -1.0f, 1.0f));
                break;
            }
        }
    }

    // ── Crosshair aim tracking ─────────────────────────────────────
    // When aiming at a block, blend player look direction with raycast direction
    if (config.aimTrackingEnabled &&
        !(dyn.isEmoting() && config.freecamDuringEmote)) {
        Vec3 aimedBlockPos = getRaycastTarget();
        if (aimedBlockPos.lengthSquared() > 0.0f) {
            Vec3 aimDir = CameraManager::directionTo(targetPos, aimedBlockPos);
            Vec3 playerLook = getPlayerLookDirection();
            Vec3 blendedDir = CameraManager::blendDirection(
                playerLook, aimDir, config.crosshairLockStrength);
            targetYaw = std::atan2(blendedDir.x, blendedDir.z);
            targetPitch = std::asin(std::clamp(blendedDir.y, -1.0f, 1.0f));
        }
    }

    // ── Smooth transition ──────────────────────────────────────────
    float smoothT = config.smoothingSpeed;
    float lerpFactor = 1.0f - std::pow(1.0f - smoothT, deltaTime * 60.0f);

    Vec3 finalPos = CameraManager::lerpVec3(currentCamPos, targetPos, lerpFactor);
    float finalYaw = CameraManager::lerpAngle(currentYaw, targetYaw, lerpFactor);
    float finalPitch = CameraManager::lerpAngle(currentPitch, targetPitch, lerpFactor);

    // ── Apply to game camera ───────────────────────────────────────
    setCameraPosition(finalPos);
    setCameraRotation(finalPitch, finalYaw);

    // ── Call original render ───────────────────────────────────────
    if (s_origCameraRender) {
        s_origCameraRender(camera, deltaTime);
    }
}

// ================================================================
// Player Model Render Hook
// Renders full body model in first-person view
// ================================================================

void hook_playerModelRender(PlayerCtx *player, float deltaTime) {
    if (!s_origPlayerModelRender) return;

    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();

    if (config.firstPersonEnabled &&
        dyn.getCurrentPerspective() == PerspectiveMode::FIRST_PERSON) {

        // Save GL state
        GLint savedProgram, savedTex, savedVao, savedFBO;
        glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTex);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVao);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);

        // Temporarily hide player's head bone mesh if configured
        if (config.hideOwnHead) {
            setPlayerHeadVisible(false);
        }

        // Render the full third-person model so you see hands/torso/legs
        glDisable(GL_CULL_FACE);  // Ensure full visibility

        s_origPlayerModelRender(player, deltaTime);

        if (config.hideOwnHead) {
            setPlayerHeadVisible(true);
        }

        // Restore GL state
        glBindVertexArray(savedVao);
        glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);
        glBindTexture(GL_TEXTURE_2D, savedTex);
        glUseProgram(savedProgram);
    }

    // Always call original to not break vanilla rendering paths
    s_origPlayerModelRender(player, deltaTime);
}

// ================================================================
// Initialization: resolve symbols & install hooks
// ================================================================

void initHooks() {
    auto &dyn = Dynamicam::instance();
    dyn.getLogger().info("Initializing hooks...");

    // ── 1. Hook eglSwapBuffers for per-frame callback ────────────
    void *eglLib = dlopen("libEGL.so", RTLD_LAZY);
    if (eglLib) {
        void *sym = dlsym(eglLib, "eglSwapBuffers");
        if (sym) {
            g_origEGLSwapBuffers = reinterpret_cast<FnEGLSwapBuffers_t>(sym);

            GHook hook(sym, reinterpret_cast<void *>(hook_EGLSwapBuffers),
                       reinterpret_cast<void **>(&g_origEGLSwapBuffers));
            if (hook) {
                dyn.getLogger().info("e glSwapBuffers hooked ✓");
            } else {
                dyn.getLogger().error("Failed to hook eglSwapBuffers");
            }
        } else {
            dyn.getLogger().warn("eglSwapBuffers symbol not found in libEGL.so");
        }
        dlclose(eglLib);
    }

    // ── 2. Hook camera & player model via libminecraftpe.so ──────
    void *mcpeLib = dlopen("libminecraftpe.so", RTLD_LAZY);
    if (mcpeLib) {
        // Try direct symbol resolution first (works for known MC versions)
        void *camRenderSym = dlsym(mcpeLib, "_ZN6Camera6renderEf");
        if (camRenderSym) {
            s_origCameraRender = reinterpret_cast<FnCameraRender_t>(camRenderSym);
            GHook hook(camRenderSym, reinterpret_cast<void *>(hook_cameraRender),
                       reinterpret_cast<void **>(&s_origCameraRender));
            if (hook) {
                dyn.getLogger().info("Camera::render hooked ✓");
            } else {
                dyn.getLogger().error("Failed to hook Camera::render");
            }
        }

        void *modelRenderSym = dlsym(mcpeLib, "_ZN6Player11renderModelEf");
        if (modelRenderSym) {
            s_origPlayerModelRender = reinterpret_cast<FnPlayerModelRender_t>(modelRenderSym);
            GHook hook(modelRenderSym, reinterpret_cast<void *>(hook_playerModelRender),
                       reinterpret_cast<void **>(&s_origPlayerModelRender));
            if (hook) {
                dyn.getLogger().info("Player::renderModel hooked ✓");
            } else {
                dyn.getLogger().error("Failed to hook Player::renderModel");
            }
        }

        dlclose(mcpeLib);
    }

    // ── 3. Fallback: signature scanning if direct symbols failed ─
    if (!s_origCameraRender || !s_origPlayerModelRender) {
        dyn.getLogger().info("Falling back to ARM64 signature scanning...");
        initSignatureScanning();
    }

    dyn.getLogger().info("Hook initialization complete");
}

// ================================================================
// Cleanup: restore originals
// ================================================================

void uninitHooks() {
    auto &dyn = Dynamicam::instance();
    dyn.getLogger().info("Unregistering hooks...");

    s_origCameraRender = nullptr;
    s_origPlayerModelRender = nullptr;
    g_origEGLSwapBuffers = nullptr;
    s_gameActive = false;
}

} // namespace dynamicam
