#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <vl53l8cx_api.h>

#define PID_FILE "/run/sensors2shm.pid"
#define MAX_SENSORS 2
#define SHM_NAME_MAX 255
#define SENSOR_TYPE_VL53L8CX_SPI 3U
#define RESOLUTION VL53L8CX_RESOLUTION_4X4
#define RANGING_FREQUENCY_HZ 60U

typedef struct {
  uint32_t timestamp_sec;
  uint8_t sensor_type;
  uint8_t resolution;
  uint8_t data_format;
  uint8_t reserved;
  uint16_t distances[64];
  uint8_t statuses[64];
} SensorFrame;

typedef struct {
  uint8_t spi_bus;
  uint8_t spi_cs;
  char shm_name[SHM_NAME_MAX + 1];
  int shm_fd;
  SensorFrame *frame;
  sem_t *semaphore;
  VL53L8CX_Configuration *uld;
  bool ranging;
} Sensor;

static volatile sig_atomic_t running = 1;

static void on_signal(int signal_number) {
  (void)signal_number;
  running = 0;
}

static void sleep_ms(long milliseconds) {
  struct timespec delay = {
      .tv_sec = milliseconds / 1000,
      .tv_nsec = (milliseconds % 1000) * 1000000L,
  };
  while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
  }
}

static int daemonize(void) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(EXIT_SUCCESS);

  if (setsid() < 0)
    return -1;

  pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(EXIT_SUCCESS);

  umask(0);
  if (chdir("/") < 0)
    return -1;

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  FILE *pid_file = fopen(PID_FILE, "w");
  if (pid_file == NULL)
    return -1;
  fprintf(pid_file, "%d\n", getpid());
  fclose(pid_file);
  return 0;
}

static void posix_name(char *output, size_t output_size, const char *prefix,
                       const char *name) {
  snprintf(output, output_size, "/%s%s", prefix, name);
}

static int create_shared_memory(Sensor *sensor) {
  char shm_object[SHM_NAME_MAX + 2];
  char semaphore_object[SHM_NAME_MAX + 6];
  posix_name(shm_object, sizeof(shm_object), "", sensor->shm_name);
  posix_name(semaphore_object, sizeof(semaphore_object), "sem_",
             sensor->shm_name);

  sensor->shm_fd = shm_open(shm_object, O_CREAT | O_RDWR, 0666);
  if (sensor->shm_fd < 0) {
    perror("shm_open");
    return -1;
  }
  if (fchmod(sensor->shm_fd, 0666) != 0 ||
      ftruncate(sensor->shm_fd, sizeof(SensorFrame)) != 0) {
    perror("shared memory setup");
    return -1;
  }

  sensor->frame = mmap(NULL, sizeof(SensorFrame), PROT_READ | PROT_WRITE,
                       MAP_SHARED, sensor->shm_fd, 0);
  if (sensor->frame == MAP_FAILED) {
    sensor->frame = NULL;
    perror("mmap");
    return -1;
  }
  memset(sensor->frame, 0, sizeof(SensorFrame));

  sensor->semaphore = sem_open(semaphore_object, O_CREAT, 0666, 1);
  if (sensor->semaphore == SEM_FAILED) {
    sensor->semaphore = NULL;
    perror("sem_open");
    return -1;
  }
  char semaphore_path[SHM_NAME_MAX + 18];
  snprintf(semaphore_path, sizeof(semaphore_path), "/dev/shm/sem.sem_%s",
           sensor->shm_name);
  if (chmod(semaphore_path, 0666) != 0) {
    perror("chmod semaphore");
    return -1;
  }
  return 0;
}

static void close_sensor(Sensor *sensor) {
  if (sensor->uld != NULL) {
    if (sensor->ranging)
      (void)vl53l8cx_stop_ranging(sensor->uld);
    (void)vl53l8cx_comms_close(&sensor->uld->platform);
    free(sensor->uld);
    sensor->uld = NULL;
  }

  if (sensor->semaphore != NULL) {
    sem_close(sensor->semaphore);
    sensor->semaphore = NULL;
  }
  if (sensor->frame != NULL) {
    munmap(sensor->frame, sizeof(SensorFrame));
    sensor->frame = NULL;
  }
  if (sensor->shm_fd >= 0) {
    close(sensor->shm_fd);
    sensor->shm_fd = -1;
  }

  if (sensor->shm_name[0] != '\0') {
    char shm_object[SHM_NAME_MAX + 2];
    char semaphore_object[SHM_NAME_MAX + 6];
    posix_name(shm_object, sizeof(shm_object), "", sensor->shm_name);
    posix_name(semaphore_object, sizeof(semaphore_object), "sem_",
               sensor->shm_name);
    shm_unlink(shm_object);
    sem_unlink(semaphore_object);
  }
}

static int initialize_sensor(Sensor *sensor) {
  const char *stage = "device identification";
  uint8_t status = 0;
  uint8_t alive = 0;

  sensor->uld = calloc(1, sizeof(*sensor->uld));
  if (sensor->uld == NULL) {
    perror("calloc VL53L8CX configuration");
    return -1;
  }
  sensor->uld->platform.spi_num = sensor->spi_bus;
  sensor->uld->platform.spi_cs = sensor->spi_cs;

  if (vl53l8cx_comms_init(&sensor->uld->platform) != 0) {
    fprintf(stderr, "Cannot open /dev/spidev%u.%u\n", sensor->spi_bus,
            sensor->spi_cs);
    goto failure;
  }

  status = vl53l8cx_is_alive(sensor->uld, &alive);
  if (status == 0 && alive != 0) {
    stage = "ULD initialization";
    status = vl53l8cx_init(sensor->uld);
  }
  if (status == 0 && alive != 0) {
    stage = "4x4 resolution setup";
    status = vl53l8cx_set_resolution(sensor->uld, RESOLUTION);
  }
  if (status == 0 && alive != 0) {
    stage = "60 Hz frequency setup";
    status = vl53l8cx_set_ranging_frequency_hz(sensor->uld,
                                                RANGING_FREQUENCY_HZ);
  }
  if (status != 0 || alive == 0) {
    fprintf(stderr,
            "VL53L8CX on spidev%u.%u failed at %s (status %u, alive %u)\n",
            sensor->spi_bus, sensor->spi_cs, stage, status, alive);
    goto failure;
  }

  if (create_shared_memory(sensor) != 0)
    goto failure;

  status = vl53l8cx_start_ranging(sensor->uld);
  if (status != 0) {
    fprintf(stderr, "VL53L8CX on spidev%u.%u cannot start ranging (status %u)\n",
            sensor->spi_bus, sensor->spi_cs, status);
    goto failure;
  }
  sensor->ranging = true;
  printf("VL53L8CX ready: /dev/spidev%u.%u -> /dev/shm/%s\n",
         sensor->spi_bus, sensor->spi_cs, sensor->shm_name);
  return 0;

failure:
  close_sensor(sensor);
  return -1;
}

static int publish_frame(Sensor *sensor) {
  uint8_t ready = 0;
  uint8_t status = vl53l8cx_check_data_ready(sensor->uld, &ready);
  if (status != 0)
    return -1;
  if (ready == 0)
    return 0;

  VL53L8CX_ResultsData results;
  status = vl53l8cx_get_ranging_data(sensor->uld, &results);
  if (status != 0)
    return -1;

  while (sem_wait(sensor->semaphore) != 0) {
    if (errno != EINTR)
      return -1;
  }
  struct timespec timestamp;
  clock_gettime(CLOCK_REALTIME, &timestamp);
  sensor->frame->timestamp_sec = (uint32_t)timestamp.tv_sec;
  sensor->frame->sensor_type = SENSOR_TYPE_VL53L8CX_SPI;
  sensor->frame->resolution = RESOLUTION;
  sensor->frame->data_format = 1;
  sensor->frame->reserved = 0;
  for (size_t zone = 0; zone < RESOLUTION; ++zone) {
    sensor->frame->distances[zone] = results.distance_mm[zone];
    sensor->frame->statuses[zone] = results.target_status[zone];
  }
  sem_post(sensor->semaphore);
  return 1;
}

static int parse_config(const char *path, Sensor sensors[], size_t *count) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    perror(path);
    return -1;
  }

  char line[512];
  *count = 0;
  while (fgets(line, sizeof(line), file) != NULL) {
    char *cursor = line;
    while (*cursor == ' ' || *cursor == '\t')
      ++cursor;
    if (*cursor == '\0' || *cursor == '\n' || *cursor == '#')
      continue;
    if (*count == MAX_SENSORS) {
      fprintf(stderr, "Only %d VL53L8CX SPI sensors are supported\n",
              MAX_SENSORS);
      fclose(file);
      return -1;
    }

    char type[16];
    Sensor *sensor = &sensors[*count];
    if (sscanf(cursor, "%15s %hhu %hhu %255s", type, &sensor->spi_bus,
               &sensor->spi_cs, sensor->shm_name) != 4 ||
        strcmp(type, "l8cx_spi") != 0) {
      fprintf(stderr, "Expected: l8cx_spi <spi_bus> <spi_cs> <shm_name>\n");
      fclose(file);
      return -1;
    }
    sensor->shm_fd = -1;
    if (sensor->spi_bus > 9 || sensor->spi_cs > 9) {
      fprintf(stderr, "Invalid SPI device spidev%u.%u\n", sensor->spi_bus,
              sensor->spi_cs);
      fclose(file);
      return -1;
    }
    for (size_t previous = 0; previous < *count; ++previous) {
      if (sensors[previous].spi_bus == sensor->spi_bus &&
          sensors[previous].spi_cs == sensor->spi_cs) {
        fprintf(stderr, "Duplicate SPI device spidev%u.%u\n", sensor->spi_bus,
                sensor->spi_cs);
        fclose(file);
        return -1;
      }
    }
    ++*count;
  }
  fclose(file);
  if (*count == 0) {
    fprintf(stderr, "No VL53L8CX SPI sensors configured\n");
    return -1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  const bool daemon_mode = argc == 2 && strcmp(argv[1], "--daemon") == 0;
  if (argc > 1 && !daemon_mode) {
    fprintf(stderr, "Usage: %s [--daemon]\n", argv[0]);
    return EXIT_FAILURE;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  Sensor sensors[MAX_SENSORS] = {0};
  size_t sensor_count = 0;
  if (parse_config("sensors_config.txt", sensors, &sensor_count) != 0)
    return EXIT_FAILURE;

  size_t active_count = 0;
  for (size_t index = 0; index < sensor_count; ++index) {
    if (initialize_sensor(&sensors[index]) == 0)
      ++active_count;
  }
  if (active_count == 0) {
    fprintf(stderr, "No VL53L8CX sensor initialized\n");
    return EXIT_FAILURE;
  }

  if (daemon_mode) {
    fflush(NULL);
    if (daemonize() != 0) {
      perror("daemonize");
      for (size_t index = 0; index < sensor_count; ++index)
        close_sensor(&sensors[index]);
      return EXIT_FAILURE;
    }
  }

  while (running) {
    for (size_t index = 0; index < sensor_count; ++index) {
      if (!sensors[index].ranging)
        continue;
      if (publish_frame(&sensors[index]) < 0 && !daemon_mode)
        fprintf(stderr, "Read failed for spidev%u.%u\n", sensors[index].spi_bus,
                sensors[index].spi_cs);
    }
    sleep_ms(5);
  }

  for (size_t index = 0; index < sensor_count; ++index)
    close_sensor(&sensors[index]);
  if (daemon_mode)
    unlink(PID_FILE);
  return EXIT_SUCCESS;
}
