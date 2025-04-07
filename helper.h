#include <pi-gpio.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#include "parser/cJSON.h"
#include "config.h"

// daemon related defines
#define SHM_NAME "/temp_monitor_daemon_shm"
#define SHM_SIZE sizeof(SharedData)
#define PID_FILE "/tmp/temp_monitor_daemon_pid"

// constants
#define PWM0 18
#define SILENT 60 // preset value to define a speed that will make the fan pretty much silent
#define FREQ 5 // seconds in-between checks of the temp

// related directly to this fan, DO NOT TOUCH
#define DIVIDER 18
#define RANGE 120

void set_speed(int speed) {
    setup();
    
    pwmSetGpio(18);

    pwmSetMode(PWM_MODE_MS); // use a fixed frequency
    pwmSetClock(DIVIDER);

    pwmSetRange(18, RANGE);
    // The following gives a precise 25kHz PWM signal
    // Noctua recommended value
    // CLOCK / (DIVIDER * RANGE)
    pwmWrite(18, RANGE * speed / 100); // duty cycle set

    cleanup();
}

int get_temp() {
    FILE *fp;
    char path[1035];

    fp = popen("vcgencmd measure_temp", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }

    int number;
    char *ptr;

    while (fgets(path, sizeof(path), fp) != NULL) {
        number = strtol(path + 5, &ptr, 10); // Skip first 5 characters ("temp=")
    }

    pclose(fp);

    return number;
}

char* get_time() {
    static char buffer[9];
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);

    return buffer;
}

int interpolate(int *temp_vals, int *speed_vals, int size, int x) {
    if (x <= temp_vals[0]) {
        return speed_vals[0];
    }
    if (x >= temp_vals[size - 1]) {
        return speed_vals[size - 1];
    }

    for (int i = 0; i < size - 1; i++) {
        if (x >= temp_vals[i] && x <= temp_vals[i + 1]) {
            int x1 = temp_vals[i], x2 = temp_vals[i + 1];
            int y1 = speed_vals[i], y2 = speed_vals[i + 1];
            return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
        }
    }

    return -1;
}

int is_night(char *time_window) {
    time_t now;
    struct tm *tm;
    now = time(0);
    tm = localtime(&now);

    int start, end;
    sscanf(time_window, "%d-%d", &start, &end);

    if (start < end) {
        return tm->tm_hour >= start && tm->tm_hour < end;
    } else {
        return tm->tm_hour >= start || tm->tm_hour < end;
    }
}

char* int_cast_string(int num) {
    static char buffer[3];
    sprintf(buffer, "%d", num);
    return buffer;
}

bool is_number(const char number[]) {
    int i = 0;
    
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}

SharedData load_config_from_file(const char *filename) {
    SharedData cfg;
    memset(&cfg, 0, sizeof(SharedData));

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Could not open config file");
        return cfg;
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    rewind(file);

    char *data = malloc(len + 1);
    fread(data, 1, len, file);
    data[len] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) {
        printf("JSON parse error\n");
        return cfg;
    }

    // Pins
    cJSON *pins = cJSON_GetObjectItem(json, "pins");
    cfg.pins.pwm = cJSON_GetObjectItem(pins, "pwm")->valueint;

    // Night
    cJSON *night = cJSON_GetObjectItem(json, "night");
    cfg.night.mode = cJSON_GetObjectItem(night, "mode")->valueint;
    strcpy(cfg.night.time_window_hours, cJSON_GetObjectItem(night, "time_window_hours")->valuestring);
    cfg.night.speed = cJSON_GetObjectItem(night, "speed")->valueint;

    // Mapping
    cJSON *mapping = cJSON_GetObjectItem(json, "mapping");
    cJSON *temps = cJSON_GetObjectItem(mapping, "temp_vals");
    cJSON *speeds = cJSON_GetObjectItem(mapping, "speed_vals");

    for (int i = 0; i < 3; i++) {
        cfg.mapping.temp_vals[i] = cJSON_GetArrayItem(temps, i)->valueint;
        cfg.mapping.speed_vals[i] = cJSON_GetArrayItem(speeds, i)->valueint;
    }

    cJSON_Delete(json);
    return cfg;
}