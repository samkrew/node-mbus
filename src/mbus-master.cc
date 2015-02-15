#include "mbus-master.h"

using namespace v8;

Persistent<Function> MbusMaster::constructor;

MbusMaster::MbusMaster(Handle<String>, int boudrate) {
  NanScope();
  long _boudrate;
  switch(boudrate) {
    case 300:
      _boudrate = 300;
      break;
    case 600:
      _boudrate = 600;
      break;
    case 1200:
      _boudrate = 1200;
      break;
    case 2400:
      _boudrate = 2400;
      break;
    case 4800:
      _boudrate = 4800;
      break;
    case 9600:
      _boudrate = 9600;
      break;
    case 19200:
      _boudrate = 19200;
      break;
    case 38400:
      _boudrate = 38400;
      break;
    default:
      _boudrate = 2400;
      break;
  }
  if (mbus_connect(handle) == -1)
  {
    fprintf(stderr,"Failed to setup connection to M-bus gateway\n");
    mbus_context_free(handle);
  }
}

MbusMaster::~MbusMaster(){
}

void MbusMaster::Init(Handle<Object> module) {
  NanScope();

  // Prepare constructor template
  Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
  tpl->SetClassName(NanNew("MbusMaster"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  //NODE_SET_PROTOTYPE_METHOD(tpl, "plusOne", PlusOne);

  NanAssignPersistent(constructor, tpl->GetFunction());
  module->Set(NanNew<String>("exports"), tpl->GetFunction());
}

NAN_METHOD(MbusMaster::New) {
  NanScope();

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new MbusMaster(...)`
    MbusMaster* obj = new MbusMaster(args[0]->ToString(),args[1]->IntegerValue());
    obj->Wrap(args.This());
    NanReturnValue(args.This());
  } else {
    // Invoked as plain function `MbusMaster(...)`, turn into construct call.
    const int argc = 1;
    Local<Value> argv[argc] = { args[0] };
    Local<Function> cons = NanNew<Function>(constructor);
    NanReturnValue(cons->NewInstance(argc, argv));
  }
}
/*
NAN_METHOD(MbusMaster::PlusOne) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());
  obj->value_ += 1;

  NanReturnValue(NanNew(obj->value_));
}*/