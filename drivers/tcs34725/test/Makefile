CC = gcc
CFLAGS = -Wall -O2 -I./src -I./interface -I./example -I./test
LDFLAGS = -lrt
TARGET = test_read_tcs

SRCS = \
    src/driver_tcs34725.c \
    interface/driver_tcs34725_interface.c \
    example/driver_tcs34725_basic.c \
    test/test_read_tcs.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 