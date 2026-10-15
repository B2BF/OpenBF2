// [CHOSEN, not recovered] shared mesh/emitter particle-instance accessors.
// Include once before the type-1 and type-3 instance implementations.
// Both instances share these field offsets; the mesh renderer refreshes recent visibility
// for type 1, while type 3 has no corresponding writer.

template<class T> const T& psmConstField(const void* object, unsigned int offset)
{ return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(object) + offset); }

// PE 101839D0: `mov al, [ecx+144h]; retn`.
bool psmShared13IsEmitting(const void* self) { return psmConstInstanceB<unsigned char>(self, 0x144) != 0; }

// PE 101839E0: `mov al, [ebp+8]; mov [ecx+21Dh], al; retn 4`.
void psmShared13SetPaused(void* self, bool paused) { psmInstanceB<unsigned char>(self, 0x21D) = paused; }

// PE 10177570: `mov al, [ecx+21Dh]; retn`.
bool psmShared13IsPaused(const void* self) { return psmConstInstanceB<unsigned char>(self, 0x21D) != 0; }

// the recently-visible byte. Start, the 60-frame emission gate and clear
// all write it too; nothing in the renderer refreshes it for type 3 (see the type-3 readme).
void psmShared13SetVisible(void* self, bool value) { psmInstanceB<unsigned char>(self, 0x21E) = value; }
bool psmShared13IsVisible(const void* self) { return psmConstInstanceB<unsigned char>(self, 0x21E) != 0; }

// plain dword store / x87 load.
void psmShared13SetSpeedScale(void* self, float value) { psmInstanceB<float>(self, 0x138) = value; }
float psmShared13SpeedScale(const void* self) { return psmConstInstanceB<float>(self, 0x138); }

// loads z, y, x (x as a dword, y/z through fld), then stores y, x, z.
void psmShared13SetDirectionScale(void* self, const Vec3& value)
{
    const float z = value.z, y = value.y, x = value.x;
    psmInstanceB<float>(self, 0x14C) = y;
    psmInstanceB<float>(self, 0x148) = x;
    psmInstanceB<float>(self, 0x150) = z;
}
// PE 10183A20: `lea eax, [ecx+148h]; retn`.
const Vec3& psmShared13DirectionScale(const void* self) { return psmConstInstanceB<Vec3>(self, 0x148); }

// same load/store order as 10183A30, at +0x1C.
void psmShared13SetVelocityOffset(void* self, const Vec3& value)
{
    const float z = value.z, y = value.y, x = value.x;
    psmInstanceB<float>(self, 0x20) = y;
    psmInstanceB<float>(self, 0x1C) = x;
    psmInstanceB<float>(self, 0x24) = z;
}

// PE 10183A60, 223 B. Distance from the emitter position (+0xA4) to the bounds centre plus the
// bounds half-diagonal. Spill points read from the body: the centre x (var_C) and the z sum
// (var_10) are rounded to float, the centre y is NOT - `fmul 0.5` stays on the stack and 10183AB5
// subtracts the position from the unrounded value before the one `fstp` to var_8.
// The tree's type-0/type-4 classes spell this same PE body with the centre y rounded first
// (`static_cast<float>(...*0.5)-position.y`). That is a one-rounding difference in their inline
// copies, reported for the integrator; this shared version follows the instructions.
float psmShared13BoundsRadius(const void* self)
{
    const Vec3& low = psmConstInstanceB<Vec3>(self, 0xFC);
    const Vec3& high = psmConstInstanceB<Vec3>(self, 0x108);
    const Vec3& position = psmConstInstanceB<Vec3>(self, 0xA4);
    const float x = static_cast<float>((static_cast<double>(low.x) + high.x) * 0.5) - position.x;
    const float y = static_cast<float>((static_cast<double>(high.y) + low.y) * 0.5 - position.y);
    const float zsum = high.z + low.z;
    const double z = zsum * 0.5 - position.z;
    const float dx = static_cast<float>((static_cast<double>(high.x) - low.x) * 0.5), dz = high.z - low.z;
    const double dy = (static_cast<double>(high.y) - low.y) * 0.5;
    return static_cast<float>(::sqrt(dz * 0.5 * (dz * 0.5) + dy * dy + static_cast<double>(dx) * dx)
        + ::sqrt(z * z + static_cast<double>(y) * y + static_cast<double>(x) * x));
}
