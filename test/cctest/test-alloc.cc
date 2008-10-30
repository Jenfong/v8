// Copyright 2007-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"
#include "top.h"

#include "cctest.h"


using namespace v8::internal;


static Object* AllocateAfterFailures() {
  static int attempts = 0;
  if (++attempts < 3) return Failure::RetryAfterGC(0);

  // New space.
  NewSpace* new_space = Heap::new_space();
  static const int kNewSpaceFillerSize = ByteArray::SizeFor(0);
  while (new_space->Available() > kNewSpaceFillerSize) {
    CHECK(!Heap::AllocateByteArray(0)->IsFailure());
  }
  CHECK(!Heap::AllocateByteArray(100)->IsFailure());
  CHECK(!Heap::AllocateFixedArray(100, NOT_TENURED)->IsFailure());

  // Make sure we can allocate through optimized allocation functions
  // for specific kinds.
  CHECK(!Heap::AllocateFixedArray(100)->IsFailure());
  CHECK(!Heap::AllocateHeapNumber(0.42)->IsFailure());
  CHECK(!Heap::AllocateArgumentsObject(Smi::FromInt(87), 10)->IsFailure());
  Object* object = Heap::AllocateJSObject(*Top::object_function());
  CHECK(!Heap::CopyJSObject(JSObject::cast(object))->IsFailure());

  // Old data space.
  OldSpace* old_data_space = Heap::old_data_space();
  static const int kOldDataSpaceFillerSize = SeqAsciiString::SizeFor(0);
  while (old_data_space->Available() > kOldDataSpaceFillerSize) {
    CHECK(!Heap::AllocateRawAsciiString(0, TENURED)->IsFailure());
  }
  CHECK(!Heap::AllocateRawAsciiString(100, TENURED)->IsFailure());

  // Large object space.
  while (!Heap::OldGenerationAllocationLimitReached()) {
    CHECK(!Heap::AllocateFixedArray(10000, TENURED)->IsFailure());
  }
  CHECK(!Heap::AllocateFixedArray(10000, TENURED)->IsFailure());

  // Map space.
  MapSpace* map_space = Heap::map_space();
  static const int kMapSpaceFillerSize = Map::kSize;
  InstanceType instance_type = JS_OBJECT_TYPE;
  int instance_size = JSObject::kHeaderSize;
  while (map_space->Available() > kMapSpaceFillerSize) {
    CHECK(!Heap::AllocateMap(instance_type, instance_size)->IsFailure());
  }
  CHECK(!Heap::AllocateMap(instance_type, instance_size)->IsFailure());

  // Test that we can allocate in old pointer space and code space.
  CHECK(!Heap::AllocateFixedArray(100, TENURED)->IsFailure());
  CHECK(!Heap::CopyCode(Builtins::builtin(Builtins::Illegal))->IsFailure());

  // Return success.
  return Smi::FromInt(42);
}

static Handle<Object> Test() {
  CALL_HEAP_FUNCTION(AllocateAfterFailures(), Object);
}


TEST(Stress) {
  v8::Persistent<v8::Context> env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
  Handle<Object> o = Test();
  CHECK(o->IsSmi() && Smi::cast(*o)->value() == 42);
  env->Exit();
}
