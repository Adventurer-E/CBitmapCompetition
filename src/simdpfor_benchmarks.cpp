#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <vector>
#include <list>
#include <queue>
#include <cassert>

#ifdef __cplusplus
extern "C" {
#endif
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#ifdef __cplusplus
}
#endif

#include "codecfactory.h"

using namespace FastPForLib;

// helper iterator for counting results
template <typename T>
class count_back_inserter {
public:
    uint64_t & count;
    typedef void value_type;
    typedef void difference_type;
    typedef void pointer;
    typedef void reference;
    typedef std::output_iterator_tag iterator_category;
    count_back_inserter(uint64_t & c) : count(c) {};
    void operator=(const T &){ }
    count_back_inserter &operator *(){ return *this; }
    count_back_inserter &operator++(){ count++;return *this; }
};
typedef count_back_inserter<uint32_t> inserter;

typedef std::vector<uint32_t> vector;

static vector  fast_logicalor(size_t n, const vector **inputs) {
          class StdVectorPtr {
          public:
            StdVectorPtr(const vector *p, bool o) : ptr(p), own(o) {}
            const vector *ptr;
            bool own; // whether to clean
            bool operator<(const StdVectorPtr &o) const {
              return o.ptr->size() < ptr->size(); // backward on purpose
            }
          };
          if (n == 0) {
                return vector();
          }
          if (n == 1) {
            return vector(*inputs[0]);
          }
          std::priority_queue<StdVectorPtr> pq;
          for (size_t i = 0; i < n; i++) {
            pq.push(StdVectorPtr(inputs[i], false));
          }
          while (pq.size() > 2) {
            StdVectorPtr x1 = pq.top();
            pq.pop();
            StdVectorPtr x2 = pq.top();
            pq.pop();
            vector * buffer = new vector();
            std::set_union(x1.ptr->begin(), x1.ptr->end(),
                           x2.ptr->begin(), x2.ptr->end(),
                           std::back_inserter(*buffer));
            if (x1.own) {
              delete x1.ptr;
            }
            if (x2.own) {
              delete x2.ptr;
            }
            pq.push(StdVectorPtr(buffer, true));
          }
          StdVectorPtr x1 = pq.top();
          pq.pop();
          StdVectorPtr x2 = pq.top();
          pq.pop();
          vector  container;
          std::set_union(x1.ptr->begin(), x1.ptr->end(),
                         x2.ptr->begin(), x2.ptr->end(),
                         std::back_inserter(container));
          if (x1.own) {
            delete x1.ptr;
          }
          if (x2.own) {
            delete x2.ptr;
          }
          return container;
        }

static std::vector<vector> compress_all(size_t *howmany,
        uint32_t **numbers, size_t count, IntegerCODEC &codec,
        uint64_t *totalsize) {
    if (numbers == NULL) return std::vector<vector>();
    std::vector<vector> answer(count);
    *totalsize = 0;
    for (size_t i = 0; i < count; i++) {
        vector comp(howmany[i] * 2 + 1024);
        size_t nvalue = comp.size();
        codec.encodeArray(numbers[i], howmany[i], comp.data(), nvalue);
        comp.resize(nvalue);
        *totalsize += nvalue * sizeof(uint32_t);
        answer[i].swap(comp);
    }
    return answer;
}

static std::vector<vector> decompress_all(const std::vector<vector> &compressed,
        size_t *howmany, size_t count, IntegerCODEC &codec) {
    std::vector<vector> answer(count);
    for (size_t i = 0; i < count; i++) {
        vector out(howmany[i] + 1024);
        size_t nvalue = out.size();
        codec.decodeArray(compressed[i].data(), compressed[i].size(),
                          out.data(), nvalue);
        out.resize(nvalue);
        answer[i].swap(out);
    }
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    printf("the -v flag turns on verbose mode");
}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool verbose = false;
    uint64_t data[15];
    while ((c = getopt(argc, argv, "ve:h")) != -1) switch (c) {
        case 'e':
            extension = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        case 'h':
            printusage(argv[0]);
            return 0;
        default:
            abort();
        }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }
    uint32_t maxvalue = 0;
    for (size_t i = 0; i < count; i++) {
      if( howmany[i] > 0 ) {
        if(maxvalue < numbers[i][howmany[i]-1]) {
           maxvalue = numbers[i][howmany[i]-1];
         }
      }
    }
    uint64_t totalcard = 0;
    for (size_t i = 0; i < count; i++) {
      totalcard += howmany[i];
    }
    uint64_t successivecard = 0;
    for (size_t i = 1; i < count; i++) {
       successivecard += howmany[i-1] + howmany[i];
    }
    uint64_t cycles_start = 0, cycles_final = 0;

    CODECFactory factory;
    std::unique_ptr<IntegerCODEC> codec = simdpfor_codec();

    uint64_t totalsize = 0;
    RDTSC_START(cycles_start);
    std::vector<vector> tmp_insert = compress_all(howmany, numbers, count,
                                                  *codec, &totalsize);
    RDTSC_FINAL(cycles_final);
    data[13] = cycles_final - cycles_start; // incremental insertion cycles
    tmp_insert.clear();

    RDTSC_START(cycles_start);
    std::vector<vector> compressed = compress_all(howmany, numbers, count,
                                                  *codec, &totalsize);
    RDTSC_FINAL(cycles_final);
    uint64_t build_cycles = cycles_final - cycles_start;
    if (compressed.empty()) return -1;
    if(verbose) printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    data[0] = totalsize;
    if(verbose) printf("Total compressed size in bytes =  %" PRIu64 " \n", totalsize);

    // decompress once and store for operations, measuring time
    RDTSC_START(cycles_start);
    std::vector<vector> bitmaps = decompress_all(compressed, howmany, count,
                                                 *codec);
    RDTSC_FINAL(cycles_final);
    data[8] = cycles_final - cycles_start;
    data[14] = data[8];

    uint64_t total_count = 0;
    for (size_t i = 0; i < count; ++i) total_count += bitmaps[i].size();
    assert(total_count == totalcard);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    uint64_t total_or = 0;
    uint64_t successive_andnot = 0;
    uint64_t successive_xor = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        vector v;
        std::set_intersection(bitmaps[i].begin(), bitmaps[i].end(),
                              bitmaps[i+1].begin(), bitmaps[i+1].end(),
                              std::back_inserter(v));
        successive_and += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[1] = cycles_final - cycles_start;
    if(verbose) printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        vector v;
        std::set_union(bitmaps[i].begin(), bitmaps[i].end(),
                       bitmaps[i+1].begin(), bitmaps[i+1].end(),
                       std::back_inserter(v));
        successive_or += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[2] = cycles_final - cycles_start;
    if(verbose) printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    if(count>1) {
        vector v;
        std::set_union(bitmaps[0].begin(), bitmaps[0].end(),
                       bitmaps[1].begin(), bitmaps[1].end(),
                       std::back_inserter(v));
        for (int i = 2; i < (int)count ; ++i) {
            vector newv;
            std::set_union(v.begin(), v.end(),
                           bitmaps[i].begin(), bitmaps[i].end(),
                           std::back_inserter(newv));
            v.swap(newv);
        }
        total_or = v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[3] = cycles_final - cycles_start;
    if(verbose) printf("Total naive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);
    RDTSC_START(cycles_start);
    if(count>1) {
        const vector  ** allofthem = new const vector* [count];
        for(int i = 0 ; i < (int) count; ++i) allofthem[i] = & bitmaps[i];
        vector totalorbitmap = fast_logicalor(count, allofthem);
        total_or = totalorbitmap.size();
        delete[] allofthem;
    }
    RDTSC_FINAL(cycles_final);
    data[4] = cycles_final - cycles_start;
    if(verbose) printf("Total heap unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    uint64_t quartcount = 0;
    for (size_t i = 0; i < count ; ++i) {
      if ( std::binary_search(bitmaps[i].begin(),bitmaps[i].end(),maxvalue/4 ) )
        quartcount ++;
      if ( std::binary_search(bitmaps[i].begin(),bitmaps[i].end(),maxvalue/2 ) )
        quartcount ++;
      if ( std::binary_search(bitmaps[i].begin(),bitmaps[i].end(),3*maxvalue/4 ) )
        quartcount ++;
    }
    RDTSC_FINAL(cycles_final);
    data[5] = cycles_final - cycles_start;
    if(verbose) printf("Quartile queries on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    if(verbose) printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 " %" PRIu64 "\n",
        successive_and,successive_or,total_or,quartcount);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        vector v;
        std::set_difference(bitmaps[i].begin(), bitmaps[i].end(),
                            bitmaps[i+1].begin(), bitmaps[i+1].end(),
                            std::back_inserter(v));
        successive_andnot += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[6] = cycles_final - cycles_start;
    if(verbose) printf("Successive differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        vector v;
        std::set_symmetric_difference(bitmaps[i].begin(), bitmaps[i].end(),
                                      bitmaps[i+1].begin(), bitmaps[i+1].end(),
                                      std::back_inserter(v));
        successive_xor += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[7] = cycles_final - cycles_start;
    if(verbose) printf("Successive symmetric differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    assert(successive_xor + successive_and == successive_or);

    uint64_t successive_andcard = 0;
    uint64_t successive_orcard = 0;
    uint64_t successive_andnotcard = 0;
    uint64_t successive_xorcard = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
      std::set_intersection(bitmaps[i].begin(), bitmaps[i].end(),
                            bitmaps[i+1].begin(), bitmaps[i+1].end(),
                            inserter(successive_andcard));
    }
    RDTSC_FINAL(cycles_final);
    data[9] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
      std::set_union(bitmaps[i].begin(), bitmaps[i].end(),
                     bitmaps[i+1].begin(), bitmaps[i+1].end(),
                     inserter(successive_orcard));
    }
    RDTSC_FINAL(cycles_final);
    data[10] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
      std::set_difference(bitmaps[i].begin(), bitmaps[i].end(),
                          bitmaps[i+1].begin(), bitmaps[i+1].end(),
                          inserter(successive_andnotcard));
    }
    RDTSC_FINAL(cycles_final);
    data[11] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
      std::set_symmetric_difference(bitmaps[i].begin(), bitmaps[i].end(),
                                    bitmaps[i+1].begin(), bitmaps[i+1].end(),
                                    inserter(successive_xorcard));
    }
    RDTSC_FINAL(cycles_final);
    data[12] = cycles_final - cycles_start;

    assert(successive_andcard == successive_and);
    assert(successive_orcard == successive_or);
    assert(successive_xorcard == successive_xor);
    assert(successive_andnotcard == successive_andnot);

    printf(" %20.4f %20.4f %20.4f %20.4f %20.4f\n",
      data[0]*25.0/totalcard,
      build_cycles*1.0/(totalcard*4),
      data[8]*1.0/(totalcard*4),
      data[13]*1.0/(totalcard*4),
      data[14]*1.0/(totalcard*4)
    );

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;
    }
    free(howmany);
    free(numbers);

    return 0;
}