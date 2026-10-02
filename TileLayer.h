#pragma once

#include <stdint.h>

#include "Xml.h"

// Tiled stores flip/rotation flags in the highest bits of each gid
static constexpr uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x8000'0000;
static constexpr uint32_t FLIPPED_VERTICALLY_FLAG = 0x4000'0000;
static constexpr uint32_t FLIPPED_DIAGONALLY_FLAG = 0x2000'0000;
static constexpr uint32_t ROTATED_HEXAGONAL_120_FLAG = 0x1000'0000;
static constexpr uint32_t GID_FLAGS_MASK = FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG | ROTATED_HEXAGONAL_120_FLAG;

typedef struct TileCell
{
    uint32_t gid;               // global tile id without flags, 0 means empty cell
    bool flippedHorizontally;
    bool flippedVertically;
    bool flippedDiagonally;
} TileCell;

typedef struct TileLayer
{
    int id;
    char* name;                 // nullptr when not set
    int width;
    int height;
    bool visible;
    bool locked;
    float opacity;
    uint32_t* data;             // raw gids with flags, row by row
} TileLayer;

// on failure the layer is left empty, nothing has to be unloaded
[[nodiscard]] bool ParseTileLayer(TileLayer* layer, const XmlElement* layerElement);
void UnloadTileLayer(TileLayer* layer);

// splits raw gid into tile gid and flip flags
[[nodiscard]] TileCell DecodeGid(uint32_t rawGid);

[[nodiscard]] static inline bool IsInsideTileLayer(const TileLayer* layer, int x, int y)
{
    return x >= 0 && y >= 0 && x < layer->width && y < layer->height;
}

[[nodiscard]] static inline uint32_t GetTileLayerRawGid(const TileLayer* layer, int x, int y)
{
    return layer->data[(size_t)y * (size_t)layer->width + (size_t)x];
}

[[nodiscard]] static inline uint32_t GetTileLayerGid(const TileLayer* layer, int x, int y)
{
    return GetTileLayerRawGid(layer, x, y) & ~GID_FLAGS_MASK;
}

[[nodiscard]] static inline TileCell GetTileLayerCell(const TileLayer* layer, int x, int y)
{
    return DecodeGid(GetTileLayerRawGid(layer, x, y));
}
