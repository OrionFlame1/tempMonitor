#include "helper.h"

void start_daemon() {
    if (access(PID_FILE, F_OK) == 0) {
        printf("Daemon is already running.\n");
        return;
    }
    if (system("sudo ./temp_monitor_daemon &") == 0) printf("Daemon started successfully.\n");
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

    printf("TEMP: %d\n", get_temp());
    printf("CURRENT SPEED: %d\n", shm_ptr->currentSpeed);
    printf("INTERPOLATED VALUE: %d\n", interpolate(shm_ptr->mapping.temp_vals, shm_ptr->mapping.speed_vals, 3, get_temp()));
    printf("SCAN TIME: %s\n", get_time());
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
        printf("Current Speed updated to: %d\n", shm_ptr->currentSpeed);
    } else {
        printf("Invalid key. No value updated.\n");
    }

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s start | stop | get | set <key> <value>\n", argv[0]);
        return 1;
    }
    if (is_number(argv[1])) {
        int speed = atoi(argv[1]);
        if (speed < 0 || speed > 100) {
            printf("Speed must be between 0 and 100\n");
            return 1;
        }
        stop_daemon();
        printf("Setting speed to %d\n", speed);
        set_speed(speed);        

        return 0;
    }
    else if (strcmp(argv[1], "start") == 0) start_daemon();
    else if (strcmp(argv[1], "stop") == 0) stop_daemon();
    else if (strcmp(argv[1], "kill") == 0) kill_daemon();
    else if (strcmp(argv[1], "status") == 0) status_daemon();
    else if (strcmp(argv[1], "get") == 0) get_value();
    else if (strcmp(argv[1], "set") == 0 && argc == 4) set_value(argv[2], argv[3]);
    else {
        printf("Invalid command. Use 'start', 'stop', 'get', or 'set <key> <value>'.\n");
        return 1;
    }
    return 0;
}