#pragma once

// Forward declaration of game types (we don't include the full SDK headers for these)
struct CameraCtx;
struct PlayerCtx;

// Function pointer types for resolved symbols
using FnEGLSwapBuffers_t = int (*)(void *display, void *surface);
using FnCameraRender_t = void (*)(CameraCtx *, float);
using FnPlayerModelRender_t = void (*)(PlayerCtx *, float);

extern FnEGLSwapBuffers_t g_origEGLSwapBuffers;

// Hook implementations that replace the originals at runtime
void hook_EGLSwapBuffers(void *display, void *surface);
void hook_cameraRender(CameraCtx *camera, float deltaTime);
void hook_playerModelRender(PlayerCtx *player, float deltaTime);

// Called from Dynamicam::enable() after Mod Menu registration
void initHooks();

// Called from Dynamicam::disable() before unregistration
void uninitHooks();
