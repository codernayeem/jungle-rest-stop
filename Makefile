# Jungle Rest Stop - Makefile (mingw32-make or make)
CXX      = g++
CC       = gcc
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude -Isrc
CFLAGS   = -O2 -Iinclude
LDFLAGS  = -Llib -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++

SRC_CPP  = $(wildcard src/*.cpp src/*/*.cpp)
OBJ      = $(patsubst src/%.cpp,build/%.o,$(SRC_CPP)) build/glad.o
TARGET   = main.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.cpp $(wildcard src/*/*.h src/*/*.hpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/glad.o: src/glad.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

shots: all
	./$(TARGET) --shots report/shots

clean:
	rm -rf build $(TARGET)

.PHONY: all run shots clean
