// Code/Tools/BF2Editor/Framework/RoadFiles.cpp - see RoadFiles.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/RoadFiles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>

namespace bf2editor
{

namespace
{

std::string trimmed(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// The line without blanks, lowered: "if v_arg1 == BF2Editor" -> "ifv_arg1==bf2editor".
std::string compact(const std::string& line)
{
    std::string flat;
    for (size_t i = 0; i < line.size(); ++i)
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
            flat += static_cast<char>(std::tolower(static_cast<unsigned char>(line[i])));
    return flat;
}

std::string vec2Text(const float v[2])
{
    return formatConsoleFloat(v[0]) + "/" + formatConsoleFloat(v[1]);
}

bool isComment(const std::string& line)
{
    const std::string low = lowered(line.substr(0, 4));
    return low == "rem" || (low.size() == 4 && low.compare(0, 3, "rem") == 0 && (low[3] == ' ' || low[3] == '\t'));
}

void putU32(std::string& data, unsigned value)
{
    char bytes[4];
    std::memcpy(bytes, &value, 4);
    data.append(bytes, 4);
}

void putF32(std::string& data, float value)
{
    char bytes[4];
    std::memcpy(bytes, &value, 4);
    data.append(bytes, 4);
}

// A little-endian reader over a byte string; every read fails once the data runs out.
class MeshReader
{
public:
    explicit MeshReader(const std::string& data) : m_data(data), m_offset(0), m_ok(true) {}
    template <typename T> T get()
    {
        T value = T();
        if (!m_ok || m_data.size() - m_offset < sizeof(T))
        {
            m_ok = false;
            return value;
        }
        std::memcpy(&value, m_data.data() + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return value;
    }
    void skip(size_t bytes)
    {
        if (!m_ok || m_data.size() - m_offset < bytes)
            m_ok = false;
        else
            m_offset += bytes;
    }
    bool ok() const { return m_ok; }
    size_t offset() const { return m_offset; }

private:
    const std::string& m_data;
    size_t             m_offset;
    bool               m_ok;
};

// The vertex record's size in a mesh of this version, as the 1.50 reader reads it.
size_t vertexSize(int major, int minor)
{
    size_t size = 12 + 8 + 8;
    if (major == 1 && minor == 2)
        size += 12 + 16;
    if (major >= 1 && minor >= 3)
        size += 4;
    return size;
}

} // namespace

std::string formatConsoleFloat(float value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string splinesConText(const std::vector<SplineRecord>& splines)
{
    if (splines.empty())
        return std::string();
    std::ostringstream out;
    for (size_t s = 0; s < splines.size(); ++s)
    {
        const SplineRecord& spline = splines[s];
        if (spline.points.empty())
            continue;
        out << "rem ** [Spline] - " << spline.id << " **\n";
        out << "SplineManager.CreateInstance\n";
        out << "SplineManager.SetId " << spline.id << "\n";
        out << "SplineManager.SetControlPointSnapModes " << spline.snapModes << "\n";
        out << "SplineManager.setLineWidth " << formatConsoleFloat(spline.lineWidth) << "\n";
        out << "SplineManager.setStepSize " << formatConsoleFloat(spline.stepSize) << "\n";
        for (size_t p = 0; p < spline.points.size(); ++p)
        {
            const SplinePointRecord& point = spline.points[p];
            out << "SplineManager.AddControlPoint " << formatConsoleFloat(point.position[0]) << "/"
                << formatConsoleFloat(point.position[1]) << "/" << formatConsoleFloat(point.position[2]) << "\n";
            if (60.0 != point.speed)
                out << "SplineManager.SetSpeed " << formatConsoleFloat(point.speed) << "\n";
            if (0.0 != point.delay)
                out << "SplineManager.SetDelay " << formatConsoleFloat(point.delay) << "\n";
            if (point.direction)
                out << "SplineManager.SetDirection " << point.direction << "\n";
            if (!point.sound.empty())
                out << "SplineManager.SetSound " << point.sound << "\n";
        }
        out << "SplineManager.End\n\n";
    }
    out << "\n";
    return out.str();
}

std::string roadsConText(const std::vector<RoadRecord>& roads)
{
    if (roads.empty())
        return std::string();
    std::ostringstream out;
    for (size_t r = 0; r < roads.size(); ++r)
    {
        const RoadRecord& road = roads[r];
        out << "rem ** [Road] - " << road.name << "**\n";
        out << "Road.CreateInstance \"" << road.name << "\" " << road.splineId << " \"" << road.templateName << "\" "
            << road.seed << "\n";
        out << "Road.SetTextureScaleOffset " << vec2Text(road.textureScaleOffset) << "\n";
        out << "Road.SetPersistance " << formatConsoleFloat(road.persistence) << "\n";
        out << "Road.SetRange " << vec2Text(road.range) << "\n";
        out << "Road.SetOctaves " << road.octaves << "\n";
        out << "Road.Prio " << road.priority << "\n";
        out << "Road.FadeDistAtStartLeft " << vec2Text(road.fadeStartLeft) << "\n";
        out << "Road.FadeDistAtStartRight " << vec2Text(road.fadeStartRight) << "\n";
        out << "Road.FadeDistAtEndLeft " << vec2Text(road.fadeEndLeft) << "\n";
        out << "Road.FadeDistAtEndRight " << vec2Text(road.fadeEndRight) << "\n";
        out << "\n";
    }
    out << "\n";
    return out.str();
}

std::string roadTemplateConText(const RoadTemplateRecord& t)
{
    std::ostringstream out;
    out << "if v_arg1 == BF2Editor\n";
    out << "rem ** [RoadTemplate] - " << t.name << " **\n";
    out << "RoadTemplate.CreateInstance\n";
    out << "RoadTemplate.SetName \"" << t.name << "\"\n";
    // The retail writer truncated the width to a whole number (an integer conversion before the stream).
    out << "RoadTemplate.SetWidth " << static_cast<long long>(t.width) << "\n";
    out << "RoadTemplate.SetMaxTesselatedError " << formatConsoleFloat(t.maxTesselatedError) << "\n";
    out << "RoadTemplate.SetBlendFactor " << formatConsoleFloat(t.blendFactor) << "\n";
    out << "RoadTemplate.SetPrio " << t.priority << "\n";
    if (t.fadeOffDistance[0] != 200.0f || t.fadeOffDistance[1] != 300.0f)
        out << "RoadTemplate.SetFadeOffDistance " << vec2Text(t.fadeOffDistance) << "\n";
    if (!t.material.empty())
        out << "RoadTemplate.SetMaterial " << t.material << "\n";
    for (int layer = 0; layer < 2; ++layer)
    {
        out << "RoadTemplate.SetIsPrimaryTexture " << (layer == 0 ? 1 : 0) << "\n";
        out << "RoadTemplateTexture.SetTextureFile \"" << t.textures[layer] << "\"\n";
        out << "RoadTemplateTexture.SetMirror " << (t.mirror[layer] ? 1 : 0) << "\n";
        out << "RoadTemplateTexture.SetScale " << vec2Text(t.scale[layer]) << "\n";
    }
    out << "endIf\n\n";
    return out.str();
}

bool splitConsoleRow(const std::string& line, ConsoleRow& row)
{
    row = ConsoleRow();
    const std::string text = trimmed(line);
    if (text.empty() || isComment(text))
        return false;
    size_t i = 0;
    std::vector<std::string> words;
    while (i < text.size())
    {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
            ++i;
        if (i >= text.size())
            break;
        std::string word;
        if (text[i] == '"')
        {
            const size_t close = text.find('"', i + 1);
            word = text.substr(i + 1, close == std::string::npos ? std::string::npos : close - i - 1);
            i = close == std::string::npos ? text.size() : close + 1;
        }
        else
        {
            const size_t end = text.find_first_of(" \t", i);
            word = text.substr(i, end == std::string::npos ? std::string::npos : end - i);
            i = end == std::string::npos ? text.size() : end;
        }
        words.push_back(word);
    }
    if (words.empty())
        return false;
    const size_t dot = words[0].find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= words[0].size())
        return false;
    row.object = words[0].substr(0, dot);
    row.method = words[0].substr(dot + 1);
    row.args.assign(words.begin() + 1, words.end());
    return true;
}

std::vector<ConsoleRow> consoleRows(const std::string& text, const std::string& argument, int& skipped)
{
    skipped = 0;
    std::vector<ConsoleRow> rows;
    std::istringstream in(text);
    std::string line;
    const std::string wanted = "ifv_arg1==" + lowered(argument);
    bool inside = argument.empty();
    bool taking = argument.empty();
    int nested = 0;
    while (std::getline(in, line))
    {
        const std::string flat = compact(line);
        if (!argument.empty())
        {
            if (!inside)
            {
                if (flat == wanted)
                    inside = taking = true;
                continue;
            }
            if (flat.compare(0, 2, "if") == 0 && flat.find("==") != std::string::npos)
            {
                ++nested;
                ++skipped;
                continue;
            }
            if (flat == "endif")
            {
                if (nested)
                    --nested;
                else
                    inside = taking = false;
                continue;
            }
            if (flat == "else" && !nested)
            {
                taking = false;
                continue;
            }
            if (nested || !taking)
                continue;
        }
        ConsoleRow row;
        if (splitConsoleRow(line, row))
            rows.push_back(row);
    }
    return rows;
}

bool parseVec2(const std::string& text, float out[2])
{
    float a = 0.0f, b = 0.0f;
    char tail = 0;
    if (std::sscanf(text.c_str(), "%f/%f%c", &a, &b, &tail) != 2)
        return false;
    out[0] = a;
    out[1] = b;
    return true;
}

bool parseVec3(const std::string& text, float out[3])
{
    float a = 0.0f, b = 0.0f, c = 0.0f;
    char tail = 0;
    if (std::sscanf(text.c_str(), "%f/%f/%f%c", &a, &b, &c, &tail) != 3)
        return false;
    out[0] = a;
    out[1] = b;
    out[2] = c;
    return true;
}

CompiledRoadsFile parseCompiledRoads(const std::string& text)
{
    CompiledRoadsFile file;
    file.newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        const std::string t = trimmed(line);
        const size_t space = t.find_first_of(" \t");
        const std::string command = lowered(space == std::string::npos ? t : t.substr(0, space));
        if (command == "object.create")
        {
            file.blocks.push_back(CompiledRoadBlock());
            file.blocks.back().lines.push_back(line);
            continue;
        }
        if (file.blocks.empty())
        {
            file.preamble.push_back(line);
            continue;
        }
        if (t.empty())
            continue;
        CompiledRoadBlock& block = file.blocks.back();
        block.lines.push_back(line);
        if (command == "object.geometry.loadmesh" && space != std::string::npos)
            block.mesh = trimmed(t.substr(space + 1));
    }
    // Trailing blank lines before the first block belong to the separation, not the preamble.
    while (!file.preamble.empty() && trimmed(file.preamble.back()).empty() && !file.blocks.empty())
        file.preamble.pop_back();
    return file;
}

std::string compiledRoadsText(const CompiledRoadsFile& file)
{
    std::string text;
    for (size_t i = 0; i < file.preamble.size(); ++i)
        text += file.preamble[i] + file.newline;
    if (!file.preamble.empty() && !file.blocks.empty())
        text += file.newline;
    for (size_t b = 0; b < file.blocks.size(); ++b)
    {
        for (size_t i = 0; i < file.blocks[b].lines.size(); ++i)
            text += file.blocks[b].lines[i] + file.newline;
        text += file.newline;
    }
    return text;
}

bool compiledMeshIsRoad(const std::string& mesh, const std::string& name)
{
    std::string path = lowered(mesh);
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::string tail = "roads/" + lowered(name) + "_compiled.mesh";
    if (path.size() < tail.size() || path.compare(path.size() - tail.size(), tail.size(), tail) != 0)
        return false;
    return path.size() == tail.size() || path[path.size() - tail.size() - 1] == '/';
}

bool readRoadMeshInfo(const std::string& data, RoadMeshInfo& info, std::string& error)
{
    info = RoadMeshInfo();
    MeshReader in(data);
    info.versionMinor = in.get<unsigned short>();
    info.versionMajor = in.get<unsigned short>();
    in.skip(12 + 4 + 12 + 12 + 4);           // origin, radius, bounds, order
    info.vertices = in.get<unsigned>();
    if (!in.ok())
    {
        error = "the mesh's header is cut short";
        return false;
    }
    const size_t stride = vertexSize(info.versionMajor, info.versionMinor);
    if (info.vertices > (data.size() - in.offset()) / stride)
    {
        error = "the mesh's vertices are cut short";
        return false;
    }
    in.skip(static_cast<size_t>(info.vertices) * stride);
    info.indices = in.get<unsigned>();
    if (!in.ok() || info.indices > (data.size() - in.offset()) / 2)
    {
        error = "the mesh's indices are cut short";
        return false;
    }
    in.skip(static_cast<size_t>(info.indices) * 2);
    if (in.offset() < data.size())
    {
        info.chunks = in.get<unsigned>();
        if (!in.ok() || info.chunks > (data.size() - in.offset()) / 24)
        {
            error = "the mesh's chunk table is cut short";
            return false;
        }
        info.hasChunkTable = true;
    }
    return true;
}

bool appendRoadMeshChunkTable(std::string& data, RoadMeshInfo& info, std::string& error)
{
    if (!readRoadMeshInfo(data, info, error))
        return false;
    if (info.versionMajor != 1 || info.versionMinor != 4)
    {
        char text[96];
        std::snprintf(text, sizeof(text), "the mesh is version %d.%d, not 1.4", info.versionMajor, info.versionMinor);
        error = text;
        return false;
    }
    if (info.hasChunkTable)
    {
        error = "the mesh already has a chunk table";
        return false;
    }
    // The vertices in world space (the file holds them relative to the mesh's origin) and the indices.
    MeshReader in(data);
    in.skip(4);
    float origin[3];
    for (int k = 0; k < 3; ++k)
        origin[k] = in.get<float>();
    in.skip(4 + 12 + 12 + 4 + 4);
    std::vector<float> positions(static_cast<size_t>(info.vertices) * 3);
    for (unsigned v = 0; v < info.vertices; ++v)
    {
        for (int k = 0; k < 3; ++k)
            positions[v * 3 + k] = in.get<float>() + origin[k];
        in.skip(8 + 8 + 4);
    }
    in.skip(4);
    std::vector<unsigned short> indices(info.indices);
    for (unsigned i = 0; i < info.indices; ++i)
        indices[i] = in.get<unsigned short>();
    if (!in.ok())
    {
        error = "the mesh could not be read";
        return false;
    }
    for (unsigned i = 0; i < info.indices; ++i)
        if (indices[i] >= info.vertices)
        {
            error = "the mesh's indices name vertices it does not have";
            return false;
        }

    const unsigned kTriangleLimit = 1024;
    const float kHalfDiagonalLimit = 32.0f;
    struct Chunk { unsigned first; unsigned count; };
    std::vector<Chunk> chunks;
    float lo[2] = { 0.0f, 0.0f }, hi[2] = { 0.0f, 0.0f };
    const unsigned triangles = info.indices / 3;
    for (unsigned t = 0; t < triangles; ++t)
    {
        float tlo[2], thi[2];
        for (int k = 0; k < 2; ++k)
        {
            const int axis = k == 0 ? 0 : 2;
            tlo[k] = thi[k] = positions[indices[t * 3] * 3 + axis];
            for (int c = 1; c < 3; ++c)
            {
                const float value = positions[indices[t * 3 + c] * 3 + axis];
                tlo[k] = std::min(tlo[k], value);
                thi[k] = std::max(thi[k], value);
            }
        }
        bool start = chunks.empty() || chunks.back().count / 3 >= kTriangleLimit;
        if (!start)
        {
            const float dx = std::max(hi[0], thi[0]) - std::min(lo[0], tlo[0]);
            const float dz = std::max(hi[1], thi[1]) - std::min(lo[1], tlo[1]);
            start = 0.5f * std::sqrt(dx * dx + dz * dz) > kHalfDiagonalLimit;
        }
        if (start)
        {
            Chunk chunk = { t * 3, 0 };
            chunks.push_back(chunk);
            lo[0] = tlo[0]; lo[1] = tlo[1];
            hi[0] = thi[0]; hi[1] = thi[1];
        }
        else
        {
            lo[0] = std::min(lo[0], tlo[0]); lo[1] = std::min(lo[1], tlo[1]);
            hi[0] = std::max(hi[0], thi[0]); hi[1] = std::max(hi[1], thi[1]);
        }
        chunks.back().count += 3;
    }

    std::string table;
    putU32(table, static_cast<unsigned>(chunks.size()));
    for (size_t c = 0; c < chunks.size(); ++c)
    {
        std::set<unsigned short> used(indices.begin() + chunks[c].first,
                                      indices.begin() + chunks[c].first + chunks[c].count);
        double centre[3] = { 0.0, 0.0, 0.0 };
        for (std::set<unsigned short>::const_iterator v = used.begin(); v != used.end(); ++v)
            for (int k = 0; k < 3; ++k)
                centre[k] += positions[*v * 3 + k];
        for (int k = 0; k < 3; ++k)
            centre[k] /= static_cast<double>(used.size());
        double radius = 0.0;
        for (std::set<unsigned short>::const_iterator v = used.begin(); v != used.end(); ++v)
        {
            const double dx = positions[*v * 3] - centre[0];
            const double dy = positions[*v * 3 + 1] - centre[1];
            const double dz = positions[*v * 3 + 2] - centre[2];
            radius = std::max(radius, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        putU32(table, chunks[c].first);
        putU32(table, chunks[c].count);
        putF32(table, static_cast<float>(radius));
        for (int k = 0; k < 3; ++k)
            putF32(table, static_cast<float>(centre[k]));
    }
    data += table;
    info.chunks = static_cast<unsigned>(chunks.size());
    info.hasChunkTable = true;
    return true;
}

bool roadProjectionCameBackFlat(const SplineRecord& spline, const std::vector<float>& heights)
{
    if (spline.points.size() < 4 || heights.size() < 8)
        return false;
    float lowest = spline.points[1].position[1];
    float highest = lowest;
    for (size_t i = 1; i + 1 < spline.points.size(); ++i)
    {
        lowest = std::min(lowest, spline.points[i].position[1]);
        highest = std::max(highest, spline.points[i].position[1]);
    }
    const std::pair<std::vector<float>::const_iterator, std::vector<float>::const_iterator> range =
        std::minmax_element(heights.begin(), heights.end());
    return highest - lowest > 0.5f && *range.second - *range.first < 0.01f;
}

} // namespace bf2editor
