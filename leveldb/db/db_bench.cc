// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <random>
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"
#include "util/trace.h"
#include "gflags/gflags.h"


using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

// Comma-separated list of operations to run in the specified order
//   Actual benchmarks:
//      fillseq       -- write N values in sequential key order in async mode
//      fillrandom    -- write N values in random key order in async mode
//      overwrite     -- overwrite N values in random key order in async mode
//      fillsync      -- write N/100 values in random key order in sync mode
//      fill100K      -- write N/1000 100K values in random order in async mode
//      deleteseq     -- delete N keys in sequential order
//      deleterandom  -- delete N keys in random order
//      readseq       -- read N times sequentially
//      readreverse   -- read N times in reverse order
//      readrandom    -- read N times in random order
//      readmissing   -- read N missing keys in random order
//      readhot       -- read N times in random order from 1% section of DB
//      seekrandom    -- N random seeks
//      open          -- cost of opening a DB
//      crc32c        -- repeated crc32c of 4K of data
//   Meta operations:
//      compact     -- Compact the entire DB
//      stats       -- Print DB stats
//      sstables    -- Print sstable info
//      heapprofile -- Dump a heap profile (if supported by this port)

DEFINE_string(
    benchmarks,
    "fillseq,"
    // "stats,"
    "readrandom,"
    "readmissing,"
    // "stats,"
    "fillrandom,"
    "readrandom,"
    "readmissing,"
    "compact,"
    "readmissing,"
    // "fillbatch,"
    // "overwrite,"
    // "readseq,"
    // "readreverse,"
    // // "compact,"
    // "readrandom,"
    // "readseq,"
    // "readreverse,"
    // "fill100K,"
    // "crc32c,"
    // "snappycomp,"
    // "snappyuncomp,"
    // "acquireload,"
    , "");

// Number of key/values to place in database
DEFINE_int64(num, 1000000, "Number of key/values to place in database");
DEFINE_int64(range, -1, "key range space");

DEFINE_int64(writes, -1, "Number of write");

DEFINE_int32(prefix_length, 16,
             "Prefix length to pass into NewFixedPrefixTransform");

// Number of read operations to do.  If negative, do FLAGS_num reads.
DEFINE_int64(reads, -1, "");

DEFINE_int64(ycsb_ops_num, 1000000, "YCSB operations num");

// YCSB workload type
static leveldb::YCSBLoadType FLAGS_ycsb_type = leveldb::kYCSB_A;

// Number of concurrent threads to run.
DEFINE_int32(threads, 1, "Number of concurrent threads to run.");
DEFINE_int32(duration, 0, "Time in seconds for the random-ops tests to run."
             " When 0 then num & reads determine the test duration");

// Size of each value
DEFINE_int32(value_size, 100, "Size of each value");

// Arrange to generate values that shrink to this fraction of
// their original size after compression
DEFINE_double(compression_ratio, 0.5, "Arrange to generate values that shrink"
              " to this fraction of their original size after compression");

// Print histogram of operation timings
DEFINE_bool(histogram, false, "Print histogram of operation timings");
DEFINE_bool(print_wa, false, "Print write amplification every stats interval");

// Number of bytes to buffer in memtable before compacting
// (initialized to default value by "main")
DEFINE_int64(write_buffer_size, 1048576,
             "Number of bytes to buffer in all memtables before compacting");

// Number of bytes written to each file.
// (initialized to default value by "main")
DEFINE_int64(max_file_size, 256 << 20, "");

// Approximate size of user data packed per block (before compression.
// (initialized to default value by "main")
DEFINE_int32(block_size, 4096,  "");
// Number of bytes to use as a cache of uncompressed data.
// Negative means use default settings.
DEFINE_int64(cache_size, 8 << 20, "");
// Maximum number of files to keep open at the same time (use default if == 0)
DEFINE_int32(open_files, 0,
             "Maximum number of files to keep open at the same time"
             " (use default if == 0)");

// Bloom filter bits per key.
// Negative means use default settings.
DEFINE_int32(bloom_bits, 10, "");

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
DEFINE_bool(use_existing_db, false, "");

// If true, reuse existing log/MANIFEST files when re-opening a database.
DEFINE_bool(reuse_logs, false, "");


DEFINE_int32(seek_nexts, 50,
             "How many times to call Next() after Seek() in "
             "RangeQuery");
             
// Use the db with the following name.
// static const char* FLAGS_db = nullptr;
DEFINE_string(db, "", "Use the db with the following name.");

DEFINE_string(logpath, "", "");

DEFINE_int64(partition, 2000, "");

DEFINE_bool(compression, true, "");

DEFINE_bool(hugepage, false, "");

DEFINE_int32(stats_interval, 1000000, "");

DEFINE_bool(log, true, "");

DEFINE_int32(low_pool, 2, "");
DEFINE_int32(high_pool, 1, "");

DEFINE_int32(log_buffer_size, 65536, "");

DEFINE_int64(batch_size, 1, "Batch size");

DEFINE_bool(mem_append, false, "mem table use append mode");
DEFINE_bool(direct_io, false, "enable direct io");
DEFINE_bool(no_close, false, "close file after pwrite");
DEFINE_bool(skiplistrep, true, "use skiplist as memtable");
DEFINE_bool(log_dio, true, "log use direct io");
DEFINE_bool(bg_cancel, false, "allow bg compaction to be canceled");

DEFINE_int32(io_record_pid, 0, "");
DEFINE_int32(rwdelay, 10, "delay in us");
DEFINE_int32(sleep, 100, "sleep for write in readwhilewriting2");
DEFINE_int64(report_interval, 20, "report time interval");

static std::vector<uint64_t> ycsb_insertion_sequence;
leveldb::Env* g_env = leveldb::Env::Default();
namespace leveldb {

namespace {

// Helper for quickly generating random data.
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    Random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};

#if defined(__linux)
static Slice TrimSpace(Slice s) {
  size_t start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  size_t limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return Slice(s.data() + start, limit - start);
}
#endif

static void AppendWithSpace(std::string* str, Slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class Stats {
 public:
  int id_;
  double start_;
  double finish_;
  double seconds_;
  uint64_t done_;
  uint64_t last_report_done_;
  uint64_t last_report_finish_;
  uint64_t next_report_;
  double next_report_time_;
  int64_t   bytes_;
  double last_op_finish_;
  HistogramImpl hist_;
  std::string message_;

 public:
  Stats() { Start(); }
  Stats(int id) { id_ = id; Start(); }
  void Start() {
    start_ = g_env->NowMicros();
    next_report_time_ = start_;
    next_report_ = 100;
    last_op_finish_ = start_;
    last_report_done_ = 0;
    last_report_finish_ = start_;
    hist_.Clear();
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    finish_ = start_;
    message_.clear();
  }

  void Merge(const Stats& other) {
    hist_.Merge(other.hist_);
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // Just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void Stop() {
    finish_ = g_env->NowMicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void AddMessage(Slice msg) {
    AppendWithSpace(&message_, msg);
  }


  void PrintSpeed() {

    uint64_t now = Env::Default()->NowMicros();
    int64_t usecs_since_last = now - last_report_finish_;

    std::string cur_time = Env::Default()->TimeToString(now/1000000);
    fprintf(stdout,
            "%s ... thread %d: (%llu,%llu) ops and "
            "(%.1f,%.1f) ops/second in (%.6f,%.6f) seconds\n",
            cur_time.c_str(), 
            id_,
            done_ - last_report_done_, done_,
            (done_ - last_report_done_) /
            (usecs_since_last / 1000000.0),
            done_ / ((now - start_) / 1000000.0),
            (now - last_report_finish_) / 1000000.0,
            (now - start_) / 1000000.0);
    last_report_finish_ = now;
    last_report_done_ = done_;

    // std::string io_res = GetStdoutFromCommand("echo q| htop -u hanson | aha --line-fix | html2text -width 999 | grep -v 'F1Help' | grep -v 'xml version=' | grep kv_bench ");
    // Log(io_log, " --- %s\n%s", cur_time.c_str(), io_res.c_str());
    fflush(stdout);
  }

  void FinishedSingleOp2(DB* db = nullptr) {
    if (FLAGS_histogram) {
      double now = g_env->NowMicros();
      double micros = now - last_op_finish_;
      hist_.Add(micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;
      fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long)done_, "");
      fflush(stderr);
    }

    if (g_env->NowMicros() > next_report_time_) {
        PrintSpeed(); 
        next_report_time_ += FLAGS_report_interval * 1000000;
        if (FLAGS_print_wa && db) {
          std::string stats;
          if (!db->GetProperty("leveldb.stats", &stats)) {
            stats = "(failed)";
          }
          fprintf(stdout, "\n%s\n", stats.c_str());
        }
    }
  }

  void FinishedSingleOp(DB* db = nullptr) {
    if (FLAGS_histogram) {
      double now = g_env->NowMicros();
      double micros = now - last_op_finish_;
      hist_.Add(micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;
      if(done_ % FLAGS_stats_interval == 0) {
        PrintSpeed(); 
        if (FLAGS_print_wa && db) {
          std::string stats;
          if (!db->GetProperty("leveldb.stats", &stats)) {
            stats = "(failed)";
          }
          fprintf(stdout, "\n%s\n", stats.c_str());
        }
      }
      fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long)done_, "");
      fflush(stderr);
    }
  }

  void AddBytes(int64_t n) {
    bytes_ += n;
  }

  void Report(const Slice& name) {
    // Pretend at least one op was done in case we are running a benchmark
    // that does not call FinishedSingleOp().
    if (done_ < 1) done_ = 1;

    std::string extra;
    double elapsed = (finish_ - start_) * 1e-6;
    if (bytes_ > 0) {
      // Rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f MB/s",
               (bytes_ / 1048576.0) / elapsed);
      extra = rate;
    }
    AppendWithSpace(&extra, message_);

    double throughput = (double)done_/elapsed;
    fprintf(stdout, "%-12s : %11.3f micros/op %ld ops/sec;%s%s\n",
            name.ToString().c_str(),
            elapsed * 1e6 / done_,
            (long)throughput,
            (extra.empty() ? "" : " "),
            extra.c_str());
    if (FLAGS_histogram) {
      fprintf(stdout, "Microseconds per op:\n%s\n", hist_.ToString().c_str());
    }
    fflush(stdout);
  }
};

// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  port::Mutex mu;
  port::CondVar cv GUARDED_BY(mu);
  int total GUARDED_BY(mu);

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized GUARDED_BY(mu);
  int num_done GUARDED_BY(mu);
  bool start GUARDED_BY(mu);

  SharedState(int total)
      : cv(&mu), total(total), num_initialized(0), num_done(0), start(false) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
  int tid;             // 0..n-1 when running in n threads
  // Random rand;         // Has different seeds for different threads
  Stats stats;
  SharedState* shared;
  Trace* trace;
  RandomGenerator gen;
  ThreadState(int index)
      : tid(index),
        // rand(1000 + index),
        stats(index) {
        trace = new TraceUniform(1000 + index * 345);
  }
};


class Duration {
 public:
  Duration(uint64_t max_seconds, int64_t max_ops, int64_t ops_per_stage = 0) {
    max_seconds_ = max_seconds;
    max_ops_= max_ops;
    ops_per_stage_ = (ops_per_stage > 0) ? ops_per_stage : max_ops;
    ops_ = 0;
    start_at_ = g_env->NowMicros();
  }

  int64_t GetStage() { return std::min(ops_, max_ops_ - 1) / ops_per_stage_; }

  bool Done(int64_t increment) {
    if (increment <= 0) increment = 1;    // avoid Done(0) and infinite loops
    ops_ += increment;

    if (max_seconds_) {
      // Recheck every appx 1000 ops (exact iff increment is factor of 1000)
      auto granularity = 1000;
      if ((ops_ / granularity) != ((ops_ - increment) / granularity)) {
        uint64_t now = g_env->NowMicros();
        return ((now - start_at_) / 1000000) >= max_seconds_;
      } else {
        return false;
      }
    } else {
      return ops_ > max_ops_;
    }
  }

 private:
  uint64_t max_seconds_;
  int64_t max_ops_;
  int64_t ops_per_stage_;
  int64_t ops_;
  uint64_t start_at_;
};

}  // namespace

class Benchmark {
 private:
  Cache* cache_;
  const FilterPolicy* filter_policy_;
  DB* db_;
  int64_t num_;
  int value_size_;
  uint64_t entries_per_batch_;
  WriteOptions write_options_;
  uint64_t reads_;
  uint64_t heap_counter_;

  void PrintHeader() {
    const int kKeySize = 16;
    PrintEnvironment();
    fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
    fprintf(stdout, "Values:     %d bytes each (%d bytes after compression)\n",
            FLAGS_value_size,
            static_cast<int>(FLAGS_value_size * FLAGS_compression_ratio + 0.5));
    fprintf(stdout, "Entries:    %lld\n", num_);
    fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
            ((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num_)
             / 1048576.0));
    fprintf(stdout, "FileSize:   %.1f MB (estimated)\n",
            (((kKeySize + FLAGS_value_size * FLAGS_compression_ratio) * num_)
             / 1048576.0));
    PrintWarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    fprintf(stdout,
            "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef NDEBUG
    fprintf(stdout,
            "WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif

    // See if snappy is working by attempting to compress a compressible string
    const char text[] = "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy";
    std::string compressed;
    if (!port::Snappy_Compress(text, sizeof(text), &compressed)) {
      fprintf(stdout, "WARNING: Snappy compression is not enabled\n");
    } else if (compressed.size() >= sizeof(text)) {
      fprintf(stdout, "WARNING: Snappy compression is not effective\n");
    }
  }

  void PrintEnvironment() {
    fprintf(stderr, "LevelDB:    version %d.%d\n",
            kMajorVersion, kMinorVersion);

#if defined(__linux)
    time_t now = time(nullptr);
    fprintf(stderr, "Date:       %s", ctime(&now));  // ctime() adds newline

    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != nullptr) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
        const char* sep = strchr(line, ':');
        if (sep == nullptr) {
          continue;
        }
        Slice key = TrimSpace(Slice(line, sep - 1 - line));
        Slice val = TrimSpace(Slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.ToString();
        } else if (key == "cache size") {
          cache_size = val.ToString();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "CPUCache:   %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  Benchmark()
  : cache_(FLAGS_cache_size >= 0 ? NewLRUCache(FLAGS_cache_size) : nullptr),
    filter_policy_(FLAGS_bloom_bits >= 0
                   ? NewBloomFilterPolicy(FLAGS_bloom_bits)
                   : nullptr),
    db_(nullptr),
    num_(FLAGS_num),
    value_size_(FLAGS_value_size),
    entries_per_batch_(1),
    reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
    heap_counter_(0) {
    std::vector<std::string> files;
    g_env->GetChildren(FLAGS_db, &files);
    for (size_t i = 0; i < files.size(); i++) {
      if (Slice(files[i]).starts_with("heap-")) {
        g_env->DeleteFile(std::string(FLAGS_db) + "/" + files[i]);
      }
    }
    if (!FLAGS_use_existing_db) {
      DestroyDB(FLAGS_db, Options());
    }
  }

  ~Benchmark() {
    delete db_;
    delete cache_;
    delete filter_policy_;
  }

  void Run() {
    PrintHeader();
    Open();

    const char* benchmarks = FLAGS_benchmarks.c_str();
    while (benchmarks != nullptr) {
      const char* sep = strchr(benchmarks, ',');
      Slice name;
      if (sep == nullptr) {
        name = benchmarks;
        benchmarks = nullptr;
      } else {
        name = Slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // Reset parameters that may be overridden below
      printf("FLAGS_num %llu, num_ %lld\n", FLAGS_num, num_);
      num_ = FLAGS_num / FLAGS_threads;
      printf("num after by threads %lld\n", num_);
      reads_ = (FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads);
      value_size_ = FLAGS_value_size;
      entries_per_batch_ = FLAGS_batch_size;
      write_options_ = WriteOptions();

      void (Benchmark::*method)(ThreadState*) = nullptr;
      bool fresh_db = false;
      int num_threads = FLAGS_threads;

      if (FLAGS_writes != -1) {
          num_ = FLAGS_writes;
      }

      if (FLAGS_range == -1) {
        FLAGS_range = FLAGS_num;
      }

      if (name == Slice("open")) {
        method = &Benchmark::OpenBench;
        num_ /= 10000;
        if (num_ < 1) num_ = 1;
      } else if (name == Slice("fillseq")) {
        fresh_db = true;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillbatch")) {
        fresh_db = true;
        entries_per_batch_ = 1000;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillrandom")) {
        fresh_db = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("overwrite")) {
        fresh_db = false;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fillsync")) {
        fresh_db = true;
        num_ /= 1000;
        write_options_.sync = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fill100K")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("readseq")) {
        method = &Benchmark::ReadSequential;
      } else if (name == Slice("readreverse")) {
        method = &Benchmark::ReadReverse;
      } else if (name == Slice("readrandom")) {
        method = &Benchmark::ReadRandom;
      } else if (name == Slice("readmissing")) {
        method = &Benchmark::ReadMissing;
      } else if (name == Slice("seekrandom")) {
        method = &Benchmark::SeekRandom;
      } else if (name == Slice("readhot")) {
        method = &Benchmark::ReadHot;
      } else if (name == Slice("readrandomsmall")) {
        reads_ /= 1000;
        method = &Benchmark::ReadRandom;
      } else if (name == Slice("deleteseq")) {
        method = &Benchmark::DeleteSeq;
      } else if (name == Slice("deleterandom")) {
        method = &Benchmark::DeleteRandom;
      } else if (name == Slice("readwhilewriting")) {
        num_threads++;  // Add extra thread for writing
        method = &Benchmark::ReadWhileWriting;
      } else if (name == Slice("readuniwhilewriting")) {
        num_threads++;  // Add extra thread for writing
        method = &Benchmark::ReadWhileWriting2;
      } else if (name == Slice("readexpwhilewriting")) {
        num_threads++;  // Add extra thread for writing
        method = &Benchmark::ReadWhileWriting3;
      } else if (name == Slice("compact")) {
        method = &Benchmark::Compact;
      } else if (name == Slice("crc32c")) {
        method = &Benchmark::Crc32c;
      } else if (name == Slice("snappycomp")) {
        method = &Benchmark::SnappyCompress;
      } else if (name == Slice("snappyuncomp")) {
        method = &Benchmark::SnappyUncompress;
      } else if (name == Slice("rangequery")) {
        method = &Benchmark::RangeQuery;
      } else if (name == Slice("load")) {
        fresh_db = true;
        num_threads = 1;
        method = &Benchmark::YCSBLoad;
      } else if (name == Slice("loadreverse")) {
        num_threads = 1;
        method = &Benchmark::YCSBReverse;
      } else if (name == Slice("ycsba")) {
        FLAGS_ycsb_type = kYCSB_A;
        method = &Benchmark::YCSB;
      } else if (name == Slice("ycsbb")) {
        FLAGS_ycsb_type = kYCSB_B;
        method = &Benchmark::YCSB;
      } else if (name == Slice("ycsbc")) {
        FLAGS_ycsb_type = kYCSB_C;
        method = &Benchmark::YCSB;
      } else if (name == Slice("ycsbd")) {
        FLAGS_ycsb_type = kYCSB_D;
        method = &Benchmark::YCSB;
      } else if (name == Slice("ycsbe")) {
        FLAGS_ycsb_type = kYCSB_E;
        method = &Benchmark::YCSB;
      } else if (name == Slice("ycsbf")) {
        FLAGS_ycsb_type = kYCSB_F;
        method = &Benchmark::YCSB;
      } else if (name == Slice("r25")) {
        method = &Benchmark::R25W75;
      } else if (name == Slice("r50")) {
        method = &Benchmark::R50W50;
      } else if (name == Slice("r75")) {
        method = &Benchmark::R75W25;
      } else if (name == Slice("r100")) {
        method = &Benchmark::R100;
      } else if (name == Slice("r0")) {
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("heapprofile")) {
        HeapProfile();
      } else if (name == Slice("stats")) {
        PrintStats("leveldb.stats");
      } else if (name == Slice("sstables")) {
        PrintStats("leveldb.sstables");
      } else {
        if (name != Slice()) {  // No error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.ToString().c_str());
        }
      }

      if (fresh_db) {
        if (FLAGS_use_existing_db) {
          fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                  name.ToString().c_str());
          method = nullptr;
        } else {
          delete db_;
          db_ = nullptr;
          DestroyDB(FLAGS_db, Options());
          Open();
        }
      }

      if (method != nullptr) {
        RunBenchmark(num_threads, name, method);
      }
    }
  }

 private:
  struct ThreadArg {
    Benchmark* bm;
    SharedState* shared;
    ThreadState* thread;
    void (Benchmark::*method)(ThreadState*);
  };

  static void ThreadBody(void* v) {
    ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
    SharedState* shared = arg->shared;
    ThreadState* thread = arg->thread;
    {
      MutexLock l(&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.SignalAll();
      }
      while (!shared->start) {
        shared->cv.Wait();
      }
    }

    thread->stats.Start();
    (arg->bm->*(arg->method))(thread);
    thread->stats.Stop();

    {
      MutexLock l(&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.SignalAll();
      }
    }
  }

  void RunBenchmark(int n, Slice name,
                    void (Benchmark::*method)(ThreadState*)) {
    SharedState shared(n);

    ThreadArg* arg = new ThreadArg[n];
    for (int i = 0; i < n; i++) {
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      arg[i].thread = new ThreadState(i);
      arg[i].thread->shared = &shared;
      g_env->StartThread(ThreadBody, &arg[i]);
    }

    shared.mu.Lock();
    while (shared.num_initialized < n) {
      shared.cv.Wait();
    }

    shared.start = true;
    shared.cv.SignalAll();
    while (shared.num_done < n) {
      shared.cv.Wait();
    }
    shared.mu.Unlock();

    for (int i = 1; i < n; i++) {
      arg[0].thread->stats.Merge(arg[i].thread->stats);
    }
    arg[0].thread->stats.Report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
  }

  void Crc32c(ThreadState* thread) {
    // Checksum about 500MB of data total
    const int size = 4096;
    const char* label = "(4K per op)";
    std::string data(size, 'x');
    int64_t bytes = 0;
    uint32_t crc = 0;
    while (bytes < 500 * 1048576) {
      crc = crc32c::Value(data.data(), size);
      thread->stats.FinishedSingleOp();
      bytes += size;
    }
    // Print so result is not dead
    fprintf(stderr, "... crc=0x%x\r", static_cast<unsigned int>(crc));

    thread->stats.AddBytes(bytes);
    thread->stats.AddMessage(label);
  }

  void SnappyCompress(ThreadState* thread) {
    RandomGenerator gen;
    Slice input = gen.Generate(Options().block_size);
    int64_t bytes = 0;
    int64_t produced = 0;
    bool ok = true;
    std::string compressed;
    while (ok && bytes < 1024 * 1048576) {  // Compress 1G
      ok = port::Snappy_Compress(input.data(), input.size(), &compressed);
      produced += compressed.size();
      bytes += input.size();
      thread->stats.FinishedSingleOp();
    }

    if (!ok) {
      thread->stats.AddMessage("(snappy failure)");
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "(output: %.1f%%)",
               (produced * 100.0) / bytes);
      thread->stats.AddMessage(buf);
      thread->stats.AddBytes(bytes);
    }
  }

  void SnappyUncompress(ThreadState* thread) {
    RandomGenerator gen;
    Slice input = gen.Generate(Options().block_size);
    std::string compressed;
    bool ok = port::Snappy_Compress(input.data(), input.size(), &compressed);
    int64_t bytes = 0;
    char* uncompressed = new char[input.size()];
    while (ok && bytes < 1024 * 1048576) {  // Compress 1G
      ok =  port::Snappy_Uncompress(compressed.data(), compressed.size(),
                                    uncompressed);
      bytes += input.size();
      thread->stats.FinishedSingleOp();
    }
    delete[] uncompressed;

    if (!ok) {
      thread->stats.AddMessage("(snappy failure)");
    } else {
      thread->stats.AddBytes(bytes);
    }
  }

  void Open() {
    assert(db_ == nullptr);
    Options options;
    options.env = g_env;
    options.create_if_missing = !FLAGS_use_existing_db;
    options.block_cache = cache_;
    options.write_buffer_size = FLAGS_write_buffer_size;
    options.max_file_size = FLAGS_max_file_size;
    options.block_size = FLAGS_block_size;
    options.max_open_files = FLAGS_open_files;
    options.filter_policy = filter_policy_;
    options.reuse_logs = FLAGS_reuse_logs;
    options.log_path = FLAGS_logpath;
    options.log = FLAGS_log;
    Status s = DB::Open(options, FLAGS_db, &db_);
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.ToString().c_str());
      exit(1);
    }
  }

  void OpenBench(ThreadState* thread) {
    for (int i = 0; i < num_; i++) {
      delete db_;
      Open();
      thread->stats.FinishedSingleOp();
    }
  }

  void WriteSeq(ThreadState* thread) {
    DoWrite(thread, true);
  }

  void WriteRandom(ThreadState* thread) {
    DoWrite(thread, false);
  }

  uint64_t
random_uint64(void)
{
  // 62 bit random value;
  const uint64_t rand64 = (((uint64_t)random()) << 31) + ((uint64_t)random());
  return rand64;
}

#define RAND64_MAX   ((((uint64_t)RAND_MAX) << 31) + ((uint64_t)RAND_MAX))
#define RAND64_MAX_D ((double)(RAND64_MAX))

  double
random_double(void)
{
  // random between 0.0 - 1.0
  const double r = (double)random_uint64();
  const double rd = r / RAND64_MAX_D;
  return rd;
}

  bool YCSBOperation(const YCSB_Op& operation, const Slice& ivalue, std::string* ovalue) {
    static ReadOptions roption;
    static WriteOptions woption;
    static TraceUniform seekrnd(333, 1, 100);
    bool res = false;
    char key[100];
    char value_buffer[256];
    snprintf(key, sizeof(key), "%016llu", (unsigned long long)operation.key);
    if (operation.type == kYCSB_Write) {
      res = db_->Put(woption,key, ivalue).ok();
    } else if (operation.type == kYCSB_Read) {
      res = db_->Get(roption, key, ovalue).ok();
    } else if (operation.type == kYCSB_Query) {
      auto* iter = db_->NewIterator(roption);
      iter->Seek(key);
      if (iter->Valid()) {
        if (iter->key() == key) {
	    		  res = true;
        }
        int seeks = FLAGS_seek_nexts * (seekrnd.Next() / 100.0);
        for (int j = 0; j < seeks && iter->Valid(); j++) {
            // Copy out iterator's value to make sure we read them.
            Slice value = iter->value();
            memcpy(value_buffer, value.data(),
            std::min(value.size(), sizeof(value_buffer)));
            iter->Next();
        }
      }
      delete iter;
      iter = nullptr;
    } else if (operation.type == kYCSB_ReadModifyWrite) {
        res = db_->Get(roption, key, ovalue).ok();
        if (res) {
          db_->Put(woption, key, ivalue);
        }
    }

    return res;
  }

  void YCSBReverse(ThreadState* thread) {
    if (ycsb_insertion_sequence.size() != FLAGS_num)
      RandomSequence(FLAGS_num, ycsb_insertion_sequence);

    // reverse insertion order, make the latest to fronter
    std::reverse(ycsb_insertion_sequence.begin(), ycsb_insertion_sequence.end());
  }

  void YCSBLoad(ThreadState* thread) {
    static WriteOptions woption;
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    
    int64_t bytes = 0;

    // generate a random sequence
    RandomSequence(FLAGS_num, ycsb_insertion_sequence);

    thread->stats.Start();
    for (uint64_t i = 0; i < FLAGS_num;) {
      batch.Clear();
      for (uint64_t j = 0; j < entries_per_batch_; j++) {
        const uint64_t k = ycsb_insertion_sequence[i + j];
        char key[100];
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        batch.Put(key, gen.Generate(value_size_));
        bytes += value_size_ + strlen(key);
        thread->stats.FinishedSingleOp(db_);
      }
      s = db_->Write(write_options_, &batch);
      i+=entries_per_batch_;
    }
    thread->stats.AddBytes(bytes);
  }

  void YCSB(ThreadState* thread) {
    Trace* ycsb_selector = nullptr;
    if (FLAGS_ycsb_type == kYCSB_D) {      
      // use exponential distribution as selector to select more on front index
      ycsb_selector = new TraceExponential(kYCSB_LATEST_SEED + thread->tid * 996, 50, FLAGS_num);
    }
    else {
      ycsb_selector = new TraceUniform(kYCSB_SEED + FLAGS_ycsb_type * 333 + thread->tid * 996);
    }

    std::vector<YCSB_Op> ycsb_ops = YCSB_LoadGenerate(FLAGS_num, FLAGS_ycsb_ops_num, FLAGS_ycsb_type, ycsb_selector, ycsb_insertion_sequence);
    Status s;
    uint64_t len = FLAGS_ycsb_ops_num;
    if (FLAGS_ycsb_type == kYCSB_E) {
      len = len / 4;
    }
    int64_t bytes = 0;

    thread->stats.Start();
    uint64_t found = 0;
    uint64_t total_ops = 0;
    std::string ovalue;
    for (uint64_t i = 0; i < len; ++i) {
      total_ops++;
      Slice ivalue = thread->gen.Generate(value_size_);
      bool op_res = YCSBOperation(ycsb_ops[i], ivalue, &ovalue);
      if ((ycsb_ops[i].type == kYCSB_Read || ycsb_ops[i].type == kYCSB_ReadModifyWrite || ycsb_ops[i].type == kYCSB_Query) && 
          op_res) {
        found++;
      }
      bytes += ycsb_ops[i].type == kYCSB_Query ? (value_size_  + 16) * FLAGS_seek_nexts : (value_size_  + 16);
      thread->stats.FinishedSingleOp();
    } 
    thread->stats.AddBytes(bytes);
    
    char msg[100];
    snprintf(msg, sizeof(msg), "(%llu of %llu found)", found, total_ops);
    thread->stats.AddMessage(msg);
  }

  void R100(ThreadState* thread) {
    printf("FLAGS_range: %llu\n", FLAGS_range);

    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    Duration duration(0, FLAGS_ycsb_ops_num);
    uint64_t found = 0;
    ReadOptions roption;
    WriteOptions woption;
    uint64_t i = 0;
    while (!duration.Done(1)) {
      // 100% read
      {     
        std::string value;
        char key[100];
        const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range  * 0.3);
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        if (db_->Get(roption, key, &value).ok()) {
          found++;
        }
      }
      thread->stats.FinishedSingleOp();
      ++i;
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : (%" PRIu64 " of %" PRIu64 " found)\n", thread->tid, found, FLAGS_ycsb_ops_num);
    thread->stats.AddMessage(msg);
  }

  void R75W25(ThreadState* thread) {
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    Duration duration(0, FLAGS_ycsb_ops_num);
    uint64_t found = 0;
    ReadOptions roption;
    WriteOptions woption;
    uint64_t i = 0;
    while (!duration.Done(1)) {
      // 25% write
      if (i % 4 == 0) {
        const uint64_t k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        db_->Put(woption, key, gen.Generate(value_size_));
      }
      else {     
        std::string value;
        char key[100];
        const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range  * 0.3 );
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        if (db_->Get(roption, key, &value).ok()) {
          found++;
        }
      }
      thread->stats.FinishedSingleOp();
      ++i;
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : (%" PRIu64 " of %" PRIu64 " found)\n", thread->tid, found, FLAGS_ycsb_ops_num);
    thread->stats.AddMessage(msg);
  }

  void R50W50(ThreadState* thread) {
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    Duration duration(0, FLAGS_ycsb_ops_num);
    uint64_t found = 0;
    ReadOptions roption;
    WriteOptions woption;
    uint64_t i = 0;
    while (!duration.Done(1)) {
      // half write
      if (i % 2 == 0) {
        const uint64_t k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        db_->Put(woption, key, gen.Generate(value_size_));
      }
      else {     
        std::string value;
        char key[100];
        const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range  * 0.3 );
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        if (db_->Get(roption, key, &value).ok()) {
          found++;
        }
      }
      thread->stats.FinishedSingleOp();
      ++i;
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : (%" PRIu64 " of %" PRIu64 " found)\n", thread->tid, found, FLAGS_ycsb_ops_num);
    thread->stats.AddMessage(msg);
  }

  void R25W75(ThreadState* thread) {
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    Duration duration(0, FLAGS_ycsb_ops_num);
    uint64_t found = 0;
    ReadOptions roption;
    WriteOptions woption;
    uint64_t i = 0;
    while (!duration.Done(1)) {
      // 80% write
      if (i % 5 != 0) {
        const uint64_t k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        db_->Put(woption, key, gen.Generate(value_size_));
      }
      else {     
        std::string value;
        char key[100];
        const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range  * 0.3);
        snprintf(key, sizeof(key), "%016" PRIu64 "", k);
        if (db_->Get(roption, key, &value).ok()) {
          found++;
        }
      }
      thread->stats.FinishedSingleOp();
      ++i;
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : (%" PRIu64 " of %" PRIu64 " found)\n", thread->tid, found, FLAGS_ycsb_ops_num);
    thread->stats.AddMessage(msg);
  }

  void DoWrite(ThreadState* thread, bool seq) {
    if (num_ != FLAGS_num) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%llu ops)", num_);
      thread->stats.AddMessage(msg);
    }

    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    int64_t bytes = 0;
    for (uint64_t i = 0; i < num_; i += entries_per_batch_) {
      batch.Clear();
      for (uint64_t j = 0; j < entries_per_batch_; j++) {
        const uint64_t k = seq ? i+j : (thread->trace->Next() % FLAGS_range);
        char key[100];
        snprintf(key, sizeof(key), "%016llu", (unsigned long long)k);
        batch.Put(key, gen.Generate(value_size_));
        bytes += value_size_ + strlen(key);
        thread->stats.FinishedSingleOp(db_);
      }
      s = db_->Write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.ToString().c_str());
        exit(1);
      }
    }
    thread->stats.AddBytes(bytes);
  }

  void ReadSequential(ThreadState* thread) {
    Iterator* iter = db_->NewIterator(ReadOptions());
    uint64_t i = 0;
    int64_t bytes = 0;
    for (iter->SeekToFirst(); i < reads_ && iter->Valid(); iter->Next()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.FinishedSingleOp();
      ++i;
    }
    delete iter;
    thread->stats.AddBytes(bytes);
  }

  void ReadReverse(ThreadState* thread) {
    Iterator* iter = db_->NewIterator(ReadOptions());
    int i = 0;
    int64_t bytes = 0;
    for (iter->SeekToLast(); i < reads_ && iter->Valid(); iter->Prev()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.FinishedSingleOp();
      ++i;
    }
    delete iter;
    thread->stats.AddBytes(bytes);
  }

  void ReadRandom(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    uint64_t found = 0;
    thread->trace = new TraceUniform(random());
    for (uint64_t i = 0; i < reads_; i++) {
      char key[100];
      const uint64_t k = thread->trace->Next() % FLAGS_range;
      snprintf(key, sizeof(key), "%016llu", (unsigned long long) k);
      if (db_->Get(options, key, &value).ok()) {
        found++;
      }
      thread->stats.FinishedSingleOp();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%llu of %llu found)", (unsigned long long)found, (unsigned long long)FLAGS_num);
    printf("thread %d (%" PRIu64 " of %" PRIu64 " found)\n", thread->tid, found, FLAGS_num);
    thread->stats.AddMessage(msg);
  }


  void RangeQuery(ThreadState* thread) {
    ReadOptions options;
    uint64_t found = 0;
    char value_buffer[256];
    for (uint64_t i = 0; i < reads_; i++) {
      auto* iter = db_->NewIterator(options);
      char key[100];
      const uint64_t k = thread->trace->Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016llu", k);
      iter->Seek(key);
      if (iter->Valid()) {
        if (iter->key() == key) {
	    		  found++;
        }
        for (int j = 0; j < FLAGS_seek_nexts && iter->Valid(); j++) {
            // make sure we read from file
            Slice value = iter->value();
            memcpy(value_buffer, value.data(),
            std::min(value.size(), sizeof(value_buffer)));
            iter->Next();
        }
      }
      delete iter;
      thread->stats.FinishedSingleOp();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, num_);
    thread->stats.AddMessage(msg);
  }

  void ReadMissing(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const uint64_t k = thread->trace->Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016llu.", k);
      db_->Get(options, key, &value);
      thread->stats.FinishedSingleOp();
    }
  }

  void ReadHot(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    const int range = (FLAGS_num + 99) / 100;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const uint64_t k = thread->trace->Next() % range;
      snprintf(key, sizeof(key), "%016llu", k);
      db_->Get(options, key, &value);
      thread->stats.FinishedSingleOp();
    }
  }

  void SeekRandom(ThreadState* thread) {
    ReadOptions options;
    uint64_t found = 0;
    for (int i = 0; i < reads_; i++) {
      Iterator* iter = db_->NewIterator(options);
      char key[100];
      const uint64_t k = thread->trace->Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016llu", k);
      iter->Seek(key);
      if (iter->Valid() && iter->key() == key) found++;
      delete iter;
      thread->stats.FinishedSingleOp();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%llu of %llu found)", found, num_);
    thread->stats.AddMessage(msg);
  }

  void DoDelete(ThreadState* thread, bool seq) {
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.Clear();
      for (int j = 0; j < entries_per_batch_; j++) {
        const int k = seq ? i+j : (thread->trace->Next() % FLAGS_num);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        batch.Delete(key);
        thread->stats.FinishedSingleOp();
      }
      s = db_->Write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "del error: %s\n", s.ToString().c_str());
        exit(1);
      }
    }
  }

  void DeleteSeq(ThreadState* thread) {
    DoDelete(thread, true);
  }

  void DeleteRandom(ThreadState* thread) {
    DoDelete(thread, false);
  }

  void ReadAll(ThreadState* thread) {
    ReadOptions options;
    uint64_t found = 0;
    thread->trace = new TraceUniform(random() + 996);
    for (uint64_t i = 0; i < FLAGS_ycsb_ops_num && FLAGS_writes > 0; i++) {
      std::string value;
      char key[100];
      const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range);
      snprintf(key, sizeof(key), "%016" PRIu64 "", k);
      if (db_->Get(options, key, &value).ok()) {
        found++;
      }
      thread->stats.FinishedSingleOp2();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : %s\n", thread->tid, msg);
    thread->stats.AddMessage(msg);
  }

  void ReadExp(ThreadState* thread) {
    ReadOptions options;
    uint64_t found = 0;
    thread->trace = new TraceExponential(random() + 996, 90, FLAGS_num);
    for (uint64_t i = 0; i < FLAGS_ycsb_ops_num && FLAGS_writes > 0; i++) {
      std::string value;
      char key[100];
      const uint64_t k = thread->trace->Next() % (uint64_t)(FLAGS_range);
      snprintf(key, sizeof(key), "%016" PRIu64 "", k);
      if (db_->Get(options, key, &value).ok()) {
        found++;
      }
      thread->stats.FinishedSingleOp2();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%" PRIu64 " of %" PRIu64 " found)", found, FLAGS_ycsb_ops_num);
    printf("thread %d : %s\n", thread->tid, msg);
    thread->stats.AddMessage(msg);
  }
  
  void ReadWhileWriting(ThreadState* thread) {
    if (thread->tid > 0) {
      ReadAll(thread);
    } else {
      // Special thread that keeps writing until other threads are done.
      RandomGenerator gen;
      while (true) {
        {
          MutexLock l(&thread->shared->mu);
          if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
            // Other threads have finished
            break;
          }
        }

        const int k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        Status s = db_->Put(write_options_, key, gen.Generate(value_size_));
        thread->stats.FinishedSingleOp();
        uint64_t delay_end = g_env->NowMicros() + FLAGS_rwdelay;
        while (g_env->NowMicros() < delay_end) {
        }
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.ToString().c_str());
          exit(1);
        }
      }

      // Do not count any of the preceding work/delay in stats.
      thread->stats.Start();
    }
  }

  void ReadWhileWriting2(ThreadState* thread) {
    if (thread->tid > 0) {
      ReadAll(thread);
    } else {
      // Special thread that keeps writing until other threads are done.
      g_env->SleepForMicroseconds(FLAGS_sleep * 1000000); // sleep for 900 s
      RandomGenerator gen;
      thread->stats.Start();
      while (FLAGS_writes-- >= 0) {
        {
          MutexLock l(&thread->shared->mu);
          if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
            // Other threads have finished
            break;
          }
        }

        const int k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        Status s = db_->Put(write_options_, key, gen.Generate(value_size_));
        thread->stats.FinishedSingleOp2(db_);
        uint64_t delay_end = g_env->NowMicros() + FLAGS_rwdelay;
        while (g_env->NowMicros() < delay_end) {
        }
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.ToString().c_str());
          exit(1);
        }
      }
      
      // Do not count any of the preceding work/delay in stats.
      thread->stats.Start();
    }
  }

  void ReadWhileWriting3(ThreadState* thread) {
    if (thread->tid > 0) {
      ReadExp(thread);
    } else {
      // Special thread that keeps writing until other threads are done.
      g_env->SleepForMicroseconds(FLAGS_sleep * 1000000); // sleep for 900 s
      RandomGenerator gen;
      thread->stats.Start();
      while (FLAGS_writes-- >= 0) {
        {
          MutexLock l(&thread->shared->mu);
          if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
            // Other threads have finished
            break;
          }
        }

        const int k = thread->trace->Next() % FLAGS_range;
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        Status s = db_->Put(write_options_, key, gen.Generate(value_size_));
        thread->stats.FinishedSingleOp2(db_);
        uint64_t delay_end = g_env->NowMicros() + FLAGS_rwdelay;
        while (g_env->NowMicros() < delay_end) {
        }
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.ToString().c_str());
          exit(1);
        }
      }

      // Do not count any of the preceding work/delay in stats.
      thread->stats.Start();
    }
  }

  void Compact(ThreadState* thread) {
    db_->CompactRange(nullptr, nullptr);
  }

  void PrintStats(const char* key) {
    std::string stats;
    if (!db_->GetProperty(key, &stats)) {
      stats = "(failed)";
    }
    fprintf(stdout, "\n%s\n", stats.c_str());
  }

  static void WriteToFile(void* arg, const char* buf, int n) {
    reinterpret_cast<WritableFile*>(arg)->Append(Slice(buf, n));
  }

  void HeapProfile() {
    char fname[100];
    snprintf(fname, sizeof(fname), "%s/heap-%04llu", FLAGS_db.c_str(), ++heap_counter_);
    WritableFile* file;
    Status s = g_env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.ToString().c_str());
      return;
    }
    bool ok = port::GetHeapProfile(WriteToFile, file);
    delete file;
    if (!ok) {
      fprintf(stderr, "heap profiling not supported\n");
      g_env->DeleteFile(fname);
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
  FLAGS_write_buffer_size = 64 << 20;
  FLAGS_max_file_size = leveldb::Options().max_file_size;
  FLAGS_block_size = leveldb::Options().block_size;
  FLAGS_open_files = leveldb::Options().max_open_files;
  std::string default_db_path;

  for (int i = 0; i < argc; ++i) {
    printf("%s ", argv[i]);
  }
  printf("\n");
  ParseCommandLineFlags(&argc, &argv, true);


  // Choose a location for the test database if none given with --db=<path>
  if (FLAGS_db == "") {
      g_env->GetTestDirectory(&default_db_path);
      default_db_path += "/dbbench";
      FLAGS_db = default_db_path.c_str();
  }

  if (FLAGS_logpath == "") {
    FLAGS_logpath = FLAGS_db;
  }

  leveldb::Benchmark benchmark;
  benchmark.Run();
  return 0;
}
