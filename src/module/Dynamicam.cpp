#include "Dynamicam.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace dynamicam {

using DynCfg = Config;
using DynMode = PerspectiveMode;

// Defined in RenderHooks.cpp
void initHooks();
void uninitHooks();

constexpr const char *kModuleId = "dynamicam.main";
constexpr const char *kCycleButtonId = "dynamicam.cycle_pov";

// SVG icon for the perspective cycle button (camera lens design)
[[maybe_unused]] static constexpr char kLogoSvg[] = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
    <rect width="64" height="64" rx="12" fill="#1A1A2E"/>
    <circle cx="32" cy="28" r="12" fill="none" stroke="#E94560" stroke-width="3"/>
    <circle cx="32" cy="28" r="5" fill="#E94560"/>
    <path d="M20 48 Q32 56 44 48" stroke="#E94560" stroke-width="2" fill="none"/>
</svg>)svg";

static std::string boolToMenuValue(bool value) { return value ? "true" : "false"; }

static int modeToIndex(PerspectiveMode mode) {
    switch (mode) {
        case PerspectiveMode::FIRST_PERSON:     return 0;
        case PerspectiveMode::SECOND_PERSON_BACK:   return 1;
        case PerspectiveMode::SECOND_PERSON_FRONT:  return 2;
    }
    return 0;
}

static PerspectiveMode modeFromIndex(int index, PerspectiveMode fallback) {
    switch (index) {
        case 0: return PerspectiveMode::FIRST_PERSON;
        case 1: return PerspectiveMode::SECOND_PERSON_BACK;
        case 2: return PerspectiveMode::SECOND_PERSON_FRONT;
        default: return fallback;
    }
}

static std::string doubleToMenuValue(double value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

static int parseInt(std::string_view value, int fallback) {
    std::string text(value);
    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

static double parseDouble(std::string_view value, double fallback) {
    std::string text(value);
    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return fallback;
    }
    return parsed;
}

static bool parseBool(std::string_view value, bool fallback) {
    if (value == "true" || value == "1" || value == "on" || value == "enabled") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off" || value == "disabled") {
        return false;
    }
    return fallback;
}

static void normalizeConfig(Config &config) {
    config.cameraOffsetX = std::clamp(config.cameraOffsetX, -2.0f, 2.0f);
    config.cameraOffsetY = std::clamp(config.cameraOffsetY, -2.0f, 2.0f);
    config.cameraOffsetZ = std::clamp(config.cameraOffsetZ, -2.0f, 2.0f);
    config.modelScale = std::clamp(config.modelScale, 0.5f, 2.0f);
    config.secondPersonBackAngle = std::clamp(config.secondPersonBackAngle, -45.0f, 45.0f);
    config.secondPersonBackDistance = std::clamp(config.secondPersonBackDistance, 2.0f, 10.0f);
    config.secondPersonFrontAngle = std::clamp(config.secondPersonFrontAngle, -45.0f, 45.0f);
    config.secondPersonFrontDistance = std::clamp(config.secondPersonFrontDistance, 2.0f, 10.0f);
    config.crosshairLockStrength = std::clamp(config.crosshairLockStrength, 0.0f, 1.0f);
    config.smoothingSpeed = std::clamp(config.smoothingSpeed, 0.01f, 1.0f);
    config.perspectiveMode = modeFromIndex(modeToIndex(config.perspectiveMode), PerspectiveMode::FIRST_PERSON);
}

// --- Static accessors used by render hooks ---

void Dynamicam::setPerspective(PerspectiveMode mode) {
    mCurrentPerspective = mode;
}

PerspectiveMode Dynamicam::getCurrentPerspective() const {
    return mCurrentPerspective;
}

void Dynamicam::setEmotingState(bool emoting) {
    mIsEmoting.store(emoting, std::memory_order_relaxed);
}

bool Dynamicam::isEmoting() const {
    return mIsEmoting.load(std::memory_order_relaxed);
}

// --- Lifecycle methods ---

bool Dynamicam::load() {
    auto &self = getSelf();
    std::lock_guard lock(mConfigMutex);

    self.getLogger().info("Loading {}", self.getName());

    // Create config dir
    std::filesystem::create_directories(self.getConfigDir());

    // Load config
    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        self.getLogger().warn("Failed to load config, using defaults");
        Config defaults{};
        mConfigFile->value() = defaults;
        mConfigFile->save();
    }

    normalizeConfig(mConfigFile->value());
    mConfigFile->save();

    self.getLogger().info("Loaded {} v{}", self.getName(), self.getVersion());
    return true;
}

bool Dynamicam::enable() {
    auto &self = getSelf();
    const auto &config = mConfigFile->value();

    self.getLogger().info("Enabling {}...", self.getName());

    // Register Mod Menu HUD module with all configuration entries
    const bool moduleOk = pl::modmenu::ModuleBuilder(kModuleId, "Dynamicam")
        .modId(self.getId())
        .description("Camera perspectives & first-person body view. By Sw1ghttt")
        .defaultEnabled(true)
        // Toggle: enable/disable first person body
        .config("firstPersonEnabled", "Enable First Person Body",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.firstPersonEnabled))
        // Sliders: camera offsets
        .config("camera_offset_x", "X Offset",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.cameraOffsetX)),
                "-2.0", "2.0")
        .config("camera_offset_y", "Y Offset",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.cameraOffsetY)),
                "-2.0", "2.0")
        .config("camera_offset_z", "Z Offset",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.cameraOffsetZ)),
                "-2.0", "2.0")
        // Slider: model scale
        .config("model_scale", "Model Scale",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.modelScale)),
                "0.5", "2.0")
        // Toggle: hide own head
        .config("hideOwnHead", "Hide Own Head",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.hideOwnHead))
        // Sliders: second person back
        .config("secondPersonBack_Angle", "2P Back Angle",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.secondPersonBackAngle)),
                "-45.0", "45.0")
        .config("secondPersonBack_Distance", "2P Back Distance",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.secondPersonBackDistance)),
                "2.0", "10.0")
        // Sliders: second person front
        .config("secondPersonFront_Angle", "2P Front Angle",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.secondPersonFrontAngle)),
                "-45.0", "45.0")
        .config("secondPersonFront_Distance", "2P Front Distance",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.secondPersonFrontDistance)),
                "2.0", "10.0")
        // Toggles
        .config("freecamDuringEmote", "Freecam During Emotes",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.freecamDuringEmote))
        .config("aimTrackingEnabled", "Crosshair Aim Tracking",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.aimTrackingEnabled))
        .config("itemAnimationEnabled", "Animated Items In Hands",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.itemAnimationEnabled))
        // Sliders: aim & smoothness
        .config("crosshairLockStrength", "Aim Lock Strength",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.crosshairLockStrength)),
                "0.0", "1.0")
        .config("smoothingSpeed", "Switch Smoothness",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(static_cast<double>(config.smoothingSpeed)),
                "0.01", "1.0")
        // Radio: perspective mode
        .config("perspectiveMode", "Default Perspective",
                pl::modmenu::ConfigType::Radio,
                std::to_string(modeToIndex(config.perspectiveMode)),
                "First Person,Second Person Back,Second Person Front")
        // Callbacks
        .onConfigChanged([](std::string_view moduleId, std::string_view key, std::string_view value) {
            instance().handleConfigChanged(moduleId, key, value);
        })
        .onToggle([](std::string_view moduleId, bool enabled) {
            instance().handleModuleToggle(moduleId, enabled);
        })
        .registerModule();

    // Register floating button to cycle perspective
    const bool buttonOk = pl::modmenu::ButtonBuilder(kCycleButtonId, "Cycle Perspective")
        .moduleId(kModuleId)
        .label("\u25CE")
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
        .textColor(0xFFFFFFFFU)
        .activeTextColor(0xFF000000U)
        .onEvent([this](std::string_view buttonId, pl::modmenu::ButtonEvent event, float value) {
            if (event == pl::modmenu::ButtonEvent::Down) {
                int nextIdx = modeToIndex(mCurrentPerspective) + 1;
                if (nextIdx > 2) nextIdx = 0;
                setPerspective(modeFromIndex(nextIdx, getCurrentPerspective()));
                mConfigFile->value().perspectiveMode = getCurrentPerspective();
                mConfigFile->save();
                getSelf().getLogger().info("Perspective -> {}",
                    static_cast<int>(getCurrentPerspective()));
            }
        })
        .registerButton();

    if (moduleOk && buttonOk) {
        self.getLogger().info("Mod Menu registered successfully");
    } else {
        self.getLogger().error("Failed to register Mod Menu (module={} button={})", moduleOk, buttonOk);
        return false;
    }

    // Initialize game hooks (defined in RenderHooks.cpp)
    initHooks();

    return true;
}

bool Dynamicam::disable() {
    // Unregister Mod Menu entries
    pl::modmenu::unregisterModule(kModuleId);
    pl::modmenu::unregisterButton(kCycleButtonId);
    uninitHooks();
    getSelf().getLogger().info("Disabled");
    return true;
}

bool Dynamicam::unload() {
    {
        std::lock_guard lock(mConfigMutex);
        mConfigFile.reset();
    }
    getSelf().getLogger().info("Unloaded");
    return true;
}

// --- Config change handlers ---

void Dynamicam::handleConfigChanged(std::string_view moduleId, std::string_view key, std::string_view value) {
    if (moduleId != kModuleId || key.empty()) return;

    std::lock_guard lock(mConfigMutex);
    if (!mConfigFile) return;

    Config &cfg = mConfigFile->value();

    if (key == "firstPersonEnabled") {
        cfg.firstPersonEnabled = parseBool(value, cfg.firstPersonEnabled);
    } else if (key == "camera_offset_x") {
        cfg.cameraOffsetX = static_cast<float>(parseDouble(value, cfg.cameraOffsetX));
    } else if (key == "camera_offset_y") {
        cfg.cameraOffsetY = static_cast<float>(parseDouble(value, cfg.cameraOffsetY));
    } else if (key == "camera_offset_z") {
        cfg.cameraOffsetZ = static_cast<float>(parseDouble(value, cfg.cameraOffsetZ));
    } else if (key == "model_scale") {
        cfg.modelScale = static_cast<float>(parseDouble(value, cfg.modelScale));
    } else if (key == "hideOwnHead") {
        cfg.hideOwnHead = parseBool(value, cfg.hideOwnHead);
    } else if (key == "secondPersonBack_Angle") {
        cfg.secondPersonBackAngle = static_cast<float>(parseDouble(value, cfg.secondPersonBackAngle));
    } else if (key == "secondPersonBack_Distance") {
        cfg.secondPersonBackDistance = static_cast<float>(parseDouble(value, cfg.secondPersonBackDistance));
    } else if (key == "secondPersonFront_Angle") {
        cfg.secondPersonFrontAngle = static_cast<float>(parseDouble(value, cfg.secondPersonFrontAngle));
    } else if (key == "secondPersonFront_Distance") {
        cfg.secondPersonFrontDistance = static_cast<float>(parseDouble(value, cfg.secondPersonFrontDistance));
    } else if (key == "freecamDuringEmote") {
        cfg.freecamDuringEmote = parseBool(value, cfg.freecamDuringEmote);
    } else if (key == "aimTrackingEnabled") {
        cfg.aimTrackingEnabled = parseBool(value, cfg.aimTrackingEnabled);
    } else if (key == "itemAnimationEnabled") {
        cfg.itemAnimationEnabled = parseBool(value, cfg.itemAnimationEnabled);
    } else if (key == "crosshairLockStrength") {
        cfg.crosshairLockStrength = static_cast<float>(parseDouble(value, cfg.crosshairLockStrength));
    } else if (key == "smoothingSpeed") {
        cfg.smoothingSpeed = static_cast<float>(parseDouble(value, cfg.smoothingSpeed));
    } else if (key == "perspectiveMode") {
        int idx = parseInt(value, modeToIndex(cfg.perspectiveMode));
        cfg.perspectiveMode = modeFromIndex(idx, getCurrentPerspective());
    } else {
        return;
    }

    normalizeConfig(cfg);
    mConfigFile->save();
}

void Dynamicam::handleModuleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId) return;
    getSelf().getLogger().info("Module {} -> {}", std::string(moduleId), enabled ? "ON" : "OFF");
}

} // namespace dynamicam
