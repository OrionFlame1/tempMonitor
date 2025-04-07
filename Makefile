CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LDFLAGS = -lrt

SRC = src/cli.c src/temp_monitor_daemon.c src/get_temp.c
OUT = build/temp_monitor_daemon

all:
	mkdir -p build
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(OUT)

clean:
	rm -rf build

.PHONY: all clean