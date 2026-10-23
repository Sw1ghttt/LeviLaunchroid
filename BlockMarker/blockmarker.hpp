#pragma once

#include "Module.hpp"
#include <bedrocktools/sdk/Types.hpp>
#include <bedrocktools/sdk/world/Actor.hpp>
#include <mutex>
#include <string>
#include <vector>

struct MarkerEntry {
    std::string name;
    int x = 0, y = 0, z = 0;   // the block cell that is highlighted
    int dim = 0;               // 0 overworld, 1 nether, 2 end
    uint32_t color = 0xFFFF3030; // ARGB
};

class BlockMarkerModule : public Module {
public:
    BlockMarkerModule();
    ~BlockMarkerModule() override;

    void onInit() override;
    void onEnable() override;
    void onDisable() override;
    void onFrame() override;
    bool onMenuConfigChanged(std::string_view key, std::string_view value) override;
    void loadConfig(const nlohmann::json& j) override;
    void saveConfig(nlohmann::json& j) override;

    // called by the tick / render code
    void onTick(bedrocktools::sdk::Player* player);
    std::vector<MarkerEntry> snapshotMarkers();
    int currentDim() const { return m_dim; }

    // settings used by the renderer
    bool m_showMarkers = true;
    float m_thickness = 2.0f;     // 1..5 nested outlines
    float m_opacity = 1.0f;
    int m_renderDistance = 64;    // blocks, 0 = unlimited
    bool m_topCross = true;       // draw an X on the top face so it is easy to see
    bool m_currentDimensionOnly = true;

private:
    void placeMarker();
    void removeMarkerUnderPlayer();
    void notify(const std::string& msg);
    bool blockUnderPlayer(int& x, int& y, int& z) const;

    std::mutex m_lock;
    std::vector<MarkerEntry> m_markers;

    // live player state
    bool m_valid = false;
    bedrocktools::sdk::Vec3 m_pos{0, 0, 0};
    bedrocktools::sdk::AABB m_box{};
    float m_yaw = 0.f;
    int m_dim = 0;

    std::string m_toast;
    double m_toastUntil = 0.0;

    // HUD placement (draggable in the HUD editor)
    float hudPosX = 20.f;
    float hudPosY = 90.f;
    bool isHudModule = true;

    // settings shown in the Mod Menu
    std::string m_markerName = "Trap";
    std::string m_markerColor = "#FFFF3030";
    int m_selectNumber = 1;       // list number used by the "selected" buttons
    bool m_showList = true;
    int m_listSize = 6;           // 1..10
    bool m_showDistance = true;
    float m_scale = 1.0f;
    bool m_confirmClear = false;
};
