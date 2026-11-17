// [CHOSEN, not recovered] Shared native DIK metadata, issue #36.
#include "KeyMap.h"

#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT) || defined(OPENBF2_HOST_TEST_SEAMS))
namespace dice { namespace hfe { namespace host {

unsigned int win32VirtualKeyFromDik(unsigned char dik)
{
    // Letters and digits: the VK code is the upper-case ASCII character.
    static const char kRowQ[] = "QWERTYUIOP";      // DIK 16..25
    static const char kRowA[] = "ASDFGHJKL";       // DIK 30..38
    static const char kRowZ[] = "ZXCVBNM";         // DIK 44..50
    if (dik >= 2 && dik <= 10)
        return '1' + (dik - 2);
    if (dik == 11)
        return '0';
    if (dik >= 16 && dik <= 25)
        return static_cast<unsigned char>(kRowQ[dik - 16]);
    if (dik >= 30 && dik <= 38)
        return static_cast<unsigned char>(kRowA[dik - 30]);
    if (dik >= 44 && dik <= 50)
        return static_cast<unsigned char>(kRowZ[dik - 44]);
    if (dik >= 59 && dik <= 68)                     // F1..F10
        return 0x70 + (dik - 59);

    switch (dik)
    {
    case 1:   return 0x1B;  // VK_ESCAPE
    case 12:  return 0xBD;  // VK_OEM_MINUS
    case 13:  return 0xBB;  // VK_OEM_PLUS
    case 14:  return 0x08;  // VK_BACK
    case 15:  return 0x09;  // VK_TAB
    case 26:  return 0xDB;  // VK_OEM_4
    case 27:  return 0xDD;  // VK_OEM_6
    case 28:  return 0x0D;  // VK_RETURN
    case 29:  return 0x11;  // VK_CONTROL
    case 39:  return 0xBA;  // VK_OEM_1
    case 40:  return 0xDE;  // VK_OEM_7
    case 41:  return 0xC0;  // VK_OEM_3
    case 42:  return 0x10;  // VK_SHIFT
    case 43:  return 0xDC;  // VK_OEM_5
    case 51:  return 0xBC;  // VK_OEM_COMMA
    case 52:  return 0xBE;  // VK_OEM_PERIOD
    case 53:  return 0xBF;  // VK_OEM_2
    case 54:  return 0x10;  // VK_SHIFT
    case 55:  return 0x6A;  // VK_MULTIPLY
    case 56:  return 0x12;  // VK_MENU
    case 57:  return 0x20;  // VK_SPACE
    case 58:  return 0x14;  // VK_CAPITAL
    case 69:  return 0x90;  // VK_NUMLOCK
    case 70:  return 0x91;  // VK_SCROLL
    case 71:  return 0x67;  // VK_NUMPAD7
    case 72:  return 0x68;  // VK_NUMPAD8
    case 73:  return 0x69;  // VK_NUMPAD9
    case 74:  return 0x6D;  // VK_SUBTRACT
    case 75:  return 0x64;  // VK_NUMPAD4
    case 76:  return 0x65;  // VK_NUMPAD5
    case 77:  return 0x66;  // VK_NUMPAD6
    case 78:  return 0x6B;  // VK_ADD
    case 79:  return 0x61;  // VK_NUMPAD1
    case 80:  return 0x62;  // VK_NUMPAD2
    case 81:  return 0x63;  // VK_NUMPAD3
    case 82:  return 0x60;  // VK_NUMPAD0
    case 83:  return 0x6E;  // VK_DECIMAL
    case 86:  return 0xE2;  // VK_OEM_102
    case 87:  return 0x7A;  // VK_F11
    case 88:  return 0x7B;  // VK_F12
    case 100: return 0x7C;  // VK_F13
    case 101: return 0x7D;  // VK_F14
    case 102: return 0x7E;  // VK_F15
    case 112: return 0x15;  // VK_KANA
    case 115: return 0xC1;  // VK_ABNT_C1
    case 123: return 0x1D;  // VK_NONCONVERT
    case 141: return 0x92;  // VK_OEM_NEC_EQUAL
    case 156: return 0x0D;  // VK_RETURN (keypad)
    case 157: return 0x11;  // VK_CONTROL
    case 160: return 0xAD;  // VK_VOLUME_MUTE
    case 174: return 0xAE;  // VK_VOLUME_DOWN
    case 176: return 0xAF;  // VK_VOLUME_UP
    case 179: return 0x6C;  // VK_SEPARATOR
    case 181: return 0x6F;  // VK_DIVIDE
    case 183: return 0x2C;  // VK_SNAPSHOT
    case 184: return 0x12;  // VK_MENU
    case 197: return 0x13;  // VK_PAUSE
    case 199: return 0x24;  // VK_HOME
    case 200: return 0x26;  // VK_UP
    case 201: return 0x21;  // VK_PRIOR
    case 203: return 0x25;  // VK_LEFT
    case 205: return 0x27;  // VK_RIGHT
    case 207: return 0x23;  // VK_END
    case 208: return 0x28;  // VK_DOWN
    case 209: return 0x22;  // VK_NEXT
    case 210: return 0x2D;  // VK_INSERT
    case 211: return 0x2E;  // VK_DELETE
    case 219: return 0x5B;  // VK_LWIN
    case 220: return 0x5C;  // VK_RWIN
    case 221: return 0x5D;  // VK_APPS
    default:  return 0;
    }
}

const char* dikDisplayName(unsigned char dik)
{
    switch (dik)
    {
    case 1:   return "Esc";
    case 2:   return "1";
    case 3:   return "2";
    case 4:   return "3";
    case 5:   return "4";
    case 6:   return "5";
    case 7:   return "6";
    case 8:   return "7";
    case 9:   return "8";
    case 10:  return "9";
    case 11:  return "0";
    case 12:  return "-";
    case 13:  return "=";
    case 14:  return "Backspace";
    case 15:  return "Tab";
    case 16:  return "Q";
    case 17:  return "W";
    case 18:  return "E";
    case 19:  return "R";
    case 20:  return "T";
    case 21:  return "Y";
    case 22:  return "U";
    case 23:  return "I";
    case 24:  return "O";
    case 25:  return "P";
    case 26:  return "[";
    case 27:  return "]";
    case 28:  return "Enter";
    case 29:  return "Ctrl";
    case 30:  return "A";
    case 31:  return "S";
    case 32:  return "D";
    case 33:  return "F";
    case 34:  return "G";
    case 35:  return "H";
    case 36:  return "J";
    case 37:  return "K";
    case 38:  return "L";
    case 39:  return ";";
    case 40:  return "'";
    case 41:  return "`";
    case 42:  return "Shift";
    case 43:  return "\\";
    case 44:  return "Z";
    case 45:  return "X";
    case 46:  return "C";
    case 47:  return "V";
    case 48:  return "B";
    case 49:  return "N";
    case 50:  return "M";
    case 51:  return ",";
    case 52:  return ".";
    case 53:  return "/";
    case 54:  return "Right Shift";
    case 55:  return "Num *";
    case 56:  return "Alt";
    case 57:  return "Space";
    case 58:  return "Caps Lock";
    case 59:  return "F1";
    case 60:  return "F2";
    case 61:  return "F3";
    case 62:  return "F4";
    case 63:  return "F5";
    case 64:  return "F6";
    case 65:  return "F7";
    case 66:  return "F8";
    case 67:  return "F9";
    case 68:  return "F10";
    case 69:  return "Num Lock";
    case 70:  return "Scroll Lock";
    case 71:  return "Num 7";
    case 72:  return "Num 8";
    case 73:  return "Num 9";
    case 74:  return "Num -";
    case 75:  return "Num 4";
    case 76:  return "Num 5";
    case 77:  return "Num 6";
    case 78:  return "Num +";
    case 79:  return "Num 1";
    case 80:  return "Num 2";
    case 81:  return "Num 3";
    case 82:  return "Num 0";
    case 83:  return "Num Del";
    case 86:  return "<";
    case 87:  return "F11";
    case 88:  return "F12";
    case 100: return "F13";
    case 101: return "F14";
    case 102: return "F15";
    case 112: return "Kana";
    case 115: return "Ro";
    case 123: return "Eisu";
    case 125: return "Yen";
    case 141: return "Num =";
    case 156: return "Num Enter";
    case 157: return "Right Ctrl";
    case 160: return "Mute";
    case 174: return "Volume Down";
    case 176: return "Volume Up";
    case 179: return "Num ,";
    case 181: return "Num /";
    case 183: return "Prnt Scrn";
    case 184: return "Right Alt";
    case 197: return "Pause";
    case 199: return "Home";
    case 200: return "Up";
    case 201: return "Page Up";
    case 203: return "Left";
    case 205: return "Right";
    case 207: return "End";
    case 208: return "Down";
    case 209: return "Page Down";
    case 210: return "Insert";
    case 211: return "Delete";
    case 219: return "Super";
    case 220: return "Right Super";
    case 221: return "Menu";
    default:  return 0;
    }
}

} } }
#endif
