#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <pi-gpio.h>

#include "config.h"
#include "helper.h"

#define SHM_NAME "/temp_monitor_daemon_shm"
#define SHM_SIZE sizeof(SharedData)
#define PID_FILE "/tmp/temp_monitor_daemon_pid"

volatile sig_atomic_t keep_running = 1;
SharedData *shm_ptr = NULL;  // Global pointer to shared memory
int shm_fd = -1;  // Global file descriptor for shared memor

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

int main() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    if (pid > 0) {
        FILE *pid_file = fopen(PID_FILE, "w");
        if (pid_file) {
            fprintf(pid_file, "%d", pid);
            fclose(pid_file);
        }
        exit(0);
    }

    setsid();
    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    ftruncate(shm_fd, SHM_SIZE); // Set the size of the shared memory
    SharedData *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    shm_ptr->pins.pwm = 18;
    shm_ptr->night.mode = 1;
    strcpy(shm_ptr->night.time_window_hours, "22-8");
    shm_ptr->night.speed = 60;
    shm_ptr->mapping.temp_vals[0] = 30;
    shm_ptr->mapping.temp_vals[1] = 40;
    shm_ptr->mapping.temp_vals[2] = 50;
    shm_ptr->mapping.speed_vals[0] = 60;
    shm_ptr->mapping.speed_vals[1] = 80;
    shm_ptr->mapping.speed_vals[2] = 100;

    shm_ptr->currentSpeed = 0;

    while (keep_running) {
        if(shm_ptr->night.mode && is_night(shm_ptr->night.time_window_hours)) {
            setSpeed(shm_ptr->night.speed, shm_ptr->pins.pwm);
        } else {
            int result = interpolate(shm_ptr->mapping.temp_vals, shm_ptr->mapping.speed_vals, 3, getTemp());
            setSpeed(result, shm_ptr->pins.pwm);
            set_value("currentSpeed", result);
            shm_ptr->currentSpeed = result;
        }
        msync(shm_ptr, sizeof(SharedData), MS_SYNC);  // Ensure memory sync

        sleep(5);
    }

    // Cleanup before exiting
    handle_signal(SIGTERM);

    return 0;
}