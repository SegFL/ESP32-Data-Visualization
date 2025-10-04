#ifndef USERINTERFACE_H
#define USERINTERFACE_H
#include "../menuTree/menuTree.h"
#include "../serialCom/serialCom.h"

#include "../WiFi/WiFi.h"
#include "../carga_electronica/carga_electronica.h"
#include "../nvs/nvs.h"


void userInterfaceInit();
void userInterfaceUpdate();

#endif // USERINTERFACE_H





/*

Pasos a seguir paracrear un menu

-Agregar el nodo en menuTree.cpp con un id nuevo
-Agregar if menu id==xx en onEnterNode para mostrar mensaje al entrar
-Agregar if menu id==xx en nodeRequiresInput si el nodo requiere datos


*/