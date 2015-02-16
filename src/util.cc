#include "util.h"
#include <cstdlib>
#include <cstring>

char *get(v8::Handle<v8::Value> value, const char *fallback) {
    if (value->IsString()) {
        v8::String::AsciiValue string(value);
        char *str = (char *) malloc(string.length() + 1);
        strcpy(str, *string);
        return str;
    }
    char *str = (char *) malloc(strlen(fallback) + 1);
    strcpy(str, fallback);
    return str;
}