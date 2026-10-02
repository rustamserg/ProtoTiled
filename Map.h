#pragma once

#include <stdint.h>

#include "raylib.h"

#include "ObjectGroup.h"
#include "TileLayer.h"
#include "Tileset.h"

typedef struct MapTileset
{
    int firstGid;
    Tileset tileset;
} MapTileset;

typedef enum MapLayerKind : uint8_t
{
    MAP_LAYER_TILES,
    MAP_LAYER_OBJECTS,
} MapLayerKind;

typedef struct MapDrawEntry
{
    MapLayerKind kind;
    int index;
} MapDrawEntry;

typedef struct Map
{
    int width;
    int height;
    int tileWidth;
    int tileHeight;
    double time;                // seconds, drives tile animations
    MapTileset* tilesets;       // sorted by firstGid
    int tilesetCount;
    TileLayer* layers;
    int layerCount;
    ObjectGroup* objectGroups;
    int objectGroupCount;
    MapDrawEntry* drawOrder;    // tile layers and object groups in the file order
    int drawCount;
} Map;

// tileset sources are resolved relative to the map file, on failure the map is left empty
[[nodiscard]] bool LoadMap(Map* map, const char* filename);
void UnloadMap(Map* map);

// advances tile animations
void UpdateMap(Map* map, float deltaTime);

// draws tile layers and tile objects in the map order
void DrawMap(const Map* map);
void DrawMapLayer(const Map* map, const TileLayer* layer);
void DrawMapObjectGroup(const Map* map, const ObjectGroup* group);

// draws outlines and names of all objects, useful to check collision and trigger shapes
void DrawMapObjectsDebug(const Map* map);

[[nodiscard]] const TileLayer* FindMapLayer(const Map* map, const char* name);
[[nodiscard]] const ObjectGroup* FindMapObjectGroup(const Map* map, const char* name);

// returns tileset owning the gid (without flags) or nullptr for empty/unknown gid
[[nodiscard]] const MapTileset* FindMapTileset(const Map* map, uint32_t gid);
