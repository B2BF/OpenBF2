// [TEMP] [CHOSEN, not recovered] Linux key names for the shared client input script, issue #36.
// [RETIRE-WHEN: client-test-automation-replaced] Replace with equivalent engine input coverage.
#include "../InputScript.h"
#if defined(BF2_LINUX_CLIENT) && !DEDICATED

namespace dice { namespace hfe { namespace inputscript { namespace keys {
namespace {
struct Key { const char* name; unsigned char dik; };
const Key names[] = {
    {"a",30},{"b",48},{"c",46},{"d",32},{"e",18},{"f",33},{"g",34},{"h",35},
    {"i",23},{"j",36},{"k",37},{"l",38},{"m",50},{"n",49},{"o",24},{"p",25},
    {"q",16},{"r",19},{"s",31},{"t",20},{"u",22},{"v",47},{"w",17},{"x",45},{"y",21},{"z",44},
    {"1",2},{"2",3},{"3",4},{"4",5},{"5",6},{"6",7},{"7",8},{"8",9},{"9",10},{"0",11},
    {"escape",1},{"esc",1},{"enter",28},{"return",28},{"space",57},{"tab",15},{"backspace",14},
    {"grave",41},{"minus",12},{"equals",13},{"leftbracket",26},{"rightbracket",27},
    {"semicolon",39},{"apostrophe",40},{"backslash",43},{"comma",51},{"period",52},{"slash",53},
    {"f1",59},{"f2",60},{"f3",61},{"f4",62},{"f5",63},{"f6",64},{"f7",65},{"f8",66},
    {"f9",67},{"f10",68},{"f11",87},{"f12",88},{"f13",100},{"f14",101},{"f15",102},
    {"up",200},{"down",208},{"left",203},{"right",205},{"arrowup",200},{"arrowdown",208},
    {"arrowleft",203},{"arrowright",205},{"home",199},{"end",207},{"pageup",201},{"pagedown",209},
    {"delete",211},{"insert",210},{"numpadenter",156},{"leftshift",42},{"shift",42},{"rightshift",54},
    {"leftctrl",29},{"ctrl",29},{"rightctrl",157},{"leftalt",56},{"alt",56},{"rightalt",184},
    {"capital",58},{"leftwin",219},{"rightwin",220},{"printscreen",183},{"scrolllock",70},{"pause",197}
};
}
bool lookup(const std::string& name, unsigned short& code)
{
    for (const Key& key : names)
        if (name == key.name) { code = key.dik; return true; }
    return false;
}
std::u16string controlCharacters(unsigned short code)
{
    switch (code) {
    case 28: case 156: return std::u16string(1, u'\r');
    case 15: return std::u16string(1, u'\t');
    case 1: return std::u16string(1, u'\x1B');
    case 14: return std::u16string(1, u'\x08');
    default: return {};
    }
}
bool isModifier(unsigned short code)
{
    switch (code) {
    case 42: case 54: case 29: case 157: case 56: case 184: case 219: case 220: case 58: return true;
    default: return false;
    }
}
bool isShift(unsigned short code) { return code == 42 || code == 54; }
unsigned short leftShift() { return 42; }
unsigned short grave() { return 41; }
unsigned short enter() { return 28; }
unsigned short w() { return 17; }
} } } }
#endif
