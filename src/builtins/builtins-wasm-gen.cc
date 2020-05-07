// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-wasm-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

TNode<WasmInstanceObject> WasmBuiltinsAssembler::LoadInstanceFromFrame() {
  return CAST(
      LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
}

TNode<NativeContext> WasmBuiltinsAssembler::LoadContextFromInstance(
    TNode<WasmInstanceObject> instance) {
  return CAST(Load(MachineType::AnyTagged(), instance,
                   IntPtrConstant(WasmInstanceObject::kNativeContextOffset -
                                  kHeapObjectTag)));
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadTablesFromInstance(
    TNode<WasmInstanceObject> instance) {
  return LoadObjectField<FixedArray>(instance,
                                     WasmInstanceObject::kTablesOffset);
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadExternalFunctionsFromInstance(
    TNode<WasmInstanceObject> instance) {
  return LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kWasmExternalFunctionsOffset);
}

TNode<Smi> WasmBuiltinsAssembler::SmiFromUint32WithSaturation(
    TNode<Uint32T> value, uint32_t max) {
  DCHECK_LE(max, static_cast<uint32_t>(Smi::kMaxValue));
  TNode<Uint32T> capped_value = SelectConstant(
      Uint32LessThan(value, Uint32Constant(max)), value, Uint32Constant(max));
  return SmiFromUint32(capped_value);
}

TF_BUILTIN(WasmFloat32ToNumber, WasmBuiltinsAssembler) {
  TNode<Float32T> val = UncheckedCast<Float32T>(Parameter(Descriptor::kValue));
  Return(ChangeFloat32ToTagged(val));
}

TF_BUILTIN(WasmFloat64ToNumber, WasmBuiltinsAssembler) {
  TNode<Float64T> val = UncheckedCast<Float64T>(Parameter(Descriptor::kValue));
  Return(ChangeFloat64ToTagged(val));
}

TF_BUILTIN(WasmGetOwnProperty, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kObject));
  TNode<Name> unique_name = CAST(Parameter(Descriptor::kUniqueName));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TVariable<Object> var_value(this);

  Label if_found(this), if_not_found(this), if_bailout(this);

  GotoIf(TaggedIsSmi(object), &if_not_found);

  GotoIf(IsUndefined(object), &if_not_found);

  TNode<Map> map = LoadMap(CAST(object));
  TNode<Uint16T> instance_type = LoadMapInstanceType(map);

  GotoIfNot(IsJSReceiverInstanceType(instance_type), &if_not_found);

  TryGetOwnProperty(context, CAST(object), CAST(object), map, instance_type,
                    unique_name, &if_found, &var_value, &if_not_found,
                    &if_bailout);

  BIND(&if_found);
  Return(var_value.value());

  BIND(&if_not_found);
  Return(UndefinedConstant());

  BIND(&if_bailout);  // This shouldn't happen when called from wasm compiler
  Unreachable();
}

TF_BUILTIN(WasmAtomicNotify, WasmBuiltinsAssembler) {
  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Uint32T> count = UncheckedCast<Uint32T>(Parameter(Descriptor::kCount));

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Number> address_number = ChangeUint32ToTagged(address);
  TNode<Number> count_number = ChangeUint32ToTagged(count);
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmAtomicNotify, context, instance,
                       address_number, count_number));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI32AtomicWait32, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<Int32T> expected_value =
      UncheckedCast<Int32T>(Parameter(Descriptor::kExpectedValue));
  TNode<Number> expected_value_number = ChangeInt32ToTagged(expected_value);

  TNode<IntPtrT> timeout_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutLow));
  TNode<IntPtrT> timeout_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutHigh));
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI32AtomicWait, context, instance,
                       address_number, expected_value_number, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI32AtomicWait64, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<Int32T> expected_value =
      UncheckedCast<Int32T>(Parameter(Descriptor::kExpectedValue));
  TNode<Number> expected_value_number = ChangeInt32ToTagged(expected_value);

  TNode<IntPtrT> timeout_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeout));
  TNode<BigInt> timeout = BigIntFromInt64(timeout_raw);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI32AtomicWait, context, instance,
                       address_number, expected_value_number, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI64AtomicWait32, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<IntPtrT> expected_value_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValueLow));
  TNode<IntPtrT> expected_value_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValueHigh));
  TNode<BigInt> expected_value =
      BigIntFromInt32Pair(expected_value_low, expected_value_high);

  TNode<IntPtrT> timeout_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutLow));
  TNode<IntPtrT> timeout_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutHigh));
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI64AtomicWait, context, instance,
                       address_number, expected_value, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI64AtomicWait64, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<IntPtrT> expected_value_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValue));
  TNode<BigInt> expected_value = BigIntFromInt64(expected_value_raw);

  TNode<IntPtrT> timeout_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeout));
  TNode<BigInt> timeout = BigIntFromInt64(timeout_raw);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI64AtomicWait, context, instance,
                       address_number, expected_value, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmTableInit, WasmBuiltinsAssembler) {
  TNode<Uint32T> dst_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kDestination));
  // We cap {dst}, {src}, and {size} by {wasm::kV8MaxWasmTableSize + 1} to make
  // sure that the values fit into a Smi.
  STATIC_ASSERT(static_cast<size_t>(Smi::kMaxValue) >=
                wasm::kV8MaxWasmTableSize + 1);
  constexpr uint32_t kCap =
      static_cast<uint32_t>(wasm::kV8MaxWasmTableSize + 1);
  TNode<Smi> dst = SmiFromUint32WithSaturation(dst_raw, kCap);
  TNode<Uint32T> src_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSource));
  TNode<Smi> src = SmiFromUint32WithSaturation(src_raw, kCap);
  TNode<Uint32T> size_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSize));
  TNode<Smi> size = SmiFromUint32WithSaturation(size_raw, kCap);
  TNode<Smi> table_index =
      UncheckedCast<Smi>(Parameter(Descriptor::kTableIndex));
  TNode<Smi> segment_index =
      UncheckedCast<Smi>(Parameter(Descriptor::kSegmentIndex));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TailCallRuntime(Runtime::kWasmTableInit, context, instance, table_index,
                  segment_index, dst, src, size);
}

TF_BUILTIN(WasmTableCopy, WasmBuiltinsAssembler) {
  // We cap {dst}, {src}, and {size} by {wasm::kV8MaxWasmTableSize + 1} to make
  // sure that the values fit into a Smi.
  STATIC_ASSERT(static_cast<size_t>(Smi::kMaxValue) >=
                wasm::kV8MaxWasmTableSize + 1);
  constexpr uint32_t kCap =
      static_cast<uint32_t>(wasm::kV8MaxWasmTableSize + 1);

  TNode<Uint32T> dst_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kDestination));
  TNode<Smi> dst = SmiFromUint32WithSaturation(dst_raw, kCap);

  TNode<Uint32T> src_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSource));
  TNode<Smi> src = SmiFromUint32WithSaturation(src_raw, kCap);

  TNode<Uint32T> size_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSize));
  TNode<Smi> size = SmiFromUint32WithSaturation(size_raw, kCap);

  TNode<Smi> dst_table =
      UncheckedCast<Smi>(Parameter(Descriptor::kDestinationTable));

  TNode<Smi> src_table =
      UncheckedCast<Smi>(Parameter(Descriptor::kSourceTable));

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TailCallRuntime(Runtime::kWasmTableCopy, context, instance, dst_table,
                  src_table, dst, src, size);
}

TF_BUILTIN(WasmAllocateArray, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Smi> map_index = CAST(Parameter(Descriptor::kMapIndex));
  TNode<Smi> length = CAST(Parameter(Descriptor::kLength));
  TNode<Smi> element_size = CAST(Parameter(Descriptor::kElementSize));
  TNode<FixedArray> maps_list = LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kManagedObjectMapsOffset);
  TNode<Map> map = CAST(LoadFixedArrayElement(maps_list, map_index));
  TNode<IntPtrT> untagged_length = SmiUntag(length);
  // instance_size = WasmArray::kHeaderSize
  //               + RoundUp(element_size * length, kObjectAlignment)
  TNode<IntPtrT> raw_size = IntPtrMul(SmiUntag(element_size), untagged_length);
  TNode<IntPtrT> rounded_size =
      WordAnd(IntPtrAdd(raw_size, IntPtrConstant(kObjectAlignmentMask)),
              IntPtrConstant(~kObjectAlignmentMask));
  TNode<IntPtrT> instance_size =
      IntPtrAdd(IntPtrConstant(WasmArray::kHeaderSize), rounded_size);
  TNode<WasmArray> result = UncheckedCast<WasmArray>(Allocate(instance_size));
  StoreMap(result, map);
  StoreObjectFieldNoWriteBarrier(result, WasmArray::kLengthOffset,
                                 TruncateIntPtrToInt32(untagged_length));
  Return(result);
}

TF_BUILTIN(WasmAllocateStruct, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Smi> map_index = CAST(Parameter(Descriptor::kMapIndex));
  TNode<FixedArray> maps_list = LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kManagedObjectMapsOffset);
  TNode<Map> map = CAST(LoadFixedArrayElement(maps_list, map_index));
  TNode<IntPtrT> instance_size =
      TimesTaggedSize(LoadMapInstanceSizeInWords(map));
  TNode<WasmStruct> result = UncheckedCast<WasmStruct>(Allocate(instance_size));
  StoreMap(result, map);
  Return(result);
}

}  // namespace internal
}  // namespace v8
