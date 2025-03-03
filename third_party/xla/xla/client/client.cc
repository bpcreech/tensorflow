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

#include "xla/client/client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "xla/client/xla_computation.h"
#include "xla/debug_options_flags.h"
#include "xla/execution_options_util.h"
#include "xla/literal.h"
#include "xla/status_macros.h"
#include "xla/types.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/logging.h"
#include "tsl/platform/protobuf.h"

namespace xla {

Client::Client(ServiceInterface* stub) : stub_(stub) {}

Client::~Client() = default;

StatusOr<Literal> Client::Transfer(const GlobalData& data,
                                   const Shape* shape_with_layout) {
  TransferToClientRequest request;
  *request.mutable_data() = data.handle();
  if (shape_with_layout != nullptr) {
    *request.mutable_shape_with_layout() = shape_with_layout->ToProto();
  }
  TransferToClientResponse response;

  VLOG(1) << "making transfer request";
  VLOG(3) << "TransferToClientRequest: {" << request.DebugString() << "}";
  Status s = stub_->TransferToClient(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  VLOG(3) << "TransferToClientResponse: {" << response.DebugString() << "}";

  if (!response.has_literal()) {
    return FailedPrecondition(
        "server provided response without a literal in "
        "TransferToClient request");
  }
  return Literal::CreateFromProto(*response.mutable_literal());
}

StatusOr<std::unique_ptr<GlobalData>> Client::TransferToServer(
    const LiteralSlice& literal, const DeviceHandle* device_handle) {
  TransferToServerRequest request;
  *request.mutable_literal() = literal.ToProto();
  if (device_handle) {
    *request.mutable_device_handle() = *device_handle;
  }
  TransferToServerResponse response;

  VLOG(1) << "making transfer to server request";
  VLOG(3) << "TransferToServerRequest: {" << request.DebugString() << "}";
  Status s = stub_->TransferToServer(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  VLOG(3) << "TransferToServerResponse: {" << response.DebugString() << "}";

  if (!response.has_data()) {
    return FailedPrecondition(
        "server provided response without a data handle in "
        "TransferToServer request");
  }

  return std::make_unique<GlobalData>(stub_, response.data());
}

Status Client::TransferToInfeed(const LiteralSlice& literal, int64_t replica_id,
                                const DeviceHandle* device_handle) {
  TransferToInfeedRequest request;
  *request.mutable_literal() = literal.ToProto();
  if (device_handle) {
    *request.mutable_device_handle() = *device_handle;
  }
  request.set_replica_id(replica_id);
  TransferToInfeedResponse response;

  VLOG(1) << "making transfer to infeed request";
  VLOG(3) << "TransferToInfeedRequest: {" << request.DebugString() << "}";
  Status s = stub_->TransferToInfeed(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  VLOG(3) << "TransferToInfeedResponse: {" << response.DebugString() << "}";
  return OkStatus();
}

StatusOr<Literal> Client::TransferFromOutfeed(
    const Shape* shape_with_layout, int64_t replica_id,
    const DeviceHandle* device_handle) {
  TransferFromOutfeedRequest request;
  if (device_handle) {
    *request.mutable_device_handle() = *device_handle;
  }
  request.set_replica_id(replica_id);
  if (shape_with_layout != nullptr) {
    *request.mutable_shape_with_layout() = shape_with_layout->ToProto();
  }
  TransferFromOutfeedResponse response;

  VLOG(1) << "making transfer from outfeed request";
  VLOG(3) << "TransferFromOutfeedRequest: {" << request.DebugString() << "}";
  Status s = stub_->TransferFromOutfeed(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  VLOG(3) << "TransferFromOutfeedResponse: {" << response.DebugString() << "}";

  if (!response.has_literal()) {
    return FailedPrecondition(
        "server provided response without a literal in "
        "TransferToClient request");
  }

  return Literal::CreateFromProto(response.literal());
}

Status Client::ResetDevice() {
  ResetDeviceRequest request;
  ResetDeviceResponse response;

  VLOG(1) << "making reset device request";
  VLOG(3) << "ResetDeviceRequest: {" << request.DebugString() << "}";
  Status s = stub_->ResetDevice(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  VLOG(3) << "ResetDeviceResponse: {" << response.DebugString() << "}";
  return OkStatus();
}

StatusOr<Literal> Client::ExecuteAndTransfer(
    const XlaComputation& computation, absl::Span<GlobalData* const> arguments,
    const ExecutionOptions* execution_options,
    ExecutionProfile* execution_profile) {
  TF_ASSIGN_OR_RETURN(
      std::unique_ptr<GlobalData> data,
      Execute(computation, arguments, execution_options, execution_profile));

  std::optional<Shape> shape_with_output_layout;
  if (execution_options && execution_options->has_shape_with_output_layout()) {
    shape_with_output_layout =
        Shape(execution_options->shape_with_output_layout());
  }
  return Transfer(*data, shape_with_output_layout.has_value()
                             ? &(*shape_with_output_layout)
                             : nullptr);
}

StatusOr<Literal> Client::ComputeConstant(const XlaComputation& computation,
                                          const Layout* output_layout) const {
  ComputeConstantGraphRequest request;
  *request.mutable_computation() = computation.proto();
  if (output_layout != nullptr) {
    *request.mutable_output_layout() = output_layout->ToProto();
  }

  ComputeConstantResponse response;

  VLOG(2) << "making compute-constant-graph request";
  Status s = stub_->ComputeConstantGraph(&request, &response);
  VLOG(2) << "done with request";

  if (!s.ok()) {
    return s;
  }

  VLOG(3) << "ComputeConstant: {" << response.DebugString() << "}";

  if (!response.has_literal()) {
    return Internal(
        "no computed literal in the provided response in ComputeConstantGraph "
        "request");
  }
  return Literal::CreateFromProto(response.literal());
}

StatusOr<XlaComputation> Client::LoadSnapshot(const HloSnapshot& module) {
  TF_RET_CHECK(module.has_hlo() && module.hlo().has_hlo_module());
  return XlaComputation(module.hlo().hlo_module());
}

StatusOr<ExecutionHandle> Client::Compile(
    const XlaComputation& computation, absl::Span<const Shape> argument_shapes,
    const ExecutionOptions* execution_options) {
  CompileRequest request;
  *request.mutable_computation() = computation.proto();

  if (execution_options == nullptr) {
    *request.mutable_execution_options() = CreateDefaultExecutionOptions();
  } else {
    *request.mutable_execution_options() = *execution_options;
  }
  if (request.execution_options().device_handles_size() > 1) {
    return InvalidArgument(
        "Compiling with multiple device handles is not supported. Use "
        "'Execute' instead.");
  }

  // The argument shapes affect how the computation is compiled.
  for (const auto& arg_shape : argument_shapes) {
    *request.add_input_shape_with_layout() = arg_shape.ToProto();
  }

  CompileResponse response;
  VLOG(1) << "making compile request: " << request.ShortDebugString();
  Status s = stub_->Compile(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  TF_RET_CHECK(response.has_handle());
  return response.handle();
}

StatusOr<std::unique_ptr<GlobalData>> Client::Execute(
    const ExecutionHandle& handle, absl::Span<GlobalData* const> arguments,
    ExecutionProfile* execution_profile) {
  ExecuteRequest request;
  *request.mutable_handle() = handle;
  for (GlobalData* argument : arguments) {
    CHECK(argument != nullptr) << "Argument pointers must not be null.";
    *request.add_arguments() = argument->handle();
  }

  ExecuteResponse response;
  VLOG(1) << "making execute request: " << request.ShortDebugString();
  Status s = stub_->Execute(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  if (execution_profile != nullptr) {
    *execution_profile = response.profile();
  }

  return std::make_unique<GlobalData>(stub_, response.output());
}

StatusOr<std::unique_ptr<GlobalData>> Client::Execute(
    const XlaComputation& computation, absl::Span<GlobalData* const> arguments,
    const ExecutionOptions* execution_options,
    ExecutionProfile* execution_profile) {
  // Create an ExecutionOptions if necessary, or set its DeviceHandles.
  std::optional<ExecutionOptions> options_storage;
  if (!execution_options || execution_options->device_handles().empty()) {
    if (execution_options) {
      options_storage.emplace(*execution_options);
    } else {
      options_storage.emplace(CreateDefaultExecutionOptions());
    }
    execution_options = &*options_storage;

    TF_ASSIGN_OR_RETURN(auto device_handles,
                        GetDeviceHandles(/*device_count=*/1));
    TF_RET_CHECK(!device_handles.empty());
    *options_storage->add_device_handles() = std::move(device_handles[0]);
  }

  std::vector<XlaComputationInstance> computation_instances = {
      XlaComputationInstance{
          computation,
          std::vector<GlobalData*>(arguments.begin(), arguments.end()),
          *execution_options, execution_profile}};

  // Instead of invoking Compile() and Execute(), invoke
  // Service::ExecuteParallel() to execute our one computation.  Compile()
  // caches the executable forever, which isn't what we want.
  VLOG(1) << "Making ExecuteParallel request: "
          << execution_options->DebugString();
  TF_ASSIGN_OR_RETURN(auto results, ExecuteParallel(computation_instances));
  VLOG(1) << "ExecuteParallel request done.";

  // The result selection is a bit hacky, but better than assuming it is
  // device 0.
  //
  // TODO(b/118493728): Allow Execute to return one result per computation.
  for (int64_t i = 0, end = results.size(); i < end; i++) {
    TF_ASSIGN_OR_RETURN(const Shape& shape, GetShape(*results[i]));
    if (!ShapeUtil::IsEmptyTuple(shape)) {
      VLOG(3) << "Fetching result from device " << i << ": "
              << ShapeUtil::HumanString(shape);
      return std::move(results[i]);
    }
  }
  TF_RET_CHECK(!results.empty());
  VLOG(1) << "Defaulting to device 0 result";
  return std::move(results[0]);
}

StatusOr<std::vector<std::unique_ptr<GlobalData>>> Client::ExecuteParallel(
    absl::Span<const XlaComputationInstance> computations) {
  ExecuteGraphParallelRequest request;

  for (const XlaComputationInstance& computation : computations) {
    ExecuteGraphRequest single_request;
    *single_request.mutable_computation() = computation.computation.proto();
    for (GlobalData* argument : computation.arguments) {
      *single_request.add_arguments() = argument->handle();
    }
    *single_request.mutable_execution_options() = computation.execution_options;
    *request.add_requests() = single_request;
  }

  ExecuteParallelResponse response;
  VLOG(1) << "making execute-graph-parallel request: "
          << request.ShortDebugString();
  Status s = stub_->ExecuteGraphParallel(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  std::vector<std::unique_ptr<GlobalData>> outputs;
  for (size_t i = 0, end = response.responses_size(); i < end; ++i) {
    outputs.push_back(
        std::make_unique<GlobalData>(stub_, response.responses(i).output()));
    if (i < computations.size() &&
        computations[i].execution_profile != nullptr) {
      *computations[i].execution_profile = response.responses(i).profile();
    }
  }

  return std::move(outputs);
}

StatusOr<std::vector<DeviceHandle>> Client::GetDeviceHandles(
    int64_t device_count) {
  if (device_count < 1) {
    return InvalidArgument("device_count must be greater than 0");
  }
  GetDeviceHandlesRequest request;
  request.set_device_count(device_count);

  GetDeviceHandlesResponse response;
  VLOG(1) << "making get device request: " << request.ShortDebugString();
  Status s = stub_->GetDeviceHandles(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  std::vector<DeviceHandle> device_handles;
  const auto& response_device_handles = response.device_handles();
  device_handles.reserve(response_device_handles.size());
  for (const DeviceHandle& device_handle : response_device_handles) {
    device_handles.push_back(device_handle);
  }

  return device_handles;
}

Status Client::Unregister(const GlobalData& data) {
  UnregisterRequest request;
  *request.add_data() = data.handle();
  UnregisterResponse response;

  VLOG(1) << "making unregister request";
  Status s = stub_->Unregister(&request, &response);
  VLOG(1) << "done with request";

  return s;
}

StatusOr<std::vector<std::unique_ptr<GlobalData>>> Client::DeconstructTuple(
    const GlobalData& data) {
  DeconstructTupleRequest request;
  *request.mutable_tuple_handle() = data.handle();
  DeconstructTupleResponse response;

  VLOG(1) << "making DestructTuple request";
  Status s = stub_->DeconstructTuple(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  std::vector<std::unique_ptr<GlobalData>> handles;
  for (auto& handle : response.element_handles()) {
    handles.push_back(std::make_unique<GlobalData>(stub_, handle));
  }
  return std::move(handles);
}

StatusOr<ComputationStats> Client::GetComputationStats(
    const XlaComputation& computation,
    const DebugOptions& debug_options) const {
  ComputationGraphStatsRequest request;

  // TODO(b/74197823): Find a way to avoid the copy of the hlo proto.
  *request.mutable_computation() = computation.proto();
  *request.mutable_debug_options() = debug_options;
  ComputationStatsResponse response;

  VLOG(1) << "making computation graph stats request";
  Status s = stub_->GetComputationGraphStats(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }
  CHECK(response.has_stats());
  return response.stats();
}

StatusOr<std::unique_ptr<ProgramShape>> Client::GetComputationShape(
    const XlaComputation& computation) {
  TF_ASSIGN_OR_RETURN(const auto& result, computation.GetProgramShape());
  return std::make_unique<ProgramShape>(result);
}

StatusOr<Shape> Client::GetShape(const GlobalData& data) {
  GetShapeRequest request;
  *request.mutable_data() = data.handle();
  GetShapeResponse response;

  VLOG(1) << "making get shape request";
  Status s = stub_->GetShape(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  return Shape(response.shape());
}

StatusOr<std::string> Client::ExecutionStatsAsString(
    const XlaComputation& computation, const ExecutionProfile& profile) {
  TF_ASSIGN_OR_RETURN(
      auto computation_stats,
      GetComputationStats(computation, GetDebugOptionsFromFlags()));
  int64_t total_flops =
      computation_stats.flop_count() + computation_stats.transcendental_count();
  if (profile.compute_time_ns() > 0) {
    int64_t nanoseconds = profile.compute_time_ns();
    int64_t cycle_count = profile.compute_cycle_count();
    double gflops = total_flops / nanoseconds;
    return absl::StrCat(
        "[Execution Statistics] flop count: ", computation_stats.flop_count(),
        ", transcendental count: ", computation_stats.transcendental_count(),
        ", compute execution time: ", nanoseconds, " nsec",
        ", compute cycles: ", cycle_count, ", performance: ", gflops,
        "gflop/s");
  }
  return std::string("[Execution Statistics] not available.");
}

StatusOr<ChannelHandle> Client::CreateChannelHandleByType(
    ChannelHandle::ChannelType type) {
  CreateChannelHandleRequest request;
  request.set_channel_type(type);
  CreateChannelHandleResponse response;

  VLOG(1) << "making create channel handle request";
  Status s = stub_->CreateChannelHandle(&request, &response);
  VLOG(1) << "done with request";

  if (!s.ok()) {
    return s;
  }

  return response.channel();
}

StatusOr<ChannelHandle> Client::CreateChannelHandle() {
  return CreateChannelHandleByType(ChannelHandle::DEVICE_TO_DEVICE);
}

StatusOr<ChannelHandle> Client::CreateHostToDeviceChannelHandle() {
  return CreateChannelHandleByType(ChannelHandle::HOST_TO_DEVICE);
}

StatusOr<ChannelHandle> Client::CreateDeviceToHostChannelHandle() {
  return CreateChannelHandleByType(ChannelHandle::DEVICE_TO_HOST);
}

}  // namespace xla
