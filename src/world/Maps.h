#pragma once

#include "world/BlockRegistry.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>

namespace mc::world {

class World;

// Maps (M28.2b; wiki: Map, Map item format). A filled map is a 128x128 picture of the
// world centred on a grid cell of 128 << scale blocks; each pixel is a colour byte =
// base colour x 4 + shade (vanilla's palette of 62 base colours, shades 180/220/255/135
// of 255). Held maps draw the terrain around the player as they walk. Saved as vanilla's
// data/minecraft/maps/<id>.dat and last_id.dat (26.1; M34).
struct MapData {
    int32_t centerX = 0, centerZ = 0;
    uint8_t scale = 0;     // 0..4: 1, 2, 4, 8, 16 blocks a pixel
    uint8_t dimension = 0; // world::Dimension
    bool locked = false;   // (a cartography table's glass pane: never updated again)
    std::array<uint8_t, 128 * 128> colors{}; // index z * 128 + x; 0 = nothing drawn yet
    uint32_t version = 0;  // bumped when a pixel changes (the renderer re-uploads)
};

// The base map colour (0..61, 0 = none: the map looks through it) of a block.
uint8_t mapColorOf(BlockId block);
// 0xRRGGBB of a colour byte (base x 4 + shade); 0 for the base colour 0.
uint32_t mapColorRgb(uint8_t color);

class Maps {
public:
    // A new map centred on the grid cell holding (x, z) (vanilla: cells of 128 << scale
    // blocks, offset by 64); returns its id.
    int create(int32_t x, int32_t z, int scale, uint8_t dimension);
    MapData* get(int id);
    const MapData* get(int id) const;
    int count() const { return int(m_maps.size()); }

    // One tick of a held map (vanilla MapItem.update): a sixteenth of the pixel columns,
    // within 128 blocks of the player, redrawn from the loaded terrain.
    static void update(const World& world, MapData& map, const glm::dvec3& player, int64_t tick);
    // Where (x, z) lands on the map, in pixels (may be outside 0..128).
    static glm::dvec2 pixelOf(const MapData& map, double x, double z);

    // maps/<id>.dat for each map changed since the last save, and maps/last_id.dat.
    bool save(const std::filesystem::path& worldDir);
    bool load(const std::filesystem::path& worldDir);

private:
    std::map<int, MapData> m_maps;
    std::map<int, uint32_t> m_savedVersion;
    int m_next = 0;
};

} // namespace mc::world
