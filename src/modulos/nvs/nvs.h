

#include <string>
#include <nvs.h>
#include "../../modulos/serialCom/serialCom.h"
#include <nvs_flash.h>

int saveValueNVS(const char* key, bool value);
bool readValueNVS(const char* key);
int readValueNVSint32_t(const char* key);
int saveStringNVS(const char* key, const char* value);
int readStringNVS(const char* key, char* out_buffer);
void nvsInit();

