# minimalist makefile
######################
# To add a competitive technique, simply add your code in the src subdirectory (follow the README.md instructions) and
# add your executable file name to the EXECUTABLES variable below.
# along with a target to build it.
#######################
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h

.PHONY: clean
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
OSFLAGS= -Wl,--no-as-needed
endif

#######################
# SunOS gcc7.2.0 modifications QSI/Jon Strabala
#########
# original CXX flag, new for FLAGS
ifeq ($(UNAME), SunOS)
# must be 64 bit compile, new for CFLAGS
OSFLAGS= -m64
# force gnu99 intead of c99 for getopt, new for CFLAGS
OSCFLAGS= -std=gnu99
endif



ifeq ($(DEBUG),1)
CFLAGS = -fuse-ld=gold -fPIC  -std=c99 -ggdb -mavx2 -mbmi2 -march=native -Wall -Wextra -Wshadow -fsanitize=undefined  -fno-omit-frame-pointer -fsanitize=address  $(OSFLAGS) $(OSCFLAGS) -ldl
CXXFLAGS = -fuse-ld=gold -fPIC  -std=c++11 -ggdb -mavx2 -mbmi2 -march=native -Wall -Wextra -Wshadow -fsanitize=undefined  -fno-omit-frame-pointer -fsanitize=address   $(OSFLAGS) -ldl
ROARFLAGS = -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
else
CFLAGS = -ggdb -fPIC -std=c99 -O3 -mavx2 -mbmi2 -march=native -Wall -Wextra -Wshadow   $(OSFLAGS) -ldl
CXXFLAGS = -fPIC -std=c++11 -O3 -mavx2 -mbmi2  -march=native -Wall -Wextra -Wshadow   $(OSFLAGS) -ldl
ROARFLAGS = -DCMAKE_BUILD_TYPE=Release
endif # debug



EXECUTABLES=wah32_benchmarks chimp_benchmarks zstd_benchmarks lz4_benchmarks snappy_benchmarks gorilla_benchmarks fpc_benchmarks concise_benchmarks roaring_benchmarks slow_roaring_benchmarks ewah32_benchmarks ewah64_benchmarks malloced_roaring_benchmarks hot_roaring_benchmarks hot_slow_roaring_benchmarks alp_benchmarks gen

all: $(EXECUTABLES)

test:
	./scripts/all.sh

bigtest:
	./scripts/big.sh

hottest:
	./scripts/hot_roaring.sh




src/roaring.c :
	(cd src && exec ../CRoaring/amalgamation.sh && rm almagamation_demo.c && rm almagamation_demo.cpp)

gen : synthetic/anh_moffat_clustered.h synthetic/gen.cpp
	$(CXX) $(CXXFLAGS) -o gen synthetic/gen.cpp -Isynthetic

roaring_benchmarks : src/roaring.c src/roaring_benchmarks.c
	$(CC) $(CFLAGS) -o roaring_benchmarks src/roaring_benchmarks.c


hot_roaring_benchmarks : src/roaring.c src/hot_roaring_benchmarks.c
	$(CC) $(CFLAGS)  -ggdb -o hot_roaring_benchmarks src/hot_roaring_benchmarks.c

malloced_roaring_benchmarks : src/roaring.c src/roaring_benchmarks.c
	$(CC) $(CFLAGS) -o malloced_roaring_benchmarks src/roaring_benchmarks.c -DRECORD_MALLOCS


slow_roaring_benchmarks : src/roaring.c src/roaring_benchmarks.c
	$(CC) $(CFLAGS) -DDISABLE_X64 -o slow_roaring_benchmarks src/roaring_benchmarks.c

hot_slow_roaring_benchmarks : src/roaring.c src/hot_roaring_benchmarks.c
	$(CC) $(CFLAGS)   -ggdb  -DDISABLE_X64 -o hot_slow_roaring_benchmarks src/hot_roaring_benchmarks.c


ewah32_benchmarks: src/ewah32_benchmarks.cpp
	$(CXX) $(CXXFLAGS)  -o ewah32_benchmarks ./src/ewah32_benchmarks.cpp -IEWAHBoolArray/headers

wah32_benchmarks: src/wah32_benchmarks.cpp
	$(CXX) $(CXXFLAGS)  -o wah32_benchmarks ./src/wah32_benchmarks.cpp -IConcise/include

concise_benchmarks: src/concise_benchmarks.cpp
	$(CXX) $(CXXFLAGS)  -o concise_benchmarks ./src/concise_benchmarks.cpp -IConcise/include

ewah64_benchmarks: src/ewah64_benchmarks.cpp
	$(CXX) $(CXXFLAGS)  -o ewah64_benchmarks ./src/ewah64_benchmarks.cpp -IEWAHBoolArray/headers

chimp_benchmarks: src/chimp_benchmarks.cpp src/memtrackingallocator.h
	$(CXX) $(CXXFLAGS)  -o chimp_benchmarks ./src/chimp_benchmarks.cpp
zstd_benchmarks: src/zstd_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -o zstd_benchmarks ./src/zstd_benchmarks.cpp -lzstd
lz4_benchmarks: src/lz4_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -o lz4_benchmarks ./src/lz4_benchmarks.cpp -llz4
snappy_benchmarks: src/snappy_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -o snappy_benchmarks ./src/snappy_benchmarks.cpp -lsnappy
gorilla_benchmarks: src/gorilla_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -o gorilla_benchmarks ./src/gorilla_benchmarks.cpp

fpc_benchmarks: src/fpc_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -o fpc_benchmarks ./src/fpc_benchmarks.cpp
  
alp_benchmarks: src/alp_benchmarks.cpp
	$(CXX) $(CXXFLAGS) -U__AVX512F__ -std=c++17 -o alp_benchmarks ./src/alp_benchmarks.cpp ALP/src/fastlanes_ffor.cpp ALP/src/fastlanes_unffor.cpp ALP/src/fastlanes_generated_ffor.cpp ALP/src/fastlanes_generated_unffor.cpp ALP/src/falp.cpp -IALP/include

clean:
	rm -r -f   $(EXECUTABLES) src/roaring.c src/roaring.h src/roaring.hh bigtmp
