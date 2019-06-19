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
#include "glow/Runtime/Provisioner/Provisioner.h"
#include "../../lib/Backends/CPU/CPUDeviceManager.h"

#include "gtest/gtest.h"

using namespace glow;
using namespace glow::runtime;

class ProvisionerTest : public ::testing::Test {};

std::unique_ptr<Module> setupModule(unsigned functionCount) {
  auto mod = llvm::make_unique<Module>();
  for (unsigned int i = 0; i < functionCount; i++) {
    auto *F = mod->createFunction("function" + std::to_string(i));
    auto *X = mod->createPlaceholder(ElemKind::FloatTy, {16, 1024}, "X", false);
    auto *W = mod->createConstant(ElemKind::FloatTy, {1024, 1024}, "W");
    auto *B = mod->createConstant(ElemKind::FloatTy, {1024}, "B");
    auto *FC = F->createFullyConnected("FC", X, W, B);
    F->createSave("save", FC);
    CompilationContext cctx;
    lower(F, cctx);
  }
  return mod;
}

DAGListTy setupSchedule(unsigned rootCount, unsigned childCount) {
  DAGListTy partitions;
  unsigned nodeIdx = 0;
  for (unsigned root = 0; root < rootCount; root++) {
    Schedule schedule("root" + std::to_string(root));
    schedule.addTask("function" + std::to_string(nodeIdx++), "CPU", {0, 1});
    for (unsigned child = 1; child <= childCount; child++) {
      unsigned taskIdx =
          schedule.addTask("function" + std::to_string(nodeIdx++), "CPU", {0});
      EXPECT_EQ(child, taskIdx);
      schedule.addChild(0, taskIdx);
    }

    partitions.push_back(schedule);
  }
  return partitions;
}

TEST_F(ProvisionerTest, provisionDag) {
  auto mod = setupModule(6);
  auto networks = setupSchedule(2, 0);

  DeviceManagerMapTy devices;
  for (int i = 0; i < 6; i++) {
    std::unique_ptr<DeviceManager> device(
        new CPUDeviceManager(DeviceConfig("CPU")));
    devices.emplace(i, std::move(device));
  }

  CompilationContext cctx;
  auto provisioner = Provisioner(devices);
  auto err = provisioner.provision(networks, *mod.get(), cctx);
  // Expect that there was no Error when provisioning
  EXPECT_FALSE(errToBool(std::move(err)));
}

TEST_F(ProvisionerTest, provisionDagFail) {
  auto mod = setupModule(6);
  auto networks = setupSchedule(2, 0);

  DeviceManagerMapTy devices;
  for (int i = 0; i < 6; i++) {
    auto config = DeviceConfig("CPU");
    config.setDeviceMemory(1000);
    std::unique_ptr<DeviceManager> device(new CPUDeviceManager(config));
    devices.emplace(i, std::move(device));
  }

  CompilationContext cctx;
  auto provisioner = Provisioner(devices);
  auto err = provisioner.provision(networks, *mod.get(), cctx);
  // Expect that there was no Error when provisioning
  EXPECT_TRUE(errToBool(std::move(err)));
}
