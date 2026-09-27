#pragma once

#include <string>
#include <vector>

#include "raylib.h"

class Tileset
{
public:
    struct Frame
    {
        int tileId;
        int duration;
    };

    struct Tile
    {
        int id;
        int x;
        int y;
        int width;
        int height;
        std::string type;
        std::vector<Frame> animation;
    };

public:
    bool LoadFromFile(const char* filename);

    const std::string& GetName() const { return m_name; }
    const Tile& GetTile(int id) const { return m_tiles[id]; }
    int GetTileCount() const { return static_cast<int>(m_tiles.size()); }
    const Texture2D& GetTexture() const { return m_texture; }

private:
    bool Parse(const char* tileXml);

private:
    std::string m_name;
    std::vector<Tile> m_tiles;
    Texture2D m_texture;
};

