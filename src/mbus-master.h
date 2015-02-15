#ifndef MBUSMASTER_H
#define MBUSMASTER_H

#include <node.h>
#include <nan.h>
extern "C" {
	#include <mbus.h>
}

class MbusMaster : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> module);

 private:
  explicit MbusMaster(v8::Handle<v8::String>, int boudrate = 2400);
  ~MbusMaster();

  static NAN_METHOD(New);
  //static NAN_METHOD(PlusOne);
  static v8::Persistent<v8::Function> constructor;
  mbus_handle *handle;
};

#endif