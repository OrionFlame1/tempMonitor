#include "helper.h"

volatile sig_atomic_t keep_running = 1;
SharedData *shm_ptr = NULL;
int shm_fd = -1;

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


void edit_value(const char *key, const char *value) {
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
    } else if (strcmp(key, "currentSpeed") == 0) {
        shm_ptr->currentSpeed = atoi(value);
        printf("Current Speed updated to: %d\n", shm_ptr->currentSpeed);
    } else {
        printf("Invalid key. No value updated.\n");
    }

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}

void write_time_to_file(const char* string, const char *filename) {
    FILE *file;
    time_t rawtime;
    struct tm *timeinfo;

    // Open file in append mode
    file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Write time to file
    fprintf(file, "%s - Current Time: %s", string, asctime(timeinfo));
    fflush(file); // Ensure data is written to file immediately

    fclose(file);
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

    SharedData loaded_config = load_config_from_file("config.json");
    memcpy(shm_ptr, &loaded_config, sizeof(SharedData));

    shm_ptr->currentSpeed = 0;

    while (1) {
        if(shm_ptr->night.mode && is_night(shm_ptr->night.time_window_hours)) {
            set_speed(shm_ptr->night.speed);
        } else {
            int result = interpolate(shm_ptr->mapping.temp_vals, shm_ptr->mapping.speed_vals, 3, get_temp());
            set_speed(result);
            shm_ptr->currentSpeed = result;
        }
        sleep(FREQ);
    }

    return 0;
}