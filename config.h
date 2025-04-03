#ifndef DATA_H
#define DATA_H

#define MAX_MAPPING_SIZE 3

typedef struct {
    int pwm;
} Pins;

typedef struct {
    int mode;
    char time_window_hours[10];
    int speed;
} NightMode;

typedef struct {
    int temp_vals[MAX_MAPPING_SIZE];
    int speed_vals[MAX_MAPPING_SIZE];
} Mapping;

typedef struct {
    Pins pins;
    NightMode night;
    Mapping mapping;
    int currentSpeed;
} SharedData;

#endif