

#include <string>
#include <nvs.h>
#include "../../modulos/serialCom/serialCom.h"
#include <nvs_flash.h>

int saveValueNVS(const char* key, char valor) ;
bool readValueNVS(const char* key, char *valor) ;
int readValueNVSint32_t(const char* key,int* value);
int saveStringNVS(const char* key, const char* valor);
int readStringNVS(const char* key, char* out_buffer);
void nvsInit();

