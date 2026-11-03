// Owns AI debug primitives.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

class DPStatic2dText;
class DPStatic3dText;
class DPStaticTriangle;
class DPStaticPolygon;

// Draw-manager interface. Registration guards the roster static, which may remain
// stale until updateStaticMethods refreshes it.
class IDPManager
{
public:
    IDPManager();
    virtual ~IDPManager();
    virtual int deleteCircle(DrawPrimitive *&) = 0;
    virtual int getCircleAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, float &, Vec3 &, bool &, int &) const = 0;
    virtual int setCircleAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, float, const Vec3&, bool, int) = 0;
    virtual int deleteLine(DrawPrimitive *&) = 0;
    virtual int getLineAttributes(DrawPrimitive *, float &, unsigned int &, std::vector<Vec3> &, float &) const = 0;
    virtual int setLineAttributes(DrawPrimitive *, float, unsigned int, const std::vector<Vec3>&, float) = 0;
    virtual int delete2dText(DrawPrimitive *&) = 0;
    virtual int get2dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec2 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) const = 0;
    virtual int set2dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int delete3dText(DrawPrimitive *&) = 0;
    virtual int get3dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) const = 0;
    virtual int set3dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int deleteTriangle(DrawPrimitive *&) = 0;
    virtual void getTriangleAttributes(DrawPrimitive *, float &, std::vector<Vec3> &, std::vector<unsigned int>&, bool &) const = 0;
    virtual void setTriangleAttributes(DrawPrimitive *, float &, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool &) = 0;
    virtual int deletePolygon(DrawPrimitive *&) = 0;
    virtual void getPolygonAttributes(DrawPrimitive *, float &, bool &) const = 0;
    virtual void setPolygonAttributes(DrawPrimitive *, float &, bool &) = 0;
    // bool
    virtual bool deleteMesh(DrawPrimitive *&) = 0;
    virtual void setTextGamma(float) = 0;
    virtual float getTextGamma() const = 0;
    virtual void drawAll() = 0;
    virtual void nodrawUpdate() = 0;
    virtual void deleteAll() = 0;
};

// IDPStaticCreator - 14 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPStaticCreator
{
public:
    IDPStaticCreator();
    virtual ~IDPStaticCreator();
    virtual DPStaticCircle * createStaticCircle(float, unsigned int, const Vec3&, float, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createStaticLine(float, unsigned int, const Vec3&, const Vec3&, float) = 0;
    virtual DrawPrimitive* createStaticLine(float, unsigned int, const std::vector<Vec3>&, float) = 0;
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::string&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    // ELF 0x3767D0 returns the allocated text pointer; QuadTreeManager stores the handle.
    virtual DrawPrimitive* createStatic3dText(float, unsigned int, const Vec3&, const std::string&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createStatic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createStaticTriangle(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool) = 0;
    virtual DrawPrimitive* createStaticPolygon(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool) = 0;
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Vec3 *, unsigned int *, int, bool) = 0;
    // Returns the new DPStaticMesh (DPManager::createStaticMesh 0x377070; QuadTree::draw stores it).
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Polygon *, Vec3 *, unsigned int *, int, bool) = 0;
    // void [INFERRED]: no caller exists and the ia-32 body (0x377240) leaves eax unset.
    virtual void testStatics(Mat4 &) = 0;
};

// IDPDynamicCreator - 12 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPDynamicCreator
{
public:
    IDPDynamicCreator();
    virtual ~IDPDynamicCreator();
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, Bot *, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, Bot *, DPDynamicTypes) = 0;
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes) = 0;
    virtual DrawPrimitive* createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, Bot *, DPDynamicTypes) = 0;
    virtual DrawPrimitive* createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes) = 0;
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
};

// IDPAIObjectCreator - 7 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPAIObjectCreator
{
public:
    IDPAIObjectCreator();
    virtual ~IDPAIObjectCreator();
    virtual DPStaticCircle * createDPAIObjectCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createAIObjectLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes) = 0;
    virtual DrawPrimitive* createAIObjectLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes) = 0;
    virtual DrawPrimitive* createAIObject3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive* createAIObject3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
};

// DPManager - sizeof == 104, from BF2AI::init step 7.
class DPManager : public IDPManager, public IDPStaticCreator, public IDPDynamicCreator, public IDPAIObjectCreator
{
public:
    DPManager();
    virtual ~DPManager();

    // IDPManager
    virtual int deleteCircle(DrawPrimitive *&);
    virtual int getCircleAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, float &, Vec3 &, bool &, int &) const;
    virtual int setCircleAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, float, const Vec3&, bool, int);
    virtual int deleteLine(DrawPrimitive *&);
    virtual int getLineAttributes(DrawPrimitive *, float &, unsigned int &, std::vector<Vec3> &, float &) const;
    virtual int setLineAttributes(DrawPrimitive *, float, unsigned int, const std::vector<Vec3>&, float);
    virtual int delete2dText(DrawPrimitive *&);
    virtual int get2dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec2 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) const;
    virtual int set2dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual int delete3dText(DrawPrimitive *&);
    virtual int get3dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) const;
    virtual int set3dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual int deleteTriangle(DrawPrimitive *&);
    virtual void getTriangleAttributes(DrawPrimitive *, float &, std::vector<Vec3> &, std::vector<unsigned int>&, bool &) const;
    virtual void setTriangleAttributes(DrawPrimitive *, float &, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool &);
    virtual int deletePolygon(DrawPrimitive *&);
    virtual void getPolygonAttributes(DrawPrimitive *, float &, bool &) const;
    virtual void setPolygonAttributes(DrawPrimitive *, float &, bool &);
    virtual bool deleteMesh(DrawPrimitive *&);
    virtual void setTextGamma(float);
    virtual float getTextGamma() const;
    virtual void drawAll();
    virtual void nodrawUpdate();
    virtual void deleteAll();

    // IDPStaticCreator
    virtual DPStaticCircle * createStaticCircle(float, unsigned int, const Vec3&, float, const Vec3&, bool, int);
    virtual DPStaticLine * createStaticLine(float, unsigned int, const Vec3&, const Vec3&, float);
    virtual DrawPrimitive* createStaticLine(float, unsigned int, const std::vector<Vec3>&, float);
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::string&, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createStatic3dText(float, unsigned int, const Vec3&, const std::string&, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createStatic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createStaticTriangle(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool);
    virtual DrawPrimitive* createStaticPolygon(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool);
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Vec3 *, unsigned int *, int, bool);
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Polygon *, Vec3 *, unsigned int *, int, bool);
    virtual void testStatics(Mat4 &);

    // IDPDynamicCreator
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, Bot *, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, Bot *, DPDynamicTypes);
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes);
    virtual DrawPrimitive* createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, Bot *, DPDynamicTypes);
    virtual DrawPrimitive* createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes);
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);

    // IDPAIObjectCreator
    virtual DPStaticCircle * createDPAIObjectCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticLine * createAIObjectLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes);
    virtual DrawPrimitive* createAIObjectLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes);
    virtual DrawPrimitive* createAIObject3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive* createAIObject3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);

private:
    // Seven maps from the caller's primitive handle to the owned static primitive, in the
    // constructor's order. Key/value types are the ELF instantiations
    // (_Rb_tree<DrawPrimitive*, pair<DrawPrimitive* const, DPStaticX*>>); the offsets come from
    // the delete* bodies: deleteLine +0x10, deleteCircle +0x1C, delete2dText +0x28,
    // delete3dText +0x34, deleteTriangle +0x40, deletePolygon +0x4C, deleteMesh +0x58.
    /* +0x10 */ std::map<DrawPrimitive*, DPStaticLine*>     m_lines;
    /* +0x1C */ std::map<DrawPrimitive*, DPStaticCircle*>   m_circles;
    /* +0x28 */ std::map<DrawPrimitive*, DPStatic2dText*>   m_text2d;
    /* +0x34 */ std::map<DrawPrimitive*, DPStatic3dText*>   m_text3d;
    /* +0x40 */ std::map<DrawPrimitive*, DPStaticTriangle*> m_triangles;
    /* +0x4C */ std::map<DrawPrimitive*, DPStaticPolygon*>  m_polygons;
    /* +0x58 */ std::map<DrawPrimitive*, DPStaticMesh*>     m_meshes;
    /* +0x64 */ float m_textGamma;             // = 1.0f
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H
