#ifndef UTIL_H
#define UTIL_H

#include <node.h>
#include <v8.h>

char *get(v8::Handle<v8::Value> value, const char *fallback = "");

#endif
