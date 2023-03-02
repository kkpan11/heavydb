
/*
 * Copyright 2022 HEAVY.AI, Inc.
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

/**
 * @file		Datum.h
 * @brief	 Definitions for core Datum union type
 *
 */

#pragma once

#include "funcannotations.h"

#ifndef __CUDACC__
#include <string_view>
#endif

#ifndef __CUDACC__
static_assert(!std::is_trivial<std::string_view>::value);
#endif

// Since std::string_view is not a Trivial class, we use StringView instead,
// which is both Trivial and has Standard-layout (aka POD).
// This is like std::string_view but can be used in the context of cuda and llvm.
struct StringView {
  char const* ptr_;
  uint64_t len_;

#ifndef __CUDACC__
  std::string_view stringView() const {
    return {ptr_, len_};
  }
#endif
};

static_assert(sizeof(char) == sizeof(int8_t));
static_assert(std::is_standard_layout<StringView>::value);
static_assert(std::is_trivial<StringView>::value);

struct VarlenDatum {
  size_t length;
  int8_t* pointer;
  bool is_null;

  DEVICE VarlenDatum() : length(0), pointer(nullptr), is_null(true) {}
  DEVICE virtual ~VarlenDatum() {}

  VarlenDatum(const size_t l, int8_t* p, const bool n)
      : length(l), pointer(p), is_null(n) {}
};

static_assert(!std::is_standard_layout<VarlenDatum>::value);
static_assert(!std::is_trivial<VarlenDatum>::value);

union Datum {
  int8_t boolval;
  int8_t tinyintval;
  int16_t smallintval;
  int32_t intval;
  int64_t bigintval;
  float floatval;
  double doubleval;
  VarlenDatum* arrayval;
#ifndef __CUDACC__
  std::string* stringval;  // string value
#endif
};
