#include "mbus-master.h"
#include "util.h"
#include <unistd.h>

using namespace v8;

Persistent<Function> MbusMaster::constructor;

MbusMaster::MbusMaster() {
  connected = false;
  serial = true;
  handle = NULL;
  uv_rwlock_init(&queueLock);
}

MbusMaster::~MbusMaster(){
  if(connected) {
    mbus_disconnect(handle);
  }
  if(handle) {
    mbus_context_free(handle);
  }
  uv_rwlock_destroy(&queueLock);
}

void MbusMaster::Init(Handle<Object> module) {
  NanScope();

  // Prepare constructor template
  Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
  tpl->SetClassName(NanNew("MbusMaster"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "openSerial", OpenSerial);
  NODE_SET_PROTOTYPE_METHOD(tpl, "openTCP", OpenTCP);
  NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(tpl, "get", Get);
  NODE_SET_PROTOTYPE_METHOD(tpl, "scan", ScanSecondary);

  NanAssignPersistent(constructor, tpl->GetFunction());
  module->Set(NanNew<String>("exports"), tpl->GetFunction());
}

NAN_METHOD(MbusMaster::New) {
  NanScope();

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new MbusMaster(...)`
    MbusMaster* obj = new MbusMaster();
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

NAN_METHOD(MbusMaster::OpenTCP) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());

  int port = (long)args[1]->IntegerValue();
  char *host = get(args[0]->ToString(), "127.0.0.1");

  obj->serial = false;

  if(!obj->connected) {
    if ((port < 0) || (port > 0xFFFF))
    {
        free(host);
        NanReturnValue(NanFalse());
    }
    if (!(obj->handle = mbus_context_tcp(host,port)))
    {
        free(host);
        NanReturnValue(NanFalse());
    }
    free(host);
    if (mbus_connect(obj->handle) == -1)
    {
      mbus_context_free(obj->handle);
      obj->handle = NULL;
      NanReturnValue(NanFalse());
    }
    obj->connected = true;
    NanReturnValue(NanTrue());
  }
  NanReturnValue(NanFalse());
}

NAN_METHOD(MbusMaster::OpenSerial) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());

  long boudrate;
  int _boudrate = args[1]->IntegerValue();
  char *port = get(args[0]->ToString(), "/dev/ttyS0");

  obj->serial = true;

  switch(_boudrate) {
    case 300:
      boudrate = 300;
      break;
    case 600:
      boudrate = 600;
      break;
    case 1200:
      boudrate = 1200;
      break;
    case 2400:
      boudrate = 2400;
      break;
    case 4800:
      boudrate = 4800;
      break;
    case 9600:
      boudrate = 9600;
      break;
    case 19200:
      boudrate = 19200;
      break;
    case 38400:
      boudrate = 38400;
      break;
    default:
      boudrate = 2400;
      break;
  }

  if(!obj->connected) {
    if (!(obj->handle = mbus_context_serial(port)))
    {
        free(port);
        NanReturnValue(NanFalse());
    }
    free(port);
    if (mbus_connect(obj->handle) == -1)
    {
      mbus_context_free(obj->handle);
      obj->handle = NULL;
      NanReturnValue(NanFalse());
    }
    if (mbus_serial_set_baudrate(obj->handle, boudrate) == -1)
    {
        mbus_disconnect(obj->handle);
        mbus_context_free(obj->handle);
        obj->handle = NULL;
        NanReturnValue(NanFalse());
    }
    obj->connected = true;
    NanReturnValue(NanTrue());
  }
  NanReturnValue(NanFalse());
}

NAN_METHOD(MbusMaster::Close) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());

  if(obj->connected) {
    mbus_disconnect(obj->handle);
    mbus_context_free(obj->handle);
    obj->handle = NULL;
    obj->connected = false;
    NanReturnValue(NanTrue());
  }
  NanReturnValue(NanFalse());
}

static int init_slaves(mbus_handle *handle)
{

    if (mbus_send_ping_frame(handle, MBUS_ADDRESS_NETWORK_LAYER, 1) == -1)
    {
        return 0;
    }

    if (mbus_send_ping_frame(handle, MBUS_ADDRESS_NETWORK_LAYER, 1) == -1)
    {
        return 0;
    }

    return 1;
}

class RecieveWorker : public NanAsyncWorker {
 public:
  RecieveWorker(NanCallback *callback,char *addr_str,uv_rwlock_t *lock, mbus_handle *handle)
    : NanAsyncWorker(callback), addr_str(addr_str), lock(lock), handle(handle) {}
  ~RecieveWorker() {
    free(addr_str);
  }

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    uv_rwlock_wrlock(lock);

    mbus_frame reply;
    mbus_frame_data reply_data;
    char error[100];
    int address;

    memset((void *)&reply, 0, sizeof(mbus_frame));
    memset((void *)&reply_data, 0, sizeof(mbus_frame_data));
    
    if (init_slaves(handle) == 0)
    {
        mbus_disconnect(handle);
        mbus_context_free(handle);
        sprintf(error, "Failed to init slaves.");
        SetErrorMessage(error);
        uv_rwlock_wrunlock(lock);
        return;
    }

    if (mbus_is_secondary_address(addr_str))
    {
        // secondary addressing

        int ret;

        ret = mbus_select_secondary_address(handle, addr_str);

        if (ret == MBUS_PROBE_COLLISION)
        {
            sprintf(error, "The address mask [%s] matches more than one device.", addr_str);
            SetErrorMessage(error);
            uv_rwlock_wrunlock(lock);
            return;
        }
        else if (ret == MBUS_PROBE_NOTHING)
        {
            sprintf(error, "The selected secondary address does not match any device [%s].", addr_str);
            SetErrorMessage(error);
            uv_rwlock_wrunlock(lock);
            return;
        }
        else if (ret == MBUS_PROBE_ERROR)
        {
            sprintf(error, "Failed to select secondary address [%s].", addr_str);
            SetErrorMessage(error);
            uv_rwlock_wrunlock(lock);
            return;
        }
        // else MBUS_PROBE_SINGLE
        
        address = MBUS_ADDRESS_NETWORK_LAYER;
    }
    else
    {
        // primary addressing
        address = atoi(addr_str);
    }

    if (mbus_send_request_frame(handle, address) == -1)
    {
        sprintf(error, "Failed to send M-Bus request frame[%s].", addr_str);
        SetErrorMessage(error);
        uv_rwlock_wrunlock(lock);
        return;
    }

    if (mbus_recv_frame(handle, &reply) != MBUS_RECV_RESULT_OK)
    {
        sprintf(error, "Failed to receive M-Bus response frame[%s].", addr_str);
        SetErrorMessage(error);
        uv_rwlock_wrunlock(lock);
        return;
    }

    //
    // parse data
    //
    if (mbus_frame_data_parse(&reply, &reply_data) == -1)
    {
        sprintf(error, "M-bus data parse error [%s].", addr_str);
        SetErrorMessage(error);
        uv_rwlock_wrunlock(lock);
        return;
    }

    //
    // generate XML and print to standard output
    //
    if ((data = mbus_frame_data_json(&reply_data)) == NULL)
    {
        sprintf(error, "Failed to generate JSON representation of MBUS frame [%s].", addr_str);
        SetErrorMessage(error);
        uv_rwlock_wrunlock(lock);
        return;
    }

    // manual free
    if (reply_data.data_var.record)
    {
        mbus_data_record_free(reply_data.data_var.record);
    }

    uv_rwlock_wrunlock(lock);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();

    Local<Value> argv[] = {
        NanNull(),
        NanNew<String>(data)
    };
    free(data);
    callback->Call(2, argv);
  };
  void HandleErrorCallback () {
    NanScope();

    Local<Value> argv[] = {
        NanError(ErrorMessage())
    };

    callback->Call(1, argv);
  }
  private:
    char *data;
    char *addr_str;
    uv_rwlock_t *lock;
    mbus_handle *handle;
};

NAN_METHOD(MbusMaster::Get) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());

  char *address = get(args[0]->ToString(),"0");
  NanCallback *callback = new NanCallback(args[1].As<Function>());
  if(obj->connected) {
    NanAsyncQueueWorker(new RecieveWorker(callback, address, &(obj->queueLock), obj->handle));
  } else {
    Local<Value> argv[] = {
        NanError("Not connected to port")
    };
    callback->Call(1, argv);
  }
  NanReturnUndefined();
}

class ScanSecondaryWorker : public NanAsyncWorker {
 public:
  ScanSecondaryWorker(NanCallback *callback,uv_rwlock_t *lock, mbus_handle *handle)
    : NanAsyncWorker(callback), lock(lock), handle(handle) {}
  ~ScanSecondaryWorker() {
  }

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    uv_rwlock_wrlock(lock);

    mbus_frame *frame = NULL, reply;
    char error[100];
    int i, i_start = 0, i_end = 9, probe_ret;
    char mask[17], matching_mask[17], buffer[22];
    int pos = 0;

    strcpy(mask,"FFFFFFFFFFFFFFFF");

    memset((void *)&reply, 0, sizeof(mbus_frame));

    frame = mbus_frame_new(MBUS_FRAME_TYPE_SHORT);

    if (frame == NULL)
    {
        sprintf(error, "Failed to allocate mbus frame.");
        free(frame);
        uv_rwlock_wrunlock(lock);
        return;
    }

    if (init_slaves(handle) == 0)
    {
        free(frame);
        uv_rwlock_wrunlock(lock);
        return;
    }

    data = strdup("[");

    for (i = i_start; i <= i_end; i++)
    {
        mask[pos] = '0'+i;

        if (handle->scan_progress)
            handle->scan_progress(handle,mask);

        probe_ret = mbus_probe_secondary_address(handle, mask, matching_mask);

        if (probe_ret == MBUS_PROBE_SINGLE)
        {
            if (!handle->found_event)
            {
                sprintf(buffer,"\"%s\",",matching_mask);
                data = (char*)realloc(data, strlen(data) + strlen(buffer) + sizeof(char));
                if(!data) {
                  sprintf(error,"Failed to allocate data");
                  SetErrorMessage(error);
                  uv_rwlock_wrunlock(lock);
                  return;
                }
                strcat(data,buffer);
            }
        }
        else if (probe_ret == MBUS_PROBE_COLLISION)
        {
            // collision, more than one device matching, restrict the search mask further
            mbus_scan_2nd_address_range(handle, pos+1, mask);
        }
        else if (probe_ret == MBUS_PROBE_NOTHING)
        {
             // nothing... move on to next address mask
        }
        else // MBUS_PROBE_ERROR
        {
            sprintf(error,"Failed to probe secondary address [%s]", mask);
            SetErrorMessage(error);
            free(data);
            uv_rwlock_wrunlock(lock);
            return;
        }
    }
    data[strlen(data) - 1] = '\0';
    uv_rwlock_wrunlock(lock);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();

    Local<Value> argv[] = {
        NanNull(),
        NanNew<String>(data)
    };
    free(data);
    callback->Call(2, argv);
  };
  void HandleErrorCallback () {
    NanScope();

    Local<Value> argv[] = {
        NanError(ErrorMessage())
    };
    callback->Call(1, argv);
  }
  private:
    char *data;
    uv_rwlock_t *lock;
    mbus_handle *handle;
};

NAN_METHOD(MbusMaster::ScanSecondary) {
  NanScope();

  MbusMaster* obj = ObjectWrap::Unwrap<MbusMaster>(args.Holder());

  NanCallback *callback = new NanCallback(args[0].As<Function>());
  if(obj->connected) {
    NanAsyncQueueWorker(new ScanSecondaryWorker(callback, &(obj->queueLock), obj->handle));
  } else {
    Local<Value> argv[] = {
        NanError("Not connected to port")
    };
    callback->Call(1, argv);
  }
  NanReturnUndefined();
}