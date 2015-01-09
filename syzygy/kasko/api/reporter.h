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

#ifndef SYZYGY_KASKO_API_REPORTER_H_
#define SYZYGY_KASKO_API_REPORTER_H_

#include "base/strings/string16.h"
#include "syzygy/kasko/api/kasko_export.h"

namespace kasko {
namespace api {

// Initializes the Kasko reporter process. Must be matched by a call to
// ShutdownReporter.
// @param endpoint_name The endpoint name that will be used by the Kasko RPC
//     service.
// @param url The URL that will be used for uploading crash reports.
// @param data_directory The directory where crash reports will be queued until
//     uploaded.
// @param permanent_failure_directory The location where reports will be stored
//     once the maximum number of upload attempts has been exceeded.
// @returns true if successful.
KASKO_EXPORT bool InitializeReporter(
    const base::char16* endpoint_name,
    const base::char16* url,
    const base::char16* data_directory,
    const base::char16* permanent_failure_directory);

// Shuts down the Kasko reporter process. Must only be called after a successful
// invocation of InitializeReporter.
void ShutdownReporter();

}  // namespace api
}  // namespace kasko

#endif  // SYZYGY_KASKO_API_REPORTER_H_