// Path:     [INFERRED] - the concrete implementations are attested to Effect.cpp and
//           FXParameter.cpp; no standalone header path is present in RendDX9.dll.
// Contents: the exact twelve-slot parameter-owner contract shared by ParameterOwner,
//           ShaderParameter and FXParameterImpl.

#ifndef BF2_RENDDX9_SHADERPARAMETER_H
#define BF2_RENDDX9_SHADERPARAMETER_H

namespace dice {
namespace hfe {
namespace dx9rend {

// PE tables 0x101DE9E8 (the process-wide unbound owner), 0x101DFB1C
// (ShaderParameter) and 0x101DFB4C (FXParameterImpl) all implement this exact slot order.
// There is deliberately no virtual destructor: none of the three retail tables has a destructor
// slot, and each concrete release() owns its destruction policy.
class ParameterOwner
{
public:
    virtual int          getRef() const = 0;                                      // +0x00
    virtual int          addRef() = 0;                                             // +0x04
    virtual int          release() = 0;                                            // +0x08
    virtual void*        getD3DXParam() const = 0;                                 // +0x0C
    virtual void         setD3DXParam(void* handle) = 0;                           // +0x10
    virtual bool         isParameterUsed() const = 0;                              // +0x14
    virtual void         setValue(const void* data, unsigned int count) = 0;       // +0x18
    virtual void         setBool(unsigned char value) = 0;                         // +0x1C
    virtual void         setInt(int value) = 0;                                    // +0x20
    virtual void         setFloat(float value) = 0;                                // +0x24
    virtual void         setMatrixPointerArray(const void* const* matrices,
                                               unsigned int count) = 0;             // +0x28
    virtual unsigned int getTypeCode() const = 0;                                  // +0x2C

protected:
    ~ParameterOwner() {}
};

// PE 0x100219E0 / 0x100218C0, twelve bytes.  This is the counted public handle stored in Effect's
// semantic-name registry.  Every operation is an ordinary typed forward onto the current owner.
class ShaderParameter final : public ParameterOwner
{
public:
    explicit ShaderParameter(ParameterOwner* owner);
    ~ShaderParameter();

    int          getRef() const;
    int          addRef();
    int          release();
    void*        getD3DXParam() const;
    void         setD3DXParam(void* handle);
    bool         isParameterUsed() const;
    void         setValue(const void* data, unsigned int count);
    void         setBool(unsigned char value);
    void         setInt(int value);
    void         setFloat(float value);
    void         setMatrixPointerArray(const void* const* matrices, unsigned int count);
    unsigned int getTypeCode() const;

private:
    friend class Effect;
    int             m_refCount; // +0x04
    ParameterOwner* m_owner;    // +0x08
};

#if defined(_M_IX86)
static_assert(sizeof(ParameterOwner) == 0x04,
              "ParameterOwner is the one-vptr twelve-slot contract");
static_assert(sizeof(ShaderParameter) == 0x0C,
              "PE 0x100219E0 constructs a twelve-byte ShaderParameter");
#endif

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_RENDDX9_SHADERPARAMETER_H
