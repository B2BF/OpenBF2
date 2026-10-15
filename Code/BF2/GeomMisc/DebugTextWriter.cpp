// Debug text writer backed by a Font instance. Unimplemented drawing slots remain frontiers.
// [INFERRED] Source placement.
// [RETIRE-WHEN: RW-58] Confirm the original source file before removing the placement uncertainty.

// The proportional and fixed-width writers share this class; the loaded font selects the appearance.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"

namespace dice {
namespace hfe {
namespace geom {

// Class and interface IDs used by module registration and queryInterface.
extern const unsigned int IID_IDebugTextWriter = 80043u;
extern const unsigned int CID_DebugTextWriter  = 81041u;

// The DebugTextWriterManager global, dword_10247C58. Declared in ModuleGeomMisc.cpp.
extern IBase* g_DebugTextWriterManager;

// FontSystem/FontSystem.cpp - the constructor builds one directly, not through the registry.
IBase* createFontPortedDirect();


// dice::hfe::geom::DebugTextWriter - 0x34 bytes (52), vtable 0x1020CD10 with 19 slots.

// The slot count is read: the table runs 0x1020CD10..0x1020CD58 and the next dword is 0.

class DebugTextWriter : public IBase
{
public:
    DebugTextWriter();

    virtual void  addRef();                                   // +0x00  0x1000CB20 (folded)
    virtual int   getRef();                                   // +0x04  0x101A9A10 (folded)
    virtual int   release();                                  // +0x08
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C  0x1009BCD0   read

    virtual bool setFont(const std::string& path);           // +0x10  0x1009B160   read
    virtual void slot_0x14();
    virtual void slot_0x18();
    virtual void slot_0x1C();
    virtual void slot_0x20();
    virtual void slot_0x24();
    virtual void slot_0x28();
    virtual void slot_0x2C();
    virtual void slot_0x30();
    virtual void slot_0x34();
    virtual void slot_0x38();
    virtual void slot_0x3C();
    virtual void slot_0x40();
    virtual void slot_0x44();
    virtual void slot_0x48();                                 // 0x1009B960   the last slot

private:
    int            m_refCount;      // +0x04  ctor: 1
    void*          m_font;          // +0x08  a geom::Font::Ported, 0xC58 bytes - see below
    unsigned int   m_unknown0C;     // +0x0C  [UNRESOLVED] - the ctor does not write it
    unsigned int   m_zero10;        // +0x10  ctor: 0
    unsigned int   m_zero14;        // +0x14  ctor: 0
    unsigned int   m_zero18;        // +0x18  ctor: 0
    unsigned int   m_unknown1C;     // +0x1C  [UNRESOLVED] - the ctor does not write it
    unsigned int   m_zero20;        // +0x20  ctor: 0
    unsigned int   m_zero24;        // +0x24  ctor: 0
    unsigned int   m_zero28;        // +0x28  ctor: 0
    unsigned char  m_flag2C;        // +0x2C  ctor: 1
    unsigned char  m_fontLoaded;    // +0x2D  ctor: 0; setFont writes 1 on success, 0 on failure
    unsigned char  m_pad2E[2];
    unsigned int   m_colour;        // +0x30  ctor: 0xFF000000 - opaque black, argb
};

#if defined(_M_IX86)
static_assert(sizeof(DebugTextWriter) == 0x34,
              "RefFactory_geom_DebugTextWriter allocates 0x34 - see 0x1007CE00");
#endif


// geom_DebugTextWriter_ctor, 0x1009B7C0.

DebugTextWriter::DebugTextWriter()
    : m_refCount(1)
    , m_font(0)
    , m_unknown0C(0)
    , m_zero10(0)
    , m_zero14(0)
    , m_zero18(0)
    , m_unknown1C(0)
    , m_zero20(0)
    , m_zero24(0)
    , m_zero28(0)
    , m_flag2C(1)
    , m_fontLoaded(0)
    , m_colour(0xFF000000u)     // the ctor stores -16777216
{
    m_pad2E[0] = 0;
    m_pad2E[1] = 0;

    // The original does
    //       v2 = bf_malloc(0xC58); if (v2) v3 = geom_Font_Ported_ctor(v2); else v3 = 0;
    //       *(this + 2) = v3;
    // and Font::Ported now exists - see FontSystem/FontSystem.cpp.
    m_font = createFontPortedDirect();

    // The manager registration is reproduced, guard included: `if (dword_10247C58) manager->+0x10(this)`.
    // With ModuleGeomMisc's DebugTextWriterManager class still unwritten the global is null and the
    // call is skipped - which is the original's own behaviour when the manager does not exist yet,
    // not a shortcut.
    if (g_DebugTextWriterManager)
    {
        // [FRONTIER] dice::hfe::geom::DebugTextWriterManager::add
        RD9_FRONTIER("dice::hfe::geom::DebugTextWriterManager::add "
                     "(a manager exists but its slot +0x10 is unwritten)", 0x10);
    }
}

void DebugTextWriter::addRef()
{
    ++m_refCount;
}

int DebugTextWriter::getRef()
{
    return m_refCount;
}

int DebugTextWriter::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::renddx9abi::engineFree(this);
    return remaining;
}

// Two ids, like the other leaf classes in this image.
void* DebugTextWriter::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IDebugTextWriter)
        return const_cast<DebugTextWriter*>(this);
    return 0;
}


// Loads the requested font and stores its ready state. Returns false when no font exists or loading fails.

bool DebugTextWriter::setFont(const std::string& path)
{
    RD9_TRACE("dice::hfe::geom::DebugTextWriter::setFont", 0x10);

    if (!m_font)
    {
        // Cannot happen now that the constructor builds one; kept because the original tolerates a
        // failed allocation here and this is that arm.
        m_fontLoaded = 0;
        return false;
    }

    // font->vslot+0x10(path), by raw offset as the original does it.
    void** vtbl = *reinterpret_cast<void***>(m_font);
    typedef bool (__thiscall* LoadFn)(void*, const std::string&);
    const bool ok = reinterpret_cast<LoadFn>(vtbl[0x10 / 4])(m_font, path);

    m_fontLoaded = ok ? 1 : 0;
    return ok;
}

#define RD9_DTW_FRONTIER(method, slotOffset)                                        \
    void DebugTextWriter::method()                                                  \
    {                                                                               \
        RD9_FRONTIER("dice::hfe::geom::DebugTextWriter::" #method, slotOffset);     \
    }

RD9_DTW_FRONTIER(slot_0x14, 0x14)
RD9_DTW_FRONTIER(slot_0x18, 0x18)
RD9_DTW_FRONTIER(slot_0x1C, 0x1C)
RD9_DTW_FRONTIER(slot_0x20, 0x20)
RD9_DTW_FRONTIER(slot_0x24, 0x24)
RD9_DTW_FRONTIER(slot_0x28, 0x28)
RD9_DTW_FRONTIER(slot_0x2C, 0x2C)
RD9_DTW_FRONTIER(slot_0x30, 0x30)
RD9_DTW_FRONTIER(slot_0x34, 0x34)
RD9_DTW_FRONTIER(slot_0x38, 0x38)
RD9_DTW_FRONTIER(slot_0x3C, 0x3C)
RD9_DTW_FRONTIER(slot_0x40, 0x40)
RD9_DTW_FRONTIER(slot_0x44, 0x44)
RD9_DTW_FRONTIER(slot_0x48, 0x48)

#undef RD9_DTW_FRONTIER

// RefFactory_geom_DebugTextWriter, 0x1007CE00. Same shape and same __fastcall convention as the
// other three factories in this image.
IBase* __fastcall createDebugTextWriter(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    DebugTextWriter* self = new DebugTextWriter();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
