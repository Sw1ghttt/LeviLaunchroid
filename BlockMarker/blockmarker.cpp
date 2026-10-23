#include "blockmarker.hpp"
#include "ModuleRegistry.hpp"
#include <bedrocktools/memory/Signatures.hpp>
#include "core/memory/Hooks.hpp"
#include <bedrocktools/sdk/Memory.hpp>
#include <bedrocktools/sdk/Offsets.hpp>
#include <bedrocktools/events/EventBus.hpp>
#include <bedrocktools/sdk/world/Dimension.hpp>
#include <bedrocktools/sdk/world/BlockSource.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

// ---------------------------------------------------------------------------
// Rendering plumbing (same proven approach BedrockTools' Light Overlay uses)
// ---------------------------------------------------------------------------
typedef void (*Tessellator_begin_t)(void* tessellator, void* debugCallback, int primitiveMode, int vertexCount, int noIndices);
typedef void (*Tessellator_color_t)(void* tessellator, float r, float g, float b, float a);
typedef void (*Tessellator_vertex_t)(void* tessellator, float x, float y, float z);
typedef void (*MeshHelpers_renderMeshImmediately_t)(void* screenContext, void* tessellator, void* material, char* pad);

struct HashedString {
    uint64_t mStrHash;
    std::string mStr;
    mutable const HashedString* mLastMatch;
    HashedString() : mStrHash(0), mStr(), mLastMatch(nullptr) {}
    explicit HashedString(const char* str) : mLastMatch(nullptr) {
        mStr = str ? str : "";
        mStrHash = computeHash(mStr);
    }
private:
    static uint64_t computeHash(const std::string& str) {
        if (str.empty()) return 0;
        constexpr uint64_t kOffset = 0xCBF29CE484222325ULL;
        constexpr uint64_t kPrime = 0x100000001B3ULL;
        uint64_t hash = kOffset;
        for (char ch : str) hash = static_cast<uint64_t>(static_cast<unsigned char>(ch)) ^ (kPrime * hash);
        return hash;
    }
};

struct MaterialPtr {
    void* sharedPtrData[2]{nullptr, nullptr};
    MaterialPtr() = default;
    MaterialPtr(const MaterialPtr&) = delete;
    MaterialPtr& operator=(const MaterialPtr&) = delete;
    MaterialPtr(MaterialPtr&& o) noexcept : sharedPtrData{o.sharedPtrData[0], o.sharedPtrData[1]} {
        o.sharedPtrData[0] = nullptr; o.sharedPtrData[1] = nullptr;
    }
    MaterialPtr& operator=(MaterialPtr&& o) noexcept {
        if (this != &o) {
            sharedPtrData[0] = o.sharedPtrData[0]; sharedPtrData[1] = o.sharedPtrData[1];
            o.sharedPtrData[0] = nullptr; o.sharedPtrData[1] = nullptr;
        }
        return *this;
    }
    ~MaterialPtr() {}
    explicit operator bool() const { return sharedPtrData[0] != nullptr; }
};

static uintptr_t resolveADRP(uint32_t* insns, size_t count, uint32_t targetReg) {
    for (size_t i = 0; i < count; i++) {
        uint32_t insn = insns[i];
        if ((insn & 0x1F) != targetReg) continue;
        if ((insn & 0x9F000000) == 0x90000000) {
            uintptr_t page = ((uintptr_t)&insns[i] & ~0xFFFULL)
                           + ((int64_t)((uint64_t)((insn >> 3) & 0x1FFFFC | (insn >> 29) & 3) << 43) >> 31);
            for (size_t j = i + 1; j < count; j++) {
                uint32_t add = insns[j];
                if ((add & 0xFF000000) == 0x91000000 &&
                    ((add >> 5) & 0x1F) == targetReg &&
                    (add & 0x1F) == targetReg) {
                    uint32_t imm12 = (add >> 10) & 0xFFF;
                    if (add & 0x400000) imm12 <<= 12;
                    return page + imm12;
                }
                if ((add & 0x1F) == targetReg) break;
            }
        }
        if ((insn & 0x9F000000) == 0x10000000) {
            int64_t imm = (int64_t)((uint64_t)((insn >> 3) & 0x1FFFFC | (insn >> 29)) << 43) >> 43;
            return (uintptr_t)&insns[i] + imm;
        }
    }
    return 0;
}

static BlockMarkerModule* g_bm = nullptr;

static Tessellator_begin_t s_tessBegin = nullptr;
static Tessellator_color_t s_tessColor = nullptr;
static Tessellator_vertex_t s_tessVertex = nullptr;
static MeshHelpers_renderMeshImmediately_t s_renderMesh = nullptr;
static MaterialPtr s_matSelection;
static uintptr_t s_renderMaterialGroup = 0;
static void (*_renderLevel_orig)(void* _this, void* screenContext, void* a3) = nullptr;

static MaterialPtr getMaterial(const char* name) {
    if (!s_renderMaterialGroup) return {};
    HashedString hs(name);
    void** vtable = *reinterpret_cast<void***>(s_renderMaterialGroup);
    if (!vtable || !vtable[bedrocktools::sdk::offsets::VTable::RenderMaterialGroup_getMaterial]) return {};
    using getMat_t = MaterialPtr (*)(void*, const HashedString*);
    return reinterpret_cast<getMat_t>(vtable[bedrocktools::sdk::offsets::VTable::RenderMaterialGroup_getMaterial])((void*)s_renderMaterialGroup, &hs);
}

static void ensureMaterials() {
    if (s_matSelection || !s_renderMaterialGroup) return;
    s_matSelection = getMaterial("selection_box");
}

static double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static uint32_t parseColor(const std::string& s, uint32_t fallback) {
    if (s.size() < 2 || s[0] != '#') return fallback;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str() + 1, &end, 16);
    if (end == s.c_str() + 1) return fallback;
    if (s.size() == 7) return 0xFF000000u | (uint32_t)v;
    if (s.size() == 9) return (uint32_t)v;
    return fallback;
}

static std::string colorToString(uint32_t c) {
    char b[16];
    std::snprintf(b, sizeof b, "#%08X", c);
    return b;
}

static std::string distText(double d) {
    char b[24];
    if (d >= 1000.0) std::snprintf(b, sizeof b, "%.1fkm", d / 1000.0);
    else std::snprintf(b, sizeof b, "%dm", (int)std::lround(d));
    return b;
}

static uint32_t withAlpha(uint32_t argb, float a) {
    uint32_t al = (uint32_t)(std::max(0.f, std::min(1.f, a)) * (float)((argb >> 24) & 0xFF));
    return (al << 24) | (argb & 0x00FFFFFFu);
}

static float wrap180(float a) {
    a = std::fmod(a + 180.f, 360.f);
    if (a < 0.f) a += 360.f;
    return a - 180.f;
}

// Draws one glowing wireframe box (plus optional X on top) around block cell (x,y,z).
// Coordinates are camera-relative like the rest of the world mesh.
static void emitBox(void* t, float ox, float oy, float oz, float lo, float hi, bool topCross) {
    auto V = [&](float x, float y, float z) { s_tessVertex(t, ox + x, oy + y, oz + z); };
    auto L = [&](float x1, float y1, float z1, float x2, float y2, float z2) { V(x1, y1, z1); V(x2, y2, z2); };
    // bottom ring
    L(lo, lo, lo, hi, lo, lo); L(hi, lo, lo, hi, lo, hi); L(hi, lo, hi, lo, lo, hi); L(lo, lo, hi, lo, lo, lo);
    // top ring
    L(lo, hi, lo, hi, hi, lo); L(hi, hi, lo, hi, hi, hi); L(hi, hi, hi, lo, hi, hi); L(lo, hi, hi, lo, hi, lo);
    // verticals
    L(lo, lo, lo, lo, hi, lo); L(hi, lo, lo, hi, hi, lo); L(hi, lo, hi, hi, hi, hi); L(lo, lo, hi, lo, hi, hi);
    if (topCross) {
        L(lo, hi, lo, hi, hi, hi);
        L(hi, hi, lo, lo, hi, hi);
    }
}

static void _renderLevel_hook(void* _this, void* screenContext, void* a3) {
    if (g_bm && g_bm->enabled && g_bm->m_showMarkers && s_tessBegin && s_tessColor && s_tessVertex && s_renderMesh) {
        auto markers = g_bm->snapshotMarkers();
        if (!markers.empty()) {
            uintptr_t tessPtr = *(uintptr_t*)((uintptr_t)screenContext + bedrocktools::sdk::offsets::ScreenContext::mTessellator);
            uintptr_t lrpPtr = *(uintptr_t*)((uintptr_t)_this + bedrocktools::sdk::offsets::LevelRenderer::mLevelRendererPlayer);
            uintptr_t holderPtr = *(uintptr_t*)((uintptr_t)screenContext + bedrocktools::sdk::offsets::ScreenContext::mColorHolder);
            if (tessPtr >= 0x1000 && lrpPtr >= 0x1000 && holderPtr >= 0x1000) {
                void* tess = (void*)tessPtr;
                float camX = *(float*)(lrpPtr + bedrocktools::sdk::offsets::LevelRendererPlayer::mCamPos);
                float camY = *(float*)(lrpPtr + bedrocktools::sdk::offsets::LevelRendererPlayer::mCamPos + 4);
                float camZ = *(float*)(lrpPtr + bedrocktools::sdk::offsets::LevelRendererPlayer::mCamPos + 8);
                ensureMaterials();
                void* mat = s_matSelection ? (void*)&s_matSelection
                                           : (void*)(lrpPtr + bedrocktools::sdk::offsets::LevelRendererPlayer::mSelectionOverlayMaterial);

                float* holder = (float*)holderPtr;
                float saved[4] = {holder[0], holder[1], holder[2], holder[3]};
                holder[0] = holder[1] = holder[2] = holder[3] = 1.0f;

                const int layers = std::max(1, std::min(5, (int)std::lround(g_bm->m_thickness)));
                const float op = std::max(0.f, std::min(1.f, g_bm->m_opacity));
                const int curDim = g_bm->currentDim();
                bool any = false;

                s_tessBegin(tess, nullptr, 4, 0, 0);
                for (auto& m : markers) {
                    if (g_bm->m_currentDimensionOnly && m.dim != curDim) continue;
                    float r = ((m.color >> 16) & 0xFF) / 255.f;
                    float g = ((m.color >> 8) & 0xFF) / 255.f;
                    float b = (m.color & 0xFF) / 255.f;
                    float a = ((m.color >> 24) & 0xFF) / 255.f * op;
                    s_tessColor(tess, r, g, b, a);
                    for (int k = 0; k < layers; ++k) {
                        float e = 0.004f + 0.012f * k;     // grow outward so layers look thicker
                        emitBox(tess, (float)m.x - camX, (float)m.y - camY, (float)m.z - camZ, -e, 1.f + e, k == 0 && g_bm->m_topCross);
                    }
                    any = true;
                }
                if (any) {
                    char pad[0x58];
                    std::memset(pad, 0, sizeof pad);
                    s_renderMesh(screenContext, tess, mat, pad);
                }
                holder[0] = saved[0]; holder[1] = saved[1]; holder[2] = saved[2]; holder[3] = saved[3];
            }
        }
    }
    if (_renderLevel_orig) _renderLevel_orig(_this, screenContext, a3);
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
BlockMarkerModule::BlockMarkerModule()
    : Module("Block Marker", "Highlight the block under you with a custom name and color. Great for marking traps.") {
    g_bm = this;
}

BlockMarkerModule::~BlockMarkerModule() {
    if (g_bm == this) g_bm = nullptr;
}

static bool s_hookInstalled = false;
static void* s_patchTarget = nullptr;

void BlockMarkerModule::onInit() {
    using namespace bedrocktools::memory;
    if (uintptr_t a = resolve(SignatureId::RenderLevel)) s_patchTarget = (void*)a;
    if (uintptr_t a = resolve(SignatureId::TessellatorBegin)) s_tessBegin = (Tessellator_begin_t)a;
    if (uintptr_t a = resolve(SignatureId::TessellatorColor)) s_tessColor = (Tessellator_color_t)a;
    if (uintptr_t a = resolve(SignatureId::TessellatorVertex)) s_tessVertex = (Tessellator_vertex_t)a;
    if (uintptr_t a = resolve(SignatureId::MeshHelpersRenderMeshImmediately2)) s_renderMesh = (MeshHelpers_renderMeshImmediately_t)a;
    else if (uintptr_t b = resolve(SignatureId::MeshHelpersRenderMeshImmediately)) s_renderMesh = (MeshHelpers_renderMeshImmediately_t)b;
    if (uintptr_t rmg = resolve(SignatureId::RenderMaterialGroupCommon)) {
        uintptr_t groupAddr = resolveADRP(reinterpret_cast<uint32_t*>(rmg), 2, 0);
        if (groupAddr) s_renderMaterialGroup = groupAddr + bedrocktools::sdk::offsets::MaterialGroup::mRenderMaterialGroupOffset;
    }
    bedrocktools::events::bus().subscribe<bedrocktools::events::LocalPlayerTickEvent>([](auto& event) {
        if (g_bm) g_bm->onTick(event.player);
    });
}

void BlockMarkerModule::onEnable() {
    if (!s_hookInstalled && s_patchTarget) {
        bedrocktools::hooks::install(s_patchTarget, (void*)_renderLevel_hook, (void**)&_renderLevel_orig);
        s_hookInstalled = true;
    }
}

void BlockMarkerModule::onDisable() {}

void BlockMarkerModule::onTick(bedrocktools::sdk::Player* player) {
    if (!player) { m_valid = false; return; }
    m_pos = player->position();
    m_box = player->bounds();
    m_yaw = player->rotation().y;
    int dim = 0;
    if (auto* d = player->dimension()) {
        if (auto* bs = d->blockSource()) dim = bs->dimensionId();
    }
    m_dim = (dim >= 0 && dim <= 2) ? dim : 0;
    m_valid = true;
}

std::vector<MarkerEntry> BlockMarkerModule::snapshotMarkers() {
    std::lock_guard<std::mutex> g(m_lock);
    return m_markers;
}

void BlockMarkerModule::notify(const std::string& msg) {
    m_toast = msg;
    m_toastUntil = nowSec() + 3.0;
}

// The block cell directly under the player's feet.
bool BlockMarkerModule::blockUnderPlayer(int& x, int& y, int& z) const {
    if (!m_valid) return false;
    const auto& b = m_box;
    float feetY, cx, cz;
    if (b.max.y > b.min.y && (b.max.x > b.min.x)) { // valid hitbox: use its bottom, which is the feet
        feetY = b.min.y;
        cx = (b.min.x + b.max.x) * 0.5f;
        cz = (b.min.z + b.max.z) * 0.5f;
    } else {                                         // fallback: position is eye level
        feetY = m_pos.y - 1.62f;
        cx = m_pos.x; cz = m_pos.z;
    }
    x = (int)std::floor(cx);
    z = (int)std::floor(cz);
    y = (int)std::floor(feetY - 0.01f);
    return true;
}

void BlockMarkerModule::placeMarker() {
    int x, y, z;
    if (!blockUnderPlayer(x, y, z)) { notify("Block Marker: join a world first"); return; }
    std::string nm = m_markerName;
    size_t b0 = nm.find_first_not_of(" \t");
    size_t b1 = nm.find_last_not_of(" \t");
    nm = (b0 == std::string::npos) ? std::string() : nm.substr(b0, b1 - b0 + 1);
    uint32_t col = parseColor(m_markerColor, 0xFFFF3030u);
    std::lock_guard<std::mutex> g(m_lock);
    for (auto& m : m_markers) {
        if (m.x == x && m.y == y && m.z == z && m.dim == m_dim) { // already marked: update it
            if (!nm.empty()) m.name = nm;
            m.color = col;
            notify("Updated: " + m.name);
            return;
        }
    }
    if (m_markers.size() >= 200) { notify("Marker limit reached (200)"); return; }
    MarkerEntry e;
    e.name = nm.empty() ? ("Marker " + std::to_string(m_markers.size() + 1)) : nm;
    e.x = x; e.y = y; e.z = z; e.dim = m_dim; e.color = col;
    m_markers.push_back(e);
    notify("Marked: " + e.name);
}

void BlockMarkerModule::removeMarkerUnderPlayer() {
    int x, y, z;
    if (!blockUnderPlayer(x, y, z)) { notify("Block Marker: join a world first"); return; }
    std::lock_guard<std::mutex> g(m_lock);
    for (size_t i = 0; i < m_markers.size(); ++i) {
        auto& m = m_markers[i];
        if (m.x == x && m.y == y && m.z == z && m.dim == m_dim) {
            notify("Removed: " + m.name);
            m_markers.erase(m_markers.begin() + (long)i);
            return;
        }
    }
    notify("No marker under you");
}

bool BlockMarkerModule::onMenuConfigChanged(std::string_view key, std::string_view value) {
    if (key == "placeMarkerButton") { placeMarker(); return true; }
    if (key == "removeMarkerButton") { removeMarkerUnderPlayer(); return true; }
    if (key == "removeSelectedButton" || key == "applyToSelectedButton") {
        std::lock_guard<std::mutex> g(m_lock);
        int i = m_selectNumber - 1;
        if (i < 0 || i >= (int)m_markers.size()) { notify("No marker with that number"); return true; }
        if (key == "removeSelectedButton") {
            notify("Removed: " + m_markers[i].name);
            m_markers.erase(m_markers.begin() + i);
        } else {
            std::string nm = m_markerName;
            size_t b0 = nm.find_first_not_of(" \t"), b1 = nm.find_last_not_of(" \t");
            if (b0 != std::string::npos) m_markers[i].name = nm.substr(b0, b1 - b0 + 1);
            m_markers[i].color = parseColor(m_markerColor, m_markers[i].color);
            notify("Updated: " + m_markers[i].name);
        }
        return true;
    }
    if (key == "clearAllButton") {
        if (!m_confirmClear) { notify("Turn on Confirm Clear first"); return true; }
        std::lock_guard<std::mutex> g(m_lock);
        m_markers.clear();
        m_confirmClear = false;
        notify("All markers cleared");
        return true;
    }
    return false;
}

void BlockMarkerModule::onFrame() {
    if (!enabled) return;
    std::deque<std::string> keep;
    std::vector<PLModMenu_DrawCommand> cmds;
    const float sc = std::max(0.3f, std::min(5.f, m_scale));
    const float fs = 11.f * sc * 2.5f;
    float x = hudPosX < 0.f ? 20.f : hudPosX;
    float y = hudPosY < 0.f ? 90.f : hudPosY;

    auto text = [&](std::string t, uint32_t col) {
        keep.push_back(std::move(t));
        PLModMenu_DrawCommand c = {};
        c.type = PL_DRAW_TEXT; c.x = x; c.y = y; c.w = 1.f; c.h = 1.f;
        c.color = col; c.size = fs; c.text = keep.back().c_str();
        cmds.push_back(c);
        y += fs + 2.f;
    };

    if (m_showList && m_valid) {
        struct Row { int num; const MarkerEntry* m; double dist; float delta; };
        auto markers = snapshotMarkers();
        std::vector<Row> rows;
        for (size_t i = 0; i < markers.size(); ++i) {
            const auto& m = markers[i];
            if (m_currentDimensionOnly && m.dim != m_dim) continue;
            double dx = m.x + 0.5 - m_pos.x, dy = m.y - m_pos.y, dz = m.z + 0.5 - m_pos.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (m_renderDistance > 0 && dist > m_renderDistance * 4.0) continue; // list reaches 4x farther than boxes
            float yawTo = (float)(std::atan2(-dx, dz) * 180.0 / M_PI);
            rows.push_back({(int)i + 1, &m, dist, wrap180(yawTo - m_yaw)});
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.dist < b.dist; });
        int n = std::max(1, std::min(10, m_listSize)), shown = 0;
        for (auto& r : rows) {
            if (shown++ >= n) break;
            const char* arrow = std::fabs(r.delta) < 15.f ? "^" : (r.delta > 0 ? (r.delta > 90.f ? ">>" : ">") : (r.delta < -90.f ? "<<" : "<"));
            char b[200];
            if (m_showDistance) std::snprintf(b, sizeof b, "%d. %s  %s  %s", r.num, r.m->name.c_str(), distText(r.dist).c_str(), arrow);
            else std::snprintf(b, sizeof b, "%d. %s", r.num, r.m->name.c_str());
            text(b, withAlpha(r.m->color, 1.f) | 0xFF000000u);
        }
    }
    if (!m_toast.empty()) {
        if (nowSec() < m_toastUntil) text(m_toast, 0xFFFFFF55u);
        else m_toast.clear();
    }
    submitDrawCommands(moduleId, cmds);
}

void BlockMarkerModule::loadConfig(const nlohmann::json& j) {
    Module::loadConfig(j);
    auto f = [&](const char* k, auto& v) {
        if (j.contains(k)) { try { v = j[k].get<std::decay_t<decltype(v)>>(); } catch (...) {} }
    };
    f("hudPosX", hudPosX); f("hudPosY", hudPosY); f("isHudModule", isHudModule);
    f("m_markerName", m_markerName); f("m_markerColor", m_markerColor);
    f("m_selectNumber", m_selectNumber);
    f("m_showMarkers", m_showMarkers); f("m_topCross", m_topCross);
    f("m_thickness", m_thickness); f("m_opacity", m_opacity);
    f("m_renderDistance", m_renderDistance); f("m_currentDimensionOnly", m_currentDimensionOnly);
    f("m_showList", m_showList); f("m_listSize", m_listSize); f("m_showDistance", m_showDistance);
    f("m_scale", m_scale); f("m_confirmClear", m_confirmClear);
    m_thickness = std::max(1.f, std::min(5.f, m_thickness));
    m_opacity = std::max(0.f, std::min(1.f, m_opacity));
    m_listSize = std::max(1, std::min(10, m_listSize));
    m_scale = std::max(0.3f, std::min(5.f, m_scale));
    if (j.contains("markers") && j["markers"].is_array()) {
        std::vector<MarkerEntry> out;
        for (auto& p : j["markers"]) {
            try {
                MarkerEntry e;
                e.name = p.value("name", std::string("Marker"));
                e.x = p.value("x", 0); e.y = p.value("y", 0); e.z = p.value("z", 0);
                e.dim = p.value("dim", 0);
                e.color = p.value("color", 0xFFFF3030u);
                out.push_back(e);
            } catch (...) {}
        }
        std::lock_guard<std::mutex> g(m_lock);
        m_markers = std::move(out);
    }
}

void BlockMarkerModule::saveConfig(nlohmann::json& j) {
    Module::saveConfig(j);
    j["hudPosX"] = hudPosX; j["hudPosY"] = hudPosY; j["isHudModule"] = isHudModule;
    // Buttons (boolean keys with "button" in the name become buttons in the menu)
    j["placeMarkerButton"] = false;
    j["removeMarkerButton"] = false;
    j["m_markerName"] = m_markerName.empty() ? std::string("Trap") : m_markerName;
    j["m_markerColor"] = m_markerColor;
    j["m_selectNumber"] = m_selectNumber;
    j["applyToSelectedButton"] = false;
    j["removeSelectedButton"] = false;
    j["m_confirmClear"] = m_confirmClear;
    j["clearAllButton"] = false;
    j["m_showMarkers"] = m_showMarkers; j["m_topCross"] = m_topCross;
    j["m_thickness"] = m_thickness; j["m_opacity"] = m_opacity;
    j["m_renderDistance"] = m_renderDistance; j["m_currentDimensionOnly"] = m_currentDimensionOnly;
    j["m_showList"] = m_showList; j["m_listSize"] = m_listSize; j["m_showDistance"] = m_showDistance;
    j["m_scale"] = m_scale;
    nlohmann::json arr = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> g(m_lock);
        for (auto& m : m_markers)
            arr.push_back({{"name", m.name}, {"x", m.x}, {"y", m.y}, {"z", m.z}, {"dim", m.dim}, {"color", m.color}});
    }
    j["markers"] = arr;
}
