#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <list>
#include <cassert>
#ifdef __cplusplus
extern "C" {
#endif
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#ifdef __cplusplus
}
#endif

#include "concise.h" /* from Concise library */


/**
 * Once you have collected all the integers, build the bitmaps.
 */
static std::vector<ConciseSet<false> > create_all_bitmaps(size_t *howmany,
        uint32_t **numbers, size_t count) {
    if (numbers == NULL) return std::vector<ConciseSet<false> >();
    std::vector<ConciseSet<false> > answer(count);
    for (size_t i = 0; i < count; i++) {
        ConciseSet<false> & bm = answer[i];
        uint32_t * mynumbers = numbers[i];
        for(size_t j = 0; j <  howmany[i] ; ++j) {
            bm.add(mynumbers[j]);
        }
        bm.compact();
        assert(bm.size() == howmany[i]);
     }
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    ;
    printf("the -v flag turns on verbose mode");

}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool verbose = false;
    uint64_t data[14];
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
    std::vector<ConciseSet<false> > tmp_insert = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    data[13] = cycles_final - cycles_start; // incremental insertion cycles
    tmp_insert.clear();

    RDTSC_START(cycles_start);
    std::vector<ConciseSet<false> > bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    uint64_t build_cycles = cycles_final - cycles_start;
    if (bitmaps.empty()) return -1;
    if(verbose) printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    uint64_t totalsize = 0;

    for (int i = 0; i < (int) count; ++i) {
        ConciseSet<false> & bv = bitmaps[i];
        totalsize += bv.sizeInBytes(); // should be close enough to memory usage
    }
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
        ConciseSet<false>  tempand = bitmaps[i].logicaland(bitmaps[i + 1]);
        successive_and += tempand.size();
    }
    RDTSC_FINAL(cycles_final);
    data[1] = cycles_final - cycles_start;
    if(verbose) printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        ConciseSet<false>  tempor = bitmaps[i].logicalor(bitmaps[i + 1]);
        successive_or += tempor.size();
    }
    RDTSC_FINAL(cycles_final);
    data[2] = cycles_final - cycles_start;
    if(verbose) printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    if(count>1) {
        ConciseSet<false>  totalorbitmap  = bitmaps[0].logicalor(bitmaps[1]);
        for(int i = 2 ; i < (int) count; ++i) {
          ConciseSet<false> tmp = totalorbitmap.logicalor(bitmaps[i]);
          totalorbitmap.swap(tmp);
        }
        total_or = totalorbitmap.size();
    }
    RDTSC_FINAL(cycles_final);
    data[3] = cycles_final - cycles_start;
    if(verbose) printf("Total naive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);
    RDTSC_START(cycles_start);
    if(count>1) {
        const ConciseSet<false>  ** allofthem = new const ConciseSet<false>* [count];
        for(int i = 0 ; i < (int) count; ++i) allofthem[i] = & bitmaps[i];
        ConciseSet<false>  totalorbitmap = ConciseSet<false>::fast_logicalor(count, allofthem);
        total_or = totalorbitmap.size();
        delete[] allofthem;
    }
    RDTSC_FINAL(cycles_final);
    data[4] = cycles_final - cycles_start;
    if(verbose) printf("Total heap unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
                           cycles_final - cycles_start);

    uint64_t quartcount;
    STARTBEST(quartile_test_repetitions)
    quartcount = 0;
    for (size_t i = 0; i < count ; ++i) {
      quartcount += bitmaps[i].contains(maxvalue/4);
      quartcount += bitmaps[i].contains(maxvalue/2);
      quartcount += bitmaps[i].contains(3*maxvalue/4);
    }
    ENDBEST(data[5])

    if(verbose) printf("Quartile queries on %zu bitmaps took %" PRIu64 " cycles\n", count,
           data[5]);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        ConciseSet<false>  tempandnot = bitmaps[i].logicalandnot(bitmaps[i + 1]);
        successive_andnot += tempandnot.size();
    }
    RDTSC_FINAL(cycles_final);
    data[6] = cycles_final - cycles_start;

    if(verbose) printf("Successive differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);


    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        ConciseSet<false>  tempxor = bitmaps[i].logicalxor(bitmaps[i + 1]);
        successive_xor += tempxor.size();
    }
    RDTSC_FINAL(cycles_final);
    data[7] = cycles_final - cycles_start;

    if(verbose) printf("Successive symmetric differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (size_t i = 0; i < count; ++i) {
        ConciseSet<false> & b = bitmaps[i];
        std::vector<uint32_t> tmp;
        for(auto j = b.begin(); j != b.end() ; ++j) {
            tmp.push_back(*j);
        }
        total_count += tmp.size();
    }
    RDTSC_FINAL(cycles_final);
    data[8] = cycles_final - cycles_start;
    assert(successive_xor + successive_and == successive_or);

    assert(total_count == totalcard);

    if(verbose) printf("Decompressing %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    /* no batch decompression timing */


    if(verbose) printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 " %" PRIu64 "\n",successive_and,successive_or,total_or,quartcount);


    /**
    * and, or, andnot and xor cardinality
    */
    uint64_t successive_andcard = 0;
    uint64_t successive_orcard = 0;
    uint64_t successive_andnotcard = 0;
    uint64_t successive_xorcard = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_andcard += bitmaps[i].logicalandCount(bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[9] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_orcard += bitmaps[i].logicalorCount(bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[10] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_andnotcard += bitmaps[i].logicalandnotCount(bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[11] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_xorcard += bitmaps[i].logicalxorCount(bitmaps[i + 1]);
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

    printf(" %20.4f %20.4f %20.4f %20.4f\n",
      data[0]*8.0/totalcard,
      build_cycles*1.0/totalcard,
      data[13]*1.0/totalcard,
      data[8]*1.0/totalcard
    );
    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
    }
    free(howmany);
    free(numbers);

    return 0;
}
