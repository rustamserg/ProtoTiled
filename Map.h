#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "ObjectGroup.h"
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

    // advances tile animations
    void Update(float deltaTime);

    // draws tile layers and tile objects in the map order
    void Draw() const;
    void DrawLayer(const TileLayer& layer) const;
    void DrawObjectGroup(const ObjectGroup& group) const;

    // draws outlines and names of all objects, useful to check collision and trigger shapes
    void DrawObjectsDebug() const;

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetTileWidth() const { return m_tileWidth; }
    int GetTileHeight() const { return m_tileHeight; }

    const std::vector<TileLayer>& GetLayers() const { return m_layers; }
    const TileLayer* FindLayer(const std::string& name) const;

    const std::vector<ObjectGroup>& GetObjectGroups() const { return m_objectGroups; }
    const ObjectGroup* FindObjectGroup(const std::string& name) const;

    // returns tileset owning the gid (without flags) or nullptr for empty/unknown gid
    const TilesetRef* FindTileset(uint32_t gid) const;

private:
    enum class LayerKind
    {
        Tiles,
        Objects,
    };

    struct DrawEntry
    {
        LayerKind kind;
        size_t index;
    };

private:
    bool Parse(const char* mapXml, const std::string& baseDir);
    // size of zero uses the tile's own size, rotation is around bottom-left corner
    void DrawTile(const TileLayer::Cell& cell, Vector2 bottomLeft, Vector2 size, float rotation, Color tint) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_tileWidth = 0;
    int m_tileHeight = 0;
    double m_time = 0.0;
    std::vector<TilesetRef> m_tilesets;
    std::vector<TileLayer> m_layers;
    std::vector<ObjectGroup> m_objectGroups;
    std::vector<DrawEntry> m_drawOrder;
};
