#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#endif

#define BUFFER_SIZE 512
#define COMMAND_PREFIX "CREATE_FILE,"
#define BASE_PATH "..\\App de MATLAB\\Data\\"

#ifndef _WIN32
typedef unsigned long DWORD;
#endif

// Variables globales para el control de archivos
int file_counter[4] = {1, 1, 1, 1}; 
FILE *files[4] = {NULL, NULL, NULL, NULL};

// Función para crear un nuevo archivo con el formato adecuado
void create_new_file(int sensor_index, unsigned long epoch_seconds) {
    if (files[sensor_index] != NULL) {
        fclose(files[sensor_index]);
    }

    char filename[128];
    sprintf(filename, "%sSensor%d_%d.csv", BASE_PATH, sensor_index + 1, file_counter[sensor_index]);
    files[sensor_index] = fopen(filename, "w");

    if (files[sensor_index]) {
        fprintf(files[sensor_index], "Epoch Time: %lu\n", epoch_seconds);
        printf("Archivo creado: %s\n", filename);
        file_counter[sensor_index]++;
    } else {
        printf("Error creando el archivo: %s\n", filename);
    }
}

#ifdef _WIN32
HANDLE open_serial_port(const char *port_name, DWORD baudrate) {
    HANDLE hSerial = CreateFile(port_name, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error abriendo el puerto %s\n", port_name);
        return NULL;
    }

    DCB dcbSerialParams = {0};
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Error obteniendo el estado del puerto\n");
        return NULL;
    }

    dcbSerialParams.BaudRate = baudrate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Error configurando el puerto serie\n");
        return NULL;
    }

    return hSerial;
}
#else
int open_serial_port(const char *port_name, int baudrate) {
    int fd = open(port_name, O_RDONLY | O_NOCTTY);
    if (fd == -1) {
        printf("Error abriendo el puerto %s\n", port_name);
        return -1;
    }
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error en tcgetattr\n");
        return -1;
    }
    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);
    tty.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}
#endif

void pedir_datos_usuario(char *port_name, DWORD *baudrate) {
    printf("Introduce el puerto serie (ej. COM2 en Windows o /dev/ttyUSB0 en Linux): ");
    fgets(port_name, 256, stdin);
    port_name[strcspn(port_name, "\n")] = 0;

    printf("Introduce el baudrate (ej. 115200): ");
    scanf("%ld", baudrate);
    getchar();
}

int main() {
    char buffer[128];
    char port_name[256];
    DWORD baudrate;
    char buffer_acumulado[BUFFER_SIZE] = {0};
    int buffer_index = 0;

    pedir_datos_usuario(port_name, &baudrate);

#ifdef _WIN32
    HANDLE serial = open_serial_port(port_name, baudrate);
    if (serial == NULL) return 1;
#else
    int serial = open_serial_port(port_name, baudrate);
    if (serial == -1) return 1;
#endif

    printf("Conectado a ESP32 en %s con baudrate %ld\n", port_name, baudrate);

    while (1) {
#ifdef _WIN32
        DWORD bytesRead;
        ReadFile(serial, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
#else
        int bytesRead = read(serial, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) continue;
#endif

        buffer[bytesRead] = '\0';

        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\r') continue;
            if (buffer[i] == '\n') {
                buffer_acumulado[buffer_index] = '\0';
                printf("Línea recibida: %s\n", buffer_acumulado);

                if (strncmp(buffer_acumulado, COMMAND_PREFIX, strlen(COMMAND_PREFIX)) == 0) {
                    unsigned long epoch_seconds = strtoul(buffer_acumulado + strlen(COMMAND_PREFIX), NULL, 10);
                    for (int sensor = 0; sensor < 4; sensor++) {
                        create_new_file(sensor, epoch_seconds);
                    }
                } else {
                    int sensor_id = buffer_acumulado[strlen(buffer_acumulado) - 1] - '0';
                    if (sensor_id >= 0 && sensor_id < 4 && files[sensor_id] != NULL) {
                        fprintf(files[sensor_id], "%s\n", buffer_acumulado);
                        fflush(files[sensor_id]);
                    }
                }

                buffer_index = 0;
            } else {
                if (buffer_index < BUFFER_SIZE - 2) {
                    buffer_acumulado[buffer_index++] = buffer[i];
                    buffer_acumulado[buffer_index] = '\0';
                }
            }
        }

#ifdef _WIN32
        Sleep(10);
#else
        usleep(1000);
#endif
    }

    return 0;
}