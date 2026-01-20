

/*


Para agregar un nuevo comando 
1) Agregar titulo y un id uncio en menuInit() en menuTree.cpp y asignar el nodo con add_child()

2) Para agregar una accion que se ejecute un unica vez agregar un case con el id correspondiente 
    en onEnterNode() en userInterface.cpp


3A) Si el nodo requiere entrada de datos agregar un case en onEnterNode() el mensaje que
    se quiere mostrar al entrar al nodo.

3B) Si el nodo requiere entrada de datos agregar un case en nodeRequiresInput()

4) Si es necesario actualizar datos periodicamente mientras se esta en un nodo agregar el case 
    en onUpdateNode()

5) El filtrado y procesamiento de los datos ingresados por el usuario se ejectuara en procesarDatos()
    agregar un if(menu->id==xx) para el id correspondiente y procesar los datos recibidos en "data"

*/






#ifndef USERINTERFACE_H
#define USERINTERFACE_H
#include "../menuTree/menuTree.h"
#include "../serialCom/serialCom.h"

#include "../WiFi/WiFi.h"
#include "../carga_electronica/carga_electronica.h"
#include "../nvs/nvs.h"
#include "../time/time.h"

void userInterfaceInit();
void userInterfaceUpdate();

#endif // USERINTERFACE_H





/*

Pasos a seguir paracrear un menu

-Agregar el nodo en menuTree.cpp con un id nuevo
-Agregar if menu id==xx en onEnterNode para mostrar mensaje al entrar
-Agregar if menu id==xx en nodeRequiresInput si el nodo requiere datos


*/