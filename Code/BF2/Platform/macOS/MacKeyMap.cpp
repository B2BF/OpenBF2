// [CHOSEN, not recovered] macOS port: kVK_ to DIK_ key table. See MacKeyMap.h and
// docs/macos-s2-4.md for the full table with key legends and the reasoning behind each
// non-obvious row.

#include "MacKeyMap.h"

// Production callers are Cocoa-only. The existing test selector also permits portable table checks.
#if (defined(__APPLE__) || defined(OPENBF2_HOST_TEST_SEAMS)) && !(defined(DEDICATED) && DEDICATED)

namespace dice {
namespace hfe {
namespace macos {

namespace {

// Indexed by kVK_ code (0x00..0x7F). 0 means "no DirectInput key". The ISO-swapped pair 0x0A /
// 0x32 holds the ANSI assignment here; dikFromMacKeyCode applies the ISO swap.
const unsigned char kDikFromKeyCode[128] =
{
    /* 0x00 A         */  30, /* 0x01 S        */  31, /* 0x02 D         */  32, /* 0x03 F        */  33,
    /* 0x04 H         */  35, /* 0x05 G        */  34, /* 0x06 Z         */  44, /* 0x07 X        */  45,
    /* 0x08 C         */  46, /* 0x09 V        */  47, /* 0x0A ISO §     */  86, /* 0x0B B        */  48,
    /* 0x0C Q         */  16, /* 0x0D W        */  17, /* 0x0E E         */  18, /* 0x0F R        */  19,
    /* 0x10 Y         */  21, /* 0x11 T        */  20, /* 0x12 1         */   2, /* 0x13 2        */   3,
    /* 0x14 3         */   4, /* 0x15 4        */   5, /* 0x16 6         */   7, /* 0x17 5        */   6,
    /* 0x18 =         */  13, /* 0x19 9        */  10, /* 0x1A 7         */   8, /* 0x1B -        */  12,
    /* 0x1C 8         */   9, /* 0x1D 0        */  11, /* 0x1E ]         */  27, /* 0x1F O        */  24,
    /* 0x20 U         */  22, /* 0x21 [        */  26, /* 0x22 I         */  23, /* 0x23 P        */  25,
    /* 0x24 Return    */  28, /* 0x25 L        */  38, /* 0x26 J         */  36, /* 0x27 '        */  40,
    /* 0x28 K         */  37, /* 0x29 ;        */  39, /* 0x2A \         */  43, /* 0x2B ,        */  51,
    /* 0x2C /         */  53, /* 0x2D N        */  49, /* 0x2E M         */  50, /* 0x2F .        */  52,
    /* 0x30 Tab       */  15, /* 0x31 Space    */  57, /* 0x32 `         */  41, /* 0x33 Delete   */  14,
    /* 0x34 PB Enter  */ 156, /* 0x35 Escape   */   1, /* 0x36 RCommand  */ 220, /* 0x37 Command  */ 219,
    /* 0x38 Shift     */  42, /* 0x39 CapsLock */  58, /* 0x3A Option    */  56, /* 0x3B Control  */  29,
    /* 0x3C RShift    */  54, /* 0x3D ROption  */ 184, /* 0x3E RControl  */ 157, /* 0x3F fn       */   0,
    /* 0x40 F17       */   0, /* 0x41 Num .    */  83, /* 0x42           */   0, /* 0x43 Num *    */  55,
    /* 0x44           */   0, /* 0x45 Num +    */  78, /* 0x46           */   0, /* 0x47 Clear    */  69,
    /* 0x48 VolUp     */ 176, /* 0x49 VolDown  */ 174, /* 0x4A Mute      */ 160, /* 0x4B Num /    */ 181,
    /* 0x4C Num Enter */ 156, /* 0x4D         */   0, /* 0x4E Num -     */  74, /* 0x4F F18      */   0,
    /* 0x50 F19       */   0, /* 0x51 Num =    */ 141, /* 0x52 Num 0     */  82, /* 0x53 Num 1    */  79,
    /* 0x54 Num 2     */  80, /* 0x55 Num 3    */  81, /* 0x56 Num 4     */  75, /* 0x57 Num 5    */  76,
    /* 0x58 Num 6     */  77, /* 0x59 Num 7    */  71, /* 0x5A F20       */   0, /* 0x5B Num 8    */  72,
    /* 0x5C Num 9     */  73, /* 0x5D JIS Yen  */ 125, /* 0x5E JIS _     */ 115, /* 0x5F JIS Num ,*/ 179,
    /* 0x60 F5        */  63, /* 0x61 F6       */  64, /* 0x62 F7        */  65, /* 0x63 F3       */  61,
    /* 0x64 F8        */  66, /* 0x65 F9       */  67, /* 0x66 JIS Eisu  */ 123, /* 0x67 F11      */  87,
    /* 0x68 JIS Kana  */ 112, /* 0x69 F13      */ 183, /* 0x6A F16       */   0, /* 0x6B F14      */  70,
    /* 0x6C           */   0, /* 0x6D F10      */  68, /* 0x6E Menu      */ 221, /* 0x6F F12      */  88,
    /* 0x70           */   0, /* 0x71 F15      */ 197, /* 0x72 Help      */ 210, /* 0x73 Home     */ 199,
    /* 0x74 PageUp    */ 201, /* 0x75 FwdDel   */ 211, /* 0x76 F4        */  62, /* 0x77 End      */ 207,
    /* 0x78 F2        */  60, /* 0x79 PageDown */ 209, /* 0x7A F1        */  59, /* 0x7B Left     */ 203,
    /* 0x7C Right     */ 205, /* 0x7D Down     */ 208, /* 0x7E Up        */ 200, /* 0x7F          */   0,
};

const unsigned short kKeyCodeIsoSection = 0x0A;
const unsigned short kKeyCodeGrave      = 0x32;
const unsigned char  kDikGrave          = 41;
const unsigned char  kDikOem102         = 86;
const unsigned char  kDikNumpadEnter    = 156;

} // namespace

unsigned char dikFromMacKeyCode(unsigned short keyCode, bool isoLayout)
{
    if (keyCode >= 128)
        return 0;
    if (isoLayout)
    {
        if (keyCode == kKeyCodeIsoSection)
            return kDikGrave;
        if (keyCode == kKeyCodeGrave)
            return kDikOem102;
    }
    return kDikFromKeyCode[keyCode];
}

unsigned short macKeyCodeFromDik(unsigned char dik, bool isoLayout)
{
    if (dik == 0)
        return 0xFFFF;
    // Two keys produce DIK_NUMPADENTER; the keypad Enter (0x4C) is the one current keyboards have.
    if (dik == kDikNumpadEnter)
        return 0x4C;
    for (unsigned short keyCode = 0; keyCode < 128; ++keyCode)
        if (dikFromMacKeyCode(keyCode, isoLayout) == dik)
            return keyCode;
    return 0xFFFF;
}

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // Cocoa client or portable host fixture
