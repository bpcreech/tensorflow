/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// LLVM-based compiler backend.
#ifndef XLA_SERVICE_GPU_LLVM_GPU_BACKEND_GPU_BACKEND_LIB_H_
#define XLA_SERVICE_GPU_LLVM_GPU_BACKEND_GPU_BACKEND_LIB_H_

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"
#include "xla/statusor.h"
#include "xla/stream_executor/device_description.h"
#include "xla/types.h"
#include "xla/xla.pb.h"

namespace xla {
namespace gpu {

namespace nvptx {

std::string CantFindCudaMessage(absl::string_view msg,
                                absl::string_view xla_gpu_cuda_data_dir);

// Get path to NVVM libdevice file.
std::string LibDevicePath(absl::string_view xla_gpu_cuda_data_dir);

// Link libdevice if functions using it are detected in the module.
absl::Status LinkLibdeviceIfNecessary(llvm::Module* module,
                                      const std::string& libdevice_path);

// Compiles the argument module and returns it. libdevice_dir_path is the parent
// directory of the libdevice bitcode libraries. The contents of the module may
// be changed.
//
// The Compile.* interfaces each create their own llvm::LLVMContext objects for
// thread safety, but note that LLVM's multithreaded support is very
// preliminary; multithreaded use is not recommended at this time.
absl::StatusOr<std::string> CompileToPtx(
    llvm::Module* module, se::GpuComputeCapability gpu_version,
    const DebugOptions& debug_options,
    std::function<void(llvm::TargetMachine*)> configure_target = nullptr);
}  // namespace nvptx

namespace amdgpu {
// Compiles the argument module and returns it with LLVM AMDGPU backend.
// rocdl_dir_path is the parent directory of ROCm-Device-Libs bitcode libraries.
// The contents of the module may be changed.
absl::StatusOr<std::vector<uint8_t>> CompileToHsaco(
    llvm::Module* module, se::GpuComputeCapability gpu_version,
    const DebugOptions& debug_options,
    const std::string& module_config_cache_key);
}  // namespace amdgpu

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_LLVM_GPU_BACKEND_GPU_BACKEND_LIB_H_
