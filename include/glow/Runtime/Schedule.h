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
#ifndef GLOW_RUNTIME_SCHEDULE_H
#define GLOW_RUNTIME_SCHEDULE_H

#include "glow/Graph/Graph.h"
#include "glow/Runtime/RuntimeTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace glow {
namespace runtime {

struct Schedule {
  Schedule(std::string f) : functionName_(f) {}

  std::string functionName_;

  // Module that was used to create this network. Everything except
  // placeholders and types have been removed from it.
  std::shared_ptr<Module> module_;

  struct Task {
    Task(Schedule *s, std::string n, std::string b, std::vector<DeviceIDTy> d)
        : name(std::move(n)), backendName(std::move(b)),
          logicalDevices(std::move(d)), parent(s) {}
    std::string name;
    std::string backendName;
    std::vector<DeviceIDTy> logicalDevices;
    std::vector<DeviceIDTy> devices;
    std::shared_ptr<RuntimeBundle> runtimeBundle;

    const Schedule *parent;

    std::vector<size_t> parents;
    std::vector<size_t> children;
  };

  std::vector<Task> tasks_;
  std::vector<Task> &tasks() { return tasks_; }
  const std::vector<Task> &tasks() const { return tasks_; }

  size_t addTask(std::string name, std::string backendName,
                 llvm::ArrayRef<DeviceIDTy> logicalDevices) {
    Task task(this, std::move(name), std::move(backendName), logicalDevices);
    tasks_.push_back(std::move(task));
    return tasks_.size() - 1;
  }

  void addChild(size_t parentIndex, size_t childIndex) {
    assert(parentIndex < tasks_.size() && childIndex < tasks_.size());
    tasks_[parentIndex].children.push_back(childIndex);
    tasks_[childIndex].parents.push_back(parentIndex);
  }

  Schedule clone() { return Schedule(*this); }
};

/// This list contains all the created DAGNodes from the Partitioner. The
/// contained DAGNodes can only refer to the DAGNodes from the same DAGListTy.
using DAGListTy = std::vector<Schedule>;

} // namespace runtime
} // namespace glow

#endif
