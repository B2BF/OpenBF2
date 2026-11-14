// [TEMP] [CHOSEN, not recovered] gameswf case-insensitive strings under the POSIX force-includes.
// PlatformCompat.h renames `stricmp`; tu_string's member of that
// name must still compare, or every stringi_hash lookup (ActionScript _global) misses.
#include "base/container.h"
#include <cstdio>

int main()
{
    int failures = 0;
    if (!(tu_stringi("Object") == tu_stringi("object"))) { std::printf("FAIL equal ignoring case\n"); ++failures; }
    if (tu_stringi("Object") == tu_stringi("Array")) { std::printf("FAIL distinct strings\n"); ++failures; }
    if (!(tu_stringi("abc") < "ABD")) { std::printf("FAIL ordering\n"); ++failures; }

    static const char* const names[] = { "trace", "Object", "Sound", "Array", "TextFormat",
        "MovieClipLoader", "String", "Number", "Boolean", "ASSetPropFlags", "setInterval",
        "clearInterval", "Color", "MovieClip", "math", "Key", "dice" };
    const int count = int(sizeof(names) / sizeof(names[0]));
    stringi_hash<int> table;
    for (int i = 0; i < count; ++i) table.set(tu_stringi(names[i]), i);
    for (int i = 0; i < count; ++i) {
        int value = -1;
        if (!table.get(tu_stringi(names[i]), &value) || value != i) {
            std::printf("FAIL lookup %s\n", names[i]); ++failures;
        }
    }
    int value = -1;
    if (!table.get(tu_stringi("OBJECT"), &value) || value != 1) { std::printf("FAIL case-folded lookup\n"); ++failures; }

    std::printf(failures ? "stringi_hash_check: %d failure(s)\n" : "stringi_hash_check: all passed\n", failures);
    return failures ? 1 : 0;
}
