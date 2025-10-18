

#include <string>
#include <nvs.h>
#include "../../modulos/serialCom/serialCom.h"


int saveValueNVS(const char* key, bool value);
bool readValueNVS(const char* key);
int readValueNVSint32_t(const char* key);

