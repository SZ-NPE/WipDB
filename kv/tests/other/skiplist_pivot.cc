#include "util/skiplist.h"
#include "gtest/gtest.h"


#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <unordered_set>
#include <set>

#include "kv/env.h"
#include "util/debug.h"
#include "util/random.h"
#include "util/generator.h"
#include "gflags/gflags.h"
#include "util/trace.h"

using namespace kv;


using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


DEFINE_int64(num, 100, "Number of key. Million");
DEFINE_int64(interval, 1, "interval in million");
DEFINE_string(patterns, "uniform", "");
DEFINE_string(path, "data/", "");
DEFINE_int64(range, 8000000000, "");

vector<Trace*> gi_array;


void SaveListPivit(std::multiset<uint64_t>& list, std::string filename, int numM) {
    FILE* fp = fopen (filename.c_str(), "w+");
    if(!fp) {
        std::perror("File opening failed");
        return;
    }
    auto iter = list.begin();
    auto iter_end = list.end();
    int i = 0;
    while(iter != iter_end){
        i++;
        if(i >= numM) { // 
            fprintf(fp, "%llu,\n", (unsigned long long) *iter);
            i = 0;
        }
        iter++;
    }
    fclose(fp);
}

Trace* &PickGi() {
    return gi_array[random() % gi_array.size()];
}

std::string WorkLoadName() {
    std::string workload_name;
    for(auto& trace: gi_array)
        workload_name += trace->gi_->get_type() + "_";
    return workload_name;
}

void PivitDraw(uint64_t num) {
    std::cout << gi_array.size() << " wordloads" << std::endl;
    for(auto& trace: gi_array) {
        fprintf(stdout, "-- %s \n", trace->gi_->get_type().c_str());
    }

    std::multiset<uint64_t> list;
    uint64_t next_report_ = 0;
    ::mkdir(FLAGS_path.c_str(), 0755);
    
    for (uint64_t i = 1; i <= num; i++) {
        Trace* gi = PickGi();
        list.insert(gi->Next());
        if (i >= next_report_) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )i, "");
            if(i % (FLAGS_interval*1000000) == 0) {
                int M = i / 1000000;
                std::string filename = FLAGS_path + "/" + WorkLoadName() + std::to_string(M) + "M.txt";
                SaveListPivit(list, filename.c_str(), M);
            }
            fflush(stderr);
        }
    }
}

Trace* GenerateGi(Slice p) {
    if(p == Slice("uniform")) return new TraceUniform(123, 0, FLAGS_range);
    else if(p == Slice("exp")) return new TraceExponential(123, 50, FLAGS_range);
    else if (p == Slice("expreverse")) return new TraceExponentialReverse(123, 50, FLAGS_range);
    else if(p == Slice("normal")) return new TraceNormal(123, 0, FLAGS_range);
    else return new TraceUniform(123, 0, FLAGS_range);
}

int main(int argc, char** argv) {
    ParseCommandLineFlags(&argc, &argv, true);
    std::srand(std::time(nullptr)); // use current time as seed for random generator

    const char* patterns = FLAGS_patterns.c_str();
    while (patterns != nullptr) {
      const char* sep = strchr(patterns, ',');
      Slice name;
      if (sep == nullptr) {
        name = patterns;
        patterns = nullptr;
      } else {
        name = Slice(patterns, sep - patterns);
        patterns = sep + 1;
      }
      printf("%s\n", name.ToString().c_str());
      gi_array.push_back(GenerateGi(name));
    }

    PivitDraw(FLAGS_num * 1000000);
   
    return 0;
}

// ./tests/other/skiplist_pivot --num=50 --interval=50 --patterns=uniform,exp --path=data