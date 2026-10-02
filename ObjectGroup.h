#pragma once

#include <stdint.h>

#include "raylib.h"

#include "Xml.h"

typedef enum ObjectShape : uint8_t
{
    OBJECT_SHAPE_RECTANGLE,
    OBJECT_SHAPE_ELLIPSE,
    OBJECT_SHAPE_POINT,
    OBJECT_SHAPE_POLYGON,
    OBJECT_SHAPE_POLYLINE,
} ObjectShape;

typedef struct ObjectProperty
{
    char* name;
    char* value;
} ObjectProperty;

typedef struct MapObject
{
    int id;
    char* name;                 // nullptr when not set
    char* type;                 // nullptr when not set
    float x;
    float y;
    float width;
    float height;
    float rotation;             // degrees, clockwise
    uint32_t gid;               // raw gid with flip flags, non zero for tile objects
    bool visible;
    ObjectShape shape;
    Vector2* points;            // polygon/polyline points relative to (x, y)
    int pointCount;
    ObjectProperty* properties;
    int propertyCount;
} MapObject;

typedef struct ObjectGroup
{
    int id;
    char* name;                 // nullptr when not set
    bool visible;
    float opacity;
    MapObject* objects;
    int objectCount;
} ObjectGroup;

// on failure the group is left empty, nothing has to be unloaded
[[nodiscard]] bool ParseObjectGroup(ObjectGroup* group, const XmlElement* groupElement);
void UnloadObjectGroup(ObjectGroup* group);

[[nodiscard]] const MapObject* FindObject(const ObjectGroup* group, const char* name);

// stores up to capacity matching objects into result and returns the total number of matches
int FindObjectsByType(const ObjectGroup* group, const char* type, const MapObject** result, int capacity);

// returns property value or nullptr if the object has no such property
[[nodiscard]] const char* GetObjectProperty(const MapObject* object, const char* name);
