

#include <string>
#include <nvs.h>
#include "../../modulos/serialCom/serialCom.h"


void saveValueNVS(const char* key, bool value);
bool readValueNVS(const char* key);

