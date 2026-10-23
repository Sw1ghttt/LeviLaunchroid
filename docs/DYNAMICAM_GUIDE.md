# 🎥 Dynamicam — Step-by-Step Mod Development Guide

**By Sw1ghttt** | For LeviLaunchroid on Minecraft Bedrock Edition  
**Target Version:** 1.26.5X.XX (Bedrock) | **Platform:** Android ARM64  

---

## 📋 What Dynamicam Does

| Perspective | Behavior |
|---|---|
| **First Person** | Camera at face level. See hands, arms, torso, legs — but not your own head. |
| **Second Person Back** | Camera behind & slightly angled to the right-left side, looking at player's back. |
| **Second Person Front** | Camera in front, facing player's face, slightly angled. |
| **Aim Tracking** | When pointing at a block, camera crosshair locks to center → accurately points at the target block. |
| **Emote Freecam** | While emoting, the camera freely rotates around without affecting the emote animation. |
| **Handheld Items** | Animated items render properly in both hands regardless of camera angle. |

---

## 🛠️ Prerequisites Checklist

Before starting, make sure you have ALL of these:

### Software Required
- [ ] **Android Studio** (Iguana or newer) — https://developer.android.com/studio
- [ ] **Android NDK r27b+** — bundled with Android Studio or standalone from https://developer.android.com/ndk
- [ ] **CMake 3.22+** — comes with Android Studio
- [ ] **JDK 21+** — comes with Android Studio
- [ ] **Git** — https://git-scm.com
- [ ] **LeviLaunchroid** installed on your Android device — https://github.com/LiteLDev/levilaunchroid
- [ ] Legitimate copy of **Minecraft Bedrock Edition** (via LeviLaunchroid)

### Hardware Required
- [ ] Android device with **ARM64 (arm64-v8a)** CPU
- [ ] At least 8GB RAM for compilation
- [ ] ~2GB free disk space

### Knowledge Required
- [ ] Basic C++ programming knowledge
- [ ] Understanding of OpenGL ES rendering concepts
- [ ] Familiarity with hooking and memory manipulation

---

## 📂 Step 1 — Create Project Structure

Open a terminal and run these commands:

```bash
# Create the project folder
mkdir -p Dynamicam/src/main
mkdir -p Dynamicam/src/pluginkit
mkdir -p Dynamicam/config
mkdir -p Dynamicam/resources

cd Dynamicam
```

Your structure should look like:
```
Dynamicam/
├── CMakeLists.txt          # Main build configuration
├── manifest.json           # LeviLaunchroid metadata
├── config/
│   ├── config.json         # Default settings file
│   └── config.schema.json  # Config schema for Mod Menu
├── resources/
│   └── logo.png            # Your mod logo (add later)
├── src/
│   ├── main.cpp            # Entry point + PL_REGISTER_MOD
│   └── pluginkit/
│       ├── Dynamicam.hpp   # Main class header
│       ├── Dynamicam.cpp   # Implementation
│       ├── CameraManager.cpp  # Camera system
│       ├── CameraManager.hpp
│       ├── RenderHooks.cpp    # OpenGL/render hooks
│       ├── RenderHooks.hpp
│       ├── PerspectiveSystem.cpp # First/second person logic
│       ├── PerspectiveSystem.hpp
│       ├── EmoteFreecam.cpp     # Emote freecamsystem
│       ├── EmoteFreecam.hpp
│       ├── HotItemRender.cpp    # Item animation rendering
│       ├── HotItemRender.hpp
│       └── SignatureScan.cpp    # Version-detection signatures
│       └── SignatureScan.hpp
└── build.sh              # Build script
```

---

## 📄 Step 2 — Create `manifest.json`

This tells LeviLaunchroid about your mod:

```json
{
  "type": "preload-native",
  "name": "Dynamicam",
  "author": "Sw1ghttt",
  "version": "1.0.0",
  "entry": "libdynamicam.so",
  "icon": "resources/logo.png",
  "minecraft_versions": ["*"],
  "overwrite_files": [],
  "overwrite_folders": []
}
```

> The `"minecraft_versions": ["*"]` means it works on all versions. Pin specific versions if needed.

---

## ⚙️ Step 3 — Create `config/config.json`

Default values your players will see:

```json
{
  "version": 1,
  "firstPersonEnabled": true,
  "camera_offset_x": 0.0,
  "camera_offset_y": -0.15,
  "camera_offset_z": 0.35,
  "model_scale": 1.0,
  "hideOwnHead": true,
  "secondPersonBack_Angle": 25.0,
  "secondPersonBack_Distance": 4.5,
  "secondPersonFront_Angle": -25.0,
  "secondPersonFront_Distance": 4.5,
  "freecamDuringEmote": true,
  "aimTrackingEnabled": true,
  "crosshairLockStrength": 0.95,
  "itemAnimationEnabled": true,
  "showInWater": false,
  "perspectiveMode": "first_person",
  "smoothingSpeed": 0.15
}
```

---

## 📝 Step 4 — Create `config/config.schema.json`

This powers the in-game Mod Menu UI:

```json
{
  "title": "Dynamicam Settings",
  "description": "Camera perspectives and customization by Sw1ghttt",
  "fields": {
    "perspectiveMode": {
      "displayName": "Default Perspective",
      "type": "radio",
      "options": "first_person,second_person_back,second_person_front"
    },
    "firstPersonEnabled": {
      "displayName": "Enable First Person Body",
      "type": "toggle"
    },
    "camera_offset_x": {
      "displayName": "1P X Offset",
      "type": "slider_float",
      "min": "-2.0",
      "max": "2.0",
      "default": "0.0"
    },
    "camera_offset_y": {
      "displayName": "1P Y Offset",
      "type": "slider_float",
      "min": "-2.0",
      "max": "2.0",
      "default": "-0.15"
    },
    "camera_offset_z": {
      "displayName": "1P Z Offset",
      "type": "slider_float",
      "min": "-2.0",
      "max": "2.0",
      "default": "0.35"
    },
    "model_scale": {
      "displayName": "Model Scale",
      "type": "slider_float",
      "min": "0.5",
      "max": "2.0",
      "default": "1.0"
    },
    "secondPersonBack_Angle": {
      "displayName": "2P Back Angle",
      "type": "slider_float",
      "min": "-45.0",
      "max": "45.0",
      "default": "25.0"
    },
    "secondPersonBack_Distance": {
      "displayName": "2P Back Distance",
      "type": "slider_float",
      "min": "2.0",
      "max": "10.0",
      "default": "4.5"
    },
    "secondPersonFront_Angle": {
      "displayName": "2P Front Angle",
      "type": "slider_float",
      "min": "-45.0",
      "max": "45.0",
      "default": "-25.0"
    },
    "secondPersonFront_Distance": {
      "displayName": "2P Front Distance",
      "type": "slider_float",
      "min": "2.0",
      "max": "10.0",
      "default": "4.5"
    },
    "freecamDuringEmote": {
      "displayName": "Freecam During Emotes",
      "type": "toggle"
    },
    "aimTrackingEnabled": {
      "displayName": "Crosshair Aim Tracking",
      "type": "toggle"
    },
    "crosshairLockStrength": {
      "displayName": "Aim Lock Strength",
      "type": "slider_float",
      "min": "0.0",
      "max": "1.0",
      "default": "0.95"
    },
    "itemAnimationEnabled": {
      "displayName": "Animated Items In Hands",
      "type": "toggle"
    },
    "smoothingSpeed": {
      "displayName": "Perspective Switch Smoothness",
      "type": "slider_float",
      "min": "0.01",
      "max": "1.0",
      "default": "0.15"
    }
  }
}
```

---

## 💻 Step 5 — Create `CMakeLists.txt`

The build file linking everything together:

```cmake
cmake_minimum_required(VERSION 3.22)
project(Dynamicam LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization for Android .so size
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -fexceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

# Fetch the preloader SDK
include(FetchContent)
FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG main
)
FetchContent_MakeAvailable(preloader_android)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# All source files
set(SOURCES
    src/main.cpp
    src/pluginkit/Dynamicam.cpp
    src/pluginkit/CameraManager.cpp
    src/pluginkit/RenderHooks.cpp
    src/pluginkit/PerspectiveSystem.cpp
    src/pluginkit/EmoteFreecam.cpp
    src/pluginkit/HotItemRender.cpp
    src/pluginkit/SignatureScan.cpp
)

# Build as shared library (.so)
add_library(dynamicam SHARED ${SOURCES})

# Link libraries
target_link_libraries(dynamicam
    preloader
    log
    android
    EGL
    GLESv3
    GLESv2
    jnigraphics
)
```

---

## 🔧 Step 6 — Create `build.sh`

Automated build script:

```bash
#!/bin/bash
set -e

echo "🔨 Building Dynamicam..."

# Detect NDK path
NDK_PATH="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-${ANDROID_HOME}/ndk/latest}}"

if [ ! -d "$NDK_PATH" ]; then
    echo "❌ NDK not found. Set ANDROID_NDK_HOME or ANDROID_HOME."
    exit 1
fi

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_NDK="$NDK_PATH" \
    -DCMAKE_SYSTEM_VERSION=26 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build . --parallel $(nproc --all)

# Copy output to expected location
DIST_DIR="../dist/arm64-v8a/dynamicam"
mkdir -p "$DIST_DIR"
cp libdynamicam.so "$DIST_DIR/libdynamicam.so"
cp ../manifest.json "$DIST_DIR/manifest.json"
cp ../config/config.json "$DIST_DIR/config.json"
cp ../config/config.schema.json "$DIST_DIR/config.schema.json"

echo ""
echo "✅ Build complete!"
echo "📦 Output: $DIST_DIR/"
echo ""
echo "To install:"
echo "  cp -r $DIST_DIR ~/.config/levilaunchroid/mods/dynamicam/"
echo "  # OR import the directory through LeviLaunchroid settings"
```

Make it executable:
```bash
chmod +x build.sh
```

---

## 🧠 Step 7 — The Core Source Code

### `src/main.cpp` — Entry Point

```cpp
#include "pluginkit/Dynamicam.hpp"
#include <pl/internal/Macro.h>

// Single long-lived instance registered with LeviLaunchroid
PL_REGISTER_MOD(Dynamicam, Dynamicam::instance())
```

### `src/pluginkit/Dynamicam.hpp` — Main Class Header

```cpp
#pragma once

#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/Hook.hpp>
#include <optional>
#include <string>
#include <mutex>

namespace dynamicam {

enum class PerspectiveMode {
    FIRST_PERSON,
    SECOND_PERSON_BACK,
    SECOND_PERSON_FRONT
};

struct Config {
    int version = 1;
    bool firstPersonEnabled = true;
    float cameraOffsetX = 0.0f;
    float cameraOffsetY = -0.15f;
    float cameraOffsetZ = 0.35f;
    float modelScale = 1.0f;
    bool hideOwnHead = true;
    float secondPersonBackAngle = 25.0f;
    float secondPersonBackDistance = 4.5f;
    float secondPersonFrontAngle = -25.0f;
    float secondPersonFrontDistance = 4.5f;
    bool freecamDuringEmote = true;
    bool aimTrackingEnabled = true;
    float crosshairLockStrength = 0.95f;
    bool itemAnimationEnabled = true;
    bool showInWater = false;
    PerspectiveMode perspectiveMode = PerspectiveMode::FIRST_PERSON;
    float smoothingSpeed = 0.15f;
};

class Dynamicam {
public:
    static Dynamicam &instance();

    [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

    // Accessors used by subsystems
    const Config& getConfig() const { return mConfig.value(); }
    void setPerspective(PerspectiveMode mode);
    PerspectiveMode getCurrentPerspective() const;
    void setEmotingState(bool emoting);
    bool isEmoting() const;

private:
    Dynamicam() : mSelf(*ll::mod::NativeMod::current()) {}

    ll::mod::NativeMod &mSelf;
    std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<Config>> mConfigFile;
    PerspectiveMode mCurrentPerspective = PerspectiveMode::FIRST_PERSON;
    bool mIsEmoting = false;
    bool mGameInitialized = false;
};

} // namespace dynamicam
```

### `src/pluginkit/Dynamicam.cpp` — Lifecycle & Config

```cpp
#include "Dynamicam.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

namespace dynamicam {

using DynCfg = dynamicam::Config;
using DynMode = dynamicam::PerspectiveMode;

constexpr const char *kModuleId = "dynamicam.main";

static constexpr unsigned char kLogoSvg[] = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
    <rect width="64" height="64" rx="12" fill="#1A1A2E"/>
    <circle cx="32" cy="28" r="12" fill="none" stroke="#E94560" stroke-width="3"/>
    <circle cx="32" cy="28" r="5" fill="#E94560"/>
    <path d="M20 48 Q32 56 44 48" stroke="#E94560" stroke-width="2" fill="none"/>
</svg>)svg";

DynCfg parseConfig(const pl::config::FieldValues &fv) {
    DynCfg cfg{};
    auto getStr = [&](const char* key, const char* fallback) {
        auto it = fv.find(key);
        return it != fv.end() ? std::string(it->second) : fallback;
    };
    auto getFloat = [&](const char* key, float fallback) {
        auto v = getStr(key, std::to_string(fallback).c_str());
        return std::strtof(v.c_str(), nullptr);
    };
    auto getBool = [&](const char* key, bool fallback) {
        auto v = getStr(key, "");
        return (v == "true" || v == "1") ? true : fallback;
    };

    cfg.version = 1;
    cfg.firstPersonEnabled = getBool("firstPersonEnabled", true);
    cfg.cameraOffsetX = getFloat("camera_offset_x", 0.0f);
    cfg.cameraOffsetY = getFloat("camera_offset_y", -0.15f);
    cfg.cameraOffsetZ = getFloat("camera_offset_z", 0.35f);
    cfg.modelScale = getFloat("model_scale", 1.0f);
    cfg.hideOwnHead = getBool("hideOwnHead", true);
    cfg.secondPersonBackAngle = getFloat("secondPersonBack_Angle", 25.0f);
    cfg.secondPersonBackDistance = getFloat("secondPersonBack_Distance", 4.5f);
    cfg.secondPersonFrontAngle = getFloat("secondPersonFront_Angle", -25.0f);
    cfg.secondPersonFrontDistance = getFloat("secondPersonFront_Distance", 4.5f);
    cfg.freecamDuringEmote = getBool("freecamDuringEmote", true);
    cfg.aimTrackingEnabled = getBool("aimTrackingEnabled", true);
    cfg.crosshairLockStrength = getFloat("crosshairLockStrength", 0.95f);
    cfg.itemAnimationEnabled = getBool("itemAnimationEnabled", true);
    cfg.showInWater = getBool("showInWater", false);
    cfg.smoothingSpeed = getFloat("smoothingSpeed", 0.15f);

    std::string modeStr = getStr("perspectiveMode", "first_person");
    if (modeStr == "second_person_back") cfg.perspectiveMode = DynMode::SECOND_PERSON_BACK;
    else if (modeStr == "second_person_front") cfg.perspectiveMode = DynMode::SECOND_PERSON_FRONT;
    else cfg.perspectiveMode = DynMode::FIRST_PERSON;

    return cfg;
}

void Dynamicam::setPerspective(PerspectiveMode mode) {
    mCurrentPerspective = mode;
}

PerspectiveMode Dynamicam::getCurrentPerspective() const {
    return mCurrentPerspective;
}

void Dynamicam::setEmotingState(bool emoting) {
    mIsEmoting = emoting;
}

bool Dynamicam::isEmoting() const {
    return mIsEmoting;
}

bool Dynamicam::load() {
    auto &self = getSelf();
    self.getLogger().info("Loading {}", self.getName());

    // Create config dir
    std::filesystem::create_directories(self.getConfigDir());

    // Load config (try typed first, fall back to manual)
    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        self.getLogger().warn("Using default config");
        mConfigFile->value() = parseConfig({});
        mConfigFile->save();
    }

    self.getLogger().info("Loaded Dynamicam v{}", self.getVersion());
    return true;
}

bool Dynamicam::enable() {
    auto &self = getSelf();
    const auto &config = mConfigFile->value();

    self.getLogger().info("Enabling Dynamicam...");

    // Register HUD module with Mod Menu
    const bool moduleOk = pl::modmenu::ModuleBuilder(kModuleId, "Dynamicam")
        .modId(self.getId())
        .description("Camera perspectives & first-person body view. By Sw1ghttt")
        .defaultEnabled(true)
        .onToggle([](std::string_view mid, bool on) {
            instance().getSelf().getLogger().info("Dynamicam {}", on ? "enabled" : "disabled");
        })
        .registerModule();

    // Register SVG icon button for quick perspective cycling
    const bool buttonOk = pl::modmenu::ButtonBuilder("dynamicam.cycle_pov", "Cycle Perspective")
        .moduleId(kModuleId)
        .label("◉")
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .svgIcon(kLogoSvg)
        .onEvent([this](std::string_view bid, pl::modmenu::ButtonEvent event, float value) {
            if (event == pl::modmenu::ButtonEvent::Down) {
                auto next = static_cast<int>(mCurrentPerspective) + 1;
                if (next > 2) next = 0;
                setPerspective(static_cast<PerspectiveMode>(next));
                mConfigFile->value().perspectiveMode = getCurrentPerspective();
                mConfigFile->save();
                getSelf().getLogger().info("Perspective changed to {}", 
                    static_cast<int>(getCurrentPerspective()));
            }
        })
        .registerButton();

    if (moduleOk && buttonOk) {
        self.getLogger().info("Mod Menu registered ✓");
    } else {
        self.getLogger().error("Failed to register Mod Menu entries");
        return false;
    }

    // Initialize game hooks
    initHooks();

    return true;
}

bool Dynamicam::disable() {
    pl::modmenu::unregisterModule(kModuleId);
    pl::modmenu::unregisterButton("dynamicam.cycle_pov");
    uninitHooks();
    getSelf().getLogger().info("Dynamicam disabled");
    return true;
}

bool Dynamicam::unload() {
    mConfigFile.reset();
    getSelf().getLogger().info("Dynamicam unloaded");
    return true;
}

} // namespace dynamicam
```

### `src/pluginkit/CameraManager.hpp` — Core Camera System

```cpp
#pragma once

#include <cmath>
#include <vector>

namespace dynamicam {

struct Vec3 {
    float x, y, z;
    
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const { float l = length(); return l > 0 ? *this * (1.0f/l) : Vec3{0,0,0}; }
};

struct CameraData {
    Vec3 position;
    float pitch;    // Up/down
    float yaw;      // Left/right rotation
    float roll;     // Tilt
    
    // Target values for smooth interpolation
    Vec3 targetPosition;
    float targetPitch, targetYaw, targetRoll;
};

class CameraManager {
public:
    // Returns the ideal camera position for a given perspective mode
    static Vec3 computeFirstPersonPosition(const Vec3& playerPos, const Vec3& lookDir);
    static Vec3 computeSecondPersonBackPosition(const Vec3& playerPos, const Vec3& lookDir, 
                                                  float angleDeg, float distance);
    static Vec3 computeSecondPersonFrontPosition(const Vec3& playerPos, const Vec3& lookDir,
                                                   float angleDeg, float distance);
    
    // Look-at math: returns direction from camera to target block
    static Vec3 computeLookAtDirection(const Vec3& camPos, const Vec3& blockPos);
    
    // Crosshair lock: blends player look direction with aimed block direction
    static Vec3 blendLookWithAim(const Vec3& playerLook, const Vec3& aimDirection, float weight);
    
    // Smooth interpolation (lerp) for transitions
    static Vec3 lerpVec3(const Vec3& from, const Vec3& to, float t);
    static float lerpAngle(float from, float to, float t);
    
    // Apply clamp to keep angles valid
    static float normalizePitch(float pitch);
    static float normalizeYaw(float yaw);
};

} // namespace dynamicam
```

### `src/pluginkit/CameraManager.cpp` — Camera Math

```cpp
#include "CameraManager.hpp"
#include <cmath>

namespace dynamicam {

Vec3 CameraManager::computeFirstPersonPosition(const Vec3& playerPos, const Vec3&) {
    // Camera sits at the player's eye position (slightly offset down so you don't see through ground)
    // You see hands/arms/torso/legs because the model renders relative to camera
    return Vec3{playerPos.x, playerPos.y + 1.6f, playerPos.z};
}

Vec3 CameraManager::computeSecondPersonBackPosition(const Vec3& playerPos, const Vec3& lookDir,
                                                      float angleDeg, float distance) {
    // Behind player, slightly off to the side (for cinematic feel)
    float angleRad = angleDeg * M_PI / 180.0f;
    
    // Perpendicular to look direction (left side when looking forward)
    Vec3 right = {-lookDir.z, 0.0f, lookDir.x};
    right = right.normalized();
    
    // Position behind + sideways
    Vec3 behind = {-lookDir.x, 0.0f, -lookDir.z};
    behind = behind.normalized();
    
    // Combine: mostly behind, slightly to the side
    Vec3 pos = behind * (distance * cos(angleRad)) + right * (distance * sin(angleRad));
    pos = playerPos + Vec3{pos.x, playerPos.y + 1.6f, pos.z};
    
    return pos;
}

Vec3 CameraManager::computeSecondPersonFrontPosition(const Vec3& playerPos, const Vec3& lookDir,
                                                       float angleDeg, float distance) {
    // In front of player, looking at them
    float angleRad = angleDeg * M_PI / 180.0f;
    
    Vec3 towardsPlayer = lookDir; // Same direction they're looking (since we're in front)
    Vec3 right = {-lookDir.z, 0.0f, lookDir.x};
    right = right.normalized();
    
    Vec3 pos = towardsPlayer * (distance * cos(angleRad)) + right * (distance * sin(angleRad));
    pos = playerPos + Vec3{pos.x, playerPos.y + 1.6f, pos.z};
    
    return pos;
}

Vec3 CameraManager::computeLookAtDirection(const Vec3& camPos, const Vec3& blockPos) {
    return (blockPos - camPos).normalized();
}

Vec3 CameraManager::blendLookWithAim(const Vec3& playerLook, const Vec3& aimDirection, float weight) {
    // weight: 0 = pure player look, 1 = pure aim direction
    float blendedX = playerLook.x * (1.0f - weight) + aimDirection.x * weight;
    float blendedY = playerLook.y * (1.0f - weight) + aimDirection.y * weight;
    float blendedZ = playerLook.z * (1.0f - weight) + aimDirection.z * weight;
    return {blendedX, blendedY, blendedZ}.normalized();
}

Vec3 CameraManager::lerpVec3(const Vec3& from, const Vec3& to, float t) {
    t = t > 1.0f ? 1.0f : (t < 0.0f ? 0.0f : t);
    return {
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t
    };
}

float CameraManager::lerpAngle(float from, float to, float t) {
    // Handle angle wrapping (0 <-> 360)
    float diff = to - from;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return from + diff * t;
}

float CameraManager::normalizePitch(float pitch) {
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
```

### `src/pluginkit/RenderHooks.hpp` — OpenGL Hooks

```cpp
#pragma once

// Forward declarations for hook targets
struct Camera {};
struct Player {};

// These function pointers are resolved at runtime via dlsym or signature scanning
typedef void (*Fn_eglSwapBuffers_t)(void*, void*);
typedef void (*Fn_CameraRender_t)(Camera*, float);
typedef void (*Fn_PlayerRender_t)(Player*, float);
typedef void (*Fn_RenderPlayerModel_t)(Player*, float);

extern Fn_eglSwapBuffers_t orig_eglSwapBuffers;
extern Fn_CameraRender_t orig_cameraRender;
extern Fn_PlayerRender_t orig_playerRender;

// Hook functions — these replace the originals at runtime
void hook_eglSwapBuffers(void* display, void* surface);
void hook_cameraRender(Camera* camera, float deltaTime);
void hook_renderPlayerModel(Player* player, float deltaTime);

// Initialization/cleanup
void initHooks();
void uninitHooks();
```

### `src/pluginkit/RenderHooks.cpp` — The Rendering Magic

```cpp
#include "RenderHooks.hpp"
#include "Dynamicam.hpp"
#include "CameraManager.hpp"
#include "PerspectiveSystem.hpp"
#include "EmoteFreecam.hpp"
#include "HotItemRender.hpp"
#include "SignatureScan.hpp"
#include <pl/Hook.hpp>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

using namespace dynamicam;

static Fn_eglSwapBuffers_t orig_eglSwapBuffers = nullptr;
static Fn_CameraRender_t orig_cameraRender = nullptr;
static Fn_RenderPlayerModel_t orig_renderPlayerModel = nullptr;

static EGLBoolean (*orig_EGLSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;
static bool g_GameActive = false;

// Hooked eglSwapBuffers — called every frame
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    if (!orig_EGLSwapBuffers) return EGL_FALSE;
    
    // Only process frames where Minecraft is actually rendering
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) {
        return orig_EGLSwapBuffers(dpy, surf);
    }
    
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surf, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
    
    if (w < 400 || h < 400) {
        return orig_EGLSwapBuffers(dpy, surf);
    }
    
    g_GameActive = true;
    
    // Before swap: apply any final camera adjustments
    PerspectiveSystem::preRenderFrame(w, h);
    
    // After swap but before presenting: draw Mod Menu overlays if enabled
    // (handled by LeviLauncher's Mod Menu system)
    
    return orig_EGLSwapBuffers(dpy, surf);
}

// Hooked camera render — modify camera position each frame
void hook_cameraRender(void* camera, float deltaTime) {
    if (!g_GameActive) {
        if (orig_cameraRender) orig_cameraRender(camera, deltaTime);
        return;
    }
    
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    
    if (!config.firstPersonEnabled && 
        dyn.getCurrentPerspective() == PerspectiveMode::FIRST_PERSON) {
        // Vanilla behavior — no modification
        if (orig_cameraRender) orig_cameraRender(camera, deltaTime);
        return;
    }
    
    // Get current camera/player state from game
    Vec3 camPos = getPlayerEyePosition();
    Vec3 lookDir = getPlayerLookDirection();
    
    // Determine target camera position based on current perspective
    Vec3 targetPos;
    float targetPitch = camPos.y > 0 ? 0.0f : 0.0f;  // Will be set from input
    float targetYaw = atan2(lookDir.x, lookDir.z);
    
    if (dyn.isEmoting() && config.freecamDuringEmote) {
        // Use emote freecams — camera orbits freely during emotes
        targetPos = EmoteFreecam::computeFreeCamPosition(camPos, lookDir, deltaTime);
        targetPitch = EmoteFreecam::getFreeCamPitch();
        targetYaw = EmoteFreecam::getFreeCamYaw();
    } else {
        switch (dyn.getCurrentPerspective()) {
            case PerspectiveMode::FIRST_PERSON:
                targetPos = CameraManager::computeFirstPersonPosition(camPos, lookDir);
                // Apply user-defined offsets
                targetPos.x += config.cameraOffsetX;
                targetPos.y += config.cameraOffsetY;
                targetPos.z += config.cameraOffsetZ;
                break;
                
            case PerspectiveMode::SECOND_PERSON_BACK:
                targetPos = CameraManager::computeSecondPersonBackPosition(
                    camPos, lookDir, config.secondPersonBackAngle, config.secondPersonBackDistance);
                break;
                
            case PerspectiveMode::SECOND_PERSON_FRONT:
                targetPos = CameraManager::computeSecondPersonFrontPosition(
                    camPos, lookDir, config.secondPersonFrontAngle, config.secondPersonFrontDistance);
                break;
        }
    }
    
    // Crosshair aim tracking
    if (config.aimTrackingEnabled && !dyn.isEmoting() || 
        (dyn.isEmoting() && !config.freecamDuringEmote)) {
        Vec3 aimedBlockPos = getRaycastTarget();
        if (aimedBlockPos.length() > 0.0f) {
            Vec3 aimDir = CameraManager::computeLookAtDirection(targetPos, aimedBlockPos);
            Vec3 playerLook = getPlayerLookDirection();
            Vec3 blendedDir = CameraManager::blendLookWithAim(playerLook, aimDir, 
                               config.crosshairLockStrength);
            targetYaw = atan2(blendedDir.x, blendedDir.z);
        }
    }
    
    // Smooth transition to target (configured smoothing speed)
    Vec3 smoothedPos = CameraManager::lerpVec3(
        getCurrentCameraPosition(), targetPos, config.smoothingSpeed);
    
    // Update the actual game camera
    setCameraPosition(smoothedPos);
    setCameraRotationFromYaw(targetYaw);
    
    // Call original camera render with our modified state
    if (orig_cameraRender) {
        orig_cameraRender(camera, deltaTime);
    }
}

// Hooked player model render — renders the full body for first-person view
void hook_renderPlayerModel(void* player, float deltaTime) {
    if (!orig_renderPlayerModel) return;
    
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    
    if (config.firstPersonEnabled && 
        dyn.getCurrentPerspective() == PerspectiveMode::FIRST_PERSON) {
        
        // Temporarily hide player's own head if configured
        if (config.hideOwnHead) {
            setPlayerHeadVisible(false);
        }
        
        // Render full third-person model at adjusted camera offset
        orig_renderPlayerModel(player, deltaTime);
        
        if (config.hideOwnHead) {
            setPlayerHeadVisible(true);
        }
    }
    
    // Always call original to avoid breaking vanilla rendering
    if (orig_renderPlayerModel) {
        orig_renderPlayerModel(player, deltaTime);
    }
}

// Entry point for setting up all hooks
void initHooks() {
    auto &dyn = Dynamicam::instance();
    dyn.getLogger().info("Initializing hooks...");
    
    // Hook eglSwapBuffers for frame interception
    void* egl = dlopen("libEGL.so", RTLD_LAZY);
    if (egl) {
        void* sym = dlsym(egl, "eglSwapBuffers");
        if (sym) {
            orig_EGLSwapBuffers = (EGLBoolean(*)(EGLDisplay, EGLSurface))sym;
            GHook hook(sym, (void*)hook_eglSwapBuffers, (void**)&orig_EGLSwapBuffers);
            if (hook) dyn.getLogger().info("✓ eglSwapBuffers hooked");
        }
        dlclose(egl);
    }
    
    // Hook camera render
    void* mcpe = dlopen("libminecraftpe.so", RTLD_LAZY);
    if (mcpe) {
        // Try direct symbol resolution first
        void* camRender = dlsym(mcpe, "_ZN6Camera6renderEf");
        if (camRender) {
            orig_cameraRender = (Fn_CameraRender_t)camRender;
            GHook hook(camRender, (void*)hook_cameraRender, (void**)&orig_cameraRender);
            if (hook) dyn.getLogger().info("✓ Camera::render hooked");
        }
        
        // Try player model render
        void* playerRender = dlsym(mcpe, "_ZN6Player11renderModelEf");
        if (playerRender) {
            orig_renderPlayerModel = (Fn_RenderPlayerModel_t)playerRender;
            GHook hook(playerRender, (void*)hook_renderPlayerModel, (void**)&orig_renderPlayerModel);
            if (hook) dyn.getLogger().info("✓ Player::renderModel hooked");
        }
        
        dlclose(mcpe);
    }
    
    // If direct symbols fail, fall back to signature scanning
    if (!orig_cameraRender || !orig_renderPlayerModel) {
        dyn.getLogger().info("Falling back to signature scanning...");
        initSignatureScanning();
    }
    
    dyn.getLogger().info("Hook initialization complete");
}

void uninitHooks() {
    auto &dyn = Dynamicam::instance();
    
    // Unhook by restoring originals (done automatically when the hooked function calls orig_XXX)
    // When the .so is unloaded, hooks are removed by the framework
    
    dyn.getLogger().info("Hooks unregistered");
}
```

### `src/pluginkit/PerspectiveSystem.hpp` — Perspective Switching

```cpp
#pragma once
#include <cstdint>

namespace dynamicam {

// Called every frame by the render hook to update perspective-dependent systems
void preRenderFrame(int screenWidth, int screenHeight);

// Internal helpers
void updateCameraTransition();
void handlePerspectiveInput();
bool isPointerDownOnBlock();
Vec3 getRaycastTarget();
Vec3 getPlayerEyePosition();
Vec3 getPlayerLookDirection();
Vec3 getCurrentCameraPosition();
void setCameraPosition(const Vec3& pos);
void setCameraRotationFromYaw(float yaw);
void setPlayerHeadVisible(bool visible);
uint64_t getWorldTime();

} // namespace dynamicam
```

### `src/pluginkit/PerspectiveSystem.cpp`

```cpp
#include "PerspectiveSystem.hpp"
#include "Dynamicam.hpp"
#include "CameraManager.hpp"
#include <cmath>

namespace dynamicam {

static float g_TransitionProgress = 1.0f;  // 1.0 = fully transitioned
static PerspectiveMode g_LastPerspective = PerspectiveMode::FIRST_PERSON;
static float g_TimeSinceSwitch = 0.0f;

void preRenderFrame(int screenWidth, int screenHeight) {
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    
    // Handle smooth transitions when switching perspectives
    if (g_LastPerspective != dyn.getCurrentPerspective()) {
        g_TransitionProgress = 0.0f;
        g_TimeSinceSwitch = 0.0f;
        g_LastPerspective = dyn.getCurrentPerspective();
    }
    
    if (g_TransitionProgress < 1.0f) {
        g_TimeSinceSwitch += 0.016f;  // ~60fps step
        float duration = 1.0f / config.smoothingSpeed;
        g_TransitionProgress = std::min(1.0f, g_TimeSinceSwitch / duration);
    }
    
    // Raycasting for aim tracking
    // This reads from Minecraft's internal raycast result
    // Implementation depends on Minecraft version
}

// Stub implementations — replace with actual game API calls
Vec3 getPlayerEyePosition() {
    // dlsym into Minecraft's PlayerEntity::getPosition() + eye height
    static auto fn = (Vec3(*)())dlsym(dlopen("libminecraftpe.so", RTLD_LAZY),
        "_ZN10PlayerEntity10getPositionEv");
    return fn ? fn() : Vec3{0, 0, 0};
}

Vec3 getPlayerLookDirection() {
    // Read from Camera class yaw/pitch
    static auto fn = (void*(*)(void*, float*, float*))dlsym(dlopen("libminecraftpe.so", RTLD_LAZY),
        "_ZN6Camera11getRotationEPfS0_");
    if (!fn) return Vec3{0, 0, -1};
    
    float pitch, yaw;
    fn(nullptr, &pitch, &yaw);
    
    float radYaw = yaw * M_PI / 180.0f;
    float radPitch = pitch * M_PI / 180.0f;
    
    return {
        -sin(radYaw) * cos(radPitch),
        sin(radPitch),
        -cos(radYaw) * cos(radPitch)
    };
}

Vec3 getCurrentCameraPosition() {
    static auto fn = (void*(*)(void*, float*, float*, float*))dlsym(dlopen("libminecraftpe.so", RTLD_LAZY),
        "_ZN6Camera11getPositionEPfS0_S0_");
    if (!fn) return Vec3{0, 0, 0};
    
    float x, y, z;
    fn(nullptr, &x, &y, &z);
    return {x, y, z};
}

void setCameraPosition(const Vec3& pos) {
    static auto fn = (void*(*)(void*, float, float, float))dlsym(dlopen("libminecraftpe.so", RTLD_LAZY),
        "_ZN6Camera11setPositionEfff");
    if (fn) fn(nullptr, pos.x, pos.y, pos.z);
}

void setCameraRotationFromYaw(float yaw) {
    static auto fn = (void*(*)(void*, float, float))dlsym(dlopen("libminecraftpe.so", RTLD_LAZY),
        "_ZN6Camera11setRotationEff");
    if (fn) fn(nullptr, 0.0f, yaw);
}

void setPlayerHeadVisible(bool visible) {
    // Part of the player model mesh — can toggle visibility
    // Implementation: modify the player's bone matrix or mesh flags
}

bool isPointerDownOnBlock() {
    // Check if the player is currently placing/breaking a block
    // Read from Minecraft's InputHandler state
    return false;
}

Vec3 getRaycastTarget() {
    // Raycast from camera position forward
    // Minecraft has built-in hit detection — read its results
    // This would dlsym into the raycast system
    return Vec3{0, 0, 0};
}

} // namespace dynamicam
```

### `src/pluginkit/EmoteFreecam.hpp` — Freecam During Emotes

```cpp
#pragma once
#include "CameraManager.hpp"

namespace dynamicam {

// When player is emoting, orbit camera around them freely
// The emote animation plays normally, but camera doesn't follow rigidly
class EmoteFreecam {
public:
    static Vec3 computeFreeCamPosition(const Vec3& basePos, const Vec3& lookAt, float deltaTime);
    static float getFreeCamPitch();
    static float getFreeCamYaw();
    
    // Touch/mouse input handlers — update freecams angles
    static void onTouchDrag(float dx, float dy);
    static void reset();
    
private:
    static float s_FreeCamPitch;
    static float s_FreeCamYaw;
    static float s_FreeCamRadius;
    static float s_FreeCamHeight;
};

} // namespace dynamicam
```

### `src/pluginkit/EmoteFreecam.cpp`

```cpp
#include "EmoteFreecam.hpp"
#include <cmath>

namespace dynamicam {

float EmoteFreecam::s_FreeCamPitch = 15.0f;
float EmoteFreecam::s_FreeCamYaw = 0.0f;
float EmoteFreecam::s_FreeCamRadius = 5.0f;
float EmoteFreecam::s_FreeCamHeight = 1.6f;

Vec3 EmoteFreecam::computeFreeCamPosition(const Vec3& basePos, const Vec3&, float deltaTime) {
    // Orbital camera around the player
    float yawRad = s_FreeCamYaw * M_PI / 180.0f;
    float pitchRad = s_FreeCamPitch * M_PI / 180.0f;
    
    Vec3 offset = {
        sin(yawRad) * cos(pitchRad) * s_FreeCamRadius,
        sin(pitchRad) * s_FreeCamRadius + s_FreeCamHeight,
        cos(yawRad) * cos(pitchRad) * s_FreeCamRadius
    };
    
    return basePos + offset;
}

float EmoteFreecam::getFreeCamPitch() {
    return s_FreeCamPitch;
}

float EmoteFreecam::getFreeCamYaw() {
    return s_FreeCamYaw;
}

void EmoteFreecam::onTouchDrag(float dx, float dy) {
    s_FreeCamYaw -= dx * 0.5f;  // Sensitivity
    s_FreeCamPitch += dy * 0.5f;
    s_FreeCamPitch = std::clamp(s_FreeCamPitch, -80.0f, 80.0f);
}

void EmoteFreecam::reset() {
    s_FreeCamPitch = 15.0f;
    s_FreeCamYaw = 0.0f;
}

} // namespace dynamicam
```

### `src/pluginkit/HotItemRender.hpp` — Animated Hand Items

```cpp
#pragma once
#include "CameraManager.hpp"

namespace dynamicam {

// Handles rendering of held items so animations play correctly
// at any camera angle (first person, second person, etc.)
class HotItemRender {
public:
    // Called each frame during the item rendering pass
    static void onPreItemRender(const Vec3& camPos);
    static void onPostItemRender();
    
    // Force redraw of item hand bones
    static void updateHandBones(float deltaTime);
    
private:
    static float s_ItemHeldBobTimer;
};

} // namespace dynamicam
```

### `src/pluginkit/HotItemRender.cpp`

```cpp
#include "HotItemRender.hpp"
#include "Dynamicam.hpp"
#include <cmath>

namespace dynamicam {

float HotItemRender::s_ItemHeldBobTimer = 0.0f;

void HotItemRender::onPreItemRender(const Vec3& camPos) {
    auto &dyn = Dynamicam::instance();
    const auto &config = dyn.getConfig();
    
    if (!config.itemAnimationEnabled) return;
    
    // Item bob animation time (swinging while walking)
    s_ItemHeldBobTimer += 0.016f;
    
    // Pass adjusted item transform to renderer
    // This ensures items swing correctly even in second-person view
    float bob = std::sin(s_ItemHeldBobTimer * 2.0f) * 0.05f;
    
    // Set item hand transforms — depends on Minecraft internals
    // setItemTransform(camPos.x, camPos.y + bob, camPos.z);
}

void HotItemRender::onPostItemRender() {
    // Restore any GL state modified during item rendering
}

void HotItemRender::updateHandBones(float deltaTime) {
    // Update skeleton animation for left/right hands
    // This keeps items animated during all camera modes
}

} // namespace dynamicam
```

### `src/pluginkit/SignatureScan.hpp` — Version Detection

```cpp
#pragma once

// When direct dlsym fails (different MC versions), use pattern scanning
void initSignatureScanning();
```

### `src/pluginkit/SignatureScan.cpp`

```cpp
#include "SignatureScan.hpp"
#include <pl/Hook.hpp>
#include <android/log.h>

#define LOG_TAG "Dynamicam"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// ARM64 memory pattern scanning utilities
// Used when symbol names change between Minecraft versions

static uint8_t* scanPattern(uint8_t* start, uint8_t* end, const char* pattern, const char* mask) {
    size_t patternLen = strlen(mask);
    for (uint8_t* p = start; p < end - patternLen; p++) {
        bool found = true;
        for (size_t i = 0; i < patternLen; i++) {
            if (mask[i] != '?' && pattern[i] != p[i]) {
                found = false;
                break;
            }
        }
        if (found) return p;
    }
    return nullptr;
}

void initSignatureScanning() {
    LOGI("Running signature scans for dynamic hook resolution...");
    
    void* mcpe = dlopen("libminecraftpe.so", RTLD_LAZY);
    if (!mcpe) return;
    
    Dl_info info;
    dladdr(mcpe, &info);
    
    // Example patterns for different Minecraft versions:
    // Camera::render patterns vary per update
    static const char* camPatterns[] = {
        "48 ?? ?? ?? 21 ?? ?? 91 ?? ?? 00 ?? ?? 91 ?? ?? 40 ?? ?? 00 00 ?? ?? 00 94",
    };
    static const char* camMasks[] = {
        "xx????xx????xxxx????xxxx????xx??????xx?",
    };
    
    for (size_t i = 0; i < sizeof(camPatterns)/sizeof(camPatterns[0]); i++) {
        void* match = scanPattern(...);  // Full implementation needed
        if (match) {
            LOGI("Found Camera::render at %p", match);
            break;
        }
    }
    
    dlclose(mcpe);
}
```

---

## 📦 Step 8 — Build & Package

### Build the Mod

```bash
# From the Dynamicam/ directory
./build.sh
```

Or manually:

```bash
mkdir -p build && cd build
cmake .. \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
    -DCMAKE_SYSTEM_VERSION=26 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Output: build/libdynamicam.so
```

### Install to LeviLaunchroid

```bash
# Method 1: Direct file copy
mkdir -p ~/.config/levilaunchroid/mods/dynamicam/
cp dist/arm64-v8a/dynamicam/*.so ~/.config/levilaunchroid/mods/dynamicam/
cp dist/arm64-v8a/dynamicam/manifest.json ~/.config/levilaunchroid/mods/dynamicam/

# Method 2: Import through launcher GUI
# Open LeviLaunchroid -> Settings -> Mods -> Import -> select the dist/arm64-v8a/dynamicam/ folder
```

### Launch & Test

```bash
# Start Minecraft through LeviLaunchroid
# The mod loads automatically on game startup

# In-game:
# 1. Press the Mod Menu button (floating ◉ icon)
# 2. Toggle Dynamicam on/off
# 3. Adjust all settings: offsets, angles, distances, smoothing
# 4. Cycle perspectives with the cycle button
```

---

## 🎨 Step 9 — Add Your Logo

After testing is complete, place your logo image in:

```
Dynamicam/resources/logo.png
```

Make sure it's a PNG, recommended size **64×64** or **128×128** pixels.

The `manifest.json` already references it via `"icon": "resources/logo.png"`.

---

## 🔍 Step 10 — Debugging & Troubleshooting

### View Logcat Output

```bash
adb logcat -s Dynamicam *:S
```

### Common Issues

| Problem | Solution |
|---------|----------|
| Mod not loading | Verify `.so` is in correct mods dir, check `manifest.json` type is `"preload-native"` |
| Game crashes on launch | Hook addresses may differ per MC version — update signatures |
| No body visible | Check `firstPersonEnabled` is `true`, verify model scale isn't 0 |
| Camera jittery | Increase `smoothingSpeed` (more toward 1.0 = faster, less toward 0.01 = slower) |
| Second person shows nothing | Verify the player model render hook (`renderModel`) is resolving correctly |

### Advanced Debugging

```bash
# Enable verbose logging
export ANDROID_LOG_TAGS=Dynamicam:D
adb logcat -s Dynamicam

# Attach debugger via Android Studio
# Set breakpoints in your C++ code
```

---

## Final Directory Tree

```
Dynamicam/
├── CMakeLists.txt
├── manifest.json
├── build.sh
├── config/
│   ├── config.json
│   └── config.schema.json
├── resources/
│   └── logo.png          <- Add your logo here
├── src/
│   ├── main.cpp
│   └── pluginkit/
│       ├── Dynamicam.hpp
│       ├── Dynamicam.cpp
│       ├── CameraManager.hpp
│       ├── CameraManager.cpp
│       ├── RenderHooks.hpp
│       ├── RenderHooks.cpp
│       ├── PerspectiveSystem.hpp
│       ├── PerspectiveSystem.cpp
│       ├── EmoteFreecam.hpp
│       ├── EmoteFreecam.cpp
│       ├── HotItemRender.hpp
│       ├── HotItemRender.cpp
│       ├── SignatureScan.hpp
│       └── SignatureScan.cpp
└── build/                 <- Generated
    └── libdynamicam.so    <- Compiled output
```

---

## Distribution

When ready to release:

1. **Create a `.zip` package**:
   ```bash
   zip -r Dynamicam_v1.0.0.zip dist/arm64-v8a/dynamicam/
   ```

2. **Package as `.levipack`** (preferred):
   ```bash
   cd dist/arm64-v8a/dynamicam/
   zip -r ../Dynamicam_v1.0.0.levipack *
   cd ../../..
   ```

3. **Upload to forums/discord** — Players import through LeviLaunchroid settings

---

## Notes on Implementation

This guide provides the **complete architecture and scaffold code**. However, there are some areas marked as stubs that need real reverse-engineered addresses for your specific Minecraft Bedrock version:

1. **`getPlayerEyePosition()`** — needs the actual C++ method address from `libminecraftpe.so`
2. **`getPlayerLookDirection()`** — needs Camera's yaw/pitch accessor
3. **`setCameraPosition()`** / **`setCameraRotationFromYaw()`** — needs Camera mutator addresses
4. **`setPlayerHeadVisible()`** — needs skeletal mesh access
5. **`getRaycastTarget()`** — needs Minecraft hit-detection result
6. **Signatures** — ARM64 instruction patterns change between Minecraft updates

The existing [FirstPersonModel for LeviLaunchroid](https://github.com/RealBatu20/FirstPersonModel) repo provides concrete dlsym addresses you can adapt. For 1.26.5X.XX specifically, you'll need to:
- Dump `libminecraftpe.so` symbols for your version
- Verify which Camera/Player methods exist
- Update the dlsym strings accordingly

---

## Credits

- **Developer:** Sw1ghttt
- **Inspired by:** [FirstPersonModel for LeviLaunchroid](https://github.com/RealBatu20/FirstPersonModel)
- **SDK:** [LiteLDev/preloader-android](https://github.com/LiteLDev/preloader-android)
- **Launcher:** [LeviLaunchroid](https://github.com/LiteLDev/levilaunchroid)
