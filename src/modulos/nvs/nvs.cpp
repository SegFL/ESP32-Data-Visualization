



#include "nvs.h"

//Logitud maxima de caracteres a leer por NVS
 size_t max_len =64;

void nvsInit(){

    esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
}

int saveValueNVS(const char* key, bool value) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(my_handle, key, value ? 1 : 0); // Guardar el valor como uint8_t
        if (err == ESP_OK) {
            err = nvs_commit(my_handle); // Confirmar los cambios
        }
        nvs_close(my_handle); // Cerrar el handle
    } else {
        writeSerialComln(String("Error al abrir NVS"));
        return -1;
    }
    return 0;
}


bool readValueNVS(const char* key) {
    nvs_handle_t my_handle;
    uint8_t value = 0; // Valor por defecto
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(my_handle, key, &value);
        nvs_close(my_handle);
        if (err == ESP_OK) {
            return value; // Retornar el valor leído
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            writeSerialComln(String("Clave no encontrada en NVS")+String(key));
        } else {
            writeSerialComln(String("Error al leer NVS"));
        }
    } else {
        writeSerialComln(String("Error al abrir NVS"));
    }
    return false; // Retornar false en caso de error
}


//Lee un valor de la memora NVS con el key como argumento .Devuelve -1 si falla
int readValueNVSint32_t(const char* key){

        nvs_handle_t handle;
        esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
        if (err == ESP_OK) {
            int stored = -1;
            if (nvs_get_i32(handle, key, &stored) == ESP_OK) {
                nvs_close(handle);
                return stored;
            }
            nvs_close(handle);
        }
        return -1;
}


int saveStringNVS(const char* key, const char* value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return ESP_FAIL;  // Error al abrir NVS
    }

    err = nvs_set_str(handle, key, value);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;       // Error en escritura
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return err; // ESP_OK si todo salió bien
}


int readStringNVS(const char* key, char* out_buffer) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;  // No se pudo abrir
    }

    size_t required_len = max_len;

    err = nvs_get_str(handle, key, out_buffer, &required_len);

    nvs_close(handle);

    return err;       // ESP_OK, ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NVS_INVALID_LENGTH, etc.
}