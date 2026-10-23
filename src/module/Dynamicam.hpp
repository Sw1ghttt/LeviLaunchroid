#pragma once

#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/Config.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <mutex>
#include <atomic>

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
    friend struct pl::mod::Registrar;
    Dynamicam() : mSelf(*ll::mod::NativeMod::current()) {}

    ll::mod::NativeMod &mSelf;
    std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<Config>> mConfigFile;
    PerspectiveMode mCurrentPerspective = PerspectiveMode::FIRST_PERSON;
    std::atomic<bool> mIsEmoting{false};
    bool mGameInitialized = false;

    void handleConfigChanged(std::string_view moduleId, std::string_view key, std::string_view value);
    void handleModuleToggle(std::string_view moduleId, bool enabled);
};

} // namespace dynamicam
