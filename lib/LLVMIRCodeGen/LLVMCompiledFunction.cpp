/**
 * Copyright (c) 2017-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "glow/LLVMIRCodeGen/LLVMCompiledFunction.h"

#include "glow/Graph/PlaceholderBindings.h"
#include "glow/Support/Compiler.h"
#include "glow/Support/Memory.h"

using namespace glow;

LLVMCompiledFunction::LLVMCompiledFunction(
    std::unique_ptr<llvm::orc::GlowJIT> JIT,
    const runtime::RuntimeBundle &runtimeBundle)
    : CompiledFunction(runtimeBundle), JIT_(std::move(JIT)) {}

LLVMCompiledFunction::~LLVMCompiledFunction() { tearDownRuns(); }

void LLVMCompiledFunction::collectConstants(Module *module) {
  runtimeBundle_.collectConstants(module);
}

void LLVMCompiledFunction::loadPlaceholders(
    PlaceholderBindings *bindings, uint8_t *baseMutableWeightVarsAddress) {
  // Copy Placeholders into allocated memory.
  auto &symbolTable = runtimeBundle_.getSymbolTable();
  for (auto PH : bindings->pairs()) {
    auto it = symbolTable.find(PH.first->getName());
    if (it == symbolTable.end()) {
      continue;
    }
    auto symbolInfo = it->second;
    auto payload = PH.second->getUnsafePtr();
    auto addr = symbolInfo.offset;
    auto numBytes = symbolInfo.size;
    // copy PH to allocated memory.
    memcpy(baseMutableWeightVarsAddress + addr, payload, numBytes);
  }
}

void LLVMCompiledFunction::updatePlaceholders(
    PlaceholderBindings *bindings, uint8_t *baseMutableWeightVarsAddress) {
  // Copy placeholders from device back into bindings.
  auto &symbolTable = runtimeBundle_.getSymbolTable();
  for (auto PH : bindings->pairs()) {
    auto it = symbolTable.find(PH.first->getName());
    if (it == symbolTable.end()) {
      continue;
    }
    auto symbolInfo = it->second;
    auto payload = baseMutableWeightVarsAddress + symbolInfo.offset;
    auto numBytes = symbolInfo.size;
    auto addr = PH.second->getUnsafePtr();
    // copy PH from allocated memory.
    memcpy(addr, payload, numBytes);
  }
}

void LLVMCompiledFunction::execute(ExecutionContext *context) {
  uint8_t *baseActivationsAddress{nullptr};

  /// Base address for Mutable weights memory block, Inputs and Outputs.
  uint8_t *baseMutableWeightVarsAddress{nullptr};

  if (runtimeBundle_.getActivationsSize() != 0) {
    baseActivationsAddress = (uint8_t *)alignedAlloc(
        runtimeBundle_.getActivationsSize(), TensorAlignment);
  }

  if (runtimeBundle_.getMutableWeightSize() != 0) {
    baseMutableWeightVarsAddress = (uint8_t *)alignedAlloc(
        runtimeBundle_.getMutableWeightSize(), TensorAlignment);
  }

  loadPlaceholders(context->getPlaceholderBindings(),
                   baseMutableWeightVarsAddress);

  auto sym = JIT_->findSymbol("jitmain");
  assert(sym && "Unable to JIT the code!");
  using JitFuncType =
      void (*)(uint8_t * constantWeightVars, uint8_t * mutableWeightVars,
               uint8_t * activations);
  auto address = sym.getAddress();
  if (address) {
    JitFuncType funcPtr = reinterpret_cast<JitFuncType>(address.get());
    funcPtr(runtimeBundle_.getConstants(), baseMutableWeightVarsAddress,
            baseActivationsAddress);
  } else {
    GLOW_UNREACHABLE("Error getting address");
  }

  updatePlaceholders(context->getPlaceholderBindings(),
                     baseMutableWeightVarsAddress);

  alignedFree(baseMutableWeightVarsAddress);
  alignedFree(baseActivationsAddress);

  translateTraceEvents(context);
}

void LLVMCompiledFunction::translateTraceEvents(
    ExecutionContext *context) const {
  auto &traceInfo = getTraceInfo();
  if (!traceInfo.enabled) {
    return;
  }

  PlaceholderBindings *bindings = context->getPlaceholderBindings();

  int tid = 0;
  for (auto &backing : traceInfo.events) {
    tid++;
    Tensor *backingTensor = bindings->get(backing.first);
    assert(backingTensor);

    auto &traceEvents = context->getTraceEvents();
    for (const TraceInfo::Event &event : backing.second) {
      uint64_t ts{0};
      memcpy(&ts,
             backingTensor->getUnsafePtr() + (event.index * traceInfo.dataSize),
             traceInfo.dataSize);
      traceEvents.push_back({event.name, ts, event.type, tid});
    }
  }
}
