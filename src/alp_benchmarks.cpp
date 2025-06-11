#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "fastlanes/ffor.hpp"
#include "fastlanes/unffor.hpp"

static void printusage(char *command) {
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n", command);
    printf("the -v flag turns on verbose mode\n");
}

static inline uint8_t bits_required(uint32_t x) {
    if(x==0) return 1;
    return 32 - __builtin_clz(x);
}

int main(int argc, char **argv) {
    bool verbose = false;
    char *extension = ".txt";
    int c;
    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c) {
        case 'v':
            verbose = true;
            break;
        case '?':
        default:
            printusage(argv[0]);
            return 0;
        }
    }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;
    size_t *howmany = NULL;
    uint32_t **numbers = read_all_integer_files(dirname, extension, &howmany, &count);
    if(numbers == NULL) {
        printf("I could not find or load any data file with extension %s in directory %s.\n", extension, dirname);
        return -1;
    }

    uint32_t maxvalue = 0;
    for(size_t i=0;i<count;i++) {
        if(howmany[i]>0 && numbers[i][howmany[i]-1]>maxvalue)
            maxvalue = numbers[i][howmany[i]-1];
    }
    uint64_t totalcard = 0;
    for(size_t i=0;i<count;i++) totalcard += howmany[i];

    uint64_t build_cycles = 0;
    uint64_t iter_cycles = 0;
    uint64_t totalsize = 0;

    uint32_t *encode_buf = (uint32_t*)malloc(sizeof(uint32_t)*1024);
    uint32_t *decode_buf = (uint32_t*)malloc(sizeof(uint32_t)*1024);
    uint32_t *tmp_buf = (uint32_t*)malloc(sizeof(uint32_t)*1024);

    for(size_t i=0;i<count;i++) {
        uint32_t *vals = numbers[i];
        size_t n = howmany[i];
        size_t pos = 0;
        while(pos < n) {
            size_t chunk = n - pos;
            if(chunk > 1024) chunk = 1024;
            uint32_t base = vals[pos];
            uint32_t maxv = base;
            for(size_t j=0;j<chunk;j++) if(vals[pos+j]>maxv) maxv = vals[pos+j];
            uint8_t bw = bits_required(maxv - base);
            if(bw>32) bw=32;
            for(size_t j=0;j<chunk;j++) tmp_buf[j] = vals[pos+j];
            for(size_t j=chunk;j<1024;j++) tmp_buf[j] = base;

            uint64_t start, end;
            RDTSC_START(start);
            ffor::ffor(tmp_buf, encode_buf, bw, &base);
            RDTSC_FINAL(end);
            build_cycles += end - start;

            RDTSC_START(start);
            unffor::unffor(encode_buf, decode_buf, bw, &base);
            RDTSC_FINAL(end);
            iter_cycles += end - start;
            for(size_t j=0;j<chunk;j++) if(decode_buf[j]!=vals[pos+j]) { fprintf(stderr,"decoding error\n"); return -1; }

            totalsize += ((uint64_t)bw * 1024 + 7)/8 + sizeof(uint32_t);
            pos += chunk;
        }
    }

    free(encode_buf);
    free(decode_buf);
    free(tmp_buf);

    for(size_t i=0;i<count;i++) {
        free(numbers[i]);
    }
    free(numbers);
    free(howmany);

    printf(" %20.4f %20.4f %20.4f\n",
           totalsize*25.0/totalcard,
           build_cycles*1.0/(totalcard*4),
           iter_cycles*1.0/(totalcard*4));
    return 0;
}
