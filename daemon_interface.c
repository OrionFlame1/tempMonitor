#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "config.h"

#define SHM_NAME "/temp_monitor_daemon_shm"
#define SHM_SIZE sizeof(SharedData)
#define PID_FILE "/tmp/temp_monitor_daemon_pid"

volatile sig_atomic_t keep_running = 1;
SharedData *shm_ptr = NULL;  // Global pointer to shared memory
int shm_fd = -1;  // Global file descriptor for shared memory

void handle_signal(int sig) {
    keep_running = 0;

    // Clean up shared memory
    if (shm_ptr != MAP_FAILED && shm_ptr != NULL) {
        munmap(shm_ptr, SHM_SIZE);
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
    shm_unlink(SHM_NAME);

    // Remove the PID file
    remove(PID_FILE);
    
    printf("Daemon stopped and memory freed.\n");
    exit(0);
}


void start_daemon() {
    if (access(PID_FILE, F_OK) == 0) {
        printf("Daemon is already running.\n");
        return;
    }
    if (system("./temp_monitor_daemon &") == 0) printf("Daemon started successfully.\n");
    else perror("Failed to start daemon");
}

void stop_daemon() {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        printf("Daemon is not running.\n");
        return;
    }
    int pid;
    fscanf(pid_file, "%d", &pid);
    fclose(pid_file);
    if (kill(pid, SIGTERM) == 0) {
        printf("Daemon stopped successfully.\n");
        remove(PID_FILE);
    } else perror("Failed to stop daemon");
}

void kill_daemon() {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        printf("Daemon is not running or PID file is missing.\n");
        return;
    }

    int pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        printf("Error reading PID file.\n");
        fclose(pid_file);
        return;
    }
    fclose(pid_file);

    // Verify the process is still running
    if (kill(pid, 0) == -1) {
        perror("Process not found");
        remove(PID_FILE); // Clean up stale PID file
        return;
    }

    // Force kill the process
    if (kill(pid, SIGKILL) == 0) {
        printf("Daemon forcefully killed (PID: %d).\n", pid);
        remove(PID_FILE);
    } else {
        perror("Failed to kill daemon");
    }
}

void status_daemon() {
    if (access(PID_FILE, F_OK) == 0) {
        FILE *pid_file = fopen(PID_FILE, "r");
        int pid;
        fscanf(pid_file, "%d", &pid);
        fclose(pid_file);
        printf("Daemon is running with PID %d\n", pid);
    } else {
        printf("Daemon is not running.\n");
    }
  }

void get_value() {
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    SharedData *shm_ptr = mmap(0, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char currentTime[9];

    printf("TEMP: %d\n", getTemp());
    printf("CURRENT SPEED: %d\n", shm_ptr->currentSpeed);
    printf("SCAN TIME: %s\n", getTime());
    // printf("PWM Pin: %d\n", shm_ptr->pins.pwm);
    // printf("Night Mode: %d\n", shm_ptr->night.mode);
    // printf("Night Time Window: %s\n", shm_ptr->night.time_window_hours);
    // printf("Night Speed: %d\n", shm_ptr->night.speed);
    // printf("Temp Values: %d, %d, %d\n", shm_ptr->mapping.temp_vals[0], shm_ptr->mapping.temp_vals[1], shm_ptr->mapping.temp_vals[2]);
    // printf("Speed Values: %d, %d, %d\n", shm_ptr->mapping.speed_vals[0], shm_ptr->mapping.speed_vals[1], shm_ptr->mapping.speed_vals[2]);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}

void set_value(char *key, char *value) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    SharedData *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if (strcmp(key, "pwm") == 0) {
        shm_ptr->pins.pwm = atoi(value);
        printf("PWM Pin updated to: %d\n", shm_ptr->pins.pwm);
    } else if (strcmp(key, "mode") == 0) {
        shm_ptr->night.mode = atoi(value);
        printf("Night Mode updated to: %d\n", shm_ptr->night.mode);
    } else if (strcmp(key, "time_window") == 0) {
        strncpy(shm_ptr->night.time_window_hours, value, sizeof(shm_ptr->night.time_window_hours)-1);
        shm_ptr->night.time_window_hours[sizeof(shm_ptr->night.time_window_hours)-1] = '\0';
        printf("Night Time Window updated to: %s\n", shm_ptr->night.time_window_hours);
    } else if (strcmp(key, "speed") == 0) {
        shm_ptr->night.speed = atoi(value);
        printf("Night Speed updated to: %d\n", shm_ptr->night.speed);
    } else if (strcmp(key, "temp_vals") == 0) {
        int i = 0;
        char *temp = strtok(value, ",");
        while (temp != NULL && i < MAX_MAPPING_SIZE) {
            shm_ptr->mapping.temp_vals[i] = atoi(temp);
            temp = strtok(NULL, ",");
            i++;
        }
        printf("Temp Values updated to: %d, %d, %d\n", shm_ptr->mapping.temp_vals[0], shm_ptr->mapping.temp_vals[1], shm_ptr->mapping.temp_vals[2]);
    } else if (strcmp(key, "speed_vals") == 0) {
        int i = 0;
        char *temp = strtok(value, ",");
        while (temp != NULL && i < MAX_MAPPING_SIZE) {
            shm_ptr->mapping.speed_vals[i] = atoi(temp);
            temp = strtok(NULL, ",");
            i++;
        }
        printf("Speed Values updated to: %d, %d, %d\n", shm_ptr->mapping.speed_vals[0], shm_ptr->mapping.speed_vals[1], shm_ptr->mapping.speed_vals[2]);
    } else if (strcmp(key, "currentSpeed") == 0) {
        shm_ptr->currentSpeed = atoi(value);
        // printf("Current Speed updated to: %d\n", shm_ptr->currentSpeed);
    } else {
        printf("Invalid key. No value updated.\n");
    }

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}
