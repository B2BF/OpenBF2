// [TEMP] [CHOSEN, not recovered] Scaffolding for the native inline-assembly arms.
// No engine, renderer, game content, sockets or factory registrations are used.
#include "../BF2/Math/PortableX87.h"
#include "../BF2/Math/Mat4.h"
#include <climits>
#if defined(BF2X87_CLIENT_FLOAT) && defined(__x86_64__) && defined(__clang__)
// [TEMP] Oracle for the binary32 client arm: the same expression arms again, in namespace
// bf2x87_pc24, on the host x87 (long double), run below with 24-bit precision control as the
// retail client's main thread runs after device creation. Not original-game evidence.
#define BF2X87_PC24_ORACLE 1
#undef BF2_PLATFORM_X87COMPAT_H
#undef BF2_MATH_PORTABLEX87_H
#undef BF2_TEST_CLIENT_FLOAT
#undef BF2X87_CLIENT_FLOAT
#define bf2x87 bf2x87_pc24
#include "../BF2/Platform/X87Compat.h"
#include "../BF2/Math/PortableX87.h"
#undef bf2x87
#undef BF2X87_NATIVE_EXTENDED
#define BF2X87_CLIENT_FLOAT 1
#endif

#pragma STDC FENV_ACCESS ON

namespace {
int checks;
void check(bool value, int line)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "portable_x87.cpp:%d failed\n", line); std::abort(); }
}
#define CHECK(value) check((value), __LINE__)
float fromBits(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4); return value; }
std::uint32_t bits(float value) { std::uint32_t result; std::memcpy(&result,&value,4); return result; }
double fromBits64(std::uint64_t bits) { double value; std::memcpy(&value,&bits,8); return value; }
std::uint64_t bits64(double value) { std::uint64_t result; std::memcpy(&result,&value,8); return result; }
// GCC ignores FENV_ACCESS and folds the loads of constant signaling NaNs; read them from memory.
float opaque(float value) { volatile float held = value; return held; }
double opaque(double value) { volatile double held = value; return held; }
void same(float actual, float expected)
{
    CHECK((std::isnan(actual) && std::isnan(expected)) || bits(actual)==bits(expected));
}

#if defined(__x86_64__) && defined(__clang__)
// [TEMP] Host x87 oracle for the native math library; not original-game evidence.
// These GNU blocks use native-width addresses and never enter production code.
long double oracleCosine(long double angle)
{
    long double result;
    asm volatile("fldt %1; fcos; fstpt %0" : "=m"(result) : "m"(angle) : "st");
    return result;
}
long double oracleSine(long double angle)
{
    long double result;
    asm volatile("fldt %1; fsin; fstpt %0" : "=m"(result) : "m"(angle) : "st");
    return result;
}
int oracleInteger(float value)
{
    int result;
    asm volatile("flds %1; frndint; fistpl %0" : "=m"(result) : "m"(value) : "st");
    return result;
}
float oracleMultiply(float factor, float alpha)
{
    float result;
    asm volatile("flds %1; fmuls %2; fstps %0" : "=m"(result) : "m"(factor), "m"(alpha) : "st");
    return result;
}
#endif
}

int main()
{
    using namespace bf2x87;
    const int rounding = std::fegetround();
    CHECK(std::fesetround(FE_TONEAREST)==0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float large = std::numeric_limits<float>::max();
    const float samples[] = {nan,0.0f,-0.0f,inf,-inf,0.5f,-0.5f,1.5f,2.5f,large,-large};

    // FISTP invalid produces the signed minimum, never a C++ out-of-range cast.
    CHECK(integer<int>(nan)==INT_MIN);
    CHECK(integer<int>(inf)==INT_MIN);
    CHECK(integer<int>(-inf)==INT_MIN);
    CHECK(integer<int>(large)==INT_MIN);
    CHECK(integer<int>(2147483648.0L)==INT_MIN);
    CHECK(integer<int>(-2147483648.0L)==INT_MIN);
    CHECK(integer<int>(2147483647.9L)==INT_MAX);
    CHECK(integer<int>(-2147483648.9L)==INT_MIN);
    CHECK(integer<std::int64_t>(std::ldexp(1.0L,63))==LLONG_MIN);
#if defined(BF2X87_CLIENT_FLOAT)
    // binary64 operands: an int64 above 2^53 is not exact (FILD qword is); LLONG_MAX loads as 2^63.
    CHECK(integer<std::int64_t>(wideExact(LLONG_MAX))==LLONG_MIN);
    CHECK(integer<std::int64_t>(wideExact(9007199254740993LL))==9007199254740992LL);
#else
    CHECK(integer<std::int64_t>(wide(LLONG_MAX))==LLONG_MAX);
#endif
    CHECK(integer<short>(32768)==SHRT_MIN);
    CHECK(integer<int>(1.5L)==1 && integer<int>(-1.5L)==-1);
    CHECK(integer<int>(std::floor(-1.5L))==-2);
    CHECK(integer<int>(std::ceil(1.5L))==2);
    CHECK(integer<int>(0.5L,false)==0 && integer<int>(-0.5L,false)==0);
    CHECK(integer<int>(1.5L,false)==2 && integer<int>(2.5L,false)==2);
    CHECK(integer<int>(-1.5L,false)==-2 && integer<int>(-2.5L,false)==-2);
    CHECK(std::fesetround(FE_DOWNWARD)==0);
    CHECK(integer<int>(1.5L,false)==1 && integer<int>(-1.5L,false)==-2);
    CHECK(integer<int>(-1.5L)==-1 && std::fegetround()==FE_DOWNWARD);
    CHECK(std::fesetround(FE_UPWARD)==0);
    CHECK(integer<int>(1.5L,false)==2 && integer<int>(-1.5L,false)==-1);
    CHECK(std::fesetround(FE_TOWARDZERO)==0);
    CHECK(integer<int>(1.5L,false)==1 && integer<int>(-1.5L,false)==-1);
    CHECK(std::fesetround(FE_TONEAREST)==0);
    CHECK(lowWord(0x12345678ffffffffLL)==-1 && lowShort(0xffff)==-1);

    CHECK(notLess(nan,0) && greaterOrUnordered(nan,0));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(notLess(nan,0) && std::fetestexcept(FE_INVALID));
    CHECK(notLess(0,-0.0L) && !greaterOrUnordered(0,-0.0L));
    same(store(decoded(-2)),1); same(store(decoded(-1)),-0.0f);
    same(store(decoded(-0.0f)),-0.0f);
    CHECK(notLess(decoded(nan),1));
    CHECK(sumNotLess(nan,1,2));
    CHECK(sumNotLess(1,1,2));
    CHECK(!sumNotLess(1,1,3));
    // No float/double spill before cancellation or the final comparison.
    same(offset(1,16777216.0f,16777216.0f),1);
    same(offset(1,large,large),0);
    CHECK(sumNotLess(16777216.0f,1,16777216.0f));
    same(center(10,3),8.5f);
    same(multiplyAdd(16777216.0f,1.00000011920928955078125f,-16777216.0f),2);
    same(multiply(-0.0f,1),-0.0f);
    CHECK(bits(multiply(fromBits(0x7fc00001),fromBits(0x7fc00002)))==0x7fc00002);
    CHECK(bits(multiply(fromBits(0x7f800001),fromBits(0x7fc00002)))==0x7fc00002);
    CHECK(bits(multiply(fromBits(0x7fc00001),fromBits(0x7f800002)))==0x7fc00001);

    same(fade(0,1,1,false),0); same(fade(1,1,1,true),1);
    same(fade(-0.0f,0,1,false),-0.0f);
    float value=37;
    CHECK(!chooseValue(nan,1,1,1,0,false,value) && value==37);
    CHECK(!chooseValue(1,1,1,1,0,true,value) && value==37);
    CHECK(chooseValue(0,1,2,1,0,false,value) && value==1);
    CHECK(chooseValue(2,1,2,1,0,false,value) && value==1);
    CHECK(chooseValue(0,1,nan,1,0,false,value) && std::isnan(value));
    CHECK(chooseValue(0,2,1,1,0,true,value) && value==1);
    CHECK(chooseValue(2,0,1,1,0,true,value) && value==1);
    CHECK(chooseValue(0,1,1,1,2,true,value) && value>0 && value<1);
    CHECK(chooseValue(1,0,1,1,2,true,value) && value>0 && value<1);
    char text[16]; char* cursor=text;
    fractionalDigits(1.25,1,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"250")==0);
    cursor=text; fractionalDigits(inf,0,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"000")==0);
    cursor=text; fractionalDigits(nan,0,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"000")==0);

    double remaining=37;
    // The binary64 1/60 constant is slightly low, so the retained product
    // leaves a positive wait even at exactly sixty seconds.
#if defined(BF2X87_CLIENT_FLOAT)
    // At 24-bit precision the product rounds to 1, so no wait remains.
    CHECK(!waitRemaining(0,1,60,remaining));
#else
    CHECK(waitRemaining(0,1,60,remaining) && remaining>0 && remaining<1e-15);
#endif
    CHECK(waitRemaining(0,1,0,remaining) && remaining==1);
    CHECK(waitRemaining(0,nan,0,remaining) && std::isnan(remaining));
    CHECK(!abilityPosition(-4000,-4000)); CHECK(abilityPosition(nan,-4000));
    float duration=2, left=0, deadline=0; bool active=false;
    beginCooldown(1,duration,left,active,deadline,true);
    CHECK(left==2 && active && deadline==3);
    active=false; beginCooldown(2,duration,left,active,deadline,false);
    CHECK(left==2 && active && deadline==4);
    float position[3]={1,2,3};
    movePointer(position,1,-1,2,true,100,60); CHECK(position[0]==50 && position[1]==-30);
    movePointer(position,1,-1,2,false,100,60); CHECK(position[0]==52 && position[1]==-32);
    value=-1; clampCoordinate(value,0,2); CHECK(value==0);
    value=3; clampCoordinate(value,0,2); CHECK(value==2);
    value=nan; clampCoordinate(value,0,2); CHECK(std::isnan(value));
    CHECK(affine(1,2,3,4,5,6,7)==51);
    float x,y,z; project(2,3,2,100,50,x,y,z); CHECK(x==50 && y==-75 && z==0.5f);
    same(store(div(wide(1),wide(2))),0.5f);
    CHECK(distanceWithX(wide(3.0f),0,0,4,0,2)==10);
    CHECK(distance(0,0,0,3,4,0,2)==10);
    pullPoint(0,0,0,0,0,0,position); CHECK(position[0]==52);
    pullPoint(1,0,0,0,0,0,position); same(position[0],0.8f); CHECK(position[1]==0 && position[2]==0);
    value=0.5f; CHECK(subtractTimer(value,0.5f,true) && value==0);
    value=0.5f; CHECK(!subtractTimer(value,0.5f,false));
    value=nan; CHECK(!subtractTimer(value,1,true) && std::isnan(value));
    value=9.5f; CHECK(addDemoTimer(value,0.5f) && value==10);
    value=nan; CHECK(!addDemoTimer(value,1));
    CHECK(mapCoordinate(100,10,0.01f,true)==std::nextafter(0.6f,0.0f));
    CHECK(mapCoordinate(100,10,0.01f,false)==0.5f);
    same(heading(0,1),0); same(heading(-0.0f,1),-0.0f);
    same(heading(1,0),1.57079632679489661923f);
    same(heading(0,-1),3.14159265358979323846f);
    CHECK(std::isnan(heading(nan,1)));
    CHECK(armorRatio(1,2,1,false)==0.5f && armorRatio(1,2,1,true)==0.75f);
    same(ammoRatio(15,0,false),1.0f); same(ammoRatio(15,0,true),0.5f);
    float fraction; cooldown(10,2,4,left,fraction); CHECK(left==8 && fraction==2);
    CHECK(glyphWidth(1,2,3,4)==10);
    float co; cofactor(co,1,2,3,4,5,6,1,1,1,1,1,1); CHECK(co==41);
    float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float inverse[16]; std::memcpy(inverse,matrix,sizeof(matrix));
    divideByDeterminant(matrix,inverse);
    for (int i=0;i<16;++i) same(inverse[i],matrix[i]);
    dice::hfe::Mat4 original, inverted;
    for (int i=0;i<16;++i) original.m[i]=matrix[i];
    original.m[0]=2; original.m[5]=4; original.m[10]=8;
    original.m[12]=6; original.m[13]=8; original.m[14]=16;
    dice::hfe::fullInvert(original,inverted);
    CHECK(inverted.m[0]==0.5f && inverted.m[5]==0.25f && inverted.m[10]==0.125f);
    CHECK(inverted.m[12]==-3 && inverted.m[13]==-2 && inverted.m[14]==-2);
    dice::hfe::fullInvert(original,original);
    for (int i=0;i<16;++i) same(original.m[i],inverted.m[i]);
    float base,near,inverseNear,far,inverseFar;
    lod(2,100,0.25f,0.5f,base,near,inverseNear,far,inverseFar);
    CHECK(base==200 && near==50 && far==100); same(inverseNear,1.0f/150); same(inverseFar,0.01f);
    CHECK(effectiveDistance(nan,1,1)==200 && effectiveDistance(200,1,1)==200);
    CHECK(effectiveDistance(300,1,1)==300);
    float s,c; trig(-0.0f,s,c); same(s,-0.0f); same(c,1);
    trig(large,s,c); CHECK(bits(s)==0xffc00000U && c==large);
    CHECK(sine(wide(large))==wide(large) && cosine(wide(large))==wide(large));
    short si,ci; packedRotation(0,si,ci); CHECK(si==0 && ci==32767);
    tableRotation(0,si,ci); CHECK(si==0 && ci==32767);
#if defined(BF2X87_CLIENT_FLOAT)
    // FSIN is exact to 64 bits, but the FMUL by 32767 rounds to 32767 at 24-bit precision.
    tableRotation(1024,si,ci); CHECK(si==32767 && ci==0);
    packedRotation(1.57079632679489661923f,si,ci); CHECK(si==32767 && ci==0);
    packedRotation(-1.57079632679489661923f,si,ci); CHECK(si==-32767 && ci==0);
#else
    tableRotation(1024,si,ci); CHECK(si==32766 && ci==0);
    packedRotation(1.57079632679489661923f,si,ci); CHECK(si==32766 && ci==0);
    packedRotation(-1.57079632679489661923f,si,ci); CHECK(si==-32766 && ci==0);
#endif
    packedRotation(nan,si,ci); CHECK(si==0 && ci==0);
    float zoomValue,inverseZoom; zoom(1,0.5f,1,zoomValue,inverseZoom); CHECK(zoomValue==1 && inverseZoom==1);
    zoom(inf,0.5f,1,zoomValue,inverseZoom); CHECK(std::isnan(zoomValue) && std::isnan(inverseZoom));
    bool flag=false;
#if defined(BF2X87_NATIVE_EXTENDED)
    // The native arm leaves FLD to the compiler, which may elide the FLD/FSTP of a value that is
    // only stored (GCC does); a constant input is checked here as before.
    value=fromBits(0x7f800001);
    std::feclearexcept(FE_ALL_EXCEPT);
    same(consoleFloat(&value,&flag),nan); CHECK(flag && std::fetestexcept(FE_INVALID));
#else
    // FLD quiets a signaling NaN and raises invalid, also where no operation follows the load
    // (wideQuiet in the binary32 arm).
    value=opaque(fromBits(0x7f800001));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits(consoleFloat(&value,&flag))==0x7fc00001U); CHECK(flag && std::fetestexcept(FE_INVALID));
    value=opaque(fromBits(0xffbfffffU)); std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits(consoleFloat(&value,&flag))==0xffffffffU && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits(store(decoded(opaque(fromBits(0x7f800001)))))==0x7fc00001U && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits(store(decoded(opaque(fromBits(0xff800001U)))))==0xffc00001U && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits64(storeDouble(wideQuiet(opaque(fromBits64(0x7ff0000000000001ULL)))))==0x7ff8000000000001ULL);
    CHECK(std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(bits64(storeDouble(wideQuiet(opaque(fromBits64(0xfff7ffffffffffffULL)))))==0xffffffffffffffffULL);
    CHECK(std::fetestexcept(FE_INVALID));
#endif
    // Quiet NaNs, infinities and denormals load without an exception.
    const float passed[] = {nan,fromBits(0xffc00001U),inf,-inf,large,fromBits(1),-0.0f};
    for (float sample:passed) {
        value=opaque(sample); std::feclearexcept(FE_ALL_EXCEPT);
        CHECK(bits(consoleFloat(&value,&flag))==bits(sample) && !std::fetestexcept(FE_INVALID));
    }
    const double passedDouble[] = {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),fromBits64(0xfff8000000000001ULL),fromBits64(1),-0.0};
    for (double sample:passedDouble) {
        std::feclearexcept(FE_ALL_EXCEPT);
        CHECK(bits64(storeDouble(wideQuiet(opaque(sample))))==bits64(sample) && !std::fetestexcept(FE_INVALID));
    }

    // Exercise every scalar family with NaN, signed zeros, infinities, halves,
    // and extreme magnitudes. Expected cases are stated above; here check the
    // shared unordered and invalid-conversion invariants across all samples.
    for (float sample:samples) {
        same(multiply(sample,1),sample);
        same(offset(sample,0,0),std::isnan(sample) ? nan : sample==0 ? 0.0f : sample);
        same(center(sample,0),sample);
        same(multiplyAdd(sample,1,0),sample==0 ? 0.0f : sample);
        CHECK(sumNotLess(sample,0,0)==!(sample<0));
        same(fade(sample,0,1,true),sample>1 ? 1.0f : sample==0 ? 0.0f : sample);
        same(fade(sample,0,1,false),sample<0 ? 0.0f : sample);
        value=37; CHECK(chooseValue(sample,sample,1,1,1,true,value)==false && value==37);
        same(armorRatio(sample,1,0,false),sample);
        same(affine(sample,1,0,1,0,1,0),sample==0 ? 0.0f : sample);
        flag=false; same(consoleFloat(&sample,&flag),sample); CHECK(flag);
        float a,b; trig(sample,a,b);
        if (std::isnan(sample)||std::isinf(sample)) CHECK(std::isnan(a)&&std::isnan(b));
        cooldown(sample,0,1,a,b); same(a,sample); same(b,sample);
        // These blocks use the same sample through independent expression
        // families, checking their caller-visible stores and branch decisions.
        project(sample,sample,1,2,1,x,y,z);
        same(x,sample); same(y,-sample); same(z,1);
#if defined(BF2X87_CLIENT_FLOAT)
        // binary32 intermediates have no extended exponent range: FLT_MAX squared overflows.
        const float reach = std::fabs(sample)==large ? inf : std::fabs(sample);
#else
        const float reach = std::fabs(sample);
#endif
        same(distance(0,0,0,sample,0,0,1),reach);
        same(distanceWithX(wide(sample),0,0,0,0,1),reach);
        same(glyphWidth(sample,0,0,0),sample==0 ? 0.0f : sample);
        cofactor(co,sample,1,0,1,0,1,0,1,0,1,0,1);
        same(co,sample==0 ? 0.0f : sample);
        same(mapCoordinate(2,sample,1,true),std::isnan(sample) ? nan : sample==0 ? 1.0f : sample+1.0f);
        same(effectiveDistance(sample,1,1),sample>200 ? sample : 200.0f);
        same(ammoRatio(0,sample,true),sample==0 ? 0.0f : sample*0.5f);
        value=37;
        const bool denied=waitRemaining(0,sample,0,remaining);
        CHECK(denied==(!(sample<=0)));
        if (denied) same(static_cast<float>(remaining),sample);
        bool busy=false; float remainingField=37,deadlineField=37;
        beginCooldown(0,sample,remainingField,busy,deadlineField,true);
        same(remainingField,sample); same(deadlineField,sample==0 ? 0.0f : sample); CHECK(busy);
        beginCooldown(0,sample,remainingField,busy,deadlineField,false);
        same(remainingField,sample); same(deadlineField,sample==0 ? 0.0f : sample); CHECK(busy);
        lod(sample,1,0.25f,0.5f,base,near,inverseNear,far,inverseFar);
        same(base,sample); same(near,sample*0.25f); same(far,sample*0.5f);
        position[0]=sample; position[1]=sample;
        movePointer(position,0,0,1,false,1,1);
        same(position[0],sample==0 ? 0.0f : sample); same(position[1],sample==0 ? 0.0f : sample);
        movePointer(position,sample,sample,1,true,2,2);
#if defined(BF2X87_CLIENT_FLOAT)
        if (std::fabs(sample)==large) { same(position[0],sample*inf); same(position[1],sample*inf); }
        else
#endif
        { same(position[0],sample); same(position[1],sample); }
        value=sample; clampCoordinate(value,-1,2);
        same(value,sample < -1 ? -1.0f : sample>1 ? 1.0f : sample);
        value=sample; CHECK(subtractTimer(value,0,false)==(sample<0));
        same(value,sample);
        value=sample; CHECK(addDemoTimer(value,0)==(sample>=10));
        same(value,sample==0 ? 0.0f : sample);
        CHECK(abilityPosition(sample,-4000));
        packedRotation(sample,si,ci);
        if (std::isnan(sample)||std::isinf(sample)||std::fabs(sample)>=std::ldexp(1.0f,63))
            CHECK(si==0 && ci==0);
    }
    CHECK(std::fesetround(rounding)==0);
#if defined(__x86_64__) && defined(__clang__)
    // Verify the extended results, not only their binary32 stores. Darwin's
    // sinl/cosl agree with the host x87 instructions for these samples.
    CHECK(std::fesetround(FE_TONEAREST)==0);
#if !defined(BF2_TEST_SOFT_EXTENDED) && !defined(BF2X87_CLIENT_FLOAT)
    std::uint32_t random=0;
    for (int i=0;i<10000;++i) {
        random=random*1664525U+1013904223U;
        const long double angle=mul(wide(lowWord(random)),0.00000001L);
        CHECK(cosine(angle)==oracleCosine(angle));
        CHECK(sine(angle)==oracleSine(angle));
    }
#endif
    const std::uint32_t alphaCases[]={0x7fc00001,0xffc00001,0x7fc00002,0x7f800001,0xff800002,0,0x80000000,0x7f800000,0xff800000,0x3f800000};
    for (auto first:alphaCases) for (auto second:alphaCases)
        CHECK(bits(multiply(fromBits(first),fromBits(second)))==bits(oracleMultiply(fromBits(first),fromBits(second))));
    const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for (int mode:modes) {
        CHECK(std::fesetround(mode)==0);
        for (float sample:samples) CHECK(integer<int>(wide(sample),false)==oracleInteger(sample));
    }
    CHECK(std::fesetround(rounding)==0);
#endif
#if defined(BF2X87_CLIENT_FLOAT)
    // Binary32 Real: loads of int32 and double operands stay exact up to the first operation.
    CHECK(std::fesetround(FE_TONEAREST)==0);
    const double justBelow3 = 2.9999999999999996;
    CHECK(toIntTruncate(justBelow3)==2 && toIntTruncate(wideExact(justBelow3))==2);
    CHECK(toIntTruncate(wide(justBelow3))==3);              // wide() rounds to binary32 first
    CHECK(toIntTruncate(16777217)==16777217 && integer<int>(wideExact(16777217))==16777217);
    CHECK(less(16777216,16777217) && greater(wideExact(16777217.0),wide(16777216.0f)));
    CHECK(!less(wide(16777216),wide(16777217)));             // both round to 2^24
    CHECK(storeDouble(wideExact(0.1))==0.1 && store(wideExact(0.1))==0.1f);
    CHECK(wideExact(0.1)!=wide(0.1f) && lessEqual(wide(0.1f),0.1)==false);
    CHECK(lowUInt32(-1.0)==0xFFFFFFFFu && lowUInt32(4294967295.0)==0xFFFFFFFFu);
    CHECK(lowUInt32(wideExact(4294967295u))==0xFFFFFFFFu);
    CHECK(store(sub(wideExact(100000.75),wideExact(100000.0)))==0.75f);
    CHECK(store(sub(wide(100000.75),wide(100000.0)))==0.75f);
    CHECK(store(sub(wideExact(16777217.0),wideExact(16777216.0)))==1.0f);
    CHECK(store(sub(wide(16777217.0),wide(16777216.0)))==0.0f);
    // Integer conversion bounds, exact on binary64 operands.
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(toIntTruncate(2147483647.0)==INT_MAX && toIntTruncate(-2147483648.0)==INT_MIN);
    CHECK(toIntTruncate(-2147483648.9)==INT_MIN && !std::fetestexcept(FE_INVALID));
    CHECK(toIntTruncate(2147483648.0)==INT_MIN && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(toIntRound(2147483647.4)==INT_MAX && !std::fetestexcept(FE_INVALID));
    CHECK(toIntRound(2147483647.5)==INT_MIN && std::fetestexcept(FE_INVALID));
    CHECK(integer<short>(32767.9)==32767 && integer<short>(-32768.9)==-32768);
    CHECK(integer<short>(32768.0)==SHRT_MIN && integer<short>(wide(32767.99999))==SHRT_MIN && integer<short>(32767.99999)==32767);
    CHECK(toInt64Truncate(9007199254740991.0)==9007199254740991LL);
    CHECK(toIntTruncate(wide(2147483520.0f))==2147483520 && toIntTruncate(wide(2147483648.0f))==INT_MIN);
    // NaN operands: unordered, FE_INVALID for every comparison, the integer indefinite.
    const double nanD = std::numeric_limits<double>::quiet_NaN();
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(!less(nanD,0) && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(!greaterEqual(wide(nan),wide(nan)) && std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(unordered(nanD,1) && !ordered(1,nanD) && !std::fetestexcept(FE_INVALID));
    compare(1,nanD); CHECK(std::fetestexcept(FE_INVALID));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(toIntTruncate(nanD)==INT_MIN && std::fetestexcept(FE_INVALID));
    CHECK(isNaN(add(nanD,1)) && isNaN(mul(wide(inf),0)) && !isFinite(div(wide(1.0f),0.0f)));
    CHECK(isFinite(wideExact(1e300)) && !isFinite(wide(1e300)));
    // One rounding per operation: binary32 operands give the binary32 operation.
    same(store(add(wide(1.0f),wide(std::ldexp(1.0f,-24)))),1.0f);
    same(store(add(wide(1.0f),wide(std::nextafter(std::ldexp(1.0f,-24),1.0f)))),std::nextafter(1.0f,2.0f));
    same(store(div(wide(1.0f),wide(3.0f))),1.0f/3.0f);
    same(store(mul(wide(16777215.0f),wide(16777215.0f))),16777215.0f*16777215.0f);
    same(store(squareRoot(wide(2.0f))),std::sqrt(2.0f));
    // Known limit: a binary64 sum is rounded twice (x87 at 24 bits would give 1 + 2^-23 here).
    same(store(add(wideExact(1.0),wideExact(std::ldexp(1.0,-24)+std::ldexp(1.0,-60)))),1.0f);
    char exactDigits[8]; char* exactCursor=exactDigits;
    fractionalDigits(1234567.89,1234567,3,exactCursor); *exactCursor=0;
    CHECK(std::strcmp(exactDigits,"889")==0);   // 24-bit products of the exact remainder
    CHECK(std::fesetround(rounding)==0);
#endif
#if defined(BF2X87_PC24_ORACLE)
    {
        // Every non-transcendental arm against the host x87 at 24-bit precision control: equal
        // stores and decisions for finite operands whose intermediates stay in binary32 range.
        // (libm sinl/cosl/atan2l would run their own arithmetic at 24 bits, so trig is left out.)
        struct Pc24 {
            unsigned short saved;
            Pc24() { unsigned short cw; asm volatile("fnstcw %0" : "=m"(cw)); saved = cw;
                     cw = static_cast<unsigned short>(cw & ~0x300); asm volatile("fldcw %0" : : "m"(cw)); }
            ~Pc24() { asm volatile("fldcw %0" : : "m"(saved)); }
        };
        std::uint32_t state=12345;
        auto next=[&]() { state=state*1664525U+1013904223U; return state; };
        auto value=[&](int span) {
            const float mantissa=1.0f+static_cast<float>(next()>>8)*(1.0f/16777216.0f);
            const int exponent=static_cast<int>(next()%(2*span+1))-span;
            const float v=std::ldexp(mantissa,exponent);
            return (next()&1) ? -v : v;
        };
        auto time=[&]() { return static_cast<double>(next()%100000000)+static_cast<double>(next())/4294967296.0; };
        int mismatches=0, cases=0;
        auto eq=[&](float a, float b) { ++cases; if (!((std::isnan(a)&&std::isnan(b))||bits(a)==bits(b))) ++mismatches; };
        auto eqi=[&](long long a, long long b) { ++cases; if (a!=b) ++mismatches; };
        CHECK(std::fesetround(FE_TONEAREST)==0);
        for (int i=0;i<20000;++i) {
            const float a=value(20), b=value(20), c=value(20), d=value(20), e=value(20), f=value(20), g=value(20);
            float o1,o2,o3,p1,p2,p3;
            { Pc24 pc; o1=bf2x87_pc24::multiply(a,b); o2=bf2x87_pc24::offset(a,b,c); o3=bf2x87_pc24::center(a,b); }
            eq(multiply(a,b),o1); eq(offset(a,b,c),o2); eq(center(a,b),o3);
            { Pc24 pc; o1=bf2x87_pc24::multiplyAdd(a,b,c); o2=bf2x87_pc24::affine(a,b,c,d,e,f,g);
              o3=bf2x87_pc24::glyphWidth(a,b,c,d); }
            eq(multiplyAdd(a,b,c),o1); eq(affine(a,b,c,d,e,f,g),o2); eq(glyphWidth(a,b,c,d),o3);
            { Pc24 pc; o1=bf2x87_pc24::fade(a,b,c,true); o2=bf2x87_pc24::fade(a,b,c,false);
              o3=bf2x87_pc24::distance(a,b,c,d,e,f,g); }
            eq(fade(a,b,c,true),o1); eq(fade(a,b,c,false),o2); eq(distance(a,b,c,d,e,f,g),o3);
            { Pc24 pc; o1=bf2x87_pc24::mapCoordinate(a,b,c,true); o2=bf2x87_pc24::armorRatio(a,b,c,true);
              o3=bf2x87_pc24::effectiveDistance(a,b,c); }
            eq(mapCoordinate(a,b,c,true),o1); eq(armorRatio(a,b,c,true),o2); eq(effectiveDistance(a,b,c),o3);
            bool r1,r2;
            { Pc24 pc; r1=bf2x87_pc24::sumNotLess(a,b,c); r2=bf2x87_pc24::abilityPosition(a,b); }
            eqi(sumNotLess(a,b,c),r1); eqi(abilityPosition(a,b),r2);
            { Pc24 pc; bf2x87_pc24::project(a,b,c,d,e,o1,o2,o3); }
            project(a,b,c,d,e,p1,p2,p3); eq(p1,o1); eq(p2,o2); eq(p3,o3);
            float base[5], mine[5];
            { Pc24 pc; bf2x87_pc24::lod(a,b,c,d,base[0],base[1],base[2],base[3],base[4]); }
            lod(a,b,c,d,mine[0],mine[1],mine[2],mine[3],mine[4]);
            for (int k=0;k<5;++k) eq(mine[k],base[k]);
            float t1=a, t2=a;
            { Pc24 pc; r1=bf2x87_pc24::subtractTimer(t1,b,true); }
            r2=subtractTimer(t2,b,true); eqi(r1,r2); eq(t1,t2);
            float out1[3]={a,b,c}, out2[3]={a,b,c};
            { Pc24 pc; bf2x87_pc24::pullPoint(a,b,c,d,e,f,out1); }
            pullPoint(a,b,c,d,e,f,out2); for (int k=0;k<3;++k) eq(out1[k],out2[k]);
            { Pc24 pc; o1=0; bf2x87_pc24::cofactor(o1,a,b,c,d,e,f,g,a,b,c,d,e); }
            p1=0; cofactor(p1,a,b,c,d,e,f,g,a,b,c,d,e); eq(p1,o1);
            // Double operands (times, durations): exact loads, one rounding per operation.
            const double now=time(), last=now-static_cast<double>(next()%1000)/7.0, cool=time()/1000.0;
            double w1=0, w2=0;
            { Pc24 pc; r1=bf2x87_pc24::waitRemaining(last,cool,now,w1); }
            r2=waitRemaining(last,cool,now,w2); eqi(r1,r2); if (r1&&r2) { ++cases; if (w1!=w2) ++mismatches; }
            float rem1=0,rem2=0,dl1=0,dl2=0; bool busy1=false,busy2=false;
            { Pc24 pc; bf2x87_pc24::beginCooldown(now,a,rem1,busy1,dl1,true); }
            beginCooldown(now,a,rem2,busy2,dl2,true); eq(rem1,rem2); eq(dl1,dl2);
            float fr1,fr2;
            { Pc24 pc; bf2x87_pc24::cooldown(b,now,c,rem1,fr1); }
            cooldown(b,now,c,rem2,fr2); eq(rem1,rem2); eq(fr1,fr2);
            char d1[8], d2[8]; char* c1=d1; char* c2=d2;
            const double magnitude=std::fabs(now);
            const int integral=static_cast<int>(magnitude);
            { Pc24 pc; bf2x87_pc24::fractionalDigits(magnitude,integral,6,c1); }
            fractionalDigits(magnitude,integral,6,c2); *c1=0; *c2=0;
            ++cases; if (std::strcmp(d1,d2)!=0) ++mismatches;
        }
        CHECK(std::fesetround(rounding)==0);
        std::printf("pc24 oracle: %d of %d cases differ\n",mismatches,cases);
        CHECK(mismatches==0);
    }
#endif
    std::printf("portable x87 arms: %d checks passed\n",checks);
}
