// HUD pointer positioning and routing. [INFERRED] Source split.
#include "BF2HudManager.h"
#if !DEDICATED
#include "Ref2System.h"
#include "HudAmmo.h"
#include "Bf2PictureNode.h"
#include "../External/BFMeme/MovePointer.h"
#include "../External/BFMeme/Stream.h"
#include <new>
namespace dice { namespace hfe { namespace hud {
namespace {
float centerPointer(float center, float dimension)
{
    const float half=0.5f;
#if defined(_M_IX86)
    float result;
    __asm {
        fld dimension
        fmul half
        fsubr center
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(center - static_cast<double>(dimension) * half);
#endif
}
float pointerDimension(int dimension) // 751200 fild and two equal float stores
{
#if defined(_M_IX86)
    float result;
    __asm {
        fild dimension
        fstp result
    }
    return result;
#else
    // The int is exact in double; the single float rounding is the fstp's.
    return static_cast<float>(static_cast<double>(dimension));
#endif
}
}
void BF2HudManager::enablePointer(std::intptr_t texture, int mode)
{
    menu::Ref2System* initialSystem=ref2System;
    m_field15c=true;
    if (!initialSystem) return;
    meme::Object* pointer=initialSystem->getPointerObject();
    if (mode==28) {
        meme::TypeDescriptor* actual=pointer->getType();
        if (actual!=meme::MovePointer::getClassType()) {
            meme::Object*& slot=ref2System->getPointerObject();
            meme::deleteObject(ref2System,slot);
            meme::TypeDescriptor* type=meme::MovePointer::getClassType();
            meme::Object*& replacement=ref2System->getPointerObject();
            meme::createObject(ref2System,replacement,type);
        }
    } else {
        meme::TypeDescriptor* actual=pointer->getType();
        if (actual!=meme::Pointer::getClassType()) {
            meme::Object*& slot=ref2System->getPointerObject();
            meme::deleteObject(ref2System,slot);
            meme::TypeDescriptor* type=meme::Pointer::getClassType();
            meme::Object*& replacement=ref2System->getPointerObject();
            meme::createObject(ref2System,replacement,type);
            // Constructor and virtual texture dispatch remain named, undefined
            // dependencies. No incomplete picture table/factory is supplied.
            void* allocation=::operator new(sizeof(meme::Bf2PictureNode));
            meme::Bf2PictureNode* graphic=allocation
                ? new(allocation) meme::Bf2PictureNode(nullptr) : nullptr;
            menu::Ref2System* system=ref2System;
            m_field258=graphic;
            static_cast<meme::Pointer*>(system->getPointerObject())->setGraphic(system,graphic);
        }
    }
    meme::dispatchPictureTexture(m_field258,texture); // no graphic-null guard, result ignored
    const bool enabled=m_field15c;
    static_cast<meme::Pointer*>(ref2System->getPointerObject())->setEnabled(enabled);
    HudAmmo* ammo=hudAmmo; // retail dereferences the owner unconditionally
    if (ammo->m_field0a4 && (mode==0 || mode==10)) {
        const float size=pointerDimension(ammo->m_field0a0);
        static_cast<meme::Pointer*>(ref2System->getPointerObject())->setSize(size,size);
    } else {
        const float width=m_field154, height=m_field158;
        static_cast<meme::Pointer*>(ref2System->getPointerObject())->setSize(width,height);
    }
    if (mode==4 || mode==5 || mode==3 || mode==7 || mode==8 || mode==30 || mode==31) {
        const float y=centerPointer(m_field1fc,ref2System->getHeight());
        const float x=centerPointer(m_field1f8,ref2System->getWidth());
        float* position=ref2System->getPointerPosition();
        position[0]=x;position[1]=y;
    } else if (mode==6) {
        const float y=centerPointer(m_field214,ref2System->getHeight());
        const float x=centerPointer(m_field210,ref2System->getWidth());
        float* position=ref2System->getPointerPosition();
        position[0]=x;position[1]=y;
    }
}
} } }
#endif
