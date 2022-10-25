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

#pragma once

#include <cstdint>
#include <unordered_map>

#include "Shared/DbObjectKeys.h"

struct TableGeneration {
  int64_t tuple_count;
  int64_t start_rowid;
};

class TableGenerations {
 public:
  void setGeneration(const shared::TableKey& table_key,
                     const TableGeneration& generation);

  const TableGeneration& getGeneration(const shared::TableKey& table_key) const;

  const std::unordered_map<shared::TableKey, TableGeneration>& asMap() const;

  void clear();

 private:
  std::unordered_map<shared::TableKey, TableGeneration> table_key_to_generation_;
};
