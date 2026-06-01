CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Wno-unused-parameter -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lraylib -lm -lpthread

SRC_DIR = src
BIN     = ajolot

SRCS = $(SRC_DIR)/ajolot.c \
       $(SRC_DIR)/data.c \
       $(SRC_DIR)/entry.c \
       $(SRC_DIR)/init.c \
       $(SRC_DIR)/loop.c \
       $(SRC_DIR)/music.c \
       $(SRC_DIR)/svg.c

OBJS = $(SRCS:.c=.o)

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)
