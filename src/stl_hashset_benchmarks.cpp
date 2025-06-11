#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <vector>
#include <unordered_set>
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



#ifdef MEMTRACKED
#include "memtrackingallocator.h"
#else
size_t memory_usage;
#endif

void initializeMemUsageCounter()  {
    memory_usage = 0;
}

uint64_t getMemUsageInBytes()  {
    return memory_usage;
}


#ifdef MEMTRACKED
typedef std::unordered_set<uint32_t,std::hash<uint32_t>,std::equal_to<uint32_t>,MemoryCountingAllocator<uint32_t> >  hashset;
#else
typedef std::unordered_set<uint32_t>  hashset;
#endif

/**
 * Once you have collected all the integers, build the bitmaps.
 */
static std::vector<hashset> create_all_bitmaps(size_t *howmany,
        uint32_t **numbers, size_t count) {
    if (numbers == NULL) return std::vector<hashset >();
    std::vector<hashset> answer(count);
    for (size_t i = 0; i < count; i++) {
        hashset & bm = answer[i];
        uint32_t * mynumbers = numbers[i];
        for(size_t j = 0; j < howmany[i] ; ++j) {
            bm.insert(mynumbers[j]);
        }
        bm.rehash(howmany[i]);
    }
    return answer;
}


static void intersection(hashset& h1, hashset& h2, hashset& answer) {
  if(h1.size() > h2.size()) {
    intersection(h2,h1,answer);
    return;
  }
  answer.clear();
  for(hashset::iterator i = h1.begin(); i != h1.end(); i++) {
    if(h2.find(*i) != h2.end())
      answer.insert(*i);
  }
}

static size_t intersection_count(hashset& h1, hashset& h2) {
  if(h1.size() > h2.size()) {
    return intersection_count(h2,h1);
  }
  size_t answer = 0;
  for(hashset::iterator i = h1.begin(); i != h1.end(); i++) {
    if(h2.find(*i) != h2.end()) ++answer;
  }
  return answer;
}


static void difference(hashset& h1, hashset& h2, hashset& answer) {
  answer.clear();
  for(hashset::iterator i = h1.begin(); i != h1.end(); i++) {
    if(h2.find(*i) == h2.end())
      answer.insert(*i);
  }
}


static size_t difference_count(hashset& h1, hashset& h2) {
  size_t answer = 0;
  for(hashset::iterator i = h1.begin(); i != h1.end(); i++) {
    if(h2.find(*i) == h2.end())
      answer++;
  }
  return answer;
}

static void symmetric_difference(hashset& h1, hashset& h2, hashset& answer) {
  answer.clear();
  answer.insert(h1.begin(), h1.end());
  for(hashset::iterator i = h2.begin(); i != h2.end(); i++) {
    auto x = answer.find(*i);
    if(x == answer.end())
      answer.insert(*i);
    else
      answer.erase(x);
  }
}

static size_t symmetric_difference_count(hashset& h1, hashset& h2) {
  return h1.size() + h2.size() - 2 * intersection_count(h1,h2);
}


static void inplace_union(hashset& h1, hashset& h2) {
  h1.insert(h2.begin(), h2.end());
}

static size_t union_count(hashset& h1, hashset& h2) {
  return h1.size() + h2.size() - intersection_count(h1,h2);
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    ;
    printf("the -v flag turns on verbose mode");

}




int hashset_size_compare (const void * a, const void * b) {
  return ( *(const hashset**)a)->size() - (*(const hashset**)b)->size() ;
}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool verbose = false;
    uint64_t data[13];
    initializeMemUsageCounter();
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

    RDTSC_START(cycles_start);
    std::vector<hashset > bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    uint64_t build_cycles = cycles_final - cycles_start;
    if (bitmaps.empty()) return -1;
    if(verbose) printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    uint64_t totalsize = getMemUsageInBytes();
    data[0] = totalsize;

    if(verbose) printf("Total size in bytes =  %" PRIu64 " \n", totalsize);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    uint64_t total_or = 0;
    uint64_t total_count = 0;
    uint64_t successive_andnot = 0;
    uint64_t successive_xor = 0;


    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        hashset v;
        intersection(bitmaps[i], bitmaps[i + 1], v);
        successive_and += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[1] = cycles_final - cycles_start;
    if(verbose) printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        hashset v (bitmaps[i]);
        inplace_union(v, bitmaps[i + 1]);
        successive_or += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[2] = cycles_final - cycles_start;
    if(verbose) printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    if(count>1) {
        hashset v (bitmaps[0]);
        inplace_union(v, bitmaps[1]);
        for (int i = 2; i < (int)count ; ++i) {
            inplace_union(v, bitmaps[i]);
        }
        total_or = v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[3] = cycles_final - cycles_start;
    if(verbose) printf("Total naive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);
    RDTSC_START(cycles_start);
    if(count>1){
      hashset **sortedbitmaps = (hashset**) malloc(sizeof(hashset*) * count);
      for (int i = 0; i < (int)count ; ++i) sortedbitmaps[i] = & bitmaps[i];
      qsort (sortedbitmaps, count, sizeof(hashset *), hashset_size_compare);
        hashset v (*sortedbitmaps[0]);
        for (int i = 1; i < (int)count ; ++i) {
            inplace_union(v, *sortedbitmaps[i]);
        }
        total_or = v.size();
        free(sortedbitmaps);
    }
    RDTSC_FINAL(cycles_final);
    data[4] = cycles_final - cycles_start;
    if(verbose) printf("Total sorted unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    uint64_t quartcount;
    STARTBEST(quartile_test_repetitions)
    quartcount = 0;
    for (size_t i = 0; i < count ; ++i) {
      quartcount += (bitmaps[i].find(maxvalue/4) == bitmaps[i].end());
      quartcount += (bitmaps[i].find(maxvalue/2) == bitmaps[i].end());
      quartcount += (bitmaps[i].find(3*maxvalue/4) == bitmaps[i].end());
    }
    ENDBEST(data[5])

    if(verbose) printf("Quartile queries on %zu bitmaps took %" PRIu64 " cycles\n", count,
           data[5]);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        hashset v;
        difference(bitmaps[i], bitmaps[i + 1], v);
        successive_andnot += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[6] = cycles_final - cycles_start;

    if(verbose) printf("Successive differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        hashset v;
        symmetric_difference(bitmaps[i], bitmaps[i + 1], v);
        successive_xor += v.size();
    }
    RDTSC_FINAL(cycles_final);
    data[7] = cycles_final - cycles_start;

    if(verbose) printf("Successive symmetric differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (size_t i = 0; i < count; ++i) {
        hashset & b = bitmaps[i];
        for(auto j = b.begin(); j != b.end() ; j++) {
            total_count++;
        }
    }
    RDTSC_FINAL(cycles_final);
    data[8] = cycles_final - cycles_start;
    assert(total_count == totalcard);

    if(verbose) printf("Iterating over %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);


    if(verbose) printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 " %" PRIu64 "\n",successive_and,successive_or,total_or,quartcount);

    assert(successive_xor + successive_and == successive_or);


    /**
    * and, or, andnot and xor cardinality
    */
    uint64_t successive_andcard = 0;
    uint64_t successive_orcard = 0;
    uint64_t successive_andnotcard = 0;
    uint64_t successive_xorcard = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_andcard += intersection_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[9] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_orcard += union_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[10] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_andnotcard += difference_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[11] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_xorcard += symmetric_difference_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[12] = cycles_final - cycles_start;

    assert(successive_andcard == successive_and);
    assert(successive_orcard == successive_or);
    assert(successive_xorcard == successive_xor);
    assert(successive_andnotcard == successive_andnot);

    /**
    * end and, or, andnot and xor cardinality
    */
    printf(" %20.2f %20.2f %20.2f\n",
      data[0]*8.0/totalcard,
      build_cycles*1.0/(totalcard*4),
      data[8]*1.0/(totalcard*4)
    );

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
    }
    free(howmany);
    free(numbers);

    return 0;
}
