// Native placement of the particle system's byte-offset objects: the five descriptor types, the five
// instance types, the five pool-record types and the four renderers the manager embeds. The code
// addresses their fields by retail x86 offset; the functions below turn a retail offset into the
// offset this build stores the field at.
//
// x86: every function is the identity and every size is the retail size, so the objects keep their
// exact retail layout.
//
// x64 [CHOSEN, not recovered] (https://github.com/B2BF/OpenBF2/issues/7): a pointer, a host string or a
// std::vector no longer fits its retail 4-, 28- or 16-byte span, and a hand-built or C++ vptr takes
// 8 bytes. Each such field moves into a block appended after the retail bytes (the tail); for the
// polymorphic objects the retail bytes +4..+7, which the wider vptr covers, move there as well.
// Every other retail byte keeps its retail offset, so runs of scalar fields stay contiguous and code
// that never touches a moved field addresses the same bytes on both architectures.

#ifndef BF2_GEOM_PARTICLESYSTEM_PARTICLERAWLAYOUT_H
#define BF2_GEOM_PARTICLESYSTEM_PARTICLERAWLAYOUT_H

#include <string.h>

namespace dice {
namespace hfe {
namespace geom {

// A field at an offset that is already native.
template<class T> inline T& psmRawField(void* object, unsigned int nativeOffset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + nativeOffset);
}

// Marks a relocation slot a layout does not use.
const unsigned int kPsmNoField = 0xFFFFFFFFu;

// Bytes per entry of the engine-allocated pointer tables (retail 4).
const unsigned int kPsmPointerSize = sizeof(void*);

// Instances. Pointer fields shared by all five types: effect controller +0x0C, owner +0x10,
// descriptor +0x2C, parent matrix +0x30 and (type 0 only) player +0x230. The particle list is +0x24
// in types 0 and 4 and +0x14 in types 1, 2 and 3; at the other offset those types keep a scalar.
const unsigned int kPsmInstanceListA = 0x24;   // types 0 and 4
const unsigned int kPsmInstanceListB = 0x14;   // types 1, 2 and 3

#if defined(_M_IX86)

inline unsigned int psmInstanceOffset(unsigned int retail, unsigned int) { return retail; }
// Bytes an instance class declares after its vptr.
#define PSM_INSTANCE_DATA(retailSize) ((retailSize) - 4u)

#else

const unsigned int kPsmInstanceTail = 0x240;   // past the largest retail instance (type 0, 0x23C)
const unsigned int kPsmInstanceSize = kPsmInstanceTail + 0x38;

inline unsigned int psmInstanceOffset(unsigned int retail, unsigned int list)
{
    if (retail - 4u < 4u)
        return kPsmInstanceTail + (retail - 4u);    // the flag bytes +4..+7
    switch (retail)
    {
    case 0x0C:  return kPsmInstanceTail + 0x08;
    case 0x10:  return kPsmInstanceTail + 0x10;
    case 0x2C:  return kPsmInstanceTail + 0x20;
    case 0x30:  return kPsmInstanceTail + 0x28;
    case 0x230: return kPsmInstanceTail + 0x30;
    }
    return retail == list ? kPsmInstanceTail + 0x18 : retail;
}
#define PSM_INSTANCE_DATA(retailSize) (kPsmInstanceSize - 8u)

#endif

// Instance fields by retail offset: types 0/4, types 1/2/3, and any type (never the particle list).
template<class T> inline T& psmInstanceA(void* instance, unsigned int retail)
{ return psmRawField<T>(instance, psmInstanceOffset(retail, kPsmInstanceListA)); }
template<class T> inline T& psmInstanceB(void* instance, unsigned int retail)
{ return psmRawField<T>(instance, psmInstanceOffset(retail, kPsmInstanceListB)); }
template<class T> inline T& psmInstance(void* instance, unsigned int retail)
{ return psmRawField<T>(instance, psmInstanceOffset(retail, kPsmNoField)); }
template<class T> inline const T& psmConstInstanceB(const void* instance, unsigned int retail)
{ return psmInstanceB<T>(const_cast<void*>(instance), retail); }

// Descriptors. Each type holds two host strings and up to three pointers:
//   type 0: strings +0x224 / +0x240, texture +0x25C, atlas region +0x260, collector +0x2A0
//   type 1: strings +0x070 / +0x1F4, geometry +0x8C, texture +0x210
//   type 2: strings +0x1E4 / +0x200, texture +0x21C
//   type 3: strings +0x06C / +0x16C, child template +0x88
//   type 4: strings +0x218 / +0x234, texture +0x250, atlas region +0x254
// The 16-bit template id at +0x04 is shared by all types.

#if defined(_M_IX86)

inline unsigned int psmDescriptorOffset(unsigned int retail, unsigned int, unsigned int,
                                        unsigned int, unsigned int, unsigned int)
{
    return retail;
}
// Bytes a descriptor of the given retail size occupies; a host string's reserved span.
#define PSM_DESCRIPTOR_SIZE(retailSize) (retailSize)
const unsigned int kPsmDescriptorStringSpan = 28;

#else

const unsigned int kPsmDescriptorTail = 0x2A8;   // past the largest retail descriptor (type 0, 0x2A4)

inline unsigned int psmDescriptorOffset(unsigned int retail, unsigned int string0,
                                        unsigned int string1, unsigned int pointer0,
                                        unsigned int pointer1, unsigned int pointer2)
{
    if (retail - 4u < 4u)
        return kPsmDescriptorTail + (retail - 4u);  // the template id word
    if (retail == string0)  return kPsmDescriptorTail + 0x08;
    if (retail == string1)  return kPsmDescriptorTail + 0x28;
    if (retail == pointer0) return kPsmDescriptorTail + 0x48;
    if (retail == pointer1) return kPsmDescriptorTail + 0x50;
    if (retail == pointer2) return kPsmDescriptorTail + 0x58;
    return retail;
}
#define PSM_DESCRIPTOR_SIZE(retailSize) (kPsmDescriptorTail + 0x60u)
const unsigned int kPsmDescriptorStringSpan = 32;

#endif

inline unsigned int psmDescriptor0Offset(unsigned int retail)
{ return psmDescriptorOffset(retail, 0x224, 0x240, 0x25C, 0x260, 0x2A0); }
inline unsigned int psmDescriptor1Offset(unsigned int retail)
{ return psmDescriptorOffset(retail, 0x070, 0x1F4, 0x08C, 0x210, kPsmNoField); }
inline unsigned int psmDescriptor2Offset(unsigned int retail)
{ return psmDescriptorOffset(retail, 0x1E4, 0x200, 0x21C, kPsmNoField, kPsmNoField); }
inline unsigned int psmDescriptor3Offset(unsigned int retail)
{ return psmDescriptorOffset(retail, 0x06C, 0x16C, 0x088, kPsmNoField, kPsmNoField); }
inline unsigned int psmDescriptor4Offset(unsigned int retail)
{ return psmDescriptorOffset(retail, 0x218, 0x234, 0x250, 0x254, kPsmNoField); }
// Any descriptor type: the scalar fields and the template id.
inline unsigned int psmDescriptorCommonOffset(unsigned int retail)
{ return psmDescriptorOffset(retail, kPsmNoField, kPsmNoField, kPsmNoField, kPsmNoField, kPsmNoField); }

// Descriptor vtable thunks. A float argument: x86 thiscall passes it in a 4-byte stack slot that the
// shipped setters store bit for bit, so the thunks keep taking it as an unsigned there; x64 passes a
// float in an XMM register. A Vec3 by value: three stack dwords on x86, a pointer to the caller's
// copy on x64. A bool: only the low byte is defined on x64.
#if defined(_M_IX86)
typedef unsigned int PsmFloatArg;
inline unsigned int psmFloatArgBits(PsmFloatArg value) { return value; }
inline PsmFloatArg psmFloatArgFromBits(unsigned int bits) { return bits; }
inline unsigned int psmBoolArg(unsigned int value) { return value; }
#define PSM_VEC3_PARAMS unsigned a, unsigned b, unsigned c
#define PSM_VEC3_X a
#define PSM_VEC3_Y b
#define PSM_VEC3_Z c
#else
typedef float PsmFloatArg;
inline unsigned int psmFloatArgBits(PsmFloatArg value)
{
    unsigned int bits;
    static_assert(sizeof(bits) == sizeof(value), "float is 32 bits");
    ::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
inline PsmFloatArg psmFloatArgFromBits(unsigned int bits)
{
    float value;
    ::memcpy(&value, &bits, sizeof(value));
    return value;
}
inline unsigned int psmBoolArg(unsigned int value) { return value & 0xFFu; }
#define PSM_VEC3_PARAMS const unsigned int* vec3
#define PSM_VEC3_X vec3[0]
#define PSM_VEC3_Y vec3[1]
#define PSM_VEC3_Z vec3[2]
#endif

template<class T> inline T& psmDescriptor0(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptor0Offset(retail)); }
template<class T> inline T& psmDescriptor1(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptor1Offset(retail)); }
template<class T> inline T& psmDescriptor2(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptor2Offset(retail)); }
template<class T> inline T& psmDescriptor3(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptor3Offset(retail)); }
template<class T> inline T& psmDescriptor4(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptor4Offset(retail)); }
template<class T> inline T& psmDescriptor(void* d, unsigned int retail) { return psmRawField<T>(d, psmDescriptorCommonOffset(retail)); }

// Pool records, by particle type. Retail word 0 is the free-list link (and the draw sorter's chain
// link); types 0 and 4 keep the particle's texture at +0x5C / +0x58 and type 3 its child instance at
// +0xAC. Every other word is a scalar and keeps its retail offset.
#if defined(_M_IX86)
inline unsigned int psmRecordStride(unsigned int type)
{
    static const unsigned int strides[5] = { 148, 192, 168, 176, 200 };
    return strides[type];
}
inline unsigned int psmRecordLinkOffset(unsigned int) { return 0; }
inline unsigned int psmRecordPointerOffset(unsigned int type)
{
    static const unsigned int offsets[5] = { 0x5C, kPsmNoField, kPsmNoField, 0xAC, 0x58 };
    return offsets[type];
}
#else
inline unsigned int psmRecordStride(unsigned int type)
{
    static const unsigned int strides[5] = { 168, 200, 176, 192, 216 };
    return strides[type];
}
inline unsigned int psmRecordLinkOffset(unsigned int type)
{
    static const unsigned int offsets[5] = { 152, 192, 168, 176, 200 };
    return offsets[type];
}
inline unsigned int psmRecordPointerOffset(unsigned int type)
{
    static const unsigned int offsets[5] = { 160, kPsmNoField, kPsmNoField, 184, 208 };
    return offsets[type];
}
#endif

inline void*& psmRecordLink(void* record, unsigned int type)
{ return psmRawField<void*>(record, psmRecordLinkOffset(type)); }
template<class T> inline T& psmRecordPointer(void* record, unsigned int type)
{ return psmRawField<T>(record, psmRecordPointerOffset(type)); }

// The renderers' {instance, last index, count} draw groups: 12 bytes on x86, 16 on x64.
struct PsmDrawGroup
{
    unsigned char* instance;   // +0x00
    int            last;       // +0x04  index of the group's last table entry
    unsigned int   count;      // +0x08
};
#if defined(_M_IX86)
static_assert(sizeof(PsmDrawGroup) == 12, "draw groups are 12-byte retail records");
#endif

#if defined(_M_IX86)

inline unsigned int psmSpriteRendererOffset(unsigned int retail) { return retail; }
inline unsigned int psmMeshRendererOffset(unsigned int retail)   { return retail; }
inline unsigned int psmTrailRendererOffset(unsigned int retail)  { return retail; }
inline unsigned int psmNsaRendererOffset(unsigned int retail)    { return retail; }
const unsigned int kPsmSpriteRendererSize = 0x5E18;
const unsigned int kPsmMeshRendererSize   = 0x1D90;
const unsigned int kPsmTrailRendererSize  = 0x0914;
const unsigned int kPsmNsaRendererSize    = 0x05A0;

#else

// A retail offset inside a run of 12-byte draw groups, as a native offset relative to the run.
inline unsigned int psmDrawGroupOffset(unsigned int relative)
{
    const unsigned int field = relative % 12u;
    return (relative / 12u) * sizeof(PsmDrawGroup) + (field < 4u ? field : field + 4u);
}

// ParticleSystemRenderer (retail 0x5E18): the instance list +0x08, the sorted and additive tables
// +0x18/+0x1C, the 2x256 draw groups +0x20, the radix buckets +0x182C, the scratch pointers +0x1DC0
// and +0x1DC8, the vertex buffer +0x1DCC, the effect +0x1DD0, 14 shader parameters +0x1DD4 and the
// vertex declaration +0x1E0C.
inline unsigned int psmSpriteRendererOffset(unsigned int retail)
{
    const unsigned int tail = 0x5E18;
    if (retail - 0x20u < 0x1800u)   return tail + 0x8C0 + psmDrawGroupOffset(retail - 0x20u);
    if (retail - 0x182Cu < 0x400u)  return tail + 0x0C0 + 2u * (retail - 0x182Cu);
    if (retail - 0x1DD4u < 0x38u)   return tail + 0x048 + 2u * (retail - 0x1DD4u);
    switch (retail)
    {
    case 0x0008: return tail + 0x00;
    case 0x0018: return tail + 0x18;
    case 0x001C: return tail + 0x20;
    case 0x1DC0: return tail + 0x28;
    case 0x1DC8: return tail + 0x30;
    case 0x1DCC: return tail + 0x38;
    case 0x1DD0: return tail + 0x40;
    case 0x1E0C: return tail + 0xB8;
    }
    return retail;
}
const unsigned int kPsmSpriteRendererSize = 0x5E18 + 0x28C0;

// MeshParticlesRenderer (retail 0x1D90): the particle table +0x04, the 2x256 draw groups +0x08, the
// radix buckets +0x1814 and the retained mesh list +0x1D80.
inline unsigned int psmMeshRendererOffset(unsigned int retail)
{
    const unsigned int tail = 0x1D90;
    if (retail - 0x08u < 0x1800u)   return tail + 0x0020 + psmDrawGroupOffset(retail - 0x08u);
    if (retail - 0x1814u < 0x400u)  return tail + 0x2020 + 2u * (retail - 0x1814u);
    switch (retail)
    {
    case 0x0004: return tail + 0x00;
    case 0x1D80: return tail + 0x08;
    }
    return retail;
}
const unsigned int kPsmMeshRendererSize = 0x1D90 + 0x2820;

// TrailRenderer (retail 0x914): the device-listener vptr at +0 widens over the index +0x04; the
// instance list +0x08, the particle table +0x18, the 64 draw groups +0x1C, the vertex buffer +0x8B4,
// the index buffers +0x8B8 and side vertex arrays +0x8C8 (indexed by side count 1..3), the effect
// +0x8D8, 13 shader parameters +0x8DC and the vertex declaration +0x910.
inline unsigned int psmTrailRendererOffset(unsigned int retail)
{
    const unsigned int tail = 0x918;
    if (retail - 4u < 4u)           return tail + 0x000 + (retail - 4u);
    if (retail - 0x1Cu < 0x300u)    return tail + 0x100 + psmDrawGroupOffset(retail - 0x1Cu);
    if (retail - 0x8B8u < 0x10u)    return tail + 0x030 + 2u * (retail - 0x8B8u);
    if (retail - 0x8C8u < 0x10u)    return tail + 0x050 + 2u * (retail - 0x8C8u);
    if (retail - 0x8DCu < 0x34u)    return tail + 0x078 + 2u * (retail - 0x8DCu);
    switch (retail)
    {
    case 0x008: return tail + 0x08;
    case 0x018: return tail + 0x20;
    case 0x8B4: return tail + 0x28;
    case 0x8D8: return tail + 0x70;
    case 0x910: return tail + 0xE0;
    }
    return retail;
}
const unsigned int kPsmTrailRendererSize = 0x918 + 0x500;

// NSAParticleSystemRenderer (retail 0x5A0): the instance list +0x0C, the table +0x1C, the radix
// buckets +0x20, the scratch pointers +0x55C and +0x564, the vertex buffer +0x568, the effect +0x56C,
// 11 shader parameters +0x570 and the vertex declaration +0x59C.
inline unsigned int psmNsaRendererOffset(unsigned int retail)
{
    const unsigned int tail = 0x5A0;
    if (retail - 0x20u < 0x400u)    return tail + 0x020 + 2u * (retail - 0x20u);
    if (retail - 0x570u < 0x2Cu)    return tail + 0x840 + 2u * (retail - 0x570u);
    switch (retail)
    {
    case 0x00C: return tail + 0x000;
    case 0x01C: return tail + 0x018;
    case 0x55C: return tail + 0x820;
    case 0x564: return tail + 0x828;
    case 0x568: return tail + 0x830;
    case 0x56C: return tail + 0x838;
    case 0x59C: return tail + 0x898;
    }
    return retail;
}
const unsigned int kPsmNsaRendererSize = 0x5A0 + 0x8A0;

#endif

template<class T> inline T& psmSpriteField(unsigned char* renderer, unsigned int retail)
{ return psmRawField<T>(renderer, psmSpriteRendererOffset(retail)); }
template<class T> inline T& psmMeshField(unsigned char* renderer, unsigned int retail)
{ return psmRawField<T>(renderer, psmMeshRendererOffset(retail)); }
template<class T> inline T& psmTrailField(unsigned char* renderer, unsigned int retail)
{ return psmRawField<T>(renderer, psmTrailRendererOffset(retail)); }
template<class T> inline T& psmNsaField(unsigned char* renderer, unsigned int retail)
{ return psmRawField<T>(renderer, psmNsaRendererOffset(retail)); }

// The draw-group arrays, by retail offset of their first group.
inline PsmDrawGroup* psmSpriteGroups(unsigned char* renderer, unsigned int retail)
{ return &psmSpriteField<PsmDrawGroup>(renderer, retail); }
inline PsmDrawGroup* psmMeshGroups(unsigned char* renderer, unsigned int retail)
{ return &psmMeshField<PsmDrawGroup>(renderer, retail); }
inline PsmDrawGroup* psmTrailGroups(unsigned char* renderer, unsigned int retail)
{ return &psmTrailField<PsmDrawGroup>(renderer, retail); }

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_PARTICLESYSTEM_PARTICLERAWLAYOUT_H
