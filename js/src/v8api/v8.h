#include "jsapi.h"
#include "jstl.h"
#include "jshashtable.h"

namespace v8 {
// Define some classes first so we can use them before fully defined
struct HandleScope;
class Value;
class Boolean;
class Number;
class Integer;
class String;
class Array;
class Object;
class Uint32;
class Int32;
class Context;
class Message;
class StackTrace;
class Function;
class ScriptOrigin;
class AccessorInfo;
class FunctionTemplate;
class ObjectTemplate;
class Signature;
class HeapStatistics;
template <class T> class Handle;
template <class T> class Local;
template <class T> class Persistent;

typedef void (*WeakReferenceCallback)(Persistent<Value> object,
                                      void* parameter);

#define UNIMPLEMENTEDAPI(...) \
  JS_BEGIN_MACRO \
  v8::internal::notImplemented(); \
  return __VA_ARGS__; \
  JS_END_MACRO

namespace internal {
class GCReference;
struct GCOps;
class GCReferenceContainer;
struct PersistentGCReference;

void notImplemented();

class GCReference {
  friend struct GCOps;
  friend struct PersistentGCReference;

  void root(JSContext *ctx) {
    JS_AddValueRoot(ctx, &mVal);
  }
  void unroot(JSContext *ctx) {
    JS_RemoveValueRoot(ctx, &mVal);
  }

protected:
  jsval mVal;
public:
  GCReference(jsval val) :
    mVal(val)
  {}
  GCReference() :
    mVal(JSVAL_VOID)
  {}
  jsval &native() {
    return mVal;
  }

  GCReference *Globalize();
  void Dispose();
  GCReference *Localize();
};

// This is allowed to violate the sacred rule that any class which inherits
// from GCReference must not increase the size of the class. The reason is
// that Persistent handles are never copied around in memory.
struct PersistentGCReference : public GCReference {
  PersistentGCReference(GCReference *ref);

  bool IsWeak() const {
    return prev != NULL || next != NULL;
  }
  bool IsNearDeath() const {
    return isNearDeath;
  }
  void MakeWeak(WeakReferenceCallback callback, void *context);
  void ClearWeak();

private:
  WeakReferenceCallback callback;
  void *context;
  bool isNearDeath;
  PersistentGCReference *prev, *next;

  static PersistentGCReference *weakPtrs;
  static bool setupWeakRefs;
  static JSBool GCCallback(JSContext *cx, JSGCStatus status);
};

template <class Inherits>
class SecretObject : public Inherits {
protected:
  SecretObject(JSObject *obj) :
    Inherits(OBJECT_TO_JSVAL(obj))
  {}
  Object& InternalObject() const {
    SecretObject *obj = const_cast<SecretObject*>(this);
    return *reinterpret_cast<Object*>(obj);
  }
};

// Hash policy for jsids
struct JSIDHashPolicy
{
  typedef jsid Key;
  typedef Key Lookup;

  static uint32 hash(const Lookup &l) {
    return *reinterpret_cast<const uint32*>(&l);
  }

  static JSBool match(const Key &k, const Lookup &l) {
    return k == l;
  }
};
} // namespace internal

struct HandleScope {
  HandleScope();
  ~HandleScope();
private:
  friend class internal::GCReference;
  static internal::GCReference *CreateHandle(internal::GCReference r);
  static bool IsLocalReference(internal::GCReference *);

  static HandleScope *sCurrent;
  size_t getHandleCount();

  internal::GCReferenceContainer *mGCReferences;
  HandleScope *mPrevious;
};

template <typename T>
class Handle {
  T* mVal;
public:
  Handle() : mVal(NULL) {}
  Handle(T *val) : mVal(val) {}

  template <typename S>
  Handle(Handle<S> h) : mVal(*h) {}

  bool IsEmpty() const {
    return mVal == 0;
  }
  T* operator ->() const {
    return mVal;
  }
  T* operator *() const {
    return mVal;
  }

  void Clear() {
    mVal = 0;
  }

  template <class S>
  static inline Handle<T> Cast(Handle<S> that) {
    return Handle<T>(T::Cast(*that));
  }

  template <class S>
  inline Handle<S> As() const {
    return Handle<S>::Cast(*this);
  }
};

template <typename T>
class Local : public Handle<T> {
public:
  Local() : Handle<T>() {}
  Local(T *val) : Handle<T>(val) {}

  static inline Local<T> New(Handle<T> other) {
    if (other.IsEmpty())
      return Local<T>();
    return reinterpret_cast<T*>(other->Localize());
  }

  template <class S>
  static inline Local<T> Cast(Local<S> that) {
    return Local<T>(T::Cast(*that));
  }

  template <class S>
  inline Local<S> As() const {
    return Local<S>::Cast(*this);
  }
};

template <typename T>
class Persistent : public Handle<T> {
  internal::PersistentGCReference *persistentRef() const {
    return reinterpret_cast<internal::PersistentGCReference*>(**this);
  }
public:
  Persistent() : Handle<T>() {}

  template <class S>
  Persistent(S *val) : Handle<T>(val) {}

  void Dispose() {
    (*this)->Dispose();
  }

  inline void MakeWeak(void* parameters, WeakReferenceCallback callback) {
    persistentRef()->MakeWeak(callback, parameters);
  }

  inline void ClearWeak() {
    persistentRef()->ClearWeak();
  }

  inline bool IsNearDeath() const {
    return persistentRef()->IsNearDeath();
  }

  inline bool IsWeak() const {
    return persistentRef()->IsWeak();
  }

  static Persistent<T> New(Handle<T> other) {
    if (other.IsEmpty())
      return Persistent<T>();
    return reinterpret_cast<T*>(other->Globalize());
  }

  template <class S>
  static inline Persistent<T> Cast(Persistent<S> that) {
    return Persistent<T>(T::Cast(*that));
  }

  template <class S>
  inline Persistent<S> As() const {
    return Persistent<S>::Cast(*this);
  }
};

class Message : public internal::SecretObject<internal::GCReference> {
  friend class TryCatch;

  Message(const char *message, JSErrorReport *report);
public:
  Local<String> Get() const;
  Local<String> GetSourceLine() const;

  Handle<Value> GetScriptResourceName() const;
  Handle<Value> GetScriptData() const;

  Handle<StackTrace> GetStackTrace() const;

  int GetLineNumber() const;
  int GetStartPosition() const;
  int GetEndPosition() const;

  int GetStartColumn() const;
  int GetEndColumn() const;

  static void PrintCurrentStackTrace(FILE* out);

  static const int kNoLineNumberInfo = 0;
  static const int kNoColumnInfo = 0;
};


// Garbage Collection
enum GCType {
  kGCTypeScavenge = 1 << 0,
  kGCTypeMarkSweepCompact = 1 << 1,
  kGCTypeAll = kGCTypeScavenge | kGCTypeMarkSweepCompact
};

enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagCompacted = 1 << 0
};

typedef void (*GCPrologueCallback)(GCType type, GCCallbackFlags flags);


// Exceptions
class Exception {
 public:
  static Local<Value> RangeError(Handle<String> message);
  static Local<Value> ReferenceError(Handle<String> message);
  static Local<Value> SyntaxError(Handle<String> message);
  static Local<Value> TypeError(Handle<String> message);
  static Local<Value> Error(Handle<String> message);
};

typedef void (*FatalErrorCallback)(const char* location, const char* message);


class V8 {
public:
  static bool Initialize();
  static bool Dispose();

  static bool IdleNotification();
  static void GetHeapStatistics(HeapStatistics* aHeapStatistics);
  static const char* GetVersion();
  static void SetFlagsFromCommandLine(int* argc, char** argv, bool aRemoveFlags);
  static void SetFatalErrorHandler(FatalErrorCallback aCallback);
  static int AdjustAmountOfExternalAllocatedMemory(int aChangeInBytes);
  static void AddGCPrologueCallback(GCPrologueCallback aCallback, GCType aGCTypeFilter = kGCTypeAll);
  static void LowMemoryNotification();

  static Handle<Value> ThrowException(Handle<Value> exception);
};

class TryCatch {
  friend class V8;
  friend class Script;
  static void ReportError(JSContext *ctx, const char *message, JSErrorReport *report);
  static void CheckForException();

  bool mHasCaught;
  bool mCaptureMessage;
  bool mRethrown;
  Persistent<Value> mException;
  Persistent<v8::Message> mMessage;
public:
  TryCatch();
  ~TryCatch();

  bool HasCaught() const {
    return mHasCaught;
  }
  bool CanContinue() const {
    return true;
  }

  Handle<Value> ReThrow();

  Local<Value> Exception() const {
    return Local<Value>::New(*mException);
  }
  Local<Value> StackTrace() const;

  Local<v8::Message> Message() const;
  void Reset();
  void SetVerbose(bool value);
  void SetCaptureMessage(bool value);
};

class Context : public internal::SecretObject<internal::GCReference> {
  Context(JSObject *global);
public:
  void Enter();
  void Exit();

  inline Local<Object> Global();

  static Local<Context> GetEntered();
  static Local<Context> GetCurrent();

  static Persistent<Context> New();

  struct Scope {
    Scope(Handle<Context> ctx) :
      mCtx(ctx) {
      mCtx->Enter();
    }
    ~Scope() {
      mCtx->Exit();
    }
  private:
    Handle<Context> mCtx;
  };
};

class ResourceConstraints {
public:
  ResourceConstraints();
  int max_young_space_size() const { return mMaxYoungSpaceSize; }
  void set_max_young_space_size(int value) { mMaxYoungSpaceSize = value; }
  int max_old_space_size() const { return mMaxOldSpaceSize; }
  void set_max_old_space_size(int value) { mMaxOldSpaceSize = value; }
  int max_executable_size() { return mMaxExecutableSize; }
  void set_max_executable_size(int value) { mMaxExecutableSize = value; }
  JSUint32* stack_limit() const { return mStackLimit; }
  void set_stack_limit(JSUint32* value) { mStackLimit = value; }
private:
  int mMaxYoungSpaceSize;
  int mMaxOldSpaceSize;
  int mMaxExecutableSize;
  JSUint32* mStackLimit;
};
bool SetResourceConstraints(ResourceConstraints *constraints);

class HeapStatistics {
 public:
  HeapStatistics();
  size_t total_heap_size() { UNIMPLEMENTEDAPI(0); }
  size_t total_heap_size_executable() { UNIMPLEMENTEDAPI(0); }
  size_t used_heap_size() { UNIMPLEMENTEDAPI(0); }
  size_t heap_size_limit() { UNIMPLEMENTEDAPI(0); }
};

class Data : public internal::GCReference {
public:
  Data() : internal::GCReference() { }
  Data(jsval val) : internal::GCReference(val) { }
};

// Parent class of every JS val / object
class Value : public Data {
public:
  Value() : Data() { }
  Value(jsval val) : Data(val) { }
  bool IsUndefined() const { return JSVAL_IS_VOID(mVal); }
  bool IsNull() const { return JSVAL_IS_NULL(mVal); }
  bool IsTrue() const { return IsBoolean() && JSVAL_TO_BOOLEAN(mVal); }
  bool IsFalse() const { return IsBoolean() && !JSVAL_TO_BOOLEAN(mVal); }

  bool IsString() const { return JSVAL_IS_STRING(mVal); }
  bool IsFunction() const;
  bool IsArray() const;
  bool IsObject() const { return !JSVAL_IS_PRIMITIVE(mVal); }
  bool IsBoolean() const { return JSVAL_IS_BOOLEAN(mVal); }
  bool IsNumber() const { return JSVAL_IS_NUMBER(mVal); }
  bool IsInt32() const { return JSVAL_IS_INT(mVal); }
  bool IsUint32() const;
  bool IsDate() const;

  Local<Boolean> ToBoolean() const;
  Local<Number> ToNumber() const;
  Local<String> ToString() const;
  Local<Object> ToObject() const;
  Local<Uint32> ToUint32() const;
  Local<Int32> ToInt32() const;

  bool BooleanValue() const;
  double NumberValue() const;
  JSInt64 IntegerValue() const;
  JSUint32 Uint32Value() const;
  JSInt32 Int32Value() const;

  bool Equals(Handle<Value> other) const;
  bool StrictEquals(Handle<Value> other) const;
};

class Primitive : public Value {
public:
  Primitive(jsval val) : Value(val) { }
};

class Boolean : public Primitive {
public:
  Boolean(JSBool val) :
    Primitive(val ? JSVAL_TRUE : JSVAL_FALSE) { }
  Boolean(jsval v) : Primitive(v) { }
  bool Value() const {
    return mVal == JSVAL_TRUE;
  }
  static Handle<Boolean> New(bool value);
};

class Number : public Primitive {
protected:
  Number(jsval v) : Primitive(v) { }
public:
  double Value() const;
  static Local<Number> New(double value);
  static Number* Cast(v8::Value* obj) {
    UNIMPLEMENTEDAPI(NULL);
  }
};

class Integer : public Number {
protected:
  Integer(jsval v) : Number(v) { }
public:
  static Local<Integer> New(JSInt32 value);
  static Local<Integer> NewFromUnsigned(JSUint32 value);
  JSInt64 Value() const;
  // XXX Cast is inline in V8
  static Integer* Cast(v8::Value* obj) {
    // TODO: check for uint32
    if (obj->IsInt32()) {
      return reinterpret_cast<Integer*>(obj);
    }
    return NULL;
  }
};

class Int32 : public Integer {
  friend class Value;
  Int32(JSInt32 i) : Integer(INT_TO_JSVAL(i)) { }
public:
  JSInt32 Value();
};

class Uint32 : public Integer {
  friend class Value;
  Uint32(JSUint32 i) : Integer(UINT_TO_JSVAL(i)) { }
public:
  JSUint32 Value();
};

class Date : public Value {
 public:
  static Local<Value> New(double time);
  double NumberValue() const {
    UNIMPLEMENTEDAPI(NULL);
  }
  static inline Date* Cast(v8::Value* obj) {
    UNIMPLEMENTEDAPI(NULL);
  }
  static void DateTimeConfigurationChangeNotification();
};


class String : public Primitive  {
  friend class Value;
  friend class Function;

  String(JSString *s) : Primitive (STRING_TO_JSVAL(s)) { }

  operator JSString*() const { return JSVAL_TO_STRING(mVal); }
public:
  int Length() const;
  int Utf8Length() const;

  enum WriteHints {
    NO_HINTS = 0,
    HINT_MANY_WRITES_EXPECTED = 1
  };
  int Write(JSUint16* buffer,
            int start = 0,
            int length = -1,
            WriteHints hints = NO_HINTS) const;
  int WriteAscii(char* buffer,
                 int start = 0,
                 int length = -1,
                 WriteHints hints = NO_HINTS) const;
  int WriteUtf8(char* buffer,
                int length= -1,
                int* nchars_ref = NULL,
                WriteHints hints = NO_HINTS) const;

  class Utf8Value {
    char* mStr;
    int mLength;
    // Disallow copying and assigning.
    Utf8Value(const Utf8Value&);
    void operator=(const Utf8Value&);
  public:
    explicit Utf8Value(Handle<v8::Value> val);
    ~Utf8Value();
    char* operator*() { return mStr; }
    const char* operator*() const { return mStr; }
    int length() const { return mLength; }
  };

  class AsciiValue {
    char* mStr;
    int mLength;
  public:
    explicit AsciiValue(Handle<v8::Value> val);
    ~AsciiValue();
    char* operator*() { return mStr; }
    const char* operator*() const { return mStr; }
    int length() const { return mLength; }
  };

  static Local<String> New(const char *data, int length = -1);
  static Local<String> FromJSID(jsid id);
  static inline String* Cast(Value *v) {
    if (v->IsString())
      return reinterpret_cast<String*>(v);
    return NULL;
  }
};


Handle<Primitive> Undefined();
Handle<Primitive> Null();
Handle<Boolean> True();
Handle<Boolean> False();

enum PropertyAttribute {
  None = 0,
  ReadOnly = 1 << 0,
  DontEnum = 1 << 1,
  DontDelete = 1 << 2
};

enum ExternalArrayType {
  kExternalByteArray = 1,
  kExternalUnsignedByteArray,
  kExternalShortArray,
  kExternalUnsignedShortArray,
  kExternalIntArray,
  kExternalUnsignedIntArray,
  kExternalFloatArray
};

typedef Handle<Value> (*AccessorGetter)(Local<String> property, const AccessorInfo &info);
typedef Handle<Value> (*AccessorSetter)(Local<String> property, Local<Value> value, const AccessorInfo &info);

enum AccessControl {
  DEFAULT = 0,
  ALL_CAN_READ = 1,
  ALL_CAN_WRITE = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2
};

class Object : public Value {
public:
  struct PrivateData;
private:
  PrivateData& GetHiddenStore();
  friend class Context;
  friend class Script;
  friend class Template;
  friend class ObjectTemplate;
  friend class Arguments;
  friend class AccessorInfo;
  friend class Message;
  friend class Function;

  static JSBool JSAPIPropertyGetter(JSContext*, JSObject* obj, jsid id, jsval* vp);
  static JSBool JSAPIPropertySetter(JSContext*, JSObject* obj, jsid id, JSBool, jsval* vp);
protected:
  Object(JSObject *obj);
public:
  operator JSObject *() const { return JSVAL_TO_OBJECT(mVal); }
  bool Set(Handle<Value> key, Handle<Value> value, PropertyAttribute attribs = None);
  bool Set(JSUint32 index, Handle<Value> value);
  bool ForceSet(Handle<Value> key, Handle<Value> value, PropertyAttribute attrib = None);
  Local<Value> Get(Handle<Value> key);
  Local<Value> Get(JSUint32 index);
  bool Has(Handle<String> key);
  bool Has(JSUint32 index);
  bool Delete(Handle<String> key);
  bool Delete(JSUint32 index);
  bool ForceDelete(Handle<String> key);
  bool SetAccessor(Handle<String> name, AccessorGetter getter, AccessorSetter setter = 0, Handle<Value> data = Handle<Value>(), AccessControl settings = DEFAULT, PropertyAttribute attribs = None);
  Local<Array> GetPropertyNames();
  Local<Value> GetPrototype();
  void SetPrototype(Handle<Value> prototype);
  Local<Object> FindInstanceInPrototypeChain(Handle<FunctionTemplate> tmpl);
  Local<String> ObjectProtoToString();
  Local<String> GetConstructorName();
  int InternalFieldCount();
  Local<Value> GetInternalField(int index);
  void SetInternalField(int index, Handle<Value> value);
  void* GetPointerFromInternalField(int index);
  void SetPointerInInternalField(int index, void* value);
  bool HasRealNamedProperty(Handle<String> key);
  bool HasRealIndexedProperty(JSUint32 index);
  bool HasRealNamedCallbackProperty(Handle<String> key);
  Local<Value> GetRealNamedPropertyInPrototypeChain(Handle<String> key);
  Local<Value> GetRealNamedProperty(Handle<String> key);
  bool HasNamedLookupInterceptor();
  bool HasIndexedLookupInterceptor();
  void TurnOnAccessCheck();
  int GetIdentityHash();
  bool SetHiddenValue(Handle<String> key, Handle<Value> value);
  Local<Value> GetHiddenValue(Handle<String> key);
  bool DeleteHiddenValue(Handle<String> key);
  bool IsDirty();
  Local<Object> Clone();
  void SetIndexedPropertiesToPixelData(JSUint8* data, int length);
  bool HasIndexedPropertiesInPixelData();
  JSUint8* GetIndexedPropertiesPixelData();
  int GetIndexedPropertiesPixelDataLength();
  void SetIndexedPropertiesToExternalArrayData(
      void* data,
      ExternalArrayType array_type,
      int number_of_elements);
  bool HasIndexedPropertiesInExternalArrayData();
  void* GetIndexedPropertiesExternalArrayData();
  ExternalArrayType GetIndexedPropertiesExternalArrayDataType();
  int GetIndexedPropertiesExternalArrayDataLength();

  static inline Object* Cast(Value *obj) {
    if (obj->IsObject())
      return reinterpret_cast<Object*>(obj);
    return NULL;
  }

  static Local<Object> New();
};

class Array : public Object {
  Array(JSObject* obj) :
    Object(obj)
  {}
 public:
  JSUint32 Length() const;
  Local<Object> CloneElementAt(JSUint32 index);

  static Local<Array> New(int length = 0);
  static inline Array* Cast(Value* obj) {
    if (obj->IsArray()) {
      return reinterpret_cast<Array*>(obj);
    }
    return NULL;
  }
};

class Function : public Object {
  Function(JSObject *obj) :
    Object(obj)
  {}
  Function(JSFunction *fn) :
    Object(JS_GetFunctionObject(fn))
  {}

  operator JSFunction *() const;

  friend class Object;
  friend class Arguments;
public:
  Local<Object> NewInstance() const;
  Local<Object> NewInstance(int argc, Handle<Value> argv[]) const;
  Local<Value> Call(Handle<Object> recv, int argc, Handle<Value> argv[]) const;
  void SetName(Handle<String> name);
  Handle<String> GetName() const;

  int GetScriptLineNumber() const;
  // UNIMPLEMENTEDAPI
  ScriptOrigin GetScriptOrigin() const;
  static inline Function* Cast(Value *v) {
    if (v->IsFunction())
      return reinterpret_cast<Function*>(v);
    return NULL;
  }
  static const int kLineOffsetNotFound;
};

class ScriptOrigin {
  Handle<Value> mResourceName;
  Handle<Integer> mResourceLineOffset;
  Handle<Integer> mResourceColumnOffset;

public:
  // XXX: V8 inlines these, should we?
  ScriptOrigin(Handle<Value> resourceName,
               Handle<Integer> resourceLineOffset = Handle<Integer>(),
               Handle<Integer> resourceColumnOffset = Handle<Integer>());
  Handle<Value> ResourceName() const;
  Handle<Integer> ResourceLineOffset() const;
  Handle<Integer> ResourceColumnOffset() const;
};

class ScriptData {
public:
  virtual ~ScriptData() { }
  static ScriptData* PreCompile(const char* input, int length);
  static ScriptData* PreCompile(Handle<String> source);
  static ScriptData* New(const char* data, int length);
  virtual int Length() = 0;
  virtual const char* Data() = 0;
  virtual bool HasError() = 0;
};

class Script : public internal::GCReference {
  Script(JSObject *s);

  operator JSObject *();
  Handle<Object> InternalObject();

  static Local<Script> Create(Handle<String> source, ScriptOrigin *origin, ScriptData *preData, Handle<String> scriptData, bool bindToCurrentContext);
public:
  static Local<Script> New(Handle<String> source, ScriptOrigin *origin = NULL,
                           ScriptData *preData = NULL,
                           Handle<String> scriptData = Handle<String>());
  static Local<Script> New(Handle<String> source, Handle<Value> fileName);
  static Local<Script> Compile(Handle<String> source, ScriptOrigin *origin = NULL,
                               ScriptData *preData = NULL,
                               Handle<String> scriptData = Handle<String>());
  static Local<Script> Compile(Handle<String> source,
                               Handle<Value> fileName,
                               Handle<String> scriptData = Handle<String>());
  Local<Value> Run();
  Local<Value> Id();
};

class Template : public internal::SecretObject<Data> {
public:
  // XXX v8 header says the second argument should be a Handle<Data>
  void Set(Handle<String> name, Handle<Value> value,
           PropertyAttribute attribs = None);

  // XXX v8 header says the second argument should be a Handle<Data>
  void Set(const char* name, Handle<Value> value);
protected:
  Template(JSClass* clasp);

  friend class FunctionTemplate;
  friend class ObjectTemplate;
};

class Arguments {
  friend class Object;
  Arguments(JSContext* cx, JSObject* thisObj, int nargs, jsval* vals, Handle<Value> data);

  JSContext *mCtx;
  jsval *mValues;
  JSObject *mThis;
  int mLength;
  Local<Value> mData;
public:
  int Length() const {
    return mLength;
  }
  Local<Value> operator[](int i) const;
  Local<Function> Callee() const;

  Local<Object> This() const {
    Object o(mThis);
    return Local<Object>::New(&o);
  }
  Local<Object> Holder() const {
    UNIMPLEMENTEDAPI(NULL);
  }
  bool IsConstructCall() const;
  Local<Value> Data() const {
    return mData;
  }
};

class AccessorInfo {
  friend class Object;
  AccessorInfo(Handle<Value> data, JSObject *obj);

  Handle<Value> mData;
  JSObject* mObj;
public:
  static const AccessorInfo MakeAccessorInfo(Handle<Value> data, JSObject* obj);
  Local<Value> Data() const {
    return Local<Value>::New(mData);
  }
  Local<Object> This() const;
  Local<Object> Holder() const {
    UNIMPLEMENTEDAPI(NULL);
  }
};

typedef Handle<Value> (*InvocationCallback)(const Arguments &args);
typedef Handle<Value> (*NamedPropertyGetter)(Local<String> property, const AccessorInfo &info);
typedef Handle<Value> (*NamedPropertySetter)(Local<String> property, Local<Value> value, const AccessorInfo &info);
typedef Handle<Integer> (*NamedPropertyQuery)(Local<String> property, const AccessorInfo &info);
typedef Handle<Boolean> (*NamedPropertyDeleter)(Local<String> property, const AccessorInfo &info);
typedef Handle<Array> (*NamedPropertyEnumerator)(const AccessorInfo &info);

typedef Handle<Value> (*IndexedPropertyGetter)(JSUint32 index, const AccessorInfo &info);
typedef Handle<Value> (*IndexedPropertySetter)(JSUint32 index, Local<Value> value, const AccessorInfo &info);
typedef Handle<Integer> (*IndexedPropertyQuery)(JSUint32 index, const AccessorInfo &info);
typedef Handle<Boolean> (*IndexedPropertyDeleter)(JSUint32 index, const AccessorInfo &info);
typedef Handle<Array> (*IndexedPropertyEnumerator)(const AccessorInfo &info);

enum AccessType {
  ACCESS_GET,
  ACCESS_SET,
  ACCESS_HAS,
  ACCESS_DELETE,
  ACCESS_KEYS
};

typedef bool (*NamedSecurityCallback)(Local<Object> host, Local<Value> key, AccessType type, Local<Value> data);
typedef bool (*IndexedSecurityCallback)(Local<Object> host, JSUint32 index, AccessType type, Local<Value> data);

class FunctionTemplate : public Template {
public:
  static Local<FunctionTemplate> New(InvocationCallback callback = 0, Handle<Value> data = Handle<Value>(), Handle<Signature> signature = Handle<Signature>()) {
    UNIMPLEMENTEDAPI(NULL);
  }

  Local<Function> GetFunction () {
    UNIMPLEMENTEDAPI(NULL);
  }

  void SetCallHandler(InvocationCallback callback, Handle<Value> data = Handle<Value>()) {
    UNIMPLEMENTEDAPI();
  }

  Local<ObjectTemplate> InstanceTemplate() {
    UNIMPLEMENTEDAPI(NULL);
  }

  void Inherit(Handle<FunctionTemplate> parent) {
    UNIMPLEMENTEDAPI();
  }

  Local<ObjectTemplate> PrototypeTemplate() {
    UNIMPLEMENTEDAPI(NULL);
  }

  void SetClassName(Handle<String> name) {
    UNIMPLEMENTEDAPI();
  }

  void SetHiddenPrototype(bool value) {
    UNIMPLEMENTEDAPI();
  }

  bool HasInstance(Handle<Value> object) {
    UNIMPLEMENTEDAPI(false);
  }
};

class ObjectTemplate : public Template {
  ObjectTemplate();
public:
  static Local<ObjectTemplate> New();
  Local<Object> NewInstance();
  void SetAccessor(Handle<String> name, AccessorGetter getter, AccessorSetter setter = 0, Handle<Value> data = Handle<Value>(), AccessControl settings = DEFAULT, PropertyAttribute attribs = None);
  void SetNamedPropertyHandler(NamedPropertyGetter getter, NamedPropertySetter setter = 0, NamedPropertyQuery query = 0, NamedPropertyDeleter deleter = 0, NamedPropertyEnumerator enumerator = 0, Handle<Value> data = Handle<Value>());
  void SetIndexedPropertyHandler(IndexedPropertyGetter getter, IndexedPropertySetter setter = 0, IndexedPropertyQuery query = 0, IndexedPropertyDeleter deleter = 0, IndexedPropertyEnumerator enumerator = 0, Handle<Value> data = Handle<Value>());
  void SetCallAsFunctionHandler(InvocationCallback callback, Handle<Value> data = Handle<Value>());
  void MarkAsUndetectable();
  void SetAccessCheckCallbacks(NamedSecurityCallback named_handler, IndexedSecurityCallback indexed_callback, Handle<Value> data = Handle<Value>(), bool turned_on_by_default = true);
  int InternalFieldCount();
  void SetInternalFieldCount(int value);
};

Local<Object> Context::Global() {
  return Local<Object>::New(&InternalObject());
}

} // namespace v8

namespace i = v8::internal;
