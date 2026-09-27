#include "Tileset.h"
#include "tinyxml.h"


bool Tileset::LoadFromFile(const char* filename)
{
    if (FileExists(filename))
    {
        char* xml = LoadFileText(filename);
        const bool result = Parse(xml);
        UnloadFileText(xml);
        return result;
    }
    return false;
}

bool Tileset::Parse(const char* tileXml)
{
    TiXmlDocument doc;
    if (!doc.Parse(tileXml))
    {
        return false;
    }

    m_name = doc.RootElement()->Attribute("name");

    if (const auto imageNode = doc.RootElement()->FirstChild("image"))
    {
        m_texture = LoadTexture(imageNode->ToElement()->Attribute("source"));
    }

    int tileWidth, tileHeight, tileCount, columns;
    doc.RootElement()->QueryIntAttribute("tilewidth", &tileWidth);
    doc.RootElement()->QueryIntAttribute("tileheight", &tileHeight);
    doc.RootElement()->QueryIntAttribute("tilecount", &tileCount);
    doc.RootElement()->QueryIntAttribute("columns", &columns);

    // Parse individual tiles
    m_tiles.reserve(tileCount);
    for (int i = 0; i < tileCount; ++i)
    {
        Tile tile{};
        tile.id = i;
        tile.x = (i % columns) * tileWidth;
        tile.y = (i / columns) * tileHeight;
        tile.width = tileWidth;
        tile.height = tileHeight;
        m_tiles.push_back(tile);
    }

    // some tiles may have types and/or animations
    auto tileChild = doc.RootElement()->IterateChildren("tile", nullptr);
    while (tileChild)
    {
        int tileId;
        tileChild->ToElement()->QueryIntAttribute("id", &tileId);

        if (const auto typeStr = tileChild->ToElement()->Attribute("type"))
        {
            m_tiles[tileId].type = typeStr;
        }

        if (const auto animRoot = tileChild->FirstChild("animation"))
        {
            auto animChild = animRoot->IterateChildren("frame", nullptr);
            while (animChild)
            {
                int animTileId, animDuration;
                animChild->ToElement()->QueryIntAttribute("tileid", &animTileId);
                animChild->ToElement()->QueryIntAttribute("duration", &animDuration);

                m_tiles[tileId].animation.push_back({ .tileId = animTileId, .duration = animDuration });
                m_tiles[tileId].animationDuration += animDuration;
                animChild = animRoot->IterateChildren("frame", animChild);
            }
        }
        tileChild = doc.RootElement()->IterateChildren("tile", tileChild);
    }
    return true;
}

int Tileset::GetAnimationFrame(int id, int timeMs) const
{
    const auto& tile = m_tiles[id];
    if (tile.animationDuration <= 0)
    {
        return id;
    }

    int time = timeMs % tile.animationDuration;
    for (const auto& frame : tile.animation)
    {
        if (time < frame.duration)
        {
            return frame.tileId;
        }
        time -= frame.duration;
    }
    return id;
}
