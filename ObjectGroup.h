#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

class TiXmlElement;

class ObjectGroup
{
public:
    enum class Shape
    {
        Rectangle,
        Ellipse,
        Point,
        Polygon,
        Polyline,
    };

    struct Object
    {
        int id = 0;
        std::string name;
        std::string type;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float rotation = 0.0f;  // degrees, clockwise
        uint32_t gid = 0;       // raw gid with flip flags, non zero for tile objects
        bool visible = true;
        Shape shape = Shape::Rectangle;
        std::vector<Vector2> points;    // polygon/polyline points relative to (x, y)
        std::unordered_map<std::string, std::string> properties;
    };

public:
    bool Parse(const TiXmlElement* groupElement);

    int GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    bool IsVisible() const { return m_visible; }
    float GetOpacity() const { return m_opacity; }

    const std::vector<Object>& GetObjects() const { return m_objects; }
    const Object* FindObject(const std::string& name) const;
    std::vector<const Object*> FindObjectsByType(const std::string& type) const;

private:
    static bool ParseObject(const TiXmlElement* objectElement, Object& object);
    static bool ParsePoints(const char* text, std::vector<Vector2>& points);

private:
    int m_id = 0;
    std::string m_name;
    bool m_visible = true;
    float m_opacity = 1.0f;
    std::vector<Object> m_objects;
};
