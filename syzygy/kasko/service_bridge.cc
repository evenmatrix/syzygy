// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "syzygy/kasko/service_bridge.h"

#include <Windows.h>  // NOLINT
#include <Rpc.h>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "syzygy/common/com_utils.h"
#include "syzygy/common/rpc/helpers.h"
#include "syzygy/kasko/minidump_request.h"
#include "syzygy/kasko/service.h"

namespace kasko {
namespace {
ServiceBridge* g_service_bridge = NULL;
}  // namespace
}  // namespace kasko

// RPC calls all come through this single free function. We use the singleton
// g_service_bridge to forward the call to the running Service.
boolean KaskoService_SendDiagnosticReport(handle_t IDL_handle,
                                          unsigned long exception_info_address,
                                          unsigned long thread_id,
                                          DumpType minidump_type,
                                          unsigned long protobuf_length,
                                          const signed char* protobuf,
                                          unsigned long crash_keys_size,
                                          const CrashKey* crash_keys) {
  DCHECK(kasko::g_service_bridge);

  base::ProcessId client_process_id =
      ::common::rpc::GetClientProcessID(IDL_handle);
  if (!client_process_id)
    return false;

  kasko::MinidumpRequest request;
  for (unsigned long i = 0; i < crash_keys_size; ++i) {
    if (!crash_keys[i].name || !crash_keys[i].value)
      continue;

    request.crash_keys.push_back(kasko::MinidumpRequest::CrashKey(
        crash_keys[i].name, crash_keys[i].value));
  }

  switch (minidump_type) {
    case SMALL_DUMP:
      request.type = kasko::MinidumpRequest::SMALL_DUMP_TYPE;
      break;
    case LARGER_DUMP:
      request.type = kasko::MinidumpRequest::LARGER_DUMP_TYPE;
      break;
    case FULL_DUMP:
      request.type = kasko::MinidumpRequest::FULL_DUMP_TYPE;
      break;
    default:
      NOTREACHED();
      break;
  }

  request.exception_info_address = exception_info_address;
  request.protobuf = reinterpret_cast<const char*>(protobuf);
  request.protobuf_length = protobuf_length;

  kasko::g_service_bridge->service_->SendDiagnosticReport(client_process_id,
                                                          thread_id, request);

  return true;
}

namespace kasko {

ServiceBridge::ServiceBridge(const base::string16& protocol,
                             const base::string16& endpoint,
                             scoped_ptr<Service> service)
    : protocol_(protocol),
      endpoint_(endpoint),
      service_(service.Pass()),
      running_(false) {
  // It's a bad idea to have two instances stepping on each other's toes.
  CHECK(!g_service_bridge);

  DCHECK(!protocol_.empty());
  DCHECK(!endpoint_.empty());
  DCHECK(service_);
  g_service_bridge = this;
}

ServiceBridge::~ServiceBridge() {
  // It's a bad idea to shut down without stopping the service. It's also a bad
  // idea to block unexpectedly in our destructor.
  CHECK(!running_);

  DCHECK_EQ(this, g_service_bridge);
  g_service_bridge = NULL;
}

bool ServiceBridge::Run() {
  if (running_) return true;

  RPC_STATUS status = ::RpcServerUseProtseqEp(
      common::rpc::AsRpcWstr(&protocol_[0]), RPC_C_LISTEN_MAX_CALLS_DEFAULT,
      common::rpc::AsRpcWstr(&endpoint_[0]), NULL /* Security descriptor. */);

  // RPC_S_DUPLICATE_ENDPOINT seems to be possible if a previous instance has
  // already registered this protocol and endpoint. The end result is still that
  // the endpoint is properly configured for this protocol.
  if (status != RPC_S_OK && status != RPC_S_DUPLICATE_ENDPOINT) {
    LOG(ERROR) << "Failed to init RPC protocol: " << ::common::LogWe(status)
               << ".";
  } else {
    scoped_ptr<common::rpc::ScopedRpcInterfaceRegistration>
        interface_registration(new common::rpc::ScopedRpcInterfaceRegistration(
            KaskoService_Kasko_v1_0_s_ifspec));

    if (interface_registration->status() == RPC_S_OK) {
      status = ::RpcServerListen(1,  // Minimum number of handler threads.
                                 RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);

      if (status != RPC_S_OK) {
        LOG(ERROR) << "Failed to run RPC server: " << ::common::LogWe(status)
                   << ".";
      } else {
        running_ = true;
        interface_registration_ = interface_registration.Pass();
      }
    }
  }

  return running_;
}

void ServiceBridge::Stop() {
  if (!running_) return;

  // This call prevents new requests from being accepted.
  RPC_STATUS status = ::RpcMgmtStopServerListening(NULL);
  if (status != RPC_S_OK) {
    // If this fails, we could end up servicing calls in a bad state.
    LOG(FATAL) << "Failed to stop the RPC server: " << ::common::LogWe(status)
               << ".";
  }

  // This call will block until all active requests are completed.
  status = ::RpcMgmtWaitServerListen();
  if (status != RPC_S_OK) {
    // If this fails, we could end up servicing calls in a bad state.
    LOG(FATAL) << "Failed to wait for RPC server shutdown: "
               << ::common::LogWe(status) << ".";
  }

  interface_registration_.reset();
  running_ = false;
}

}  // namespace kasko
