#include "Map.h"
#include "Utils.h"
#include "Xml.h"

#include <math.h>


static int CompareTilesets(const void* a, const void* b)
{
    const MapTileset* left = a;
    const MapTileset* right = b;
    return (left->firstGid > right->firstGid) - (left->firstGid < right->firstGid);
}

static bool ParseMap(Map* map, const XmlElement* root, const char* filename)
{
    if (strcmp(root->name, "map") != 0)
    {
        return false;
    }

    QueryXmlInt(root, "width", &map->width);
    QueryXmlInt(root, "height", &map->height);
    QueryXmlInt(root, "tilewidth", &map->tileWidth);
    QueryXmlInt(root, "tileheight", &map->tileHeight);

    const int tilesetCount = CountXmlChildren(root, "tileset");
    const int layerCount = CountXmlChildren(root, "layer");
    const int objectGroupCount = CountXmlChildren(root, "objectgroup");

    // calloc of zero elements may return nullptr, so always allocate at least one
    map->tilesets = calloc((size_t)tilesetCount + 1, sizeof *map->tilesets);
    map->layers = calloc((size_t)layerCount + 1, sizeof *map->layers);
    map->objectGroups = calloc((size_t)objectGroupCount + 1, sizeof *map->objectGroups);
    map->drawOrder = calloc((size_t)(layerCount + objectGroupCount) + 1, sizeof *map->drawOrder);
    if (!map->tilesets || !map->layers || !map->objectGroups || !map->drawOrder)
    {
        return false;
    }

    // only external tilesets (.tsx) are supported
    XML_FOR_EACH_CHILD(tilesetElement, root, "tileset")
    {
        const char* source = GetXmlAttribute(tilesetElement, "source");
        char path[MAX_PATH_LENGTH];
        if (!source || !ResolveRelativePath(path, sizeof path, filename, source))
        {
            return false;
        }

        MapTileset* ref = &map->tilesets[map->tilesetCount];
        QueryXmlInt(tilesetElement, "firstgid", &ref->firstGid);
        if (ref->firstGid <= 0 || !LoadTileset(&ref->tileset, path))
        {
            return false;
        }
        ++map->tilesetCount;
    }

    qsort(map->tilesets, (size_t)map->tilesetCount, sizeof *map->tilesets, CompareTilesets);

    // tile layers and object groups share the draw order they have in the file
    XML_FOR_EACH_CHILD(element, root, nullptr)
    {
        if (strcmp(element->name, "layer") == 0)
        {
            if (!ParseTileLayer(&map->layers[map->layerCount], element))
            {
                return false;
            }
            map->drawOrder[map->drawCount++] = (MapDrawEntry){ MAP_LAYER_TILES, map->layerCount++ };
        }
        else if (strcmp(element->name, "objectgroup") == 0)
        {
            if (!ParseObjectGroup(&map->objectGroups[map->objectGroupCount], element))
            {
                return false;
            }
            map->drawOrder[map->drawCount++] = (MapDrawEntry){ MAP_LAYER_OBJECTS, map->objectGroupCount++ };
        }
    }
    return true;
}

bool LoadMap(Map* map, const char* filename)
{
    *map = (Map){};

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
        TraceLog(LOG_WARNING, "MAP: [%s] %s at line %d", filename, doc.error, doc.errorLine);
        return false;
    }

    const bool result = ParseMap(map, doc.root, filename);
    UnloadXml(&doc);

    if (!result)
    {
        TraceLog(LOG_WARNING, "MAP: [%s] Failed to load map", filename);
        UnloadMap(map);
    }
    return result;
}

void UnloadMap(Map* map)
{
    for (int i = 0; i < map->tilesetCount; ++i)
    {
        UnloadTileset(&map->tilesets[i].tileset);
    }
    for (int i = 0; i < map->layerCount; ++i)
    {
        UnloadTileLayer(&map->layers[i]);
    }
    for (int i = 0; i < map->objectGroupCount; ++i)
    {
        UnloadObjectGroup(&map->objectGroups[i]);
    }
    free(map->tilesets);
    free(map->layers);
    free(map->objectGroups);
    free(map->drawOrder);
    *map = (Map){};
}

void UpdateMap(Map* map, float deltaTime)
{
    map->time += deltaTime;
}

const TileLayer* FindMapLayer(const Map* map, const char* name)
{
    for (int i = 0; i < map->layerCount; ++i)
    {
        if (StringEquals(map->layers[i].name, name))
        {
            return &map->layers[i];
        }
    }
    return nullptr;
}

const ObjectGroup* FindMapObjectGroup(const Map* map, const char* name)
{
    for (int i = 0; i < map->objectGroupCount; ++i)
    {
        if (StringEquals(map->objectGroups[i].name, name))
        {
            return &map->objectGroups[i];
        }
    }
    return nullptr;
}

const MapTileset* FindMapTileset(const Map* map, uint32_t gid)
{
    if (gid == 0)
    {
        return nullptr;
    }

    // tilesets are sorted by firstGid, the owner is the last one starting at or before gid
    for (int i = map->tilesetCount - 1; i >= 0; --i)
    {
        if ((uint32_t)map->tilesets[i].firstGid <= gid)
        {
            return &map->tilesets[i];
        }
    }
    return nullptr;
}

// size of zero uses the tile's own size, rotation is around bottom-left corner
static void DrawTile(const Map* map, TileCell cell, Vector2 bottomLeft, Vector2 size, float rotation, Color tint)
{
    const auto ref = FindMapTileset(map, cell.gid);
    if (!ref)
    {
        return;
    }

    const Tileset* tileset = &ref->tileset;
    const int64_t localId = (int64_t)cell.gid - ref->firstGid;
    if (localId >= tileset->tileCount)
    {
        return;
    }

    // pick current animation frame if the tile is animated
    const int frameId = GetTilesetAnimationFrame(tileset, (int)localId, (int64_t)(map->time * 1000.0));
    if (frameId < 0 || frameId >= tileset->tileCount)
    {
        return;
    }
    const TilesetTile* tile = &tileset->tiles[frameId];

    // zero size means the tile's own size
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        size = (Vector2){ (float)tile->width, (float)tile->height };
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

    const float tileWidth = (float)tile->width;
    const float tileHeight = (float)tile->height;
    const Rectangle source = {
        (float)tile->x,
        (float)tile->y,
        flipX ? -tileWidth : tileWidth,
        flipY ? -tileHeight : tileHeight,
    };

    // object rotation is around bottom-left corner, flip rotation around tile center:
    // rotate the center around the corner and spin the tile around its center by both angles
    const float radians = rotation * DEG2RAD;
    const float halfX = size.x / 2.0f;
    const float halfY = -size.y / 2.0f;
    const Vector2 center = {
        bottomLeft.x + halfX * cosf(radians) - halfY * sinf(radians),
        bottomLeft.y + halfX * sinf(radians) + halfY * cosf(radians),
    };

    const Rectangle dest = { center.x, center.y, size.x, size.y };
    DrawTexturePro(tileset->texture, source, dest, (Vector2){ halfX, size.y / 2.0f }, rotation + flipRotation, tint);
}

void DrawMap(const Map* map)
{
    for (int i = 0; i < map->drawCount; ++i)
    {
        const MapDrawEntry entry = map->drawOrder[i];
        switch (entry.kind)
        {
        case MAP_LAYER_TILES:
            DrawMapLayer(map, &map->layers[entry.index]);
            break;
        case MAP_LAYER_OBJECTS:
            DrawMapObjectGroup(map, &map->objectGroups[entry.index]);
            break;
        }
    }
}

void DrawMapLayer(const Map* map, const TileLayer* layer)
{
    if (!layer->visible)
    {
        return;
    }

    const Color tint = Fade(WHITE, layer->opacity);
    for (int y = 0; y < layer->height; ++y)
    {
        for (int x = 0; x < layer->width; ++x)
        {
            const TileCell cell = GetTileLayerCell(layer, x, y);
            if (cell.gid == 0)
            {
                continue;
            }

            // tiles are aligned to the bottom-left corner of the cell and keep their own size
            const Vector2 bottomLeft = { (float)(x * map->tileWidth), (float)((y + 1) * map->tileHeight) };
            DrawTile(map, cell, bottomLeft, (Vector2){}, 0.0f, tint);
        }
    }
}

void DrawMapObjectGroup(const Map* map, const ObjectGroup* group)
{
    if (!group->visible)
    {
        return;
    }

    // only tile objects have visual representation, other shapes are game data
    const Color tint = Fade(WHITE, group->opacity);
    for (int i = 0; i < group->objectCount; ++i)
    {
        const MapObject* object = &group->objects[i];
        if (object->gid == 0 || !object->visible)
        {
            continue;
        }

        // tile object is anchored at bottom-left and stretched to object size
        DrawTile(map, DecodeGid(object->gid), (Vector2){ object->x, object->y },
            (Vector2){ object->width, object->height }, object->rotation, tint);
    }
}

static void DrawObjectDebug(const MapObject* object, Color color)
{
    const Vector2 position = { object->x, object->y };
    switch (object->shape)
    {
    case OBJECT_SHAPE_RECTANGLE:
        if (object->gid != 0)
        {
            // tile objects are anchored at bottom-left
            DrawRectanglePro((Rectangle){ object->x, object->y, object->width, object->height },
                (Vector2){ 0.0f, object->height }, object->rotation, Fade(color, 0.2f));
        }
        else if (object->width > 0.0f && object->height > 0.0f)
        {
            DrawRectanglePro((Rectangle){ object->x, object->y, object->width, object->height },
                (Vector2){}, object->rotation, Fade(color, 0.2f));
        }
        else
        {
            // Tiled draws rectangles without size as points
            DrawCircleV(position, 3.0f, color);
        }
        break;
    case OBJECT_SHAPE_ELLIPSE:
        DrawEllipseLines((int)(object->x + object->width / 2.0f), (int)(object->y + object->height / 2.0f),
            object->width / 2.0f, object->height / 2.0f, color);
        break;
    case OBJECT_SHAPE_POINT:
        DrawCircleV(position, 3.0f, color);
        break;
    case OBJECT_SHAPE_POLYGON:
    case OBJECT_SHAPE_POLYLINE:
    {
        const int count = object->pointCount;
        const int segments = object->shape == OBJECT_SHAPE_POLYGON ? count : count - 1;
        for (int i = 0; i < segments; ++i)
        {
            const Vector2 a = object->points[i];
            const Vector2 b = object->points[(i + 1) % count];
            DrawLineEx((Vector2){ position.x + a.x, position.y + a.y }, (Vector2){ position.x + b.x, position.y + b.y }, 1.0f, color);
        }
        break;
    }
    }

    const char* label = object->name && object->name[0] != '\0' ? object->name : object->type;
    if (label && label[0] != '\0')
    {
        DrawText(label, (int)object->x, (int)object->y - 10, 10, color);
    }
}

void DrawMapObjectsDebug(const Map* map)
{
    for (int i = 0; i < map->objectGroupCount; ++i)
    {
        const ObjectGroup* group = &map->objectGroups[i];
        for (int j = 0; j < group->objectCount; ++j)
        {
            DrawObjectDebug(&group->objects[j], RED);
        }
    }
}
