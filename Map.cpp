#include "Map.h"
#include "tinyxml.h"

#include <algorithm>


bool Map::LoadFromFile(const char* filename)
{
    if (!FileExists(filename))
    {
        return false;
    }

    // tileset sources in the map are relative to the map file
    const std::string path = filename;
    const auto slash = path.find_last_of("/\\");
    const std::string baseDir = slash == std::string::npos ? "" : path.substr(0, slash + 1);

    char* xml = LoadFileText(filename);
    const bool result = Parse(xml, baseDir);
    UnloadFileText(xml);
    return result;
}

void Map::Unload()
{
    for (const auto& ref : m_tilesets)
    {
        UnloadTexture(ref.tileset.GetTexture());
    }
    m_tilesets.clear();
    m_layers.clear();
}

bool Map::Parse(const char* mapXml, const std::string& baseDir)
{
    TiXmlDocument doc;
    doc.Parse(mapXml);
    if (doc.Error() || !doc.RootElement())
    {
        return false;
    }

    const auto root = doc.RootElement();
    root->QueryIntAttribute("width", &m_width);
    root->QueryIntAttribute("height", &m_height);
    root->QueryIntAttribute("tilewidth", &m_tileWidth);
    root->QueryIntAttribute("tileheight", &m_tileHeight);

    // only external tilesets (.tsx) are supported
    for (auto tsElement = root->FirstChildElement("tileset"); tsElement; tsElement = tsElement->NextSiblingElement("tileset"))
    {
        const auto source = tsElement->Attribute("source");
        if (!source)
        {
            return false;
        }

        TilesetRef ref{};
        tsElement->QueryIntAttribute("firstgid", &ref.firstGid);
        if (!ref.tileset.LoadFromFile((baseDir + source).c_str()))
        {
            return false;
        }
        m_tilesets.push_back(std::move(ref));
    }

    std::sort(m_tilesets.begin(), m_tilesets.end(),
        [](const TilesetRef& a, const TilesetRef& b) { return a.firstGid < b.firstGid; });

    for (auto layerElement = root->FirstChildElement("layer"); layerElement; layerElement = layerElement->NextSiblingElement("layer"))
    {
        TileLayer layer;
        if (!layer.Parse(layerElement))
        {
            return false;
        }
        m_layers.push_back(std::move(layer));
    }
    return true;
}

const TileLayer* Map::FindLayer(const std::string& name) const
{
    for (const auto& layer : m_layers)
    {
        if (layer.GetName() == name)
        {
            return &layer;
        }
    }
    return nullptr;
}

const Map::TilesetRef* Map::FindTileset(uint32_t gid) const
{
    if (gid == 0)
    {
        return nullptr;
    }

    // tilesets are sorted by firstGid, the owner is the last one starting at or before gid
    for (auto it = m_tilesets.rbegin(); it != m_tilesets.rend(); ++it)
    {
        if (static_cast<uint32_t>(it->firstGid) <= gid)
        {
            return &*it;
        }
    }
    return nullptr;
}

void Map::Draw() const
{
    for (const auto& layer : m_layers)
    {
        DrawLayer(layer);
    }
}

void Map::DrawLayer(const TileLayer& layer) const
{
    if (!layer.IsVisible())
    {
        return;
    }

    const Color tint = Fade(WHITE, layer.GetOpacity());
    for (int y = 0; y < layer.GetHeight(); ++y)
    {
        for (int x = 0; x < layer.GetWidth(); ++x)
        {
            const auto cell = layer.GetCell(x, y);
            if (cell.gid != 0)
            {
                DrawTile(cell, x, y, tint);
            }
        }
    }
}

void Map::DrawTile(const TileLayer::Cell& cell, int x, int y, Color tint) const
{
    const auto ref = FindTileset(cell.gid);
    if (!ref)
    {
        return;
    }

    const auto& tileset = ref->tileset;
    const int localId = static_cast<int>(cell.gid) - ref->firstGid;
    if (localId >= tileset.GetTileCount())
    {
        return;
    }

    // pick current animation frame if the tile is animated
    const Tileset::Tile* tile = &tileset.GetTile(localId);
    if (!tile->animation.empty())
    {
        int totalDuration = 0;
        for (const auto& frame : tile->animation)
        {
            totalDuration += frame.duration;
        }

        if (totalDuration > 0)
        {
            int time = static_cast<int>(GetTime() * 1000.0) % totalDuration;
            for (const auto& frame : tile->animation)
            {
                if (time < frame.duration)
                {
                    tile = &tileset.GetTile(frame.tileId);
                    break;
                }
                time -= frame.duration;
            }
        }
    }

    // Tiled flips: diagonal flip is a transpose, then horizontal/vertical flips are applied.
    // raylib flips the source first and then rotates, so transpose becomes 90 degrees rotation
    // with a vertical source flip, and post-rotation flips swap their axes.
    bool flipX = cell.flippedHorizontally;
    bool flipY = cell.flippedVertically;
    float rotation = 0.0f;
    if (cell.flippedDiagonally)
    {
        rotation = 90.0f;
        flipX = cell.flippedVertically;
        flipY = !cell.flippedHorizontally;
    }

    const float width = static_cast<float>(tile->width);
    const float height = static_cast<float>(tile->height);
    const Rectangle source = {
        static_cast<float>(tile->x),
        static_cast<float>(tile->y),
        flipX ? -width : width,
        flipY ? -height : height
    };

    // tiles are aligned to the bottom-left corner of the cell, rotate around the tile center
    const float left = static_cast<float>(x * m_tileWidth);
    const float bottom = static_cast<float>((y + 1) * m_tileHeight);
    const Rectangle dest = { left + width / 2.0f, bottom - height / 2.0f, width, height };

    DrawTexturePro(tileset.GetTexture(), source, dest, { width / 2.0f, height / 2.0f }, rotation, tint);
}
