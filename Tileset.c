#include "Tileset.h"
#include "Utils.h"
#include "Xml.h"


static bool ParseAnimation(TilesetTile* tile, const XmlElement* animationElement, int tileCount)
{
    const int frameCount = CountXmlChildren(animationElement, "frame");
    if (frameCount == 0)
    {
        return true;
    }

    tile->animation = calloc((size_t)frameCount, sizeof *tile->animation);
    if (!tile->animation)
    {
        return false;
    }

    XML_FOR_EACH_CHILD(frameElement, animationElement, "frame")
    {
        TilesetFrame frame = {};
        QueryXmlInt(frameElement, "tileid", &frame.tileId);
        QueryXmlInt(frameElement, "duration", &frame.duration);
        if (frame.tileId < 0 || frame.tileId >= tileCount || frame.duration <= 0)
        {
            return false;
        }

        tile->animation[tile->animationCount++] = frame;
        tile->animationDuration += frame.duration;
    }
    return true;
}

static bool ParseTileset(Tileset* tileset, const XmlElement* root, const char* filename)
{
    if (strcmp(root->name, "tileset") != 0)
    {
        return false;
    }

    tileset->name = CopyString(GetXmlAttribute(root, "name"));

    // image collection tilesets have no columns and are not supported
    int tileWidth = 0, tileHeight = 0, tileCount = 0, columns = 0;
    QueryXmlInt(root, "tilewidth", &tileWidth);
    QueryXmlInt(root, "tileheight", &tileHeight);
    QueryXmlInt(root, "tilecount", &tileCount);
    QueryXmlInt(root, "columns", &columns);
    if (tileWidth <= 0 || tileHeight <= 0 || tileCount <= 0 || columns <= 0)
    {
        return false;
    }

    const XmlElement* imageElement = FirstXmlChild(root, "image");
    const char* source = imageElement ? GetXmlAttribute(imageElement, "source") : nullptr;
    char path[MAX_PATH_LENGTH];
    if (!source || !ResolveRelativePath(path, sizeof path, filename, source))
    {
        return false;
    }

    tileset->texture = LoadTexture(path);
    if (!IsTextureValid(tileset->texture))
    {
        return false;
    }

    tileset->tiles = calloc((size_t)tileCount, sizeof *tileset->tiles);
    if (!tileset->tiles)
    {
        return false;
    }

    tileset->tileCount = tileCount;
    for (int i = 0; i < tileCount; ++i)
    {
        tileset->tiles[i] = (TilesetTile){
            .id = i,
            .x = (i % columns) * tileWidth,
            .y = (i / columns) * tileHeight,
            .width = tileWidth,
            .height = tileHeight,
        };
    }

    // some tiles may have types and/or animations
    XML_FOR_EACH_CHILD(tileElement, root, "tile")
    {
        int id = -1;
        QueryXmlInt(tileElement, "id", &id);
        if (id < 0 || id >= tileCount)
        {
            return false;
        }

        TilesetTile* tile = &tileset->tiles[id];

        // Tiled 1.9+ writes "class" instead of "type"
        const char* type = GetXmlAttribute(tileElement, "type");
        tile->type = CopyString(type ? type : GetXmlAttribute(tileElement, "class"));

        const XmlElement* animationElement = FirstXmlChild(tileElement, "animation");
        if (animationElement && !ParseAnimation(tile, animationElement, tileCount))
        {
            return false;
        }
    }
    return true;
}

bool LoadTileset(Tileset* tileset, const char* filename)
{
    *tileset = (Tileset){};

    char* text = LoadFileText(filename);
    if (!text)
    {
        return false;
    }

    XmlDocument doc;
    const bool parsed = ParseXml(&doc, text);
    UnloadFileText(text);
    if (!parsed)
    {
        TraceLog(LOG_WARNING, "TILESET: [%s] %s at line %d", filename, doc.error, doc.errorLine);
        return false;
    }

    const bool result = ParseTileset(tileset, doc.root, filename);
    UnloadXml(&doc);

    if (!result)
    {
        TraceLog(LOG_WARNING, "TILESET: [%s] Failed to load tileset", filename);
        UnloadTileset(tileset);
    }
    return result;
}

void UnloadTileset(Tileset* tileset)
{
    for (int i = 0; i < tileset->tileCount; ++i)
    {
        free(tileset->tiles[i].type);
        free(tileset->tiles[i].animation);
    }
    free(tileset->tiles);
    free(tileset->name);

    if (tileset->texture.id != 0)
    {
        UnloadTexture(tileset->texture);
    }
    *tileset = (Tileset){};
}

int GetTilesetAnimationFrame(const Tileset* tileset, int id, int64_t timeMs)
{
    const TilesetTile* tile = &tileset->tiles[id];
    if (tile->animationDuration <= 0)
    {
        return id;
    }

    int64_t time = timeMs % tile->animationDuration;
    for (int i = 0; i < tile->animationCount; ++i)
    {
        const TilesetFrame* frame = &tile->animation[i];
        if (time < frame->duration)
        {
            return frame->tileId;
        }
        time -= frame->duration;
    }
    return id;
}
