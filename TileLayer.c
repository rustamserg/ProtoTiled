#include "TileLayer.h"
#include "Utils.h"

#include <stdckdint.h>


static bool ParseCsv(TileLayer* layer, const char* text, size_t cellCount)
{
    if (!text)
    {
        return false;
    }

    layer->data = malloc(cellCount * sizeof *layer->data);
    if (!layer->data)
    {
        return false;
    }

    size_t count = 0;
    const char* cursor = text;
    while (*cursor)
    {
        char* end = nullptr;
        const unsigned long gid = strtoul(cursor, &end, 10);
        if (end == cursor)
        {
            // skip separators and whitespace
            ++cursor;
            continue;
        }
        if (count == cellCount)
        {
            return false;
        }
        layer->data[count++] = (uint32_t)gid;
        cursor = end;
    }
    return count == cellCount;
}

static bool ParseTileLayerData(TileLayer* layer, const XmlElement* layerElement)
{
    layer->name = CopyString(GetXmlAttribute(layerElement, "name"));
    QueryXmlInt(layerElement, "id", &layer->id);
    QueryXmlInt(layerElement, "width", &layer->width);
    QueryXmlInt(layerElement, "height", &layer->height);
    QueryXmlFloat(layerElement, "opacity", &layer->opacity);
    QueryXmlBool(layerElement, "visible", &layer->visible);
    QueryXmlBool(layerElement, "locked", &layer->locked);

    size_t cellCount = 0;
    if (layer->width <= 0 || layer->height <= 0 || ckd_mul(&cellCount, (size_t)layer->width, (size_t)layer->height))
    {
        return false;
    }

    const XmlElement* dataElement = FirstXmlChild(layerElement, "data");
    if (!dataElement)
    {
        return false;
    }

    // only csv encoding is supported for now
    if (!StringEquals(GetXmlAttribute(dataElement, "encoding"), "csv"))
    {
        return false;
    }

    return ParseCsv(layer, dataElement->text, cellCount);
}

bool ParseTileLayer(TileLayer* layer, const XmlElement* layerElement)
{
    *layer = (TileLayer){ .visible = true, .opacity = 1.0f };
    if (!ParseTileLayerData(layer, layerElement))
    {
        UnloadTileLayer(layer);
        return false;
    }
    return true;
}

void UnloadTileLayer(TileLayer* layer)
{
    free(layer->name);
    free(layer->data);
    *layer = (TileLayer){};
}

TileCell DecodeGid(uint32_t rawGid)
{
    return (TileCell){
        .gid = rawGid & ~GID_FLAGS_MASK,
        .flippedHorizontally = (rawGid & FLIPPED_HORIZONTALLY_FLAG) != 0,
        .flippedVertically = (rawGid & FLIPPED_VERTICALLY_FLAG) != 0,
        .flippedDiagonally = (rawGid & FLIPPED_DIAGONALLY_FLAG) != 0,
    };
}
