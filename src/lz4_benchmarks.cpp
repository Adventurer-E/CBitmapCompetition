#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <cassert>
#include <lz4.h>

extern "C" {
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "floatsfromcsv.h"
}

static void printusage(char *command) {
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n", command);
    printf("the -v flag turns on verbose mode\n");
}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool verbose = false;
    bool floatdata = false;
    while ((c = getopt(argc, argv, "ve:fh")) != -1) switch (c) {
        case 'e': extension = optarg; break;
        case 'v': verbose = true; break;
        case 'f': floatdata = true; break;
        case 'h': printusage(argv[0]); return 0;
        default: abort();
    }
    if (optind >= argc) { printusage(argv[0]); return -1; }
    char *dirname = argv[optind];
    size_t count;
    size_t *howmany = NULL;
    uint32_t **numbers = floatdata ?
        read_all_float_files(dirname, extension, &howmany, &count) :
        read_all_integer_files(dirname, extension, &howmany, &count);
    if(numbers==NULL) {
        printf("I could not find or load any data file with extension %s in directory %s.\n", extension, dirname);
        return -1;
    }

    uint64_t totalcard = 0;
    for(size_t i=0;i<count;i++) totalcard += howmany[i];
    uint64_t build_cycles = 0, iter_cycles = 0, insert_cycles = 0, totalsize = 0;

    for(size_t i=0;i<count;i++) {
        size_t len = howmany[i]*sizeof(uint32_t);
        std::vector<char> input(len);
        memcpy(input.data(), numbers[i], len);
        int maxout = LZ4_compressBound(len);
        std::vector<char> out(maxout);
        uint64_t start,end;
        RDTSC_START(start);
        int csize = LZ4_compress_default(input.data(), out.data(), (int)len, maxout);
        RDTSC_FINAL(end);
        insert_cycles += end-start;
        (void)csize;
    }

    for(size_t i=0;i<count;i++) {
        size_t len = howmany[i]*sizeof(uint32_t);
        std::vector<char> input(len);
        memcpy(input.data(), numbers[i], len);
        int maxout = LZ4_compressBound(len);
        std::vector<char> out(maxout);
        uint64_t start,end;
        RDTSC_START(start);
        int csize = LZ4_compress_default(input.data(), out.data(), (int)len, maxout);
        RDTSC_FINAL(end);
        build_cycles += end-start;
        if(csize < 0) { fprintf(stderr,"compression error\n"); return -1; }
        out.resize(csize);
        totalsize += out.size();

        std::vector<char> decompressed(len);
        RDTSC_START(start);
        int dsize = LZ4_decompress_safe(out.data(), decompressed.data(), csize, (int)len);
        RDTSC_FINAL(end);
        iter_cycles += end-start;
        if(dsize != (int)len) { fprintf(stderr,"decoding error\n"); return -1; }
        if(memcmp(decompressed.data(), input.data(), len) != 0) { fprintf(stderr,"decoding error\n"); return -1; }
        if(verbose) fprintf(stderr, "compressed set %zu, bytes=%zu\n", i, out.size());
    }

    for(size_t i=0;i<count;i++) free(numbers[i]);
    free(numbers); free(howmany);

    printf(" %20.4f %20.4f %20.4f %20.4f\n",
           totalsize*8.0/totalcard,
           build_cycles*1.0/totalcard,
           insert_cycles*1.0/totalcard,
           iter_cycles*1.0/totalcard);
    return 0;
}

