// [CHOSEN, not recovered] ABI regression scaffolding. No engine is started.
// Exercise real native virtual dispatch into the explicit-self table adapters.
#include "../RendDX9Abi/NativeVtable.h"
#include "../BF2/Game/Objects/GeometryPartAccess.h"
#include "../BF2/Geom/DecalSystem/DecalObjectAccess.h"
#include "../SwiffPlayerAbi/NativeRenderCalls.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>

namespace {
struct Point { float x, y, z; };
struct View {
    virtual const std::string& text() const = 0;
    virtual std::string name(unsigned index) const = 0;
    virtual Point point() const = 0;
    virtual void setText(const std::string& value) = 0;
    virtual bool enabled() const = 0;
};
struct Object {
    void* const* table;
    std::string text;
    const std::string* lastArgument;
};
const std::string* text(void* self) { return &static_cast<Object*>(self)->text; }
std::string* name(void* self, std::string* out, unsigned index)
{
    return new(out) std::string(static_cast<Object*>(self)->text + std::to_string(index));
}
void* point(void*, float* out) { out[0]=1.25f; out[1]=-3.5f; out[2]=9.0f; return out; }
void setText(void* self, const std::string* value)
{
    static_cast<Object*>(self)->lastArgument = value;
    static_cast<Object*>(self)->text = *value;
}
int enabled(void*) { return 1; }
void* const table[] = {
    BF2_NATIVE_SLOT(View, text, text),
    BF2_NATIVE_SLOT(View, name, name),
    BF2_NATIVE_SLOT(View, point, point),
    BF2_NATIVE_SLOT(View, setText, setText),
    BF2_NATIVE_SLOT(View, enabled, enabled),
};
void* legacyDestructor(void* self, int flag) { *static_cast<int*>(self)=flag; return self; }
// Scaffolding for the selector only: the callbacks record which entry was used.
struct Geometry { void** table; int selected; int value; };
int partCount(void*) { return 23; }
int lod(void*) { return 31; }
int lodCount(void*) { return 32; }
void* getTemplate(void* self) { return self; }
void setLod(void* self, int value)
{ static_cast<Geometry*>(self)->selected=30; static_cast<Geometry*>(self)->value=value; }
void setPart(void* self, const dice::hfe::Mat4*, int value)
{ static_cast<Geometry*>(self)->selected=19; static_cast<Geometry*>(self)->value=value; }
void clearDecals(void* self) { static_cast<Geometry*>(self)->selected=6; }
void removeDecal(void* self, unsigned value)
{ static_cast<Geometry*>(self)->selected=8; static_cast<Geometry*>(self)->value=static_cast<int>(value); }
void releaseDecals(void* self) { static_cast<Geometry*>(self)->selected=11; }
unsigned addDecal(void* self, const dice::hfe::Mat4*, void* decalTemplate, float, void*, void*)
{ static_cast<Geometry*>(self)->selected=7; return decalTemplate == self ? 333u : 0u; }
struct Render { void** table; float x,y; void* line; const wchar_t* text; int mouseX,mouseY,buttons; };
void scale(void* self, float x, float y) { static_cast<Render*>(self)->x=x; static_cast<Render*>(self)->y=y; }
int textSet(void* self, void* line, const wchar_t* value)
{ static_cast<Render*>(self)->line=line; static_cast<Render*>(self)->text=value; return 19; }
void mouse(void* self, int x, int y, int buttons)
{ static_cast<Render*>(self)->mouseX=x; static_cast<Render*>(self)->mouseY=y; static_cast<Render*>(self)->buttons=buttons; }
// The five evidence-derived transports: the receiver sees the
// caller's pointer, its out pointers and its float registers unchanged.
struct Transport { void** table; void* image; void* line; int* pos; int* a; int* b; float f[4];
                   const std::string* name; int index; };
int rgb(void* self, void* image) { static_cast<Transport*>(self)->image=image; return -1; }
char hit(void* self, void* line, int* pos, float x, float y)
{ Transport* t=static_cast<Transport*>(self); t->line=line; t->pos=pos; t->f[0]=x; t->f[1]=y; *pos=17; return 1; }
char selection(void* self, void* line, int* a, int* b)
{ Transport* t=static_cast<Transport*>(self); t->line=line; t->a=a; t->b=b; *a=3; *b=8; return 1; }
char setSelection(void* self, void* line, float x1, float y1, float x2, float y2)
{ Transport* t=static_cast<Transport*>(self); t->line=line; t->f[0]=x1; t->f[1]=y1; t->f[2]=x2; t->f[3]=y2; return 0; }
void* technique(void* self, const std::string* name, int index)
{ Transport* t=static_cast<Transport*>(self); t->name=name; t->index=index; return &t->index; }
}

int main()
{
    Object object{table, std::string(128, 'x'), nullptr};
    View* view = reinterpret_cast<View*>(&object);
    if (&view->text() != &object.text || view->name(7) != object.text + "7") return 1;
    const Point p = view->point();
    if (p.x != 1.25f || p.y != -3.5f || p.z != 9.0f) return 2;
    const std::string value(256, 'y');
    view->setText(value);
    if (object.lastArgument != &value || view->text() != value || !view->enabled()) return 3;
    int flag=-1;
    using Destructor = renddx9abi::NativeDestructor<decltype(&legacyDestructor), &legacyDestructor>;
    Destructor::complete(&flag);
    if (flag != 0) return 4;
    Destructor::deleting(&flag);
    if (flag != 1) return 5;
    void* geometryTable[64] = {};
    geometryTable[14]=reinterpret_cast<void*>(&getTemplate);
    geometryTable[19]=reinterpret_cast<void*>(&setPart);
    geometryTable[23]=reinterpret_cast<void*>(&partCount);
    geometryTable[30]=reinterpret_cast<void*>(&setLod);
    geometryTable[31]=reinterpret_cast<void*>(&lod);
    geometryTable[32]=reinterpret_cast<void*>(&lodCount);
    Geometry geometry{geometryTable, -1, -1};
    namespace part = dice::hfe::world::geometry_part;
    if (part::getTemplate(&geometry) != &geometry || part::count(&geometry) != 23 ||
        part::getLod(&geometry) != 31 || part::getLodCount(&geometry) != 32) return 6;
    part::setLod(&geometry, 7);
    if (geometry.selected != 30 || geometry.value != 7) return 7;
    part::setTransform(&geometry, nullptr, 11);
    if (geometry.selected != 19 || geometry.value != 11) return 8;
    namespace decals = dice::hfe::geom::decal_object;
    geometryTable[6]=reinterpret_cast<void*>(&clearDecals);
    geometryTable[7]=reinterpret_cast<void*>(&addDecal);
    geometryTable[8]=reinterpret_cast<void*>(&removeDecal);
    geometryTable[11]=reinterpret_cast<void*>(&releaseDecals);
    if (decals::add(&geometry, nullptr, &geometry, 1.0f, nullptr, nullptr) != 333u || geometry.selected != 7) return 9;
    decals::remove(&geometry, 27);
    if (geometry.selected != 8 || geometry.value != 27) return 10;
    decals::clear(&geometry);
    if (geometry.selected != 6) return 11;
    decals::release(&geometry);
    if (geometry.selected != 11) return 12;
    void* renderTable[67] = {};
    renderTable[28]=reinterpret_cast<void*>(&textSet);
    renderTable[44]=reinterpret_cast<void*>(&scale);
    renderTable[46]=reinterpret_cast<void*>(&mouse);
    Render render{renderTable,0,0,nullptr,nullptr,0,0,0};
    namespace native = dice::hfe::ui::native_render;
    native::rcSetScale(&render,1.25f,-3.5f);
    const wchar_t text[] = L"native";
    native::rcTextSet(&render,&geometry,text);
    native::rcSetMouseState(&render,127,231,5);
    if (render.x!=1.25f || render.y!=-3.5f || render.line!=&geometry || render.text!=text ||
        render.mouseX!=127 || render.mouseY!=231 || render.buttons!=5) return 13;
    void* transportTable[67] = {};
    transportTable[8]=reinterpret_cast<void*>(&rgb);
    transportTable[32]=reinterpret_cast<void*>(&hit);
    transportTable[33]=reinterpret_cast<void*>(&selection);
    transportTable[34]=reinterpret_cast<void*>(&setSelection);
    transportTable[53]=reinterpret_cast<void*>(&technique);
    Transport transport{transportTable,nullptr,nullptr,nullptr,nullptr,nullptr,{0,0,0,0},nullptr,0};
    void* const image = reinterpret_cast<void*>(static_cast<uintptr_t>(0x123456789ABCull));   // never dereferenced
    if (native::rcCreateRgb(&transport,image) != -1 || transport.image != image) return 14;
    int pos=-1;
    if (native::rcTextHit(&transport,&geometry,&pos,12.75f,-0.5f) != 1 || transport.line != &geometry ||
        transport.pos != &pos || pos != 17 || transport.f[0] != 12.75f || transport.f[1] != -0.5f) return 15;
    int start=-1, end=-1;
    if (native::rcTextSelection(&transport,&geometry,&start,&end) != 1 || transport.a != &start ||
        transport.b != &end || start != 3 || end != 8) return 16;
    if (native::rcTextSetSelection(&transport,&geometry,1.5f,2.25f,30.5f,4.75f) != 0 || transport.f[0] != 1.5f ||
        transport.f[1] != 2.25f || transport.f[2] != 30.5f || transport.f[3] != 4.75f) return 17;
    const std::string textureName(64, 't');
    native::rcSetPairState(&transport,textureName,6);
    if (transport.name != &textureName || transport.index != 6) return 18;
    std::puts("native vtable adapters: references, string/aggregate returns, arguments, Boolean and destructors passed");
    std::puts("native geometry selectors: template, count, LOD and transform dispatch passed");
    std::puts("native decal selectors: add, remove, clear and release dispatch passed");
    std::puts("native Swiff transport: floating-point, pointer and integer arguments passed");
    std::puts("native Swiff transport: RGB, text hit/selection/set-selection and texture technique passed");
}
