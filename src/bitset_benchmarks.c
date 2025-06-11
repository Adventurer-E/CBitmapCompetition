#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <assert.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "bitset.h"
/**
 * Once you have collected all the integers, build the bitmaps.
 */
static bitset_t **create_all_bitmaps(size_t *howmany,
        uint32_t **numbers, size_t count) {
    if (numbers == NULL) return NULL;
    bitset_t **answer = (bitset_t**) malloc(sizeof(bitset_t *) * count);
    for (size_t i = 0; i < count; i++) {
      uint32_t biggest = numbers[i][howmany[i]-1];
      answer[i] = bitset_create_with_capacity(biggest+1);
      for(size_t j = 0; j < howmany[i] ; ++j)
        bitset_set(answer[i],numbers[i][j]);
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

int bitset_size_compare (const void * a, const void * b) {
   return ( bitset_size_in_bytes(*(const bitset_t**)a) - bitset_size_in_bytes(*(const bitset_t**)b) );
}

bool increment(size_t value, void *param) {
   uint64_t k; 
   memcpy(&k, param, sizeof(uint64_t));
   k += 1;
   memcpy(param, &k, sizeof(uint64_t));
   return true;
}

int main(int argc, char **argv) {
    int c;
    bool verbose = false;
    char *extension = (char *) ".txt";
    uint64_t data[13];
    while ((c = getopt(argc, argv, "vre:h")) != -1) switch (c) {
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
    bitset_t **bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    uint64_t build_cycles = cycles_final - cycles_start;
    if (bitmaps == NULL) return -1;
    if(verbose) printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    uint64_t totalsize = 0;
    for (int i = 0; i < (int) count; ++i) {
        totalsize += bitset_size_in_bytes(bitmaps[i]);
    }
    data[0] = totalsize;
    if(verbose) printf("Total size in bytes =  %" PRIu64 " \n", totalsize);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    uint64_t successive_andnot = 0;
    uint64_t successive_xor = 0;
    uint64_t total_or = 0;
    uint64_t total_count = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bitset_t *tempand = bitset_copy(bitmaps[i]);
        bitset_inplace_intersection(tempand,bitmaps[i + 1]);
        successive_and += bitset_count(tempand);
        bitset_free(tempand);
    }
    RDTSC_FINAL(cycles_final);
    data[1] = cycles_final - cycles_start;

    if(verbose) printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bitset_t *tempor = bitset_copy(bitmaps[i]);
        if(!bitset_inplace_union(tempor,bitmaps[i + 1])) printf("failed to compute union");
        successive_or += bitset_count(tempor);
        bitset_free(tempor);
    }
    RDTSC_FINAL(cycles_final);
    if(verbose) printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    data[2] = cycles_final - cycles_start;
    RDTSC_START(cycles_start);
    if(count>1){
      bitset_t * totalorbitmap = bitset_copy(bitmaps[0]);
      for(size_t i = 1; i < count; ++i) {
        if(!bitset_inplace_union(totalorbitmap,bitmaps[i])) printf("failed to compute union");
      }
      total_or = bitset_count(totalorbitmap);
      bitset_free(totalorbitmap);
    }
    RDTSC_FINAL(cycles_final);
    if(verbose) printf("Total unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    data[3] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    if(count>1){
      bitset_t **sortedbitmaps = (bitset_t**) malloc(sizeof(bitset_t *) * count);
      memcpy(sortedbitmaps, bitmaps, sizeof(bitset_t *) * count);
      qsort (sortedbitmaps, count, sizeof(bitset_t **), bitset_size_compare);
      bitset_t * totalorbitmap = bitset_copy(sortedbitmaps[0]);
      for(size_t i = 1; i < count; ++i) {
        if(!bitset_inplace_union(totalorbitmap,sortedbitmaps[i])) printf("failed to compute union");
      }
      total_or = bitset_count(totalorbitmap);
      bitset_free(totalorbitmap);
      free(sortedbitmaps);
    }
    RDTSC_FINAL(cycles_final);
    if(verbose) printf("Total sorted unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    data[4] = cycles_final - cycles_start;

    uint64_t quartcount;
    STARTBEST(quartile_test_repetitions)
    quartcount = 0;
    for (size_t i = 0; i < count ; ++i) {
      quartcount += bitset_get(bitmaps[i],maxvalue/4);
      quartcount += bitset_get(bitmaps[i],maxvalue/2);
      quartcount += bitset_get(bitmaps[i],3*maxvalue/4);
    }
    ENDBEST(data[5])

    if(verbose) printf("Quartile queries on %zu bitmaps took %" PRIu64 " cycles\n", count,
           data[5]);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bitset_t *tempandnot = bitset_copy(bitmaps[i]);
        bitset_inplace_difference(tempandnot,bitmaps[i + 1]);
        successive_andnot += bitset_count(tempandnot);
        bitset_free(tempandnot);
    }
    RDTSC_FINAL(cycles_final);
    data[6] = cycles_final - cycles_start;

    if(verbose) printf("Successive differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bitset_t *tempxor = bitset_copy(bitmaps[i]);
        bitset_inplace_symmetric_difference(tempxor,bitmaps[i + 1]);
        successive_xor += bitset_count(tempxor);
        bitset_free(tempxor);
    }
    RDTSC_FINAL(cycles_final);
    data[7] = cycles_final - cycles_start;

    if(verbose) printf("Successive symmetric differences on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (size_t i = 0; i < count; ++i) {
        bitset_t * b = bitmaps[i];
        bitset_for_each(b,increment,&total_count);
        //for(size_t j = 0; nextSetBit(b,&j) ; j++) {
        //    total_count++;
        //}
    }
    RDTSC_FINAL(cycles_final);
    data[8] = cycles_final - cycles_start;
    assert(total_count == totalcard);

    if(verbose) printf("Iterating over %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    if(verbose) printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",successive_and,successive_or,successive_andnot,successive_xor,total_or,quartcount);

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
        successive_andcard += bitset_intersection_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[9] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_orcard += bitset_union_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[10] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_andnotcard += bitset_difference_count(bitmaps[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    data[11] = cycles_final - cycles_start;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        successive_xorcard += bitset_symmetric_difference_count(bitmaps[i], bitmaps[i + 1]);
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
      data[0]*25.0/totalcard,
      build_cycles*1.0/(totalcard*4),
      data[8]*1.0/(totalcard*4)
    );
    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        bitset_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return 0;
}
