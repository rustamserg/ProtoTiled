#include "Map.h"
#include "tinyxml.h"

#include <algorithm>
#include <cmath>
#include <cstring>


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
    m_objectGroups.clear();
    m_drawOrder.clear();
    m_time = 0.0;
}

void Map::Update(float deltaTime)
{
    m_time += deltaTime;
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

    // tile layers and object groups share the draw order they have in the file
    for (auto element = root->FirstChildElement(); element; element = element->NextSiblingElement())
    {
        if (strcmp(element->Value(), "layer") == 0)
        {
            TileLayer layer;
            if (!layer.Parse(element))
            {
                return false;
            }
            m_drawOrder.push_back({ LayerKind::Tiles, m_layers.size() });
            m_layers.push_back(std::move(layer));
        }
        else if (strcmp(element->Value(), "objectgroup") == 0)
        {
            ObjectGroup group;
            if (!group.Parse(element))
            {
                return false;
            }
            m_drawOrder.push_back({ LayerKind::Objects, m_objectGroups.size() });
            m_objectGroups.push_back(std::move(group));
        }
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

const ObjectGroup* Map::FindObjectGroup(const std::string& name) const
{
    for (const auto& group : m_objectGroups)
    {
        if (group.GetName() == name)
        {
            return &group;
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
    for (const auto& entry : m_drawOrder)
    {
        if (entry.kind == LayerKind::Tiles)
        {
            DrawLayer(m_layers[entry.index]);
        }
        else
        {
            DrawObjectGroup(m_objectGroups[entry.index]);
        }
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
            if (cell.gid == 0)
            {
                continue;
            }

            // tiles are aligned to the bottom-left corner of the cell and keep their own size
            const Vector2 bottomLeft = { static_cast<float>(x * m_tileWidth), static_cast<float>((y + 1) * m_tileHeight) };
            DrawTile(cell, bottomLeft, { 0.0f, 0.0f }, 0.0f, tint);
        }
    }
}

void Map::DrawObjectGroup(const ObjectGroup& group) const
{
    if (!group.IsVisible())
    {
        return;
    }

    // only tile objects have visual representation, other shapes are game data
    const Color tint = Fade(WHITE, group.GetOpacity());
    for (const auto& object : group.GetObjects())
    {
        if (object.gid == 0 || !object.visible)
        {
            continue;
        }

        // tile object is anchored at bottom-left and stretched to object size
        DrawTile(TileLayer::DecodeGid(object.gid), { object.x, object.y }, { object.width, object.height }, object.rotation, tint);
    }
}

void Map::DrawObjectsDebug() const
{
    const Color color = RED;
    for (const auto& group : m_objectGroups)
    {
        for (const auto& object : group.GetObjects())
        {
            const Vector2 position = { object.x, object.y };
            switch (object.shape)
            {
            case ObjectGroup::Shape::Rectangle:
                if (object.gid != 0)
                {
                    // tile objects are anchored at bottom-left
                    DrawRectanglePro({ object.x, object.y, object.width, object.height }, { 0.0f, object.height }, object.rotation, Fade(color, 0.2f));
                }
                else if (object.width > 0.0f && object.height > 0.0f)
                {
                    DrawRectanglePro({ object.x, object.y, object.width, object.height }, { 0.0f, 0.0f }, object.rotation, Fade(color, 0.2f));
                }
                else
                {
                    // Tiled draws rectangles without size as points
                    DrawCircleV(position, 3.0f, color);
                }
                break;
            case ObjectGroup::Shape::Ellipse:
                DrawEllipseLines(static_cast<int>(object.x + object.width / 2.0f), static_cast<int>(object.y + object.height / 2.0f),
                    object.width / 2.0f, object.height / 2.0f, color);
                break;
            case ObjectGroup::Shape::Point:
                DrawCircleV(position, 3.0f, color);
                break;
            case ObjectGroup::Shape::Polygon:
            case ObjectGroup::Shape::Polyline:
            {
                const size_t count = object.points.size();
                const size_t segments = object.shape == ObjectGroup::Shape::Polygon ? count : count - 1;
                for (size_t i = 0; i < segments; ++i)
                {
                    const auto& a = object.points[i];
                    const auto& b = object.points[(i + 1) % count];
                    DrawLineEx({ position.x + a.x, position.y + a.y }, { position.x + b.x, position.y + b.y }, 1.0f, color);
                }
                break;
            }
            }

            const std::string& label = object.name.empty() ? object.type : object.name;
            if (!label.empty())
            {
                DrawText(label.c_str(), static_cast<int>(object.x), static_cast<int>(object.y) - 10, 10, color);
            }
        }
    }
}

void Map::DrawTile(const TileLayer::Cell& cell, Vector2 bottomLeft, Vector2 size, float rotation, Color tint) const
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
    const int frameId = tileset.GetAnimationFrame(localId, static_cast<int>(m_time * 1000.0));
    if (frameId < 0 || frameId >= tileset.GetTileCount())
    {
        return;
    }
    const auto& tile = tileset.GetTile(frameId);

    // zero size means the tile's own size
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        size = { static_cast<float>(tile.width), static_cast<float>(tile.height) };
    }

    // Tiled flips: diagonal flip is a transpose, then horizontal/vertical flips are applied.
    // raylib flips the source first and then rotates, so transpose becomes 90 degrees rotation
    // with a vertical source flip, and post-rotation flips swap their axes.
    bool flipX = cell.flippedHorizontally;
    bool flipY = cell.flippedVertically;
    float flipRotation = 0.0f;
    if (cell.flippedDiagonally)
    {
        flipRotation = 90.0f;
        flipX = cell.flippedVertically;
        flipY = !cell.flippedHorizontally;
    }

    const float tileWidth = static_cast<float>(tile.width);
    const float tileHeight = static_cast<float>(tile.height);
    const Rectangle source = {
        static_cast<float>(tile.x),
        static_cast<float>(tile.y),
        flipX ? -tileWidth : tileWidth,
        flipY ? -tileHeight : tileHeight
    };

    // object rotation is around bottom-left corner, flip rotation around tile center:
    // rotate the center around the corner and spin the tile around its center by both angles
    const float radians = rotation * DEG2RAD;
    const float halfX = size.x / 2.0f;
    const float halfY = -size.y / 2.0f;
    const Vector2 center = {
        bottomLeft.x + halfX * cosf(radians) - halfY * sinf(radians),
        bottomLeft.y + halfX * sinf(radians) + halfY * cosf(radians)
    };

    const Rectangle dest = { center.x, center.y, size.x, size.y };
    DrawTexturePro(tileset.GetTexture(), source, dest, { halfX, size.y / 2.0f }, rotation + flipRotation, tint);
}
