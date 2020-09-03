TARGET = chip8
INSTALL_PATH = /usr/local/bin

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)

CC = gcc
CPPFLAGS += -Iinclude -pedantic
CFLAGS += -Wall -std=c99 -O3
LDFLAGS += -Llib
LDLIBS += -lSDL2

CP=cp
MOVE = mv
MKDIR_P = mkdir -p

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run:
	./$(TARGET)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTALL_PATH)

clean:
	$(RM) $(OBJ) $(TARGET)
