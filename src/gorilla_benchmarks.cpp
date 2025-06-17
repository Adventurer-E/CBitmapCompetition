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
}

class OutputBitStream {
public:
    std::vector<uint8_t> buffer;
    uint8_t current{0};
    int bitsInCurrent{0};
    size_t bitsWritten{0};

    void writeBit(bool bit) {
        current <<= 1;
        if(bit) current |= 1;
        bitsInCurrent++;
        bitsWritten++;
        if(bitsInCurrent==8) {
            buffer.push_back(current);
            current = 0;
            bitsInCurrent = 0;
        }
    }
    void writeBits(uint32_t value, int bits) {
        for(int i=bits-1;i>=0;i--) writeBit((value>>i)&1);
    }
    void flush(){
        if(bitsInCurrent>0){
            current <<= (8-bitsInCurrent);
            buffer.push_back(current);
            current = 0;
            bitsInCurrent = 0;
        }
    }
};

class InputBitStream {
public:
    const std::vector<uint8_t>& buffer;
    size_t pos{0};
    int bitPos{0};
    explicit InputBitStream(const std::vector<uint8_t>& buf):buffer(buf){}
    bool readBit(){
        uint8_t b = buffer[pos];
        bool bit = (b>>(7-bitPos))&1;
        bitPos++;
        if(bitPos==8){bitPos=0;pos++;}
        return bit;
    }
    uint32_t readBits(int bits){
        uint32_t v=0;for(int i=0;i<bits;i++) v=(v<<1)|(readBit()?1:0);return v;
    }
};

class Gorilla32Compressor{
public:
    OutputBitStream out;
    uint32_t prev{0};
    int storedLeading{32};
    int storedTrailing{0};
    bool first=true;
    void addValue(uint32_t val){
        if(first){first=false;prev=val;out.writeBits(val,32);return;}
        uint32_t xorv = prev ^ val;
        if(xorv==0){
            out.writeBit(0);
        }else{
            int leading=__builtin_clz(xorv);
            int trailing=__builtin_ctz(xorv);
            if(leading>=storedLeading && trailing>=storedTrailing){
                out.writeBits(0b10,2);
                int significant = 32 - storedLeading - storedTrailing;
                out.writeBits(xorv>>storedTrailing, significant);
            }else{
                out.writeBits(0b11,2);
                out.writeBits(leading,5);
                int significant = 32 - leading - trailing;
                out.writeBits(significant-1,6);
                out.writeBits(xorv>>trailing, significant);
                storedLeading=leading;
                storedTrailing=trailing;
            }
        }
        prev=val;
    }
    void flush(){out.flush();}
    size_t getSize()const{return out.bitsWritten;}
    const std::vector<uint8_t>& getBuffer()const{return out.buffer;}
};

class Gorilla32Decompressor{
public:
    InputBitStream in;
    uint32_t prev{0};
    int storedLeading{32};
    int storedTrailing{0};
    bool first=true;
    explicit Gorilla32Decompressor(const std::vector<uint8_t>& buf):in(buf){}
    bool readValue(uint32_t& v){
        if(first){first=false;prev=in.readBits(32);v=prev;return true;}
        if(!in.readBit()){
            v=prev;
        }else if(!in.readBit()){
            int significant = 32 - storedLeading - storedTrailing;
            uint32_t val = in.readBits(significant);
            val <<= storedTrailing;
            v = prev ^ val;
        }else{
            storedLeading = in.readBits(5);
            int significant = in.readBits(6)+1;
            storedTrailing = 32 - storedLeading - significant;
            uint32_t val = in.readBits(significant);
            val <<= storedTrailing;
            v = prev ^ val;
        }
        prev=v;
        return true;
    }
};

static void printusage(char *command){
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n", command);
    printf("the -v flag turns on verbose mode\n");
}

int main(int argc,char**argv){
    int c;const char*extension=".txt";bool verbose=false;
    while((c=getopt(argc,argv,"ve:h"))!=-1)switch(c){case'e':extension=optarg;break;case'v':verbose=true;break;case'h':printusage(argv[0]);return 0;default:abort();}
    if(optind>=argc){printusage(argv[0]);return -1;}
    char*dirname=argv[optind];size_t count;size_t*howmany=NULL;uint32_t**numbers=read_all_integer_files(dirname,extension,&howmany,&count);if(numbers==NULL){printf("I could not find or load any data file with extension %s in directory %s.\n",extension,dirname);return -1;}
    uint64_t totalcard=0;for(size_t i=0;i<count;i++) totalcard+=howmany[i];
    uint64_t build_cycles=0,iter_cycles=0,insert_cycles=0,totalsize=0;

    for(size_t i=0;i<count;i++){
        Gorilla32Compressor tmp;uint64_t start,end;RDTSC_START(start);for(size_t j=0;j<howmany[i];j++) tmp.addValue(numbers[i][j]);tmp.flush();RDTSC_FINAL(end);insert_cycles+=end-start;}

    for(size_t i=0;i<count;i++){
        Gorilla32Compressor cmp;uint64_t start,end;RDTSC_START(start);for(size_t j=0;j<howmany[i];j++) cmp.addValue(numbers[i][j]);cmp.flush();RDTSC_FINAL(end);build_cycles+=end-start;totalsize+=(cmp.getSize()+7)/8;if(verbose) fprintf(stderr,"compressed set %zu, bits=%zu\n",i,cmp.getSize());Gorilla32Decompressor dec(cmp.getBuffer());RDTSC_START(start);for(size_t j=0;j<howmany[i];j++){uint32_t val;dec.readValue(val);if(val!=numbers[i][j]){fprintf(stderr,"decoding error\n");return -1;}}RDTSC_FINAL(end);iter_cycles+=end-start;if(verbose) fprintf(stderr,"decompressed set %zu\n",i);}

    for(size_t i=0;i<count;i++) free(numbers[i]);free(numbers);free(howmany);

    printf(" %20.4f %20.4f %20.4f %20.4f\n",totalsize*8.0/totalcard,build_cycles*1.0/totalcard,insert_cycles*1.0/totalcard,iter_cycles*1.0/totalcard);return 0;
}
