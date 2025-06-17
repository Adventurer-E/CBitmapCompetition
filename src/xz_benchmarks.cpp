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
#include <lzma.h>

extern "C" {
#include "benchmark.h"
#include "numbersfromtextfiles.h"
}

static void printusage(char *command) {
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n", command);
    printf("the -v flag turns on verbose mode\n");
}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool verbose = false;
    while ((c = getopt(argc, argv, "ve:h")) != -1) switch (c) {
        case 'e': extension = optarg; break;
        case 'v': verbose = true; break;
        case 'h': printusage(argv[0]); return 0;
        default: abort();
    }
    if (optind >= argc) { printusage(argv[0]); return -1; }
    char *dirname = argv[optind];
    size_t count;
    size_t *howmany = NULL;
    uint32_t **numbers = read_all_integer_files(dirname, extension, &howmany, &count);
    if(numbers==NULL) {
        printf("I could not find or load any data file with extension %s in directory %s.\n", extension, dirname);
        return -1;
    }

    uint64_t totalcard = 0;
    for(size_t i=0;i<count;i++) totalcard += howmany[i];
    uint64_t build_cycles = 0, iter_cycles = 0, insert_cycles = 0, totalsize = 0;

    for(size_t i=0;i<count;i++) {
        size_t len = howmany[i]*sizeof(uint32_t);
        std::vector<uint8_t> input(len);
        memcpy(input.data(), numbers[i], len);
        size_t outsize = len + len/3 + 128; // rough upper bound
        std::vector<uint8_t> out(outsize);
        size_t out_pos = 0;
        uint64_t start,end;
        RDTSC_START(start);
        lzma_ret r = lzma_easy_buffer_encode(LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64, NULL,
                                             input.data(), len,
                                             out.data(), &out_pos, outsize);
        RDTSC_FINAL(end);
        insert_cycles += end-start;
        (void)r;
    }

    for(size_t i=0;i<count;i++) {
        size_t len = howmany[i]*sizeof(uint32_t);
        std::vector<uint8_t> input(len);
        memcpy(input.data(), numbers[i], len);
        size_t outsize = len + len/3 + 128;
        std::vector<uint8_t> out(outsize);
        size_t out_pos = 0;
        uint64_t start,end;
        RDTSC_START(start);
        lzma_ret r = lzma_easy_buffer_encode(LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64, NULL,
                                             input.data(), len,
                                             out.data(), &out_pos, outsize);
        RDTSC_FINAL(end);
        build_cycles += end-start;
        if(r != LZMA_OK) { fprintf(stderr,"compression error\n"); return -1; }
        out.resize(out_pos);
        totalsize += out.size();

        std::vector<uint8_t> decompressed(len);
        size_t dec_pos = 0; size_t in_pos = 0;
        RDTSC_START(start);
        lzma_ret dr = lzma_stream_buffer_decode(NULL, 0, NULL,
                                               out.data(), &in_pos, out.size(),
                                               decompressed.data(), &dec_pos, len);
        RDTSC_FINAL(end);
        iter_cycles += end-start;
        if(dr != LZMA_OK || dec_pos != len) { fprintf(stderr,"decoding error\n"); return -1; }
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

