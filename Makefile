# Works with MSYS2 MinGW64 (Windows) and Linux.  Run:  make   |   make clean
CC      ?= gcc
CFLAGS  ?= -g -O2 -Wall
SRC      = src/main.c src/cpu.c src/memory.c src/gpu.c src/timer.c
TARGET   = gb

$(TARGET): $(SRC) src/gb.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) -lSDL2

clean:
	rm -f $(TARGET) $(TARGET).exe
