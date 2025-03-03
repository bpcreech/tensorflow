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

#ifndef XLA_SERVICE_GPU_RUNTIME3_CONDITIONAL_THUNK_H_
#define XLA_SERVICE_GPU_RUNTIME3_CONDITIONAL_THUNK_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/gpu/buffer_allocations.h"
#include "xla/service/gpu/runtime3/sequential_thunk.h"
#include "xla/service/gpu/thunk.h"
#include "xla/status.h"
#include "xla/stream_executor/stream_executor.h"

namespace xla {
namespace gpu {

struct ConditionalThunkConfig {
  bool branch_index_is_bool;
  int64_t branch_count;
  std::vector<std::unique_ptr<SequentialThunk>> branch_thunks;
};

// ConditionalThunk implements the conditional instruction on GPU by reading the
// predicate of the conditional and executing the true or the false computation
// depending on the value of the predicate.
//
// ConditionalThunk assumes that the buffers of the conditional result and the
// result of the true and false computations share the same allocation. Also,
// the buffers of the true operand of the conditional and that of the parameter
// instruction of the true computation share the same allocation. Similarly, the
// buffers of the false operand and that of the parameter instruction of the
// false computation share the same allocation.
class ConditionalThunk : public Thunk {
 public:
  ConditionalThunk(ThunkInfo thunk_info, ConditionalThunkConfig config,
                   const BufferAllocation::Slice& branch_index_buffer_index);

  ConditionalThunk(const ConditionalThunk&) = delete;
  ConditionalThunk& operator=(const ConditionalThunk&) = delete;

  absl::Status Initialize(const InitializeParams& params) override;
  absl::Status ExecuteOnStream(const ExecuteParams& params) override;

  absl::Span<const std::unique_ptr<SequentialThunk>> branch_thunks() const {
    return config_.branch_thunks;
  }

  const BufferAllocation::Slice& branch_index_buffer() const {
    return branch_index_buffer_index_;
  }

 private:
  const ConditionalThunkConfig config_;
  const BufferAllocation::Slice branch_index_buffer_index_;
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_RUNTIME3_CONDITIONAL_THUNK_H_
