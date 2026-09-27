#include "ObjectGroup.h"
#include "tinyxml.h"

#include <cstdlib>


bool ObjectGroup::Parse(const TiXmlElement* groupElement)
{
    if (!groupElement)
    {
        return false;
    }

    if (const auto nameStr = groupElement->Attribute("name"))
    {
        m_name = nameStr;
    }

    groupElement->QueryIntAttribute("id", &m_id);
    groupElement->QueryFloatAttribute("opacity", &m_opacity);

    int visible = 1;
    groupElement->QueryIntAttribute("visible", &visible);
    m_visible = visible != 0;

    for (auto objectElement = groupElement->FirstChildElement("object"); objectElement; objectElement = objectElement->NextSiblingElement("object"))
    {
        Object object;
        if (!ParseObject(objectElement, object))
        {
            return false;
        }
        m_objects.push_back(std::move(object));
    }
    return true;
}

bool ObjectGroup::ParseObject(const TiXmlElement* objectElement, Object& object)
{
    objectElement->QueryIntAttribute("id", &object.id);
    objectElement->QueryFloatAttribute("x", &object.x);
    objectElement->QueryFloatAttribute("y", &object.y);
    objectElement->QueryFloatAttribute("width", &object.width);
    objectElement->QueryFloatAttribute("height", &object.height);
    objectElement->QueryFloatAttribute("rotation", &object.rotation);

    if (const auto nameStr = objectElement->Attribute("name"))
    {
        object.name = nameStr;
    }

    // Tiled 1.9+ writes "class" instead of "type"
    if (const auto typeStr = objectElement->Attribute("type"))
    {
        object.type = typeStr;
    }
    else if (const auto classStr = objectElement->Attribute("class"))
    {
        object.type = classStr;
    }

    // gid can use the highest bit for flip flag, so it doesn't fit into int
    if (const auto gidStr = objectElement->Attribute("gid"))
    {
        object.gid = static_cast<uint32_t>(strtoul(gidStr, nullptr, 10));
    }

    int visible = 1;
    objectElement->QueryIntAttribute("visible", &visible);
    object.visible = visible != 0;

    if (objectElement->FirstChildElement("ellipse"))
    {
        object.shape = Shape::Ellipse;
    }
    else if (objectElement->FirstChildElement("point"))
    {
        object.shape = Shape::Point;
    }
    else if (const auto polygon = objectElement->FirstChildElement("polygon"))
    {
        object.shape = Shape::Polygon;
        if (!ParsePoints(polygon->Attribute("points"), object.points))
        {
            return false;
        }
    }
    else if (const auto polyline = objectElement->FirstChildElement("polyline"))
    {
        object.shape = Shape::Polyline;
        if (!ParsePoints(polyline->Attribute("points"), object.points))
        {
            return false;
        }
    }

    if (const auto propertiesElement = objectElement->FirstChildElement("properties"))
    {
        for (auto property = propertiesElement->FirstChildElement("property"); property; property = property->NextSiblingElement("property"))
        {
            const auto name = property->Attribute("name");
            if (!name)
            {
                continue;
            }

            // multiline string values are stored as element text
            const auto value = property->Attribute("value");
            const auto text = property->GetText();
            object.properties[name] = value ? value : (text ? text : "");
        }
    }
    return true;
}

bool ObjectGroup::ParsePoints(const char* text, std::vector<Vector2>& points)
{
    if (!text)
    {
        return false;
    }

    // format: "x1,y1 x2,y2 ..."
    const char* cursor = text;
    while (*cursor)
    {
        char* end = nullptr;
        const float x = strtof(cursor, &end);
        if (end == cursor || *end != ',')
        {
            return false;
        }

        cursor = end + 1;
        const float y = strtof(cursor, &end);
        if (end == cursor)
        {
            return false;
        }

        points.push_back({ x, y });
        cursor = end;
        while (*cursor == ' ')
        {
            ++cursor;
        }
    }
    return !points.empty();
}

const ObjectGroup::Object* ObjectGroup::FindObject(const std::string& name) const
{
    for (const auto& object : m_objects)
    {
        if (object.name == name)
        {
            return &object;
        }
    }
    return nullptr;
}

std::vector<const ObjectGroup::Object*> ObjectGroup::FindObjectsByType(const std::string& type) const
{
    std::vector<const Object*> result;
    for (const auto& object : m_objects)
    {
        if (object.type == type)
        {
            result.push_back(&object);
        }
    }
    return result;
}
