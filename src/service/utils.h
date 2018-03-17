// Copyright (c) 2015-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define UNUSED_FUNC __attribute__((unused))
#define UNUSED_VAR __attribute__((unused))

#include <string>
#include <sstream>
#include <vector>

std::vector<std::string> &splitString(const std::string &s, char delim,
                                      std::vector<std::string> &elems);
std::vector<std::string> splitString(const std::string &s, char delim);
