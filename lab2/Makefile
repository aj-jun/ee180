# Set compiler args
CC=g++ # Use g++ as the C++ compiler (required for OpenCV + C++ sources)

# Compiler flags:
# -O3                : aggressive optimization (loop unrolling, inlining, vectorization)
# -ftree-vectorize   : allow compiler auto-vectorization where possible
# -mfpu=neon         : enable ARM NEON SIMD unit
CFLAGS=-Wall -c -O3 -ftree-vectorize -mfpu=neon -march=armv7-a -mtune=cortex-a9
LDFLAGS=

# Linker libraries: pthread for multithreading
LDLIBS=-L /usr/lib $$(pkg-config --cflags --libs opencv) -pthread

ifeq ($(shell arch), armv7l)
	LDLIBS += -lpfm
endif
SOURCES=main.cpp pc.cpp sobel_st.cpp sobel_mt.cpp sobel_calc.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=sobel
TAR=lab2.tar.gz
SUBMIT_FILES=lab2/*.cpp lab2/*.h lab2/README lab2/Makefile

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE):$(OBJECTS)
	$(CC) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

run:
	./sobel
clean:
	\rm -f *.o $(EXECUTABLE) $(TAR)

submit: clean
	ln -s . lab2
	tar -czf $(TAR) $(SUBMIT_FILES)
	rm -f lab2
