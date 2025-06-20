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

extern "C" {
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "floatsfromcsv.h"
}

// Simple bit stream writer using big-endian bit order
class OutputBitStream {
public:
    std::vector<uint8_t> buffer;
    uint8_t current{0};
    int bitsInCurrent{0};
    size_t bitsWritten{0};

    void writeBit(bool bit) {
        current <<= 1;
        if (bit) current |= 1;
        bitsInCurrent++;
        bitsWritten++;
        if(bitsInCurrent==8) {
            buffer.push_back(current);
            current = 0;
            bitsInCurrent = 0;
        }
    }

    void writeBits(uint32_t value, int bits) {
        for(int i=bits-1;i>=0;--i) writeBit((value>>i)&1);
    }

    void flush() {
        if(bitsInCurrent>0) {
            current <<= (8-bitsInCurrent);
            buffer.push_back(current);
            current = 0;
            bitsInCurrent = 0;
        }
    }
};

// Simple bit stream reader matching OutputBitStream
class InputBitStream {
public:
    const std::vector<uint8_t>& buffer;
    size_t pos{0};
    int bitPos{0};

    explicit InputBitStream(const std::vector<uint8_t>& buf) : buffer(buf) {}

    bool readBit() {
        uint8_t b = buffer[pos];
        bool bit = (b >> (7-bitPos)) & 1;
        bitPos++;
        if(bitPos==8) { bitPos=0; pos++; }
        return bit;
    }

    uint32_t readBits(int bits) {
        uint32_t v=0;
        for(int i=0;i<bits;i++) {
            v = (v<<1) | (readBit()?1:0);
        }
        return v;
    }
};

// Chimp32 compressor (simplified C++ port)
// C++ implementation of the Chimp32 codec as found in the Java
// submodule (src/main/java/gr/aueb/delorean/chimp).  The logic
// mirrors the Java version so that this benchmark can run
// without requiring a JVM.
class Chimp32Compressor {
public:
    static const int THRESHOLD = 5;
    static const uint32_t NAN_INT = 0x7fc00000u;
    static const uint8_t leadingRepresentation[64];
    static const uint8_t leadingRound[64];

    OutputBitStream out;
    int storedLeadingZeros = INT32_MAX;
    uint32_t storedVal = 0;
    bool first = true;

    void addValue(uint32_t value) {
        if(first) {
            first = false;
            storedVal = value;
            out.writeBits(storedVal,32);
            return;
        }
        uint32_t xorv = storedVal ^ value;
        if(xorv==0) {
            out.writeBit(0); out.writeBit(0);
            storedLeadingZeros = 33;
        } else {
            int leading = leadingRound[__builtin_clz(xorv)];
            int trailing = __builtin_ctz(xorv);
            if(trailing > THRESHOLD) {
                int significant = 32 - leading - trailing;
                out.writeBit(0); out.writeBit(1);
                out.writeBits(leadingRepresentation[leading],3);
                out.writeBits(significant,5);
                out.writeBits(xorv>>trailing, significant);
                storedLeadingZeros = 33;
            } else if(leading == storedLeadingZeros) {
                int significant = 32 - leading;
                out.writeBit(1); out.writeBit(0);
                out.writeBits(xorv, significant);
            } else {
                storedLeadingZeros = leading;
                int significant = 32 - leading;
                out.writeBits(24 + leadingRepresentation[leading],5);
                out.writeBits(xorv, significant);
            }
        }
        storedVal = value;
    }

    void flush() { out.flush(); }
    void close() {
        addValue(NAN_INT);
        out.writeBit(false);
        flush();
    }
    size_t getSize() const { return out.bitsWritten; }
    const std::vector<uint8_t>& getBuffer() const { return out.buffer; }
};

const uint8_t Chimp32Compressor::leadingRepresentation[64] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,4,4,5,5,6,6,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
const uint8_t Chimp32Compressor::leadingRound[64] = {
    0,0,0,0,0,0,0,0,8,8,8,8,12,12,12,12,
    16,16,18,18,20,20,22,22,24,24,24,24,24,24,24,24,
    24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,
    24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24
};

// Decompressor
class Chimp32Decompressor {
public:
    static const uint8_t leadingRepresentation[8];
    InputBitStream in;
    int storedLeadingZeros = INT32_MAX;
    int storedTrailingZeros = 0;
    uint32_t storedVal = 0;
    bool first = true;
    bool end = false;

    explicit Chimp32Decompressor(const std::vector<uint8_t>& buf) : in(buf) {}

    bool readValue(uint32_t &v) {
        if(end) return false;
        if(first) {
            first = false;
            storedVal = in.readBits(32);
            if(storedVal == Chimp32Compressor::NAN_INT) { end = true; return false; }
            v = storedVal;
            return true;
        }
        if(in.readBit()) { // 1?
            if(in.readBit()) { //11
                storedLeadingZeros = leadingRepresentation[in.readBits(3)];
            }
            int significant = 32 - storedLeadingZeros;
            if(significant==0) significant = 32;
            uint32_t val = in.readBits(32 - storedLeadingZeros);
            val = storedVal ^ val;
            storedVal = val;
            if(storedVal == Chimp32Compressor::NAN_INT) { end = true; return false; }
            v = val;
        } else if(in.readBit()) { //01
            storedLeadingZeros = leadingRepresentation[in.readBits(3)];
            int significant = in.readBits(5);
            if(significant==0) significant = 32;
            storedTrailingZeros = 32 - significant - storedLeadingZeros;
            uint32_t val = in.readBits(32 - storedLeadingZeros - storedTrailingZeros);
            val <<= storedTrailingZeros;
            val = storedVal ^ val;
            storedVal = val;
            if(storedVal == Chimp32Compressor::NAN_INT) { end = true; return false; }
            v = val;
        } else { //00
            v = storedVal;
        }
        if(storedVal == Chimp32Compressor::NAN_INT) { end = true; return false; }
        return true;
    }
};

const uint8_t Chimp32Decompressor::leadingRepresentation[8] = {0,8,12,16,18,20,22,24};

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

    /* measure incremental insertion speed */
    for(size_t i=0;i<count;i++) {
        Chimp32Compressor tmp;
        uint64_t start,end;
        RDTSC_START(start);
        for(size_t j=0;j<howmany[i];j++) tmp.addValue(numbers[i][j]);
        tmp.flush();
        RDTSC_FINAL(end);
        insert_cycles += end-start;
    }

    for(size_t i=0;i<count;i++) {
        Chimp32Compressor cmp;
        uint64_t start,end;
        RDTSC_START(start);
        for(size_t j=0;j<howmany[i];j++) cmp.addValue(numbers[i][j]);
        cmp.close();
        RDTSC_FINAL(end);
        build_cycles += end-start;
        totalsize += (cmp.getSize()+7)/8;

        if(verbose) fprintf(stderr, "compressed set %zu, bits=%zu\n", i, cmp.getSize());

        Chimp32Decompressor dec(cmp.getBuffer());
        RDTSC_START(start);
        size_t idx = 0;
        uint32_t val;
        while(dec.readValue(val)) {
            if(val!=numbers[i][idx]) { fprintf(stderr,"decoding error\n"); return -1; }
            idx++;
        }
        RDTSC_FINAL(end);
        iter_cycles += end-start;
        if(verbose) fprintf(stderr, "decompressed set %zu\n", i);
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

