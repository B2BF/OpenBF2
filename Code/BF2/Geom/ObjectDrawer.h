// Collects visible scene objects and submits their geometry.
// Set the render view and call updateVisible before drawing.


#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_OBJECTDRAWER_H
#define BF2_GEOM_OBJECTDRAWER_H

#include "../Core/IBase.h"
#include "../Math/Plane3.h"

#include <map>
#include <vector>

namespace dice {
namespace hfe {

// Only a pointer to it is stored; Game/Objects/IObject.h is included by the .cpp.
namespace world { class IObject; }
struct LocalBoundingBox;   // isBoundsOccluded's argument (Math/LocalBoundingBox.h, included by the .cpp)

namespace geom {


// The two trees at +0x10 / +0x1C. Retail nodes are 0x20 bytes with a 12-byte value; nothing
// recovered inserts, finds or reads, so the key/value split is [UNRESOLVED] and the drawer only
// constructs and destroys them.
struct ObjectDrawerTreeValue
{
    unsigned int words[2];
};
typedef std::map<unsigned int, ObjectDrawerTreeValue> ObjectDrawerTree;


// ObjectManager fills these vectors by reference; their layout must match the host STL.
// Compatibility builds use the VC7.1 allocator-prefixed layout.

typedef std::vector< ::dice::hfe::world::IObject*> ObjectDrawerSet;

class ObjectDrawer
{
public:
    ObjectDrawer();
    ~ObjectDrawer();

    // the IBase quartet, primary slots +0x00..+0x0C
    void  addRef();                                          // +0x00  (shared, folded)
    int   getRef();                                          // +0x04  (shared, folded)
    int   release();                                         // +0x08
    void* queryInterface(unsigned int iid) const;            // +0x0C

    // the recovered leaf slots
    // +0x24, 0x100FAFD0, 19 B.  `return which ? &m_drawSet : &m_visibleSet` - note the sense: a
    // zero argument selects the first vector.
    ObjectDrawerSet* getObjectVector(int which);

    void            setRenderView(void* view);               // +0x28
    void*           getRenderView() const;                   // +0x2C

    // +0x30, 0x100F7CE0, 44 B.  Release the old, store, addRef the new - and the store happens
    // between the two, so handing it the occluder it already holds releases and re-addRefs rather
    // than short-circuiting.  Reproduced.
    void            setOccluder(IBase* occluder);
    IBase*          getOccluder() const;                     // +0x34

    // Whether the optional culling-count text overlay is enabled, independently of the counters.
    bool isCullingDebugEnabled() const { return m_byte70 != 0; }

    // Draws visible objects, forwarding frame time to grid queries and visual updates.
    void  draw(float time);
    void  updateVisible(float time);                         // +0x14
    // Submits the deferred geometry pass under the same frame gate as visibility and drawing.
    void  slot_0x18();
    void  slot_0x1C();                                       // 0x100F8DE0, 324 B [FRONTIER]
    void  slot_0x20(void* object);                           // 0x100F8F30,  51 B [FRONTIER]
    // +0x38, 0x100F8220, 520 B. is this geometry hidden
    // behind one of the occluders at +0x50? The OverGrowth draw is its one recovered caller.
    bool  isGeometryOccluded(void* geometry);
    // +0x3C, 0x100F8000, 531 B. is this axis-aligned world
    // box hidden behind one of the occluders at +0x50? The OverGrowth base update (0x1015F32B) asks
    // it for every candidate cell, and the Undergrowth render (0x10149ACA) for every built patch,
    // each with the cell's bounds. It refreshes the box's derived data in place (the bounds are
    // not const).
    bool  isBoundsOccluded(LocalBoundingBox* bounds);

    // Offset comments identify retail x86 positions; native builds access named members.
    void* const*           m_vptr;                  // +0x00  ObjectDrawer's primary table
    void* const*           m_eventVptr;             // +0x04  the 1-slot ECCore handler subobject
    int                    m_refCount;              // +0x08  ctor 1
    unsigned char          m_byte0C;                // +0x0C  ctor 0
    unsigned char          m_updateVisibleHasRun;   // +0x0D  ctor 0; updateVisible sets it
    unsigned char          m_byte0E;                // +0x0E  ctor 0
    unsigned char          m_pad0F;
    ObjectDrawerTree       m_tree10;                // +0x10
    ObjectDrawerTree       m_tree1C;                // +0x1C
    void*                  m_renderView;            // +0x28  ctor 0; slot +0x28 writes it
    IBase*                 m_occluder;              // +0x2C  ctor 0; refcounted, slot +0x30
    ObjectDrawerSet        m_visibleSet;            // +0x30  getObjectVector(0)
    ObjectDrawerSet        m_drawSet;               // +0x40  getObjectVector(non-zero)
    // +0x50 - owned occlusion volumes: one plane list per occluder, rebuilt each enabled
    // visibility update and read by object, geometry and bounds tests. Never handed to the host.
    std::vector<std::vector<Plane3> > m_vector50;   // +0x50
    unsigned char          m_byte60;                // +0x60  ctor 1
    unsigned char          m_pad61[3];
    float                  m_float64;               // +0x64  ctor 60.0f (0x42700000)
    unsigned char          m_byte68;                // +0x68  ctor 1
    unsigned char          m_pad69[3];
    unsigned int           m_dword6C;               // +0x6C  ctor 0
    unsigned char          m_byte70;                // retail +0x70; culling debug overlay, ctor 0
    unsigned char          m_pad71[3];
    int                    m_culledCount;           // +0x74  ctor 0
    unsigned int           m_dword78;               // +0x78  ctor 0
    unsigned int           m_dword7C;               // +0x7C  ctor 0
    unsigned int           m_dword80;               // +0x80  per-frame counter, updateVisible zeroes
    unsigned int           m_dword84;               // +0x84  per-frame counter, updateVisible zeroes
    unsigned char          m_byte88;                // +0x88  ctor 0
    unsigned char          m_pad89[3];
};

// The retail size holds only where the retail vector does - see the ABI note above.  In the
// native flavour the two object sets are the host's own twelve-byte std::vector and the object is
// correspondingly smaller, which is correct rather than a drift: nothing outside this DLL indexes
// an ObjectDrawer by offset.  Its own factory allocates sizeof(ObjectDrawer).

// RefFactory_geom_ObjectDrawer, 0x100B0ADE.  registered in ModuleGeom - see the header note.
IBase* __fastcall createObjectDrawer(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_OBJECTDRAWER_H
#endif // BF2_IMAGE_RENDDX9
