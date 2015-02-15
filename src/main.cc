#include <nan.h>
#include "mbus-master.h"

using namespace v8;

void init(Handle<Object> exports, Handle<Object> module) {
  MbusMaster::Init(module);
}

NODE_MODULE(mbus, init)