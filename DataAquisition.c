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


// Vector con los nombres de los archivos
const char *filenames[] = {"..\\App de MATLAB\\Data\\Sensor1.csv", "..\\App de MATLAB\\Data\\Sensor2.csv", "..\\App de MATLAB\\Data\\Sensor3.csv", "..\\App de MATLAB\\Data\\Sensor4.csv"}; 


#define BUFFER_SIZE 512
#ifndef _WIN32
typedef unsigned long DWORD;
#endif
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
    FILE *file;
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
            if (buffer[i] == '\r') {
                continue;
            } else if (buffer[i] == '\n') {
                buffer_acumulado[buffer_index] = '\0';

                char *last_comma = strrchr(buffer_acumulado, ',');
                if (last_comma != NULL && *(last_comma + 1) != '\0') {
                    int index = *(last_comma + 1) - '0'; // Convertir el último caracter a entero
                    if (index >= 0 && index < 4) {
                        FILE *file = fopen(filenames[index], "a");
                        if (file != NULL) {
                            fprintf(file, "%s\n", buffer_acumulado);
                            fclose(file);
                            printf("Escrito en %s: %s\n", filenames[index], buffer_acumulado);
                        } else {
                            printf("Error al abrir %s\n", filenames[index]);
                        }
                    } else {
                        printf("Índice fuera de rango: %d\n", index);
                    }
                } else {
                    printf("Error al procesar la línea: %s\n", buffer_acumulado);
                }
                buffer_index = 0;
                buffer_acumulado[0] = '\0';
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
