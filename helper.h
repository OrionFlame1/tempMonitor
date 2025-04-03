#include <pi-gpio.h>
#include <time.h>

#define PWM0 18                    // default - to physical pin 12
#define SILENT 60 // preset value to define a speed that will make the fan pretty much silent
#define FREQ 5 // seconds in-between checks of the temp

#define DIVIDER 18
#define RANGE 120

void setSpeed(int speed, int pwmPort) {
    if (!pwmPort) {
        pwmPort = PWM0;
    }

    setup();
    
    pwmSetGpio(pwmPort);

    pwmSetMode(PWM_MODE_MS); // use a fixed frequency
    pwmSetClock(DIVIDER);

    pwmSetRange(pwmPort, RANGE);
    // The following gives a precise 25kHz PWM signal
    // Noctua recommended value
    // CLOCK / (DIVIDER * RANGE)
    pwmWrite(pwmPort, RANGE * speed / 100); // duty cycle set

    cleanup();
}

int getTemp() {
    FILE *fp;
    char path[1035];

    /* Open the command for reading. */
    fp = popen("vcgencmd measure_temp", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }

    int number;
    char *ptr;

    /* Read the output a line at a time - output it. */
    while (fgets(path, sizeof(path), fp) != NULL) {
        number = strtol(path + 5, &ptr, 10); // Skip first 5 characters ("temp=")
    }

    /* close */
    pclose(fp);

    return number;
}

char* getTime() {
    static char buffer[9];  // Buffer to hold formatted time (HH:mm:ss)
    time_t t;
    struct tm *tm_info;

    // Get current time
    time(&t);

    // Convert time to local time
    tm_info = localtime(&t);

    // Format the time as HH:mm:ss
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

    return -1; // Should not happen
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

// void update_time(SharedData *shm_ptr) {
//     if (shm_ptr == NULL) return;  // Ensure the pointer is valid

//     time_t now = time(NULL);
//     struct tm *tm_info = localtime(&now);
    
//     // Correctly write to shared memory
//     strftime(shm_ptr->currentTime, sizeof(shm_ptr->currentTime), "%H:%M:%S", tm_info);
    
//     msync(shm_ptr, sizeof(SharedData), MS_SYNC);  // Ensure memory sync
// }