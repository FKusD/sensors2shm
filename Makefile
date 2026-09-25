CC := gcc

CPPFLAGS := -DSPI \
	-I./drivers/l8cx_uld/user/uld-driver/inc \
	-I./drivers/l8cx_uld/user/platform
CFLAGS := -Wall -Wextra -Werror -Wno-missing-braces -Wno-unused-parameter -Os -g0

TARGET := background_ranging
SOURCES := background_ranging.c \
	$(wildcard drivers/l8cx_uld/user/uld-driver/src/*.c) \
	$(wildcard drivers/l8cx_uld/user/platform/*.c)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)
