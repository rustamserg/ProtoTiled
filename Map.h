#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "TileLayer.h"
#include "Tileset.h"

class Map
{
public:
    struct TilesetRef
    {
        int firstGid;
        Tileset tileset;
    };

public:
    bool LoadFromFile(const char* filename);
    void Unload();

    void Draw() const;
    void DrawLayer(const TileLayer& layer) const;

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetTileWidth() const { return m_tileWidth; }
    int GetTileHeight() const { return m_tileHeight; }

    const std::vector<TileLayer>& GetLayers() const { return m_layers; }
    const TileLayer* FindLayer(const std::string& name) const;

    // returns tileset owning the gid (without flags) or nullptr for empty/unknown gid
    const TilesetRef* FindTileset(uint32_t gid) const;

private:
    bool Parse(const char* mapXml, const std::string& baseDir);
    void DrawTile(const TileLayer::Cell& cell, int x, int y, Color tint) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_tileWidth = 0;
    int m_tileHeight = 0;
    std::vector<TilesetRef> m_tilesets;
    std::vector<TileLayer> m_layers;
};
