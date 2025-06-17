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
#include <cmath>

extern "C" {
#include "benchmark.h"
#include "numbersfromtextfiles.h"
}

class FcmPredictor {
public:
    std::vector<uint64_t> table;
    int hash{0};
    explicit FcmPredictor(int logSize=16):table(1<<logSize,0){}
    uint64_t getPrediction() const { return table[hash]; }
    void update(uint64_t value){ table[hash]=value; hash = (int)(((hash<<6) ^ (value>>48)) & (table.size()-1)); }
};

class DfcmPredictor {
public:
    std::vector<uint64_t> table;
    int hash{0};
    uint64_t last{0};
    explicit DfcmPredictor(int logSize=16):table(1<<logSize,0){}
    uint64_t getPrediction() const { return table[hash] + last; }
    void update(uint64_t value){ table[hash]=value-last; hash = (int)(((hash<<2) ^ ((value-last)>>40)) & (table.size()-1)); last=value; }
};

static int encodeZeroBytes(uint64_t diff){
    int lzb = __builtin_clzll(diff)/8;
    if(lzb>=4) lzb--; return lzb;
}

static void toByteArray(uint64_t diff, std::vector<uint8_t>& out){
    int z = encodeZeroBytes(diff); if(z>3) z++; int len = 8 - z;
    for(int i=0;i<len;i++){ out.push_back(diff & 0xff); diff >>= 8; }
}

static uint64_t fromBytes(const uint8_t*ptr,int len){
    uint64_t v=0; for(int i=len-1;i>=0;--i){ v=(v<<8)|ptr[i]; } return v;
}

class FpcCompressor {
public:
    FcmPredictor p1; DfcmPredictor p2; std::vector<uint8_t> buf;
    void encode(double d,double e){
        uint64_t dBits; memcpy(&dBits,&d,8);
        uint64_t diff1d=p1.getPrediction() ^ dBits;
        uint64_t diff2d=p2.getPrediction() ^ dBits;
        bool p1Better=__builtin_clzll(diff1d)>=__builtin_clzll(diff2d);
        p1.update(dBits); p2.update(dBits);
        uint64_t eBits; memcpy(&eBits,&e,8);
        uint64_t diff1e=p1.getPrediction() ^ eBits;
        uint64_t diff2e=p2.getPrediction() ^ eBits;
        bool p1BetterE=__builtin_clzll(diff1e)>=__builtin_clzll(diff2e);
        p1.update(eBits); p2.update(eBits);
        uint8_t code=0;
        if(p1Better){ code|=encodeZeroBytes(diff1d)<<4; }else{ code|=0x80; code|=encodeZeroBytes(diff2d)<<4; }
        if(p1BetterE){ code|=encodeZeroBytes(diff1e); }else{ code|=0x08; code|=encodeZeroBytes(diff2e); }
        buf.push_back(code);
        if(p1Better) toByteArray(diff1d,buf); else toByteArray(diff2d,buf);
        if(p1BetterE) toByteArray(diff1e,buf); else toByteArray(diff2e,buf);
    }
    void encodeAndPad(double d){
        uint64_t dBits; memcpy(&dBits,&d,8);
        uint64_t diff1d=p1.getPrediction() ^ dBits;
        uint64_t diff2d=p2.getPrediction() ^ dBits;
        bool p1Better=__builtin_clzll(diff1d)>=__builtin_clzll(diff2d);
        p1.update(dBits); p2.update(dBits);
        uint8_t code=0;
        if(p1Better){ code|=encodeZeroBytes(diff1d)<<4; }else{ code|=0x80; code|=encodeZeroBytes(diff2d)<<4; }
        code|=0x06;
        buf.push_back(code);
        if(p1Better) toByteArray(diff1d,buf); else toByteArray(diff2d,buf);
        buf.push_back(0);
    }
};

class FpcDecompressor {
public:
    FcmPredictor p1; DfcmPredictor p2; const std::vector<uint8_t>& buf; size_t pos{0};
    explicit FpcDecompressor(const std::vector<uint8_t>& b):buf(b){}
    bool decode(double& d,double& e,bool& hasSecond){
        if(pos>=buf.size()) return false;
        uint8_t header=buf[pos++];
        uint64_t prediction = (header&0x80)?p2.getPrediction():p1.getPrediction();
        int z=(header>>4)&7; if(z>3) z++; int len=8 - z; uint64_t diff=fromBytes(&buf[pos],len); pos+=len; uint64_t actual=prediction^diff; p1.update(actual); p2.update(actual); memcpy(&d,&actual,8);
        prediction=(header&0x08)?p2.getPrediction():p1.getPrediction();
        z=header&7; if(z>3) z++; len=8 - z; diff=fromBytes(&buf[pos],len); pos+=len;
        if(z==7 && diff==0){ hasSecond=false; return true; }
        actual=prediction^diff; p1.update(actual); p2.update(actual); memcpy(&e,&actual,8); hasSecond=true; return true;
    }
};

static void printusage(char*command){
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n",command);
    printf("the -v flag turns on verbose mode\n");
}

int main(int argc,char**argv){
    int c;const char*extension=".txt";bool verbose=false;
    while((c=getopt(argc,argv,"ve:h"))!=-1)switch(c){case'e':extension=optarg;break;case'v':verbose=true;break;case'h':printusage(argv[0]);return 0;default:abort();}
    if(optind>=argc){printusage(argv[0]);return -1;}
    char*dirname=argv[optind];size_t count;size_t*howmany=NULL;uint32_t**numbers=read_all_integer_files(dirname,extension,&howmany,&count);if(numbers==NULL){printf("I could not find or load any data file with extension %s in directory %s.\n",extension,dirname);return -1;}
    uint64_t totalcard=0;for(size_t i=0;i<count;i++) totalcard+=howmany[i];
    uint64_t build_cycles=0,iter_cycles=0,insert_cycles=0,totalsize=0;
    for(size_t i=0;i<count;i++){FpcCompressor tmp;uint64_t start,end;RDTSC_START(start);for(size_t j=0;j<howmany[i];j++){double v=numbers[i][j];if(j+1==howmany[i]) tmp.encodeAndPad(v); else {double e=numbers[i][j+1];tmp.encode(v,e);j++;}}RDTSC_FINAL(end);insert_cycles+=end-start;}
    for(size_t i=0;i<count;i++){FpcCompressor cmp;uint64_t start,end;RDTSC_START(start);for(size_t j=0;j<howmany[i];j++){double v=numbers[i][j];if(j+1==howmany[i]) cmp.encodeAndPad(v); else {double e=numbers[i][j+1];cmp.encode(v,e);j++;}}RDTSC_FINAL(end);build_cycles+=end-start;totalsize+=cmp.buf.size();if(verbose) fprintf(stderr,"compressed set %zu, bytes=%zu\n",i,cmp.buf.size());FpcDecompressor dec(cmp.buf);RDTSC_START(start);size_t idx=0;while(idx<howmany[i]){double a,b;bool hasSecond;dec.decode(a,b,hasSecond);if(a!=numbers[i][idx]){fprintf(stderr,"decoding error\n");return -1;}idx++;if(hasSecond){if(idx>=howmany[i]|| b!=numbers[i][idx]){fprintf(stderr,"decoding error\n");return -1;}idx++;}}RDTSC_FINAL(end);iter_cycles+=end-start;if(verbose) fprintf(stderr,"decompressed set %zu\n",i);}
    for(size_t i=0;i<count;i++) free(numbers[i]);free(numbers);free(howmany);
    printf(" %20.4f %20.4f %20.4f %20.4f\n",totalsize*8.0/totalcard,build_cycles*1.0/totalcard,insert_cycles*1.0/totalcard,iter_cycles*1.0/totalcard);return 0;
}
