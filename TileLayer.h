#pragma once

#include <cstdint>
#include <string>
#include <vector>

class TiXmlElement;

class TileLayer
{
public:
    // Tiled stores flip/rotation flags in the highest bits of each gid
    static constexpr uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    static constexpr uint32_t FLIPPED_VERTICALLY_FLAG = 0x40000000;
    static constexpr uint32_t FLIPPED_DIAGONALLY_FLAG = 0x20000000;
    static constexpr uint32_t ROTATED_HEXAGONAL_120_FLAG = 0x10000000;
    static constexpr uint32_t FLAGS_MASK = FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG | ROTATED_HEXAGONAL_120_FLAG;

    struct Cell
    {
        uint32_t gid;   // global tile id without flags, 0 means empty cell
        bool flippedHorizontally;
        bool flippedVertically;
        bool flippedDiagonally;
    };

public:
    bool Parse(const TiXmlElement* layerElement);

    int GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsVisible() const { return m_visible; }
    bool IsLocked() const { return m_locked; }
    float GetOpacity() const { return m_opacity; }

    bool IsInside(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }
    uint32_t GetRawGid(int x, int y) const { return m_data[y * m_width + x]; }
    uint32_t GetGid(int x, int y) const { return GetRawGid(x, y) & ~FLAGS_MASK; }
    Cell GetCell(int x, int y) const;

private:
    bool ParseCsv(const char* text);

private:
    int m_id = 0;
    std::string m_name;
    int m_width = 0;
    int m_height = 0;
    bool m_visible = true;
    bool m_locked = false;
    float m_opacity = 1.0f;
    std::vector<uint32_t> m_data;
};
