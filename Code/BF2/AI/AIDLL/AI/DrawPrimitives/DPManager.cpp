#include "DPManager.h"

// <iostream>'s ios_base::Init object is the static initialisation and teardown of DPManager.cpp
// (ELF 0x3797D0, __tcf_0 0x379830, _GLOBAL__I 0x379860). The four interface constructors below
// were separate TUs in retail (file symbols IDPAIObjectCreator.cpp, IDPDynamicCreator.cpp,
// IDPManager.cpp, IDPStaticCreator.cpp), each with the same Init object: 0x381640/0x3816A0/
// 0x3816D0, 0x3819F0/0x381A50/0x381A80, 0x381DA0/0x381E00/0x381E30, 0x382150/0x3821B0/0x3821E0.
// [INFERRED] keeping them in this TU.
#include <iostream>

#include <string>

#include "../../../../Core/ClassManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIMain.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AIDllFrontier.h"
#include "../AIUtils/DebugRendering/DPDynamicPrimitives.h"
#include "DPStatic2dText.h"
#include "DPStatic3dText.h"
#include "DPStaticCircle.h"
#include "DPStaticLine.h"
#include "DPStaticMesh.h"
#include "DPStaticPolygon.h"
#include "DPStaticTriangle.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IDPManager
IDPManager::IDPManager()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iDPManager)
        classManager->registerSimpleSingleton("IDPManager", this);
}

IDPManager::~IDPManager()
{
    if (CommonSingletonServer::m_iDPManager == this)
        classManager->unregisterSimpleSingleton("IDPManager");
}

// IDPStaticCreator
IDPStaticCreator::IDPStaticCreator()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!AISingletonServer::m_iDPDynamicCreator)
        classManager->registerSimpleSingleton("IDPStaticCreator", this);
}

IDPStaticCreator::~IDPStaticCreator()
{
    if (AISingletonServer::m_iDPDynamicCreator == this)
        classManager->unregisterSimpleSingleton("IDPStaticCreator");
}

// IDPDynamicCreator
IDPDynamicCreator::IDPDynamicCreator()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!AISingletonServer::m_iDPDynamicCreator)
        classManager->registerSimpleSingleton("IDPDynamicCreator", this);
}

IDPDynamicCreator::~IDPDynamicCreator()
{
    if (AISingletonServer::m_iDPDynamicCreator == this)
        classManager->unregisterSimpleSingleton("IDPDynamicCreator");
}

// IDPAIObjectCreator
IDPAIObjectCreator::IDPAIObjectCreator()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!AISingletonServer::m_iDPDynamicCreator)
        classManager->registerSimpleSingleton("IDPAIObjectCreator", this);
}

IDPAIObjectCreator::~IDPAIObjectCreator()
{
    if (AISingletonServer::m_iDPDynamicCreator == this)
        classManager->unregisterSimpleSingleton("IDPAIObjectCreator");
}

// Initializes primitive collections and their four interface bases.
DPManager::DPManager()
    : m_textGamma(1.0f)
{
}

// - seven tree teardowns, which is what ~DPManager emits on its own.
DPManager::~DPManager()
{
}

// The delete*, get*Attributes and set*Attributes slots share one shape: the handle is looked up in
// that kind's map with find(); an unknown handle answers 0 (or does nothing in the void slots).

// ELF 0x374260. A known handle deletes the value through its deleting destructor (null-tested),
// erases the node, nulls the caller's handle and answers 1.
int DPManager::deleteCircle(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStaticCircle*>::iterator found = m_circles.find(primitive);
    if (found == m_circles.end())
        return 0;
    delete found->second;
    m_circles.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374320.
int DPManager::getCircleAttributes(DrawPrimitive * primitive, float & lifeTime, unsigned int & colour,
                                   Vec3 & position, float & radius, Vec3 & normal, bool & filled,
                                   int & segments) const
{
    std::map<DrawPrimitive*, DPStaticCircle*>::const_iterator found = m_circles.find(primitive);
    if (found == m_circles.end())
        return 0;
    found->second->getAttributes(lifeTime, colour, position, radius, normal, filled, segments);
    return 1;
}

// ELF 0x3743C0. DPStaticCircle::setAttributes takes the position by value; the copy is made here.
int DPManager::setCircleAttributes(DrawPrimitive * primitive, float lifeTime, unsigned int colour,
                                   const Vec3& position, float radius, const Vec3& normal,
                                   bool filled, int segments)
{
    std::map<DrawPrimitive*, DPStaticCircle*>::iterator found = m_circles.find(primitive);
    if (found == m_circles.end())
        return 0;
    found->second->setAttributes(lifeTime, colour, position, radius, normal, filled, segments);
    return 1;
}

// ELF 0x374480.
int DPManager::deleteLine(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStaticLine*>::iterator found = m_lines.find(primitive);
    if (found == m_lines.end())
        return 0;
    delete found->second;
    m_lines.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374540.
int DPManager::getLineAttributes(DrawPrimitive * primitive, float & lifeTime, unsigned int & colour,
                                 std::vector<Vec3> & points, float & width) const
{
    std::map<DrawPrimitive*, DPStaticLine*>::const_iterator found = m_lines.find(primitive);
    if (found == m_lines.end())
        return 0;
    found->second->getAttributes(lifeTime, colour, points, width);
    return 1;
}

// ELF 0x3745C0. DPStaticLine::setAttributes takes references, so the by-value arguments are passed
// by address.
int DPManager::setLineAttributes(DrawPrimitive * primitive, float lifeTime, unsigned int colour,
                                 const std::vector<Vec3>& points, float width)
{
    std::map<DrawPrimitive*, DPStaticLine*>::iterator found = m_lines.find(primitive);
    if (found == m_lines.end())
        return 0;
    found->second->setAttributes(lifeTime, colour, points, width);
    return 1;
}

// ELF 0x374640.
int DPManager::delete2dText(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStatic2dText*>::iterator found = m_text2d.find(primitive);
    if (found == m_text2d.end())
        return 0;
    delete found->second;
    m_text2d.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374700.
int DPManager::get2dTextAttributes(DrawPrimitive * primitive, float & lifeTime, unsigned int & colour,
                                   Vec2 & position, std::vector<std::string> & lines, int & size,
                                   AITextAlignmentX & alignX, AITextAlignmentY & alignY) const
{
    std::map<DrawPrimitive*, DPStatic2dText*>::const_iterator found = m_text2d.find(primitive);
    if (found == m_text2d.end())
        return 0;
    found->second->getAttributes(lifeTime, colour, position, lines, size, alignX, alignY);
    return 1;
}

// ELF 0x3747A0.
int DPManager::set2dTextAttributes(DrawPrimitive * primitive, float lifeTime, unsigned int colour,
                                   const Vec2& position, const std::vector<std::string>& lines,
                                   int size, AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    std::map<DrawPrimitive*, DPStatic2dText*>::iterator found = m_text2d.find(primitive);
    if (found == m_text2d.end())
        return 0;
    found->second->setAttributes(lifeTime, colour, position, lines, size, alignX, alignY);
    return 1;
}

// ELF 0x374840.
int DPManager::delete3dText(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStatic3dText*>::iterator found = m_text3d.find(primitive);
    if (found == m_text3d.end())
        return 0;
    delete found->second;
    m_text3d.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374900.
int DPManager::get3dTextAttributes(DrawPrimitive * primitive, float & lifeTime, unsigned int & colour,
                                   Vec3 & position, std::vector<std::string> & lines, int & size,
                                   AITextAlignmentX & alignX, AITextAlignmentY & alignY) const
{
    std::map<DrawPrimitive*, DPStatic3dText*>::const_iterator found = m_text3d.find(primitive);
    if (found == m_text3d.end())
        return 0;
    found->second->getAttributes(lifeTime, colour, position, lines, size, alignX, alignY);
    return 1;
}

// ELF 0x3749A0.
int DPManager::set3dTextAttributes(DrawPrimitive * primitive, float lifeTime, unsigned int colour,
                                   const Vec3& position, const std::vector<std::string>& lines,
                                   int size, AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    std::map<DrawPrimitive*, DPStatic3dText*>::iterator found = m_text3d.find(primitive);
    if (found == m_text3d.end())
        return 0;
    found->second->setAttributes(lifeTime, colour, position, lines, size, alignX, alignY);
    return 1;
}

// ELF 0x374A40.
int DPManager::deleteTriangle(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStaticTriangle*>::iterator found = m_triangles.find(primitive);
    if (found == m_triangles.end())
        return 0;
    delete found->second;
    m_triangles.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374B00. No result register is written on either path.
void DPManager::getTriangleAttributes(DrawPrimitive * primitive, float & lifeTime,
                                      std::vector<Vec3> & vertices,
                                      std::vector<unsigned int>& colours, bool & filled) const
{
    std::map<DrawPrimitive*, DPStaticTriangle*>::const_iterator found = m_triangles.find(primitive);
    if (found != m_triangles.end())
        found->second->getAttributes(lifeTime, vertices, colours, filled);
}

// ELF 0x374B80.
void DPManager::setTriangleAttributes(DrawPrimitive * primitive, float & lifeTime,
                                      const std::vector<Vec3>& vertices,
                                      const std::vector<unsigned int>& colours, bool & filled)
{
    std::map<DrawPrimitive*, DPStaticTriangle*>::iterator found = m_triangles.find(primitive);
    if (found != m_triangles.end())
        found->second->setAttributes(lifeTime, vertices, colours, filled);
}

// ELF 0x374C00.
int DPManager::deletePolygon(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStaticPolygon*>::iterator found = m_polygons.find(primitive);
    if (found == m_polygons.end())
        return 0;
    delete found->second;
    m_polygons.erase(found);
    primitive = 0;
    return 1;
}

// ELF 0x374CC0.
void DPManager::getPolygonAttributes(DrawPrimitive * primitive, float & lifeTime, bool & filled) const
{
    std::map<DrawPrimitive*, DPStaticPolygon*>::const_iterator found = m_polygons.find(primitive);
    if (found != m_polygons.end())
        found->second->getAttributes(lifeTime, filled);
}

// ELF 0x374D30.
void DPManager::setPolygonAttributes(DrawPrimitive * primitive, float & lifeTime, bool & filled)
{
    std::map<DrawPrimitive*, DPStaticPolygon*>::iterator found = m_polygons.find(primitive);
    if (found != m_polygons.end())
        found->second->setAttributes(lifeTime, filled);
}

// ELF 0x374DA0. Deletes a known mesh and clears the caller's handle; an unknown handle returns false.
bool DPManager::deleteMesh(DrawPrimitive *& primitive)
{
    std::map<DrawPrimitive*, DPStaticMesh*>::iterator found = m_meshes.find(primitive);
    if (found == m_meshes.end())
        return false;
    delete found->second;
    m_meshes.erase(found);
    primitive = 0;
    return true;
}

// ELF 0x374E60
void DPManager::setTextGamma(float gamma)
{
    m_textGamma = gamma;
}

// ELF 0x374E70
float DPManager::getTextGamma() const
{
    return m_textGamma;
}

namespace {

// [CHOSEN, not recovered] Helper names for the loops drawAll, nodrawUpdate and deleteAll repeat
// once per map; each instantiation is inlined into its caller as the shipped bodies show.
// A primitive goes when it has expired, or (when drawing) when draw() answers false.
template <class Map>
inline void drawMap(Map& primitives, double now, IDebugRenderer* renderer)
{
    for (typename Map::iterator it = primitives.begin(); it != primitives.end();)
    {
        if (it->second->expired(now) || !it->second->draw(renderer))
        {
            delete it->second;
            primitives.erase(it++);
        }
        else
            ++it;
    }
}

template <class Map>
inline void updateMap(Map& primitives, double now)
{
    for (typename Map::iterator it = primitives.begin(); it != primitives.end();)
    {
        if (it->second->expired(now))
        {
            delete it->second;
            primitives.erase(it++);
        }
        else
            ++it;
    }
}

template <class Map>
inline void deleteMap(Map& primitives)
{
    for (typename Map::iterator it = primitives.begin(); it != primitives.end();)
    {
        delete it->second;
        primitives.erase(it++);
    }
}

} // namespace

// ELF 0x374E80. Does nothing without an AI main. The renderer is read before the game time; the
// maps are walked circles, 2D text, 3D text, triangles, polygons, meshes, lines.
void DPManager::drawAll()
{
    if (!IAIMain::m_instance)
        return;
    IDebugRenderer* const renderer = static_cast<IDebugRenderer*>(CommonSingletonServer::m_iDebugRenderer);
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    drawMap(m_circles, now, renderer);
    drawMap(m_text2d, now, renderer);
    drawMap(m_text3d, now, renderer);
    drawMap(m_triangles, now, renderer);
    drawMap(m_polygons, now, renderer);
    drawMap(m_meshes, now, renderer);
    drawMap(m_lines, now, renderer);
}

// ELF 0x375550. drawAll's expiry sweep without drawing, in the same map order.
void DPManager::nodrawUpdate()
{
    if (!IAIMain::m_instance)
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    updateMap(m_circles, now);
    updateMap(m_text2d, now);
    updateMap(m_text3d, now);
    updateMap(m_triangles, now);
    updateMap(m_polygons, now);
    updateMap(m_meshes, now);
    updateMap(m_lines, now);
}

// ELF 0x375B00. Every map is drained in this order - circles, 2D text, 3D text, triangles,
// polygons, meshes, lines - deleting each value and erasing its node.
void DPManager::deleteAll()
{
    deleteMap(m_circles);
    deleteMap(m_text2d);
    deleteMap(m_text3d);
    deleteMap(m_triangles);
    deleteMap(m_polygons);
    deleteMap(m_meshes);
    deleteMap(m_lines);
}

// The create* slots share one shape: operator new of the concrete primitive, checkPointer (the
// AIUtils/checkPointer.h report on a null result), insert (primitive, primitive) into that kind's
// map, return the primitive. A null result is still inserted; the insert follows the report.

// ELF 0x375F30 (sizeof 0x38).
DPStaticCircle * DPManager::createStaticCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                               float radius, const Vec3& normal, bool filled, int segments)
{
    DPStaticCircle* const circle = checkPointer(
        new DPStaticCircle(lifeTime, colour, position, radius, normal, filled, segments));
    m_circles.insert(std::make_pair(circle, circle));
    return circle;
}

// ELF 0x376100 (sizeof 0x24).
DPStaticLine * DPManager::createStaticLine(float lifeTime, unsigned int colour, const Vec3& start,
                                           const Vec3& end, float width)
{
    DPStaticLine* const line = checkPointer(new DPStaticLine(lifeTime, colour, start, end, width));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x3762B0 (sizeof 0x24).
DrawPrimitive* DPManager::createStaticLine(float lifeTime, unsigned int colour,
                                           const std::vector<Vec3>& points, float width)
{
    DPStaticLine* const line = checkPointer(new DPStaticLine(lifeTime, colour, points, width));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x376450 (sizeof 0x34).
DrawPrimitive * DPManager::createStatic2dText(float lifeTime, unsigned int colour, const Vec2& position,
                                              const std::string& text, int size,
                                              AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPStatic2dText* const primitive = checkPointer(
        new DPStatic2dText(lifeTime, colour, position, text, size, alignX, alignY));
    m_text2d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x376610 (sizeof 0x34).
DrawPrimitive * DPManager::createStatic2dText(float lifeTime, unsigned int colour, const Vec2& position,
                                              const std::vector<std::string>& lines, int size,
                                              AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPStatic2dText* const primitive = checkPointer(
        new DPStatic2dText(lifeTime, colour, position, lines, size, alignX, alignY));
    m_text2d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x3767D0 (sizeof 0x38).
DrawPrimitive* DPManager::createStatic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                             const std::string& text, int size,
                                             AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPStatic3dText* const primitive = checkPointer(
        new DPStatic3dText(lifeTime, colour, position, text, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x376990 (sizeof 0x38).
DrawPrimitive* DPManager::createStatic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                             const std::vector<std::string>& lines, int size,
                                             AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPStatic3dText* const primitive = checkPointer(
        new DPStatic3dText(lifeTime, colour, position, lines, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x376B50 (sizeof 0x30).
DrawPrimitive* DPManager::createStaticTriangle(float lifeTime, const std::vector<Vec3>& vertices,
                                               const std::vector<unsigned int>& colours, bool filled)
{
    DPStaticTriangle* const primitive = checkPointer(
        new DPStaticTriangle(lifeTime, vertices, colours, filled));
    m_triangles.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x376D00 (sizeof 0x30).
DrawPrimitive* DPManager::createStaticPolygon(float lifeTime, const std::vector<Vec3>& vertices,
                                              const std::vector<unsigned int>& colours, bool filled)
{
    DPStaticPolygon* const primitive = checkPointer(
        new DPStaticPolygon(lifeTime, vertices, colours, filled));
    m_polygons.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x376EB0 (sizeof 0x2C).
DPStaticMesh * DPManager::createStaticMesh(float lifeTime, unsigned int * colours, Vec3 * vertices,
                                           unsigned int * indices, int count, bool filled)
{
    DPStaticMesh* const mesh = checkPointer(
        new DPStaticMesh(lifeTime, colours, vertices, indices, count, filled));
    m_meshes.insert(std::make_pair(mesh, mesh));
    return mesh;
}

// ELF 0x377070 (sizeof 0x2C).
DPStaticMesh * DPManager::createStaticMesh(float lifeTime, unsigned int * colours, Polygon * polygons,
                                           Vec3 * vertices, unsigned int * indices, int count,
                                           bool filled)
{
    DPStaticMesh* const mesh = checkPointer(
        new DPStaticMesh(lifeTime, colours, polygons, vertices, indices, count, filled));
    m_meshes.insert(std::make_pair(mesh, mesh));
    return mesh;
}

// ELF 0x377240. A demo of every static primitive kind at `transform`'s position, a tenth of a
// second each: a filled and an outlined circle of radius 5 facing up, a small circle 10 units
// along the third row facing along it, a line along the third row, the same line shifted a
// quarter unit along the first row as a point list (its end point pushed twice, shipped), and
// 2D and 3D text in both the line-list and single-string forms. Every call goes through this
// object's own virtual creators. Nothing is returned: the ia-32 body leaves eax unset, so the
// slot is void [INFERRED].
void DPManager::testStatics(Mat4& transform)
{
    const float lifeTime = 0.1f;
    float size = 10.0f;
    const float width = 0.05f;
    const Vec3& position = *reinterpret_cast<const Vec3*>(&transform.m[12]);

    Vec3 normal = {0.0f, 1.0f, 0.0f};
    const float radius = size * 0.5f;
    createStaticCircle(lifeTime, 0xFF00FF00u, position, radius, normal, true, 255);
    createStaticCircle(lifeTime, 0xFF00FF00u, position, radius, normal, false, 5);

    normal.x = transform.m[8];
    normal.y = transform.m[9];
    normal.z = transform.m[10];
    Vec3 end;
    end.x = normal.x * size + position.x;
    end.y = normal.y * size + position.y;
    end.z = normal.z * size + position.z;
    createStaticCircle(lifeTime, 0xFF00FF00u, end, radius / 20.0f, normal, false, 255);

    Vec3 start = position;
    end.x = size * transform.m[8] + position.x;
    end.y = size * transform.m[9] + position.y;
    end.z = size * transform.m[10] + position.z;
    createStaticLine(lifeTime, 0xFFFF0000u, start, end, width);

    std::vector<Vec3> points;
    const float shiftX = width * transform.m[0] * 5.0f;
    const float shiftY = width * transform.m[1] * 5.0f;
    const float shiftZ = width * transform.m[2] * 5.0f;
    start.x = start.x + shiftX;
    start.y = start.y + shiftY;
    start.z = start.z + shiftZ;
    end.x = shiftX + end.x;
    end.y = shiftY + end.y;
    end.z = shiftZ + end.z;
    points.push_back(start);
    points.push_back(end);
    points.push_back(end);
    createStaticLine(lifeTime, 0xFFFF0000u, points, width + width);

    std::vector<std::string> lines2d;
    lines2d.push_back(std::string("Static 2D Text Demo"));
    lines2d.push_back(std::string("Line 2"));
    Vec2 screenPosition = {10.0f, 10.0f};
    createStatic2dText(lifeTime, 0xFFFFFF00u, screenPosition, lines2d, 5,
                       static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(1));
    screenPosition.x = 40.0f;
    screenPosition.y = 10.0f;
    createStatic2dText(lifeTime, 0xFFFFFF00u, screenPosition, std::string("PureString"), 5,
                       static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(1));

    std::vector<std::string> lines3d;
    lines3d.push_back(std::string("Static 3D Text Demo"));
    lines3d.push_back(std::string("Line 2"));
    createStatic3dText(lifeTime, 0xFFFF00FFu, position, lines3d, 5,
                       static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(1));
    const std::string pureString("PureString");
    size = size * 0.2f;
    Vec3 textPosition;
    textPosition.x = position.x;
    textPosition.y = size + position.y;
    textPosition.z = position.z;
    createStatic3dText(lifeTime, 0xFFFF00FFu, textPosition, pureString, 5,
                       static_cast<AITextAlignmentX>(0), static_cast<AITextAlignmentY>(1));
}

// The dynamic and AI-object creators follow the static shape and file their primitive in the map
// of the static base: circles, lines or 3D text.

// ELF 0x377D00 (sizeof 0x44).
DPStaticCircle * DPManager::createDPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                                  float radius, Bot * bot, DPDynamicTypes type,
                                                  const Vec3& normal, bool filled, int segments)
{
    DPDynamicCircle* const circle = checkPointer(new DPDynamicCircle(
        lifeTime, colour, position, radius, bot, type, normal, filled, segments));
    m_circles.insert(std::make_pair(circle, circle));
    return circle;
}

// ELF 0x377EE0 (sizeof 0x44).
DPStaticCircle * DPManager::createDPDynamicCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                                  float radius, unsigned int information,
                                                  DPDynamicTypes type, const Vec3& normal, bool filled,
                                                  int segments)
{
    DPDynamicCircle* const circle = checkPointer(new DPDynamicCircle(
        lifeTime, colour, position, radius, information, type, normal, filled, segments));
    m_circles.insert(std::make_pair(circle, circle));
    return circle;
}

// ELF 0x3780C0 (sizeof 0x3C).
DPStaticLine * DPManager::createDynamicLine(float lifeTime, unsigned int colour, const Vec3& start,
                                            const Vec3& end, float width, Bot * bot, DPDynamicTypes type)
{
    DPDynamicLine* const line = checkPointer(
        new DPDynamicLine(lifeTime, colour, start, end, width, bot, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x378280 (sizeof 0x3C).
DPStaticLine * DPManager::createDynamicLine(float lifeTime, unsigned int colour, const Vec3& start,
                                            const Vec3& end, float width, unsigned int information,
                                            DPDynamicTypes type)
{
    DPDynamicLine* const line = checkPointer(
        new DPDynamicLine(lifeTime, colour, start, end, width, information, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x378440 (sizeof 0x3C).
DrawPrimitive* DPManager::createDynamicLine(float lifeTime, unsigned int colour,
                                            const std::vector<Vec3>& points, float width, Bot * bot,
                                            DPDynamicTypes type)
{
    DPDynamicLine* const line = checkPointer(
        new DPDynamicLine(lifeTime, colour, points, width, bot, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x3785F0 (sizeof 0x3C).
DrawPrimitive* DPManager::createDynamicLine(float lifeTime, unsigned int colour,
                                            const std::vector<Vec3>& points, float width,
                                            unsigned int information, DPDynamicTypes type)
{
    DPDynamicLine* const line = checkPointer(
        new DPDynamicLine(lifeTime, colour, points, width, information, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x3787A0 (sizeof 0x44).
DrawPrimitive* DPManager::createDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                              const std::string& text, Bot * bot, DPDynamicTypes type,
                                              int size, AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPDynamic3dText* const primitive = checkPointer(new DPDynamic3dText(
        lifeTime, colour, position, text, bot, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x378970 (sizeof 0x44).
DrawPrimitive* DPManager::createDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                              const std::vector<std::string>& lines, Bot * bot,
                                              DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                              AITextAlignmentY alignY)
{
    DPDynamic3dText* const primitive = checkPointer(new DPDynamic3dText(
        lifeTime, colour, position, lines, bot, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x378B40 (sizeof 0x44).
DrawPrimitive* DPManager::createDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                              const std::string& text, unsigned int information,
                                              DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                              AITextAlignmentY alignY)
{
    DPDynamic3dText* const primitive = checkPointer(new DPDynamic3dText(
        lifeTime, colour, position, text, information, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x378D10 (sizeof 0x44).
DrawPrimitive* DPManager::createDynamic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                              const std::vector<std::string>& lines,
                                              unsigned int information, DPDynamicTypes type, int size,
                                              AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPDynamic3dText* const primitive = checkPointer(new DPDynamic3dText(
        lifeTime, colour, position, lines, information, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x378EE0 (sizeof 0x40).
DPStaticCircle * DPManager::createDPAIObjectCircle(float lifeTime, unsigned int colour, const Vec3& position,
                                                   float radius, unsigned int information,
                                                   DPDynamicTypes type, const Vec3& normal, bool filled,
                                                   int segments)
{
    DPAIObjectCircle* const circle = checkPointer(new DPAIObjectCircle(
        lifeTime, colour, position, radius, information, type, normal, filled, segments));
    m_circles.insert(std::make_pair(circle, circle));
    return circle;
}

// ELF 0x3790C0 (sizeof 0x38).
DPStaticLine * DPManager::createAIObjectLine(float lifeTime, unsigned int colour, const Vec3& start,
                                             const Vec3& end, float width, unsigned int information,
                                             DPDynamicTypes type)
{
    DPAIObjectLine* const line = checkPointer(
        new DPAIObjectLine(lifeTime, colour, start, end, width, information, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x379280 (sizeof 0x38).
DrawPrimitive* DPManager::createAIObjectLine(float lifeTime, unsigned int colour,
                                             const std::vector<Vec3>& points, float width,
                                             unsigned int information, DPDynamicTypes type)
{
    DPAIObjectLine* const line = checkPointer(
        new DPAIObjectLine(lifeTime, colour, points, width, information, type));
    m_lines.insert(std::make_pair(line, line));
    return line;
}

// ELF 0x379430 (sizeof 0x44).
DrawPrimitive* DPManager::createAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                               const std::string& text, unsigned int information,
                                               DPDynamicTypes type, int size, AITextAlignmentX alignX,
                                               AITextAlignmentY alignY)
{
    DPAIObject3dText* const primitive = checkPointer(new DPAIObject3dText(
        lifeTime, colour, position, text, information, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

// ELF 0x379600 (sizeof 0x44).
DrawPrimitive* DPManager::createAIObject3dText(float lifeTime, unsigned int colour, const Vec3& position,
                                               const std::vector<std::string>& lines,
                                               unsigned int information, DPDynamicTypes type, int size,
                                               AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    DPAIObject3dText* const primitive = checkPointer(new DPAIObject3dText(
        lifeTime, colour, position, lines, information, type, size, alignX, alignY));
    m_text3d.insert(std::make_pair(primitive, primitive));
    return primitive;
}

} // namespace ai
} // namespace hfe
} // namespace dice
