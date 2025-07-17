CC = gcc
CFLAGS = -Wall -O2
INCLUDES = -I../platform -I../core -I../../Example/Inc
SOURCES1 = example_1.c ../platform/vl53l1_platform.c ../core/VL53L1X_api.c
SOURCES2 = example_2.c ../platform/vl53l1_platform.c ../core/VL53L1X_api.c
TARGET1 = example_1
TARGET2 = example_2

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(SOURCES1)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SOURCES1)

$(TARGET2): $(SOURCES2)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SOURCES2)

clean:
	rm -f $(TARGET1) $(TARGET2) 
