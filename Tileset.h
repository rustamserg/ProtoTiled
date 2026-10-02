#pragma once

#include <stdint.h>

#include "raylib.h"

typedef struct TilesetFrame
{
    int tileId;
    int duration;               // milliseconds
} TilesetFrame;

typedef struct TilesetTile
{
    int id;
    int x;
    int y;
    int width;
    int height;
    char* type;                 // nullptr when not set
    TilesetFrame* animation;
    int animationCount;
    int animationDuration;      // sum of frame durations in milliseconds
} TilesetTile;

typedef struct Tileset
{
    char* name;
    TilesetTile* tiles;
    int tileCount;
    Texture2D texture;
} Tileset;

// image source is resolved relative to the tileset file
[[nodiscard]] bool LoadTileset(Tileset* tileset, const char* filename);
void UnloadTileset(Tileset* tileset);

// returns id of the tile to display at given time, tile itself for non animated tiles
[[nodiscard]] int GetTilesetAnimationFrame(const Tileset* tileset, int id, int64_t timeMs);
