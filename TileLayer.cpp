#include "TileLayer.h"
#include "tinyxml.h"

#include <cstdlib>
#include <cstring>


bool TileLayer::Parse(const TiXmlElement* layerElement)
{
    if (!layerElement)
    {
        return false;
    }

    if (const auto nameStr = layerElement->Attribute("name"))
    {
        m_name = nameStr;
    }

    layerElement->QueryIntAttribute("id", &m_id);
    layerElement->QueryIntAttribute("width", &m_width);
    layerElement->QueryIntAttribute("height", &m_height);
    layerElement->QueryFloatAttribute("opacity", &m_opacity);

    int visible = 1, locked = 0;
    layerElement->QueryIntAttribute("visible", &visible);
    layerElement->QueryIntAttribute("locked", &locked);
    m_visible = visible != 0;
    m_locked = locked != 0;

    if (m_width <= 0 || m_height <= 0)
    {
        return false;
    }

    const auto dataElement = layerElement->FirstChildElement("data");
    if (!dataElement)
    {
        return false;
    }

    // only csv encoding is supported for now
    const auto encoding = dataElement->Attribute("encoding");
    if (!encoding || strcmp(encoding, "csv") != 0)
    {
        return false;
    }

    return ParseCsv(dataElement->GetText());
}

bool TileLayer::ParseCsv(const char* text)
{
    m_data.clear();
    m_data.reserve(m_width * m_height);

    const char* cursor = text;
    while (cursor && *cursor)
    {
        char* end = nullptr;
        const unsigned long gid = strtoul(cursor, &end, 10);
        if (end == cursor)
        {
            // skip separators and whitespace
            ++cursor;
            continue;
        }
        m_data.push_back(static_cast<uint32_t>(gid));
        cursor = end;
    }

    return m_data.size() == static_cast<size_t>(m_width * m_height);
}

TileLayer::Cell TileLayer::GetCell(int x, int y) const
{
    const uint32_t raw = GetRawGid(x, y);

    Cell cell{};
    cell.gid = raw & ~FLAGS_MASK;
    cell.flippedHorizontally = (raw & FLIPPED_HORIZONTALLY_FLAG) != 0;
    cell.flippedVertically = (raw & FLIPPED_VERTICALLY_FLAG) != 0;
    cell.flippedDiagonally = (raw & FLIPPED_DIAGONALLY_FLAG) != 0;
    return cell;
}
