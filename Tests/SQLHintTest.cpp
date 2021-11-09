/*
 * Copyright 2020 OmniSci, Inc.
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

#include "TestHelpers.h"

#include <gtest/gtest.h>

#include "Catalog/Catalog.h"
#include "Catalog/DBObject.h"
#include "DBHandlerTestHelpers.h"
#include "QueryEngine/Execute.h"
#include "QueryEngine/QueryHint.h"
#include "QueryRunner/QueryRunner.h"

namespace po = boost::program_options;

#ifndef BASE_PATH
#define BASE_PATH "./tmp"
#endif

using namespace Catalog_Namespace;

extern bool g_enable_table_functions;
extern bool g_enable_test_table_functions;

using QR = QueryRunner::QueryRunner;

bool skip_tests(const ExecutorDeviceType device_type) {
#ifdef HAVE_CUDA
  return device_type == ExecutorDeviceType::GPU && !(QR::get()->gpusPresent());
#else
  return device_type == ExecutorDeviceType::GPU;
#endif
}

#define SKIP_NO_GPU()                                        \
  if (skip_tests(dt)) {                                      \
    CHECK(dt == ExecutorDeviceType::GPU);                    \
    LOG(WARNING) << "GPU not available, skipping GPU tests"; \
    continue;                                                \
  }

bool approx_eq(const double v, const double target, const double eps = 0.01) {
  const auto v_u64 = *reinterpret_cast<const uint64_t*>(may_alias_ptr(&v));
  const auto target_u64 = *reinterpret_cast<const uint64_t*>(may_alias_ptr(&target));
  return v_u64 == target_u64 || (target - eps < v && v < target + eps);
}

inline void run_ddl_statement(const std::string& create_table_stmt) {
  QR::get()->runDDLStatement(create_table_stmt);
}

std::shared_ptr<ResultSet> run_query(const std::string& query_str,
                                     const ExecutorDeviceType device_type) {
  return QR::get()->runSQL(query_str, device_type, true, true);
}

std::pair<std::shared_ptr<HashTable>, std::optional<RegisteredQueryHint>>
getCachedHashTable(std::set<QueryPlanHash>& already_visited,
                   CacheItemType cache_item_type) {
  auto cached_ht = QR::get()->getCachedHashtableWithoutCacheKey(
      already_visited, cache_item_type, 0 /* CPU_DEVICE_IDENTIFIER*/);
  auto cache_key = std::get<0>(cached_ht);
  already_visited.insert(cache_key);
  return std::make_pair(std::get<1>(cached_ht),
                        std::get<2>(cached_ht)->registered_query_hint);
}

void createTable() {
  QR::get()->runDDLStatement(
      "CREATE TABLE SQL_HINT_DUMMY(key int, ts1 timestamp(0) encoding fixed(32), ts2 "
      "timestamp(0) encoding fixed(32), str1 TEXT ENCODING DICT(16));");
  QR::get()->runDDLStatement(
      "CREATE TABLE geospatial_test(id INT, p POINT, l LINESTRING, poly POLYGON);");
  QR::get()->runDDLStatement(
      "CREATE TABLE geospatial_inner_join_test(id INT, p POINT, l LINESTRING, poly "
      "POLYGON);");
}

void populateTable() {
  std::vector<std::string> geospatial_test_data{
      "0,'POINT (0 0)','LINESTRING (0 0,0 0)','POLYGON ((0 0,1 0,0 1,0 0))'",
      "1,'POINT (1 1)','LINESTRING (1 0,2 2,3 3)','POLYGON ((0 0,2 0,0 2,0 0))'",
      "2,'POINT (2 2)','LINESTRING (2 0,4 4)','POLYGON ((0 0,3 0,0 3,0 0))'",
      "3,'POINT (3 3)','LINESTRING (3 0,6 6,7 7)','POLYGON ((0 0,4 0,0 4,0 0))'",
      "4,'POINT (4 4)','LINESTRING (4 0,8 8)','POLYGON ((0 0,5 0,0 5,0 0))'",
      "5,'POINT (5 5)','LINESTRING (5 0,10 10,11 11)','POLYGON ((0 0,6 0,0 6,0 0))'",
      "6,'POINT (6 6)','LINESTRING (6 0,12 12)','POLYGON ((0 0,7 0,0 7,0 0))'",
      "7,'POINT (7 7)','LINESTRING (7 0,14 14,15 15)','POLYGON ((0 0,8 0,0 8,0 0))'",
      "8,'POINT (8 8)','LINESTRING (8 0,16 16)','POLYGON ((0 0,9 0,0 9,0 0))'",
      "9,'POINT (9 9)','LINESTRING (9 0,18 18,19 19)','POLYGON ((0 0,10 0,0 10,0 0))'"};

  for (const auto& data : geospatial_test_data) {
    const auto data_str = "INSERT INTO geospatial_test VALUES(" + data + ");";
    run_query(data_str, ExecutorDeviceType::CPU);
  }

  std::vector<std::string> geospatial_inner_test_data{
      "0,'POINT (0 0)','LINESTRING (0 0,0 0)','POLYGON ((0 0,1 0,0 1,0 0))'",
      "2,'POINT (2 2)','LINESTRING (2 0,4 4)','POLYGON ((0 0,3 0,0 3,0 0))'",
      "4,'POINT (4 4)','LINESTRING (4 0,8 8)','POLYGON ((0 0,5 0,0 5,0 0))'",
      "6,'POINT (6 6)','LINESTRING (6 0,12 12)','POLYGON ((0 0,7 0,0 7,0 0))'",
      "8,'POINT (8 8)','LINESTRING (8 0,16 16)','POLYGON ((0 0,9 0,0 9,0 0))'"};

  for (const auto& data : geospatial_inner_test_data) {
    const auto data_str = "INSERT INTO geospatial_inner_join_test VALUES(" + data + ");";
    run_query(data_str, ExecutorDeviceType::CPU);
  }
}

void dropTable() {
  QR::get()->runDDLStatement("DROP TABLE IF EXISTS SQL_HINT_DUMMY;");
  QR::get()->runDDLStatement("DROP TABLE IF EXISTS geospatial_test;");
  QR::get()->runDDLStatement("DROP TABLE IF EXISTS geospatial_inner_join_test;");
}

TEST(kCpuMode, ForceToCPUMode) {
  const auto query_with_cpu_mode_hint = "SELECT /*+ cpu_mode */ * FROM SQL_HINT_DUMMY";
  const auto query_without_cpu_mode_hint = "SELECT * FROM SQL_HINT_DUMMY";
  if (QR::get()->gpusPresent()) {
    auto query_hints = QR::get()->getParsedQueryHint(query_with_cpu_mode_hint);
    const bool cpu_mode_enabled = query_hints.isHintRegistered(QueryHint::kCpuMode);
    EXPECT_TRUE(cpu_mode_enabled);
    query_hints = QR::get()->getParsedQueryHint(query_without_cpu_mode_hint);
    EXPECT_FALSE(query_hints.isAnyQueryHintDelivered());
  }
}

TEST(QueryHint, QueryHintForOverlapsJoin) {
  const auto overlaps_join_status_backup = g_enable_overlaps_hashjoin;
  g_enable_overlaps_hashjoin = true;
  ScopeGuard reset_loop_join_state = [&overlaps_join_status_backup] {
    g_enable_overlaps_hashjoin = overlaps_join_status_backup;
  };

  {
    const auto q1 =
        "SELECT /*+ overlaps_bucket_threshold(0.718) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto q1_hints = QR::get()->getParsedQueryHint(q1);
    EXPECT_TRUE(q1_hints.isHintRegistered(QueryHint::kOverlapsBucketThreshold) &&
                approx_eq(q1_hints.overlaps_bucket_threshold, 0.718));
  }
  {
    const auto q2 =
        "SELECT /*+ overlaps_max_size(2021) */ a.id FROM geospatial_test a INNER JOIN "
        "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto q2_hints = QR::get()->getParsedQueryHint(q2);
    EXPECT_TRUE(q2_hints.isHintRegistered(QueryHint::kOverlapsMaxSize) &&
                (q2_hints.overlaps_max_size == 2021));
  }

  {
    const auto q3 =
        "SELECT /*+ overlaps_bucket_threshold(0.718), overlaps_max_size(2021) */ a.id "
        "FROM "
        "geospatial_test a INNER JOIN geospatial_inner_join_test b ON "
        "ST_Contains(b.poly, "
        "a.p);";
    auto q3_hints = QR::get()->getParsedQueryHint(q3);
    EXPECT_TRUE(q3_hints.isHintRegistered(QueryHint::kOverlapsBucketThreshold) &&
                q3_hints.isHintRegistered(QueryHint::kOverlapsMaxSize) &&
                (q3_hints.overlaps_max_size == 2021) &&
                approx_eq(q3_hints.overlaps_bucket_threshold, 0.718));
  }

  {
    const auto query =
        R"(SELECT /*+ overlaps_allow_gpu_build */ a.id FROM geospatial_test a INNER JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);)";
    const auto hints = QR::get()->getParsedQueryHint(query);
    EXPECT_TRUE(hints.isHintRegistered(QueryHint::kOverlapsAllowGpuBuild));
    EXPECT_TRUE(hints.overlaps_allow_gpu_build);
  }
  {
    const auto q4 =
        "SELECT /*+ overlaps_bucket_threshold(0.1) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto q4_hints = QR::get()->getParsedQueryHint(q4);
    EXPECT_TRUE(q4_hints.isHintRegistered(QueryHint::kOverlapsBucketThreshold) &&
                approx_eq(q4_hints.overlaps_bucket_threshold, 0.1));
  }
  {
    const auto q5 =
        "SELECT /*+ overlaps_keys_per_bin(0.1) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto q5_hints = QR::get()->getParsedQueryHint(q5);
    EXPECT_TRUE(q5_hints.isHintRegistered(QueryHint::kOverlapsKeysPerBin) &&
                approx_eq(q5_hints.overlaps_keys_per_bin, 0.1));
  }
  {
    const auto q6 =
        "SELECT /*+ overlaps_keys_per_bin(19980909.01) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto q6_hints = QR::get()->getParsedQueryHint(q6);
    EXPECT_TRUE(q6_hints.isHintRegistered(QueryHint::kOverlapsKeysPerBin) &&
                approx_eq(q6_hints.overlaps_keys_per_bin, 19980909.01));
  }

  {
    const auto query_without_hint =
        "SELECT a.id FROM geospatial_test a INNER JOIN geospatial_inner_join_test b ON "
        "ST_Contains(b.poly, a.p);";
    auto query_without_hint_res = QR::get()->getParsedQueryHint(query_without_hint);
    EXPECT_TRUE(!query_without_hint_res.isAnyQueryHintDelivered());
  }

  {
    const auto wrong_q1 =
        "SELECT /*+ overlaps_bucket_threshold(-0.718) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto wrong_q1_hints = QR::get()->getParsedQueryHint(wrong_q1);
    EXPECT_TRUE(!wrong_q1_hints.isHintRegistered(QueryHint::kOverlapsBucketThreshold));
  }

  {
    const auto wrong_q2 =
        "SELECT /*+ overlaps_bucket_threshold(91.718) */ a.id FROM geospatial_test a "
        "INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto wrong_q2_hints = QR::get()->getParsedQueryHint(wrong_q2);
    EXPECT_TRUE(!wrong_q2_hints.isHintRegistered(QueryHint::kOverlapsBucketThreshold));
  }

  {
    const auto wrong_q3 =
        "SELECT /*+ overlaps_max_size(-2021) */ a.id FROM geospatial_test a INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto wrong_q3_hints = QR::get()->getParsedQueryHint(wrong_q3);
    EXPECT_TRUE(!wrong_q3_hints.isHintRegistered(QueryHint::kOverlapsMaxSize));
  }
  {
    const auto wrong_q4 =
        "SELECT /*+ overlaps_keys_per_bin(-0.1) */ a.id FROM geospatial_test a INNER "
        "JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p);";
    auto wrong_q4_hints = QR::get()->getParsedQueryHint(wrong_q4);
    EXPECT_TRUE(!wrong_q4_hints.isHintRegistered(QueryHint::kOverlapsKeysPerBin));
  }
  {
    // overlaps_keys_per_bin needs to below then DOUBLE_MAX
    auto double_max = std::to_string(std::numeric_limits<double>::max());
    const auto wrong_q5 =
        "SELECT /*+ overlaps_keys_per_bin(" + double_max +
        ") */ a.id "
        "FROM geospatial_test a INNER JOIN geospatial_inner_join_test b "
        "ON ST_Contains(b.poly, a.p);";
    auto wrong_q5_hints = QR::get()->getParsedQueryHint(wrong_q5);
    EXPECT_TRUE(!wrong_q5_hints.isHintRegistered(QueryHint::kOverlapsKeysPerBin));
  }
}

TEST(QueryHint, QueryLayoutHintWithEnablingColumnarOutput) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = true;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };

  const auto q1 = "SELECT /*+ columnar_output */ * FROM SQL_HINT_DUMMY";
  const auto q2 = "SELECT /*+ rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q3 = "SELECT /*+ columnar_output, rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q4 = "SELECT /*+ rowwise_output, columnar_output */ * FROM SQL_HINT_DUMMY";
  const auto q5 =
      "SELECT /*+ rowwise_output, columnar_output, rowwise_output */ * FROM "
      "SQL_HINT_DUMMY";
  const auto q6 = "SELECT /*+ rowwise_output, rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q7 = "SELECT /*+ columnar_output, columnar_output */ * FROM SQL_HINT_DUMMY";
  {
    auto query_hints = QR::get()->getParsedQueryHint(q1);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kColumnarOutput);
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q2);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kRowwiseOutput);
    EXPECT_TRUE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q3);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q4);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q5);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q6);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kRowwiseOutput);
    EXPECT_TRUE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q7);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kColumnarOutput);
    EXPECT_FALSE(hint_enabled);
  }
}

TEST(QueryHint, QueryLayoutHintWithoutEnablingColumnarOutput) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = false;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };
  const auto q1 = "SELECT /*+ columnar_output */ * FROM SQL_HINT_DUMMY";
  const auto q2 = "SELECT /*+ rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q3 = "SELECT /*+ columnar_output, rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q4 = "SELECT /*+ rowwise_output, columnar_output */ * FROM SQL_HINT_DUMMY";
  const auto q5 =
      "SELECT /*+ rowwise_output, columnar_output, rowwise_output */ * FROM "
      "SQL_HINT_DUMMY";
  const auto q6 = "SELECT /*+ rowwise_output, rowwise_output */ * FROM SQL_HINT_DUMMY";
  const auto q7 = "SELECT /*+ columnar_output, columnar_output */ * FROM SQL_HINT_DUMMY";
  {
    auto query_hints = QR::get()->getParsedQueryHint(q1);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kColumnarOutput);
    EXPECT_TRUE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q2);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kRowwiseOutput);
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q3);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q4);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q5);
    auto hint_enabled = query_hints.isAnyQueryHintDelivered();
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q6);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kRowwiseOutput);
    EXPECT_FALSE(hint_enabled);
  }

  {
    auto query_hints = QR::get()->getParsedQueryHint(q7);
    auto hint_enabled = query_hints.isHintRegistered(QueryHint::kColumnarOutput);
    EXPECT_TRUE(hint_enabled);
  }
}

TEST(QueryHint, UDF) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = false;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };

  const auto q1 =
      "SELECT out0 FROM TABLE(get_max_with_row_offset(cursor(SELECT /*+ columnar_output "
      "*/ key FROM SQL_HINT_DUMMY)));";
  const auto q2 =
      "SELECT out0 FROM TABLE(get_max_with_row_offset(cursor(SELECT /*+ columnar_output, "
      "cpu_mode */ key FROM SQL_HINT_DUMMY)));";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q1);
    EXPECT_TRUE(query_hints);
    EXPECT_EQ(query_hints->size(), static_cast<size_t>(1));
    EXPECT_TRUE(query_hints->begin()->second.begin()->second.isHintRegistered(
        QueryHint::kColumnarOutput));
  }
  {
    auto query_hints = QR::get()->getParsedQueryHints(q2);
    EXPECT_TRUE(query_hints);
    EXPECT_EQ(query_hints->size(), static_cast<size_t>(1));
    EXPECT_TRUE(query_hints->begin()->second.begin()->second.isHintRegistered(
        QueryHint::kColumnarOutput));
    EXPECT_TRUE(query_hints->begin()->second.begin()->second.isHintRegistered(
        QueryHint::kCpuMode));
  }
}

TEST(QueryHint, PerQueryBlockHint) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = false;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };

  const auto q1 =
      "SELECT /*+ cpu_mode */ T2.k FROM SQL_HINT_DUMMY T1, (SELECT /*+ columnar_output "
      "*/ key as k FROM SQL_HINT_DUMMY WHERE key = 1) T2 WHERE T1.key = T2.k;";
  const auto q2 =
      "SELECT /*+ cpu_mode */ out0 FROM TABLE(get_max_with_row_offset(cursor(SELECT /*+ "
      "columnar_output */ key FROM SQL_HINT_DUMMY)));";
  // to recognize query hint for a specific query block, we need more complex hint getter
  // func in QR but for test, it is enough to check the functionality in brute-force
  // manner
  auto check_registered_hint =
      [](std::unordered_map<size_t, std::unordered_map<unsigned, RegisteredQueryHint>>&
             hints) {
        bool find_columnar_hint = false;
        bool find_cpu_mode_hint = false;
        CHECK(hints.size() == static_cast<size_t>(2));
        for (auto& kv : hints) {
          for (auto& kv2 : kv.second) {
            auto hint = kv2.second;
            if (hint.isHintRegistered(QueryHint::kColumnarOutput)) {
              find_columnar_hint = true;
              EXPECT_FALSE(hint.isHintRegistered(QueryHint::kCpuMode));
              continue;
            }
            if (hint.isHintRegistered(QueryHint::kCpuMode)) {
              find_cpu_mode_hint = true;
              EXPECT_FALSE(hint.isHintRegistered(QueryHint::kColumnarOutput));
              continue;
            }
          }
        }
        EXPECT_TRUE(find_columnar_hint);
        EXPECT_TRUE(find_cpu_mode_hint);
      };
  {
    auto query_hints = QR::get()->getParsedQueryHints(q1);
    EXPECT_TRUE(query_hints);
    check_registered_hint(query_hints.value());
  }
  {
    auto query_hints = QR::get()->getParsedQueryHints(q2);
    EXPECT_TRUE(query_hints);
    check_registered_hint(query_hints.value());
  }
}

TEST(QueryHint, WindowFunction) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = false;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };

  const auto q1 =
      "SELECT /*+ columnar_output */ str1, timestampdiff(minute, lag(ts1) over "
      "(partition by str1 order by ts1), ts2) as m_el FROM SQL_HINT_DUMMY;";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q1);
    EXPECT_TRUE(query_hints);
    for (auto& kv : *query_hints) {
      auto& query_hint_map = kv.second;
      for (auto& kv2 : query_hint_map) {
        auto query_hint = kv2.second;
        EXPECT_TRUE(query_hint.isHintRegistered(QueryHint::kColumnarOutput));
      }
    }
  }
  const auto q2 =
      "SELECT /*+ columnar_output */ count(1) FROM (SELECT /*+ columnar_output */ str1, "
      "timestampdiff(minute, lag(ts1) over (partition by str1 order by ts1), ts2) as "
      "m_el FROM SQL_HINT_DUMMY) T1 WHERE T1.m_el < 30;";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q2);
    EXPECT_TRUE(query_hints);
    for (auto& kv : *query_hints) {
      auto& query_hint_map = kv.second;
      for (auto& kv2 : query_hint_map) {
        auto query_hint = kv2.second;
        EXPECT_TRUE(query_hint.isHintRegistered(QueryHint::kColumnarOutput));
      }
    }
  }
}

TEST(QueryHint, GlobalHint_OverlapsJoinHashtable) {
  const auto overlaps_join_status_backup = g_enable_overlaps_hashjoin;
  const auto data_recycler_flag_backup = g_enable_data_recycler;
  const auto hashtable_cache_flag_backup = g_use_hashtable_cache;
  g_enable_overlaps_hashjoin = true;
  g_enable_data_recycler = true;
  g_use_hashtable_cache = true;
  ScopeGuard reset_loop_join_state = [&overlaps_join_status_backup,
                                      &data_recycler_flag_backup,
                                      &hashtable_cache_flag_backup] {
    g_enable_overlaps_hashjoin = overlaps_join_status_backup;
    g_enable_data_recycler = data_recycler_flag_backup;
    g_use_hashtable_cache = hashtable_cache_flag_backup;
  };

  // testing global query hint for overlaps join is tricky since we apply all registered
  // hint during hashtable building time, so it's hard to get the result at that time
  // instead by exploiting cached hashtable we can check whether hints are registered &
  // applied correctly in indirect manner

  // q1 and q2: global query hint registered to the main query block
  const auto q1 =
      "SELECT /*+ g_overlaps_no_cache */ t1.ID FROM (SELECT a.id FROM geospatial_test a "
      "INNER JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T1;";
  {
    auto res = run_query(q1, ExecutorDeviceType::CPU);
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(0));
  }

  if (QR::get()->gpusPresent()) {
    const auto q2 =
        "SELECT /*+ g_overlaps_allow_gpu_build */ t1.ID FROM (SELECT a.id FROM "
        "geospatial_test a INNER JOIN geospatial_inner_join_test b ON "
        "ST_Contains(b.poly, a.p)) T1;";
    auto res = run_query(q2, ExecutorDeviceType::GPU);
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(0));
  }

  // q3 and q4: two (e.g., multiple) subqueries and we disallow to put hashtable to cache
  // for one of them so we should have just a single overlaps join hashtable with
  // registered global hint
  std::set<QueryPlanHash> visited_hashtable_key;
  const auto q3 =
      "SELECT /*+ g_overlaps_max_size(7777) */ t1.ID, t2.ID FROM \n"
      "(SELECT a.id FROM geospatial_test a INNER JOIN geospatial_inner_join_test b ON "
      "ST_Contains(b.poly, a.p)) T1, \n"
      "(SELECT /*+ overlaps_no_cache */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T2 \n"
      "WHERE t1.ID = t2.ID;";
  {
    auto res = run_query(q3, ExecutorDeviceType::CPU);
    auto cached_ht_info =
        getCachedHashTable(visited_hashtable_key, CacheItemType::OVERLAPS_HT);
    auto query_hint = cached_ht_info.second;
    EXPECT_TRUE(query_hint.has_value());
    EXPECT_EQ(query_hint->overlaps_max_size, static_cast<size_t>(7777));
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(1));
    QR::get()->clearCpuMemory();
    visited_hashtable_key.clear();
  }

  if (QR::get()->gpusPresent()) {
    const auto q4 =
        "SELECT /*+ g_overlaps_bucket_threshold(0.718) */ t1.ID, t2.ID FROM \n"
        "(SELECT a.id FROM geospatial_test a INNER JOIN geospatial_inner_join_test b ON "
        "ST_Contains(b.poly, a.p)) T1,\n"
        "(SELECT /*+ overlaps_allow_gpu_build */ a.id FROM geospatial_test a INNER JOIN "
        "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T2\n"
        "WHERE t1.ID = t2.ID;";
    auto res = run_query(q4, ExecutorDeviceType::GPU);
    auto cached_ht_info =
        getCachedHashTable(visited_hashtable_key, CacheItemType::OVERLAPS_HT);
    auto query_hint = cached_ht_info.second;
    EXPECT_TRUE(query_hint.has_value());
    EXPECT_TRUE(approx_eq(query_hint->overlaps_bucket_threshold, 0.718));
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(1));
    QR::get()->clearCpuMemory();
    visited_hashtable_key.clear();
  }

  // q5, q6 and q7: a subquery block which is allowed to interact with hashtable cache
  // should have the info related to both global and local query hint(s)
  const auto q5 =
      "SELECT /*+ g_overlaps_keys_per_bin(0.1) */ t1.ID, t2.ID FROM \n"
      "(SELECT /*+ overlaps_max_size(7777) */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T1,\n"
      "(SELECT /*+ overlaps_no_cache */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T2\n"
      "WHERE t1.ID = t2.ID;";
  {
    auto res = run_query(q5, ExecutorDeviceType::CPU);
    auto cached_ht_info =
        getCachedHashTable(visited_hashtable_key, CacheItemType::OVERLAPS_HT);
    auto query_hint = cached_ht_info.second;
    EXPECT_TRUE(query_hint.has_value());
    EXPECT_TRUE(approx_eq(query_hint->overlaps_keys_per_bin, 0.1));
    EXPECT_EQ(query_hint->overlaps_max_size, static_cast<size_t>(7777));
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(1));
    QR::get()->clearCpuMemory();
    visited_hashtable_key.clear();
  }

  const auto q6 =
      "SELECT /*+ g_overlaps_keys_per_bin(0.1) */ t1.ID, t2.ID FROM \n"
      "(SELECT /*+ g_overlaps_bucket_threshold(0.718) */ a.id FROM geospatial_test a "
      "INNER JOIN geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T1,\n"
      "(SELECT /*+ overlaps_no_cache */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T2\n"
      "WHERE t1.ID = t2.ID;";
  {
    auto res = run_query(q6, ExecutorDeviceType::CPU);
    auto cached_ht_info =
        getCachedHashTable(visited_hashtable_key, CacheItemType::OVERLAPS_HT);
    auto query_hint = cached_ht_info.second;
    EXPECT_TRUE(query_hint.has_value());
    EXPECT_TRUE(approx_eq(query_hint->overlaps_keys_per_bin, 0.1));
    EXPECT_TRUE(approx_eq(query_hint->overlaps_bucket_threshold, 0.718));
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(1));
    QR::get()->clearCpuMemory();
    visited_hashtable_key.clear();
  }

  const auto q7 =
      "SELECT /*+ g_overlaps_max_size(7777) */ t1.ID, t2.ID FROM \n"
      "(SELECT /*+ overlaps_keys_per_bin(0.1) */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T1,\n"
      "(SELECT /*+ overlaps_no_cache */ a.id FROM geospatial_test a INNER JOIN "
      "geospatial_inner_join_test b ON ST_Contains(b.poly, a.p)) T2\n"
      "WHERE t1.ID = t2.ID;";
  {
    auto res = run_query(q7, ExecutorDeviceType::CPU);
    auto cached_ht_info =
        getCachedHashTable(visited_hashtable_key, CacheItemType::OVERLAPS_HT);
    auto query_hint = cached_ht_info.second;
    EXPECT_TRUE(query_hint.has_value());
    EXPECT_TRUE(approx_eq(query_hint->overlaps_keys_per_bin, 0.1));
    EXPECT_EQ(query_hint->overlaps_max_size, static_cast<size_t>(7777));
    EXPECT_EQ(QR::get()->getNumberOfCachedOverlapsHashTables(), static_cast<size_t>(1));
    QR::get()->clearCpuMemory();
    visited_hashtable_key.clear();
  }
}

TEST(QueryHint, GlobalHint_ResultsetLayoutAndCPUMode) {
  const auto enable_columnar_output = g_enable_columnar_output;
  g_enable_columnar_output = false;
  ScopeGuard reset_columnar_output = [&enable_columnar_output] {
    g_enable_columnar_output = enable_columnar_output;
  };

  // check whether we can see the enabled global hint in the outer query block
  const auto q1 =
      "SELECT T2.k FROM SQL_HINT_DUMMY T1, (SELECT /*+ g_cpu_mode */ key as k FROM "
      "SQL_HINT_DUMMY WHERE key = 1) T2 WHERE T1.key = T2.k;";
  {
    auto global_query_hints = QR::get()->getParsedGlobalQueryHints(q1);
    CHECK(global_query_hints);
    EXPECT_TRUE(global_query_hints->isHintRegistered(QueryHint::kCpuMode));
  }

  // check whether inner query has enabled cpu hint
  const auto q2 =
      "SELECT /*+ g_cpu_mode */ T2.k FROM SQL_HINT_DUMMY T1, (SELECT key as k FROM "
      "SQL_HINT_DUMMY WHERE key = 1) T2 WHERE T1.key = T2.k;";
  {
    auto global_query_hints = QR::get()->getParsedGlobalQueryHints(q2);
    EXPECT_TRUE(global_query_hints);
    EXPECT_TRUE(global_query_hints->isHintRegistered(QueryHint::kCpuMode));
  }

  // check whether we can see not only cpu mode hint but also global columnar output hint
  const auto q3 =
      "SELECT /*+ cpu_mode */ out0 FROM TABLE(get_max_with_row_offset(cursor(SELECT /*+ "
      "g_columnar_output */ key FROM SQL_HINT_DUMMY)));";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q3);
    EXPECT_TRUE(query_hints);
    bool found_local_hint = false;
    for (auto& kv : *query_hints) {
      for (auto& kv2 : kv.second) {
        auto& hint = kv2.second;
        if (hint.isAnyQueryHintDelivered()) {
          if (hint.isHintRegistered(QueryHint::kCpuMode)) {
            found_local_hint = true;
          }
        }
      }
    }
    EXPECT_TRUE(found_local_hint);
    auto global_query_hints = QR::get()->getParsedGlobalQueryHints(q3);
    EXPECT_TRUE(global_query_hints);
    EXPECT_TRUE(global_query_hints->isHintRegistered(QueryHint::kColumnarOutput));
  }

  const auto q4 =
      "SELECT /*+ columnar_output */ out0 FROM "
      "TABLE(get_max_with_row_offset(cursor(SELECT /*+ g_rowwise_output */ key FROM "
      "SQL_HINT_DUMMY)));";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q4);
    EXPECT_TRUE(query_hints);
    bool found_local_hint = false;
    for (auto& kv : *query_hints) {
      for (auto& kv2 : kv.second) {
        auto& hint = kv2.second;
        if (hint.isAnyQueryHintDelivered()) {
          if (hint.isHintRegistered(QueryHint::kColumnarOutput)) {
            found_local_hint = true;
          }
        }
      }
    }
    EXPECT_TRUE(found_local_hint);
    auto global_query_hints = QR::get()->getParsedGlobalQueryHints(q4);
    EXPECT_TRUE(global_query_hints);
    EXPECT_FALSE(global_query_hints->isHintRegistered(QueryHint::kRowwiseOutput));
  }

  // we disable columnar output so rowwise global hint is ignored too
  // thus we expect to see the enabled global columnar output hint
  const auto q5 =
      "SELECT /*+ g_rowwise_output */ out0 FROM "
      "TABLE(get_max_with_row_offset(cursor(SELECT /*+ g_columnar_output */ key FROM "
      "SQL_HINT_DUMMY)));";
  {
    auto query_hints = QR::get()->getParsedQueryHints(q5);
    EXPECT_TRUE(query_hints);
    bool columnar_enabled_local = false;
    bool rowwise_enabled_local = false;
    for (auto& kv : *query_hints) {
      for (auto& kv2 : kv.second) {
        auto& hint = kv2.second;
        if (hint.isAnyQueryHintDelivered()) {
          if (hint.isHintRegistered(QueryHint::kColumnarOutput)) {
            columnar_enabled_local = true;
          }
          if (hint.isHintRegistered(QueryHint::kRowwiseOutput)) {
            rowwise_enabled_local = true;
          }
        }
      }
    }
    EXPECT_TRUE(columnar_enabled_local);
    EXPECT_FALSE(rowwise_enabled_local);
    auto global_query_hints = QR::get()->getParsedGlobalQueryHints(q5);
    EXPECT_TRUE(global_query_hints);
    EXPECT_TRUE(global_query_hints->isHintRegistered(QueryHint::kColumnarOutput));
    EXPECT_FALSE(global_query_hints->isHintRegistered(QueryHint::kRowwiseOutput));
  }
}

int main(int argc, char** argv) {
  TestHelpers::init_logger_stderr_only(argc, argv);
  testing::InitGoogleTest(&argc, argv);

  g_enable_table_functions = true;
  g_enable_dev_table_functions = true;
  QR::init(BASE_PATH);
  int err{0};

  try {
    createTable();
    populateTable();
    err = RUN_ALL_TESTS();
  } catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }
  dropTable();
  QR::reset();
  return err;
}
