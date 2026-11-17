// Queued debug text, flushed by the writer manager before the device ends its scene.
// [INFERRED] Source placement.
// [RETIRE-WHEN: RW-58] Confirm the original source file before removing the placement uncertainty.

// The proportional and fixed-width writers share this class; the loaded font selects the appearance.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "DebugTextWriter.h"
#include "../Math/Frustum.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <cfenv>
#include <cmath>
#include <climits>
#include <list>
#include <new>
#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

// Class and interface IDs used by module registration and queryInterface.
extern const unsigned int IID_IDebugTextWriter = 80043u;
extern const unsigned int CID_DebugTextWriter  = 81041u;

// The DebugTextWriterManager global, dword_10247C58. Declared in ModuleGeomMisc.cpp.
extern IBase* g_DebugTextWriterManager;

// FontSystem/FontSystem.cpp - the constructor builds one directly, not through the registry.
// The typed adapters dispatch through Font::Ported's slots +0x10, +0x18, +0x1C, +0x24, +0x30
// and +0x34, the slots the shipped bodies below call by raw offset.
IBase* createFontPortedDirect();
bool fontPortedLoad(IBase* font, const std::string& path);
int fontPortedGetHeight(IBase* font);
int fontPortedGetTextWidth(IBase* font, const std::string& text);
unsigned fontPortedGetColour(IBase* font);
void fontPortedSetColour(IBase* font, unsigned colour);
void fontPortedDrawText(IBase* font, float x, float y, const std::string& text);

class DebugTextWriter;
void debugTextWriterManagerAdd(DebugTextWriter* writer);
void debugTextWriterManagerRemove(DebugTextWriter* writer);

// One queued line: the 0x2C-byte record queueTextColoured (0x1009B830) allocates.
// x +0x00, y +0x04, text +0x08, colour +0x24, alignment +0x28 (1 = right-aligned).
struct DebugTextLine
{
    int x, y;
    std::string text;
    unsigned colour;
    int alignment;
    DebugTextLine(int px, int py, unsigned c, const std::string& s, int a)
        : x(px), y(py), text(s), colour(c), alignment(a) {}
};

// One queued text group, as draw (0x1009B5F0) and clear (0x1009B960) read it: x +0x00, y +0x04,
// the line vector +0x08 (0x1C-byte strings), spacing +0x18, colour +0x1C, alignment +0x20,
// bottom-aligned flag +0x24.
struct DebugTextGroup
{
    int x, y;
    std::vector<std::string> lines;
    int spacing;
    unsigned colour;
    int alignment, bottomAligned;

    DebugTextGroup(int px, int py, unsigned c, const std::vector<std::string>& text,
                   int gap, int align, int bottom)
        : x(px), y(py), lines(text), spacing(gap), colour(c),
          alignment(align), bottomAligned(bottom) {}
};


// dice::hfe::geom::DebugTextWriter - 0x34 bytes (52), vtable 0x1020CD10 with 19 slots.

// The slot count is read: the table runs 0x1020CD10..0x1020CD58 and the next dword is 0.

class DebugTextWriter : public IBase
{
public:
    DebugTextWriter();
    ~DebugTextWriter();                                       // 0x1009BA40, called by release

    virtual void  addRef();                                   // +0x00  0x1000CB20 (folded)
    virtual int   getRef();                                   // +0x04  0x101A9A10 (folded)
    virtual int   release();                                  // +0x08  0x1009BD00
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C  0x1009BCD0   read

    virtual bool setFont(const std::string& path);            // +0x10  0x1009B160   read
    virtual bool isEnabled() const;                           // +0x14  0x1009B1E0   read
    virtual void setEnabled(bool enabled);                    // +0x18  0x1009B1F0   read
    virtual unsigned getColour() const;                       // +0x1C  0x1009B210   read
    virtual void setColour(unsigned colour);                  // +0x20  0x1009B220   read
    virtual void queueWorldGroupColoured(const Vec3& position, unsigned colour,
        const std::vector<std::string>& text, int spacing, int alignment, int bottomAligned); // +0x24
    virtual void queueWorldTextColoured(const Vec3& position, unsigned colour,
        const std::string& text, int alignment);              // +0x28
    virtual void queueGroupColoured(int x, int y, unsigned colour,
        const std::vector<std::string>& text, int spacing, int alignment, int bottomAligned); // +0x2C
    virtual void queueTextColoured(int x, int y, unsigned colour,
                                   const std::string& text, int alignment);   // +0x30  0x1009B830
    virtual void queueWorldGroup(const Vec3& position, const std::vector<std::string>& text,
        int spacing, int alignment, int bottomAligned);       // +0x34
    virtual void queueWorldText(const Vec3& position, const std::string& text, int alignment); // +0x38
    virtual void queueGroup(int x, int y, const std::vector<std::string>& text,
        int spacing, int alignment, int bottomAligned);       // +0x3C
    virtual void queueText(int x, int y, const std::string& text, int alignment);  // +0x40  0x1009B190
    virtual void draw();                                      // +0x44  0x1009B5F0   read
    virtual void clear();                                     // +0x48  0x1009B960   read

private:
    bool project(Vec3& position); // [INFERRED] name; PE 0x1009B230
    int            m_refCount;      // +0x04  ctor: 1
    void*          m_font;          // +0x08  a geom::Font::Ported, 0xC58 bytes - see below
    unsigned int   m_unknown0C;     // +0x0C  [UNRESOLVED] - the ctor does not write it; the VC7.1
                                    //        vector's allocator word in the image
    std::vector<DebugTextLine*>  m_lines;    // +0x10..+0x18  ctor: 0, 0, 0
    unsigned int   m_unknown1C;     // +0x1C  [UNRESOLVED] - as +0x0C
    std::vector<DebugTextGroup*> m_groups;   // +0x20..+0x28  ctor: 0, 0, 0
    unsigned char  m_flag2C;        // +0x2C  ctor: 1; the enabled flag (+0x14/+0x18)
    unsigned char  m_fontLoaded;    // +0x2D  ctor: 0; setFont and draw store the load result
    unsigned char  m_pad2E[2];
    unsigned int   m_colour;        // +0x30  ctor: 0xFF000000 - opaque black, argb
};

// [CHOSEN, not recovered] Checked only where std::vector is three pointers (x86 MSVC without
// iterator debugging), the layout the image's +0x10/+0x20 vectors have.
#if defined(_M_IX86) && (!defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL == 0)
static_assert(sizeof(DebugTextWriter) == 0x34,
              "RefFactory_geom_DebugTextWriter allocates 0x34 - see 0x1007CE00");
#endif

// Creates the writer's Font::Ported and registers with the writer manager when one exists.
// PE 0x1009B7C0
DebugTextWriter::DebugTextWriter()
    : m_refCount(1)
    , m_flag2C(1)
    , m_fontLoaded(0)
    , m_colour(0xFF000000u)     // the ctor stores -16777216
{
    // The original does
    //       v2 = bf_malloc(0xC58); if (v2) v3 = geom_Font_Ported_ctor(v2); else v3 = 0;
    //       *(this + 2) = v3;
    // and Font::Ported now exists - see FontSystem/FontSystem.cpp.
    m_font = createFontPortedDirect();

    // The manager registration is reproduced, guard included: `if (dword_10247C58) manager->+0x10(this)`.
    // The manager keeps the pointer in its list without taking a reference.
    if (g_DebugTextWriterManager)
        debugTextWriterManagerAdd(this);
}

// 0x1009BA40: clear (+0x48's body, called directly), font +0x08 release, manager +0x14 removal
// when a manager exists, then the two vectors free their buffers (+0x20 first, then +0x10).
DebugTextWriter::~DebugTextWriter()
{
    clear();
    static_cast<IBase*>(m_font)->release();
    if (g_DebugTextWriterManager)
        debugTextWriterManagerRemove(this);
}

void DebugTextWriter::addRef()
{
    ++m_refCount;
}

int DebugTextWriter::getRef()
{
    return m_refCount;
}

// 0x1009BD00: decrement; at zero the destructor (0x1009BA40) runs before the free.
int DebugTextWriter::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~DebugTextWriter();
        ::renddx9abi::engineFree(this);
    }
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

// +0x14, 0x1009B1E0: `mov al, [ecx+2Ch]`.
bool DebugTextWriter::isEnabled() const { return m_flag2C != 0; }

// +0x18, 0x1009B1F0: store the byte; a false argument then calls +0x48 through the vtable.
void DebugTextWriter::setEnabled(bool enabled)
{
    m_flag2C = enabled;
    if (!enabled)
        clear();
}

// +0x1C (0x1009B210) and +0x20 (0x1009B220): the default colour at +0x30.
unsigned DebugTextWriter::getColour() const { return m_colour; }
void DebugTextWriter::setColour(unsigned colour) { m_colour = colour; }

// +0x30, 0x1009B830: allocate the 0x2C-byte record (copying the string), then push_back the
// pointer. A failed allocation pushes null, as the image does.
void DebugTextWriter::queueTextColoured(int x, int y, unsigned colour,
                                        const std::string& text, int alignment)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextLine));
    DebugTextLine* line = storage ? new (storage) DebugTextLine(x, y, colour, text, alignment) : 0;
    m_lines.push_back(line);
}

// +0x40, 0x1009B190: +0x30 through the vtable with the writer's colour inserted third.
void DebugTextWriter::queueText(int x, int y, const std::string& text, int alignment)
{
    queueTextColoured(x, y, m_colour, text, alignment);
}

// +0x44, 0x1009B5F0. An unloaded writer first loads "Fonts/BF1942.font" through font +0x10 and
// stores the result at +0x2D; if that fails it returns with the queue kept - the image does not
// clear it there, so queued lines accumulate while no font loads (shipped behaviour). Otherwise:
// save the font colour (+0x34); per line, a right-aligned line is shifted left by its width
// (+0x24) before the colour (+0x30) is set, then drawText (+0x18) at the converted int x + offset
// and y; per group, the advance is font height (+0x1C) + spacing + 1 and a bottom-aligned group
// starts count * advance higher; finally the saved colour is restored and +0x48 clears the queue.
void DebugTextWriter::draw()
{
    IBase* const font = static_cast<IBase*>(m_font);
    if (!m_fontLoaded)
    {
        m_fontLoaded = fontPortedLoad(font, std::string("Fonts/BF1942.font")) ? 1 : 0;
        if (!m_fontLoaded)
            return;                                         // 0x1009B629, queue left in place
    }

    const unsigned previousColour = fontPortedGetColour(font);
    for (std::vector<DebugTextLine*>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
    {
        DebugTextLine* const line = *it;                    // unguarded, as shipped
        const int offset = line->alignment == 1 ? -fontPortedGetTextWidth(font, line->text) : 0;
        fontPortedSetColour(font, line->colour);
        fontPortedDrawText(font, static_cast<float>(line->x + offset),
                           static_cast<float>(line->y), line->text);
    }
    for (std::vector<DebugTextGroup*>::iterator it = m_groups.begin(); it != m_groups.end(); ++it)
    {
        DebugTextGroup* const group = *it;
        const int advance = fontPortedGetHeight(font) + group->spacing + 1;
        const int top = group->bottomAligned
                            ? -(static_cast<int>(group->lines.size()) * advance) : 0;
        int y = 0;
        for (std::vector<std::string>::const_iterator line = group->lines.begin();
             line != group->lines.end(); ++line)
        {
            const float offset = group->alignment == 1
                                     ? -static_cast<float>(fontPortedGetTextWidth(font, *line))
                                     : 0.0f;
            fontPortedSetColour(font, group->colour);
            fontPortedDrawText(font, static_cast<float>(group->x) + offset,
                               static_cast<float>(group->y + y + top), *line);
            y += advance;
        }
    }
    fontPortedSetColour(font, previousColour);
    clear();                                                // through the vtable (+0x48)
}

// +0x48, 0x1009B960: destroy and free every line record (null entries skipped), destroy each
// group's strings and free its buffer and the group, then free both vector buffers and zero them.
void DebugTextWriter::clear()
{
    for (std::vector<DebugTextLine*>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
    {
        if (DebugTextLine* const line = *it)
        {
            line->~DebugTextLine();
            ::renddx9abi::engineFree(line);
        }
    }
    for (std::vector<DebugTextGroup*>::iterator it = m_groups.begin(); it != m_groups.end(); ++it)
    {
        if (DebugTextGroup* const group = *it)
        {
            group->~DebugTextGroup();
            ::renddx9abi::engineFree(group);
        }
    }
    std::vector<DebugTextLine*>().swap(m_lines);
    std::vector<DebugTextGroup*>().swap(m_groups);
}

namespace {

// The world-text slots use FISTP with chop, including the integer indefinite on overflow.
int textCoordinate(float value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved, chop;
    int result;
    __asm {
        fld value
        fnstcw saved
        mov ax, saved
        or ax, 0C00h
        mov chop, ax
        fldcw chop
        fistp result
        fldcw saved
    }
    return result;
#elif (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Existing native x87 conversion policy.
    return bf2x87::integer<int>(bf2x87::wide(value));
#else
    // [CHOSEN, not recovered] Native processors retain FISTP's invalid-result carrier.
    if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
    {
        std::feraiseexcept(FE_INVALID);
        return INT_MIN;
    }
    return static_cast<int>(value);
#endif
}

} // namespace

// PE 0x1009B230. Only plane four rejects text; viewport origins are not added.
bool DebugTextWriter::project(Vec3& position)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();
    const Plane3& plane = view->getFrustumPtr()->m_planes[4];
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve x87 operation order and the explicit single stores
    // using the established native extended policy; retail PC=24 identity is not established.
    using namespace bf2x87;
    const Real side = add(add(add(mul(wide(plane.normal.z), wide(position.z)),
                                 mul(wide(plane.normal.y), wide(position.y))),
                             mul(wide(position.x), wide(plane.normal.x))), wide(plane.d));
    if (greater(side, wide(0))) return false;
    const float halfWidth = store(mul(wide(view->getViewport()[2]), wide(0.5f)));
    const float halfHeight = store(mul(wide(view->getViewport()[3]), wide(0.5f)));
#else
    if (plane.normal.z * position.z + plane.normal.y * position.y +
        position.x * plane.normal.x + plane.d > 0.0f) return false;
    const float halfWidth = view->getViewport()[2] * 0.5f;
    const float halfHeight = view->getViewport()[3] * 0.5f;
#endif
    const Vec3 original = position;
    Mat4 inverse = *view->getViewMatrixPtr();
    const Mat4 source = inverse;
    fullInvert(source, inverse);
    const float* const m = inverse.m;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const float z = store(add(add(add(mul(wide(m[10]), wide(original.z)),
                                     mul(wide(m[6]), wide(original.y))),
                                 mul(wide(m[2]), wide(original.x))), wide(m[14])));
    const float y = store(add(add(add(mul(wide(m[9]), wide(original.z)),
                                     mul(wide(m[5]), wide(original.y))),
                                 mul(wide(m[1]), wide(original.x))), wide(m[13])));
    const float x = store(add(add(add(mul(wide(m[8]), wide(original.z)),
                                     mul(wide(m[4]), wide(original.y))),
                                 mul(wide(m[0]), wide(original.x))), wide(m[12])));
#else
    const float z = m[10] * original.z + m[6] * original.y + m[2] * original.x + m[14];
    const float y = m[9] * original.z + m[5] * original.y + m[1] * original.x + m[13];
    const float x = m[8] * original.z + m[4] * original.y + m[0] * original.x + m[12];
#endif
    const float* const p = view->getProjectionMatrixPtr()->m;
    // Retail divides by projected Z, rather than the homogeneous W component.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const Real projectedZ = add(add(add(mul(wide(y), wide(p[6])),
                                       mul(wide(z), wide(p[10]))),
                                   mul(wide(x), wide(p[2]))), wide(p[14]));
    const float projectedY = store(add(add(add(mul(wide(y), wide(p[5])),
                                              mul(wide(z), wide(p[9]))),
                                          mul(wide(x), wide(p[1]))), wide(p[13])));
    const float projectedX = store(add(add(add(mul(wide(y), wide(p[4])),
                                              mul(wide(z), wide(p[8]))),
                                          mul(wide(x), wide(p[0]))), wide(p[12])));
    const Real inverseZ = div(wide(1.0f), projectedZ);
    const Real scaledX = mul(mul(wide(projectedX), inverseZ), wide(halfWidth));
    const float offsetY = store(mul(mul(-wide(halfHeight), wide(projectedY)), inverseZ));
    position.x = store(add(scaledX, wide(halfWidth)));
    position.y = store(add(wide(offsetY), wide(halfHeight)));
    position.z = store(inverseZ);
#else
    const float projectedZ = y * p[6] + z * p[10] + x * p[2] + p[14];
    const float projectedY = y * p[5] + z * p[9] + x * p[1] + p[13];
    const float projectedX = y * p[4] + z * p[8] + x * p[0] + p[12];
    const float inverseZ = 1.0f / projectedZ;
    const float offsetY = -halfHeight * projectedY * inverseZ;
    position.x = projectedX * inverseZ * halfWidth + halfWidth;
    position.y = offsetY + halfHeight;
    position.z = inverseZ;
#endif
    return true;
}

// PE 0x1009B560, slot +0x24.
void DebugTextWriter::queueWorldGroupColoured(const Vec3& position, unsigned colour,
    const std::vector<std::string>& text, int spacing, int alignment, int bottomAligned)
{
    Vec3 screen = position;
    if (project(screen))
    {
        const int y = textCoordinate(screen.y);
        const int x = textCoordinate(screen.x);
        queueGroupColoured(x, y, colour, text, spacing, alignment, bottomAligned);
    }
}

// PE 0x1009B450, slot +0x28.
void DebugTextWriter::queueWorldTextColoured(const Vec3& position, unsigned colour,
    const std::string& text, int alignment)
{
    Vec3 screen = position;
    if (project(screen))
    {
        const int y = textCoordinate(screen.y);
        const int x = textCoordinate(screen.x);
        queueTextColoured(x, y, colour, text, alignment);
    }
}

// PE 0x1009B8C0, slot +0x2C. A failed allocation still queues a null record.
void DebugTextWriter::queueGroupColoured(int x, int y, unsigned colour,
    const std::vector<std::string>& text, int spacing, int alignment, int bottomAligned)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextGroup));
    DebugTextGroup* group = storage ? new (storage) DebugTextGroup(
        x, y, colour, text, spacing, alignment, bottomAligned) : 0;
    m_groups.push_back(group);
}

// PE 0x1009B4D0, slot +0x34.
void DebugTextWriter::queueWorldGroup(const Vec3& position, const std::vector<std::string>& text,
    int spacing, int alignment, int bottomAligned)
{
    Vec3 screen = position;
    if (project(screen))
    {
        const unsigned colour = m_colour;
        const int y = textCoordinate(screen.y);
        const int x = textCoordinate(screen.x);
        queueGroupColoured(x, y, colour, text, spacing, alignment, bottomAligned);
    }
}

// PE 0x1009B3D0, slot +0x38.
void DebugTextWriter::queueWorldText(const Vec3& position, const std::string& text, int alignment)
{
    Vec3 screen = position;
    if (project(screen))
    {
        const unsigned colour = m_colour;
        const int y = textCoordinate(screen.y);
        const int x = textCoordinate(screen.x);
        queueTextColoured(x, y, colour, text, alignment);
    }
}

// PE 0x1009B1B0, slot +0x3C.
void DebugTextWriter::queueGroup(int x, int y, const std::vector<std::string>& text,
    int spacing, int alignment, int bottomAligned)
{
    queueGroupColoured(x, y, m_colour, text, spacing, alignment, bottomAligned);
}

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


// dice::hfe::geom::DebugTextWriterManager - 0x14 bytes, vtable 0x1020CD80 with 7 slots, ctor
// 0x1009BEA0. A std::list of writers it does not reference-count.
class DebugTextWriterManager : public IBase
{
public:
    // PE 0x1009BEA0
    DebugTextWriterManager() : m_refCount(1) {}

    virtual void addRef() { ++m_refCount; }                   // +0x00  0x1000CB20 (folded)
    virtual int getRef() { return m_refCount; }               // +0x04  0x101A9A10 (folded)

    // +0x08, 0x1009BFA0: decrement; at zero the deleting destructor (0x1009BFC0).
    virtual int release()
    {
        const int refs = --m_refCount;
        if (!refs)
        {
            this->~DebugTextWriterManager();
            ::renddx9abi::engineFree(this);
        }
        return refs;
    }

    // +0x0C, 0x1009BF70: IBase (1) or dword_1020AE98 (0x138AC, 80044).
    virtual void* queryInterface(unsigned iid) const
    {
        return iid == RD9_IID_IBASE || iid == 80044u ? const_cast<DebugTextWriterManager*>(this) : 0;
    }

    // +0x10, 0x1009BEE0: a non-null writer is inserted at the list's end.
    virtual void add(DebugTextWriter* writer) { if (writer) m_writers.push_back(writer); }
    // +0x14, 0x1009BE80: a non-null writer is removed (list::remove).
    virtual void remove(DebugTextWriter* writer) { if (writer) m_writers.remove(writer); }

    // +0x18, 0x1009BE40: per writer, +0x14 (enabled) selects +0x44 (draw) or +0x48 (clear).
    // RendDX9Draw.cpp's end-scene path calls it before EndScene (0x10005665).
    virtual void draw()
    {
        for (std::list<DebugTextWriter*>::iterator it = m_writers.begin(); it != m_writers.end(); ++it)
        {
            if ((*it)->isEnabled())
                (*it)->draw();
            else
                (*it)->clear();
        }
    }

private:
    int m_refCount;                                           // +0x04  ctor: 1
    std::list<DebugTextWriter*> m_writers;                    // +0x08
};

// The factory registered as "dice.hfe.geom.DebugTextWriterManager", 0x1007CE50: allocate 0x14,
// construct, queryInterface, release on a refused id. The allocation result is not checked
// before the queryInterface call, as in the image.
IBase* __fastcall createDebugTextWriterManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextWriterManager));
    DebugTextWriterManager* manager = storage ? new (storage) DebugTextWriterManager() : 0;
    void* result = manager->queryInterface(iid);
    if (!result)
        manager->release();
    return static_cast<IBase*>(result);
}

// Typed bridges for the writer's constructor/destructor and the renderer's callers.
void debugTextWriterManagerAdd(DebugTextWriter* writer)
{
    static_cast<DebugTextWriterManager*>(g_DebugTextWriterManager)->add(writer);
}

void debugTextWriterManagerRemove(DebugTextWriter* writer)
{
    static_cast<DebugTextWriterManager*>(g_DebugTextWriterManager)->remove(writer);
}

void debugTextWriterManagerDraw(IBase* manager)
{
    static_cast<DebugTextWriterManager*>(manager)->draw();
}

void debugTextWriterSetColour(IBase* writer, unsigned colour)
{
    static_cast<DebugTextWriter*>(writer)->setColour(colour);
}

unsigned debugTextWriterGetColour(IBase* writer)
{
    return static_cast<DebugTextWriter*>(writer)->getColour();
}

void debugTextWriterQueueWorldText(IBase* writer, const Vec3& position, unsigned colour,
                                    const std::string& text, int alignment)
{
    static_cast<DebugTextWriter*>(writer)->queueWorldTextColoured(position, colour, text, alignment);
}

void debugTextWriterQueueText(IBase* writer, int x, int y, const std::string& text, int alignment)
{
    static_cast<DebugTextWriter*>(writer)->queueText(x, y, text, alignment);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
