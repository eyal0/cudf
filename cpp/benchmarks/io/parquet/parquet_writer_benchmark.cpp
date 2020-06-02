/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
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

#include <benchmark/benchmark.h>

#include <cudf/column/column.hpp>
#include <cudf/table/table.hpp>

#include "../generate_input.hpp"

#include <tests/utilities/base_fixture.hpp>
#include <tests/utilities/column_utilities.hpp>
#include <tests/utilities/column_wrapper.hpp>

#include <benchmarks/fixture/benchmark_fixture.hpp>
#include <benchmarks/synchronization/synchronization.hpp>

#include <cudf/io/functions.hpp>

// to enable, run cmake with -DBUILD_BENCHMARKS=ON

constexpr int64_t data_size = 512 << 20;

namespace cudf_io = cudf::io;

template <typename T>
class ParquetWrite : public cudf::benchmark {
};

template <typename T>
void PQ_write(benchmark::State& state)
{
  int64_t const total_desired_bytes = state.range(0);
  cudf::size_type const num_cols    = state.range(1);
  cudf_io::compression_type const compression =
    state.range(2) ? cudf_io::compression_type::SNAPPY : cudf_io::compression_type::NONE;

  int64_t const col_bytes = total_desired_bytes / num_cols;

  auto const tbl  = create_random_table<T>(num_cols, col_bytes, true);
  auto const view = tbl->view();

  for (auto _ : state) {
    cuda_event_timer raii(state, true);  // flush_l2_cache = true, stream = 0
    cudf_io::write_parquet_args args{cudf_io::sink_info(), view, nullptr, compression};
    cudf_io::write_parquet(args);
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

#define UNCOMPRESSED 0
#define SNAPPY 1

#define PWBM_BENCHMARK(name, type, compression)           \
  BENCHMARK_TEMPLATE_DEFINE_F(ParquetWrite, name, type)   \
  (::benchmark::State & state) { PQ_write<type>(state); } \
  BENCHMARK_REGISTER_F(ParquetWrite, name)                \
    ->Args({data_size, 64, compression})                  \
    ->Unit(benchmark::kMillisecond)                       \
    ->UseManualTime();

#define PWBM_BENCH_ALL_TYPES(compression)                                      \
  PWBM_BENCHMARK(Boolean##compression, bool, compression);                     \
  PWBM_BENCHMARK(Byte##compression, int8_t, compression);                      \
  PWBM_BENCHMARK(Short##compression, int16_t, compression);                    \
  PWBM_BENCHMARK(Int##compression, int32_t, compression);                      \
  PWBM_BENCHMARK(Long##compression, int64_t, compression);                     \
  PWBM_BENCHMARK(Float##compression, float, compression);                      \
  PWBM_BENCHMARK(Double##compression, double, compression);                    \
  PWBM_BENCHMARK(String##compression, std::string, compression);               \
  PWBM_BENCHMARK(Timestamp_days##compression, cudf::timestamp_D, compression); \
  PWBM_BENCHMARK(Timestamp_sec##compression, cudf::timestamp_s, compression);  \
  PWBM_BENCHMARK(Timestamp_ms##compression, cudf::timestamp_ms, compression);  \
  PWBM_BENCHMARK(Timestamp_us##compression, cudf::timestamp_us, compression);  \
  PWBM_BENCHMARK(Timestamp_ns##compression, cudf::timestamp_ns, compression);

PWBM_BENCH_ALL_TYPES(UNCOMPRESSED)

PWBM_BENCH_ALL_TYPES(SNAPPY)