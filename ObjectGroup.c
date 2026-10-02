#include "ObjectGroup.h"
#include "Utils.h"


static bool ParsePoints(const char* text, MapObject* object)
{
    if (!text)
    {
        return false;
    }

    // format: "x1,y1 x2,y2 ...", every point has exactly one comma
    int capacity = 0;
    for (const char* c = text; *c; ++c)
    {
        capacity += *c == ',';
    }
    if (capacity == 0)
    {
        return false;
    }

    object->points = calloc((size_t)capacity, sizeof *object->points);
    if (!object->points)
    {
        return false;
    }

    const char* cursor = text;
    while (*cursor)
    {
        char* end = nullptr;
        const float x = strtof(cursor, &end);
        if (end == cursor || *end != ',' || object->pointCount == capacity)
        {
            return false;
        }

        cursor = end + 1;
        const float y = strtof(cursor, &end);
        if (end == cursor)
        {
            return false;
        }

        object->points[object->pointCount++] = (Vector2){ x, y };
        cursor = end;
        while (*cursor == ' ')
        {
            ++cursor;
        }
    }
    return true;
}

static bool ParseProperties(const XmlElement* propertiesElement, MapObject* object)
{
    const int count = CountXmlChildren(propertiesElement, "property");
    if (count == 0)
    {
        return true;
    }

    object->properties = calloc((size_t)count, sizeof *object->properties);
    if (!object->properties)
    {
        return false;
    }

    XML_FOR_EACH_CHILD(propertyElement, propertiesElement, "property")
    {
        const char* name = GetXmlAttribute(propertyElement, "name");
        if (!name)
        {
            continue;
        }

        // multiline string values are stored as element text
        const char* value = GetXmlAttribute(propertyElement, "value");
        if (!value)
        {
            value = propertyElement->text ? propertyElement->text : "";
        }

        ObjectProperty* property = &object->properties[object->propertyCount];
        property->name = CopyString(name);
        property->value = CopyString(value);
        if (!property->name || !property->value)
        {
            free(property->name);
            free(property->value);
            return false;
        }
        ++object->propertyCount;
    }
    return true;
}

static void UnloadObject(MapObject* object)
{
    for (int i = 0; i < object->propertyCount; ++i)
    {
        free(object->properties[i].name);
        free(object->properties[i].value);
    }
    free(object->properties);
    free(object->points);
    free(object->name);
    free(object->type);
    *object = (MapObject){};
}

static bool ParseObject(const XmlElement* objectElement, MapObject* object)
{
    *object = (MapObject){ .visible = true, .shape = OBJECT_SHAPE_RECTANGLE };

    QueryXmlInt(objectElement, "id", &object->id);
    QueryXmlFloat(objectElement, "x", &object->x);
    QueryXmlFloat(objectElement, "y", &object->y);
    QueryXmlFloat(objectElement, "width", &object->width);
    QueryXmlFloat(objectElement, "height", &object->height);
    QueryXmlFloat(objectElement, "rotation", &object->rotation);
    QueryXmlBool(objectElement, "visible", &object->visible);

    object->name = CopyString(GetXmlAttribute(objectElement, "name"));

    // Tiled 1.9+ writes "class" instead of "type"
    const char* type = GetXmlAttribute(objectElement, "type");
    object->type = CopyString(type ? type : GetXmlAttribute(objectElement, "class"));

    // gid can use the highest bit for flip flag, so it doesn't fit into int
    const char* gid = GetXmlAttribute(objectElement, "gid");
    if (gid)
    {
        object->gid = (uint32_t)strtoul(gid, nullptr, 10);
    }

    bool ok = true;
    const XmlElement* polygon = FirstXmlChild(objectElement, "polygon");
    const XmlElement* polyline = FirstXmlChild(objectElement, "polyline");
    if (FirstXmlChild(objectElement, "ellipse"))
    {
        object->shape = OBJECT_SHAPE_ELLIPSE;
    }
    else if (FirstXmlChild(objectElement, "point"))
    {
        object->shape = OBJECT_SHAPE_POINT;
    }
    else if (polygon)
    {
        object->shape = OBJECT_SHAPE_POLYGON;
        ok = ParsePoints(GetXmlAttribute(polygon, "points"), object);
    }
    else if (polyline)
    {
        object->shape = OBJECT_SHAPE_POLYLINE;
        ok = ParsePoints(GetXmlAttribute(polyline, "points"), object);
    }

    const XmlElement* propertiesElement = FirstXmlChild(objectElement, "properties");
    if (ok && propertiesElement)
    {
        ok = ParseProperties(propertiesElement, object);
    }

    if (!ok)
    {
        UnloadObject(object);
    }
    return ok;
}

bool ParseObjectGroup(ObjectGroup* group, const XmlElement* groupElement)
{
    *group = (ObjectGroup){ .visible = true, .opacity = 1.0f };

    group->name = CopyString(GetXmlAttribute(groupElement, "name"));
    QueryXmlInt(groupElement, "id", &group->id);
    QueryXmlFloat(groupElement, "opacity", &group->opacity);
    QueryXmlBool(groupElement, "visible", &group->visible);

    const int count = CountXmlChildren(groupElement, "object");
    if (count == 0)
    {
        return true;
    }

    group->objects = calloc((size_t)count, sizeof *group->objects);
    if (!group->objects)
    {
        UnloadObjectGroup(group);
        return false;
    }

    XML_FOR_EACH_CHILD(objectElement, groupElement, "object")
    {
        if (!ParseObject(objectElement, &group->objects[group->objectCount]))
        {
            UnloadObjectGroup(group);
            return false;
        }
        ++group->objectCount;
    }
    return true;
}

void UnloadObjectGroup(ObjectGroup* group)
{
    for (int i = 0; i < group->objectCount; ++i)
    {
        UnloadObject(&group->objects[i]);
    }
    free(group->objects);
    free(group->name);
    *group = (ObjectGroup){};
}

const MapObject* FindObject(const ObjectGroup* group, const char* name)
{
    for (int i = 0; i < group->objectCount; ++i)
    {
        if (StringEquals(group->objects[i].name, name))
        {
            return &group->objects[i];
        }
    }
    return nullptr;
}

int FindObjectsByType(const ObjectGroup* group, const char* type, const MapObject** result, int capacity)
{
    int count = 0;
    for (int i = 0; i < group->objectCount; ++i)
    {
        if (StringEquals(group->objects[i].type, type))
        {
            if (count < capacity)
            {
                result[count] = &group->objects[i];
            }
            ++count;
        }
    }
    return count;
}

const char* GetObjectProperty(const MapObject* object, const char* name)
{
    for (int i = 0; i < object->propertyCount; ++i)
    {
        if (strcmp(object->properties[i].name, name) == 0)
        {
            return object->properties[i].value;
        }
    }
    return nullptr;
}
