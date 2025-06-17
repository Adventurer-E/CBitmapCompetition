#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "alp/encoder.hpp"
#include "alp/decoder.hpp"
#include "fastlanes/ffor.hpp"
#include "fastlanes/unffor.hpp"
#include "alp/constants.hpp"

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

    uint64_t totalcard = 0;
    for(size_t i=0;i<count;i++) totalcard += howmany[i];

    uint64_t build_cycles = 0;
    uint64_t iter_cycles = 0;
    double totalsize = 0;

    double *input_buf = (double*)malloc(sizeof(double)*1024);
    double *exceptions = (double*)malloc(sizeof(double)*1024);
    uint16_t *exc_pos = (uint16_t*)malloc(sizeof(uint16_t)*1024);
    uint16_t *exc_count = (uint16_t*)malloc(sizeof(uint16_t));
    int64_t *encoded = (int64_t*)malloc(sizeof(int64_t)*1024);
    int64_t *ffor_buf = (int64_t*)malloc(sizeof(int64_t)*1024);
    int64_t *unffor_buf = (int64_t*)malloc(sizeof(int64_t)*1024);
    int64_t *base = (int64_t*)malloc(sizeof(int64_t));
    double *decoded = (double*)malloc(sizeof(double)*1024);
    double *sample = (double*)malloc(sizeof(double)*1024);

    for(size_t i=0;i<count;i++) {
        uint32_t *vals = numbers[i];
        size_t n = howmany[i];
        size_t pos = 0;
        while(pos < n) {
            size_t chunk = n - pos;
            if(chunk > 1024) chunk = 1024;
            for(size_t j=0;j<chunk;j++) input_buf[j] = (double)vals[pos+j];
            for(size_t j=chunk;j<1024;j++) input_buf[j] = input_buf[chunk-1];

            alp::state<double> stt;
            alp::encoder<double>::init(input_buf, 0, chunk, sample, stt);

            uint64_t start,end;
            RDTSC_START(start);
            alp::encoder<double>::encode(input_buf, exceptions, exc_pos, exc_count, encoded, stt);
            alp::encoder<double>::analyze_ffor(encoded, stt.bit_width, base);
            ffor::ffor(encoded, ffor_buf, stt.bit_width, base);
            RDTSC_FINAL(end);
            build_cycles += end - start;

            RDTSC_START(start);
            unffor::unffor(ffor_buf, unffor_buf, stt.bit_width, base);
            alp::decoder<double>::decode(unffor_buf, stt.fac, stt.exp, decoded);
            alp::decoder<double>::patch_exceptions(decoded, exceptions, exc_pos, exc_count);
            RDTSC_FINAL(end);
            iter_cycles += end - start;

            for(size_t j=0;j<chunk;j++) if(decoded[j] != (double)vals[pos+j]) { fprintf(stderr,"decoding error\n"); return -1; }

            totalsize += stt.bit_width * 1024.0;
            totalsize += exc_count[0] * ((double)alp::Constants<double>::EXCEPTION_SIZE + alp::EXCEPTION_POSITION_SIZE);
            totalsize += 8 + 8 + 8 + sizeof(int64_t)*8;

            pos += chunk;
        }
    }

    free(input_buf);
    free(exceptions);
    free(exc_pos);
    free(exc_count);
    free(encoded);
    free(ffor_buf);
    free(unffor_buf);
    free(base);
    free(decoded);
    free(sample);

    for(size_t i=0;i<count;i++) {
        free(numbers[i]);
    }
    free(numbers);
    free(howmany);

    printf(" %20.4f %20.4f %20.4f\n",
           totalsize/totalcard,
           build_cycles*1.0/totalcard,
           iter_cycles*1.0/totalcard);
    return 0;
}
