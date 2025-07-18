#include "menuTree.h"

bool hasChildWithKey(MenuNode *node, char key);

MenuNode* create_node(const char* title, char key, int id) {
    MenuNode *node = (MenuNode*)malloc(sizeof(MenuNode));
    if (node == nullptr) {
        return nullptr;
    }

    // Inicializa los punteros a nullptr
    memset(node->children, 0, sizeof(node->children));

    // Copia el título de manera segura
    strncpy(node->title, title, sizeof(node->title) - 1);
    node->title[sizeof(node->title) - 1] = '\0';  // Asegurar terminación de cadena

    node->key = key;
    node->parent = nullptr;
    node->child_count = 0;
    node->id = id;  // Inicializar el ID en 0

    return node;
}

// Agregar hijo con tecla de acceso
void add_child(MenuNode *parent, MenuNode *child) {
    if (parent == nullptr || child == nullptr) {
        writeSerialComln("Error: Invalid parent or child node");
        return;
    }

    // Verificar si ya existe un hijo con la misma tecla
    if (hasChildWithKey(parent, child->key)) {
        writeSerialComln("Error: Child with this key already exists");
        return;
    }

    // Buscar un espacio disponible en el array de hijos
    if (parent->child_count < MAX_CHILDREN) {
        int i = 0;
        while (i < MAX_CHILDREN && parent->children[i] != nullptr) {
            i++;
        }

        // Asegurarse de que no se excede el límite
        if (i < MAX_CHILDREN) {
            parent->children[i] = child;
            child->parent = parent;  // Se asigna el nodo padre
            parent->child_count++;
        } else {
            writeSerialComln("Error: No space available for new child");
        }
    }
}

MenuNode* menuInit() {
    MenuNode* root = create_node("Bienvenido al menu de configuracion", 'a',0);
    if (root == nullptr) {
        writeSerialComln("Error: Failed to create root node");
        return nullptr;
    }

    MenuNode* child1 = create_node("Entradas analogicas", '1',1);
    add_child(root, child1);
    MenuNode* child2 = create_node("Configuracion de WiFi", '2',2);
    add_child(root, child2);

        MenuNode* child4 = create_node("Entre SSID", '1',3);
        add_child(child2, child4);

        MenuNode* child3 = create_node("Entre constraseña", '2',4);
        add_child(child2, child3);

    MenuNode* child5 = create_node("Configuracion PWM", '3',5);
    add_child(root, child5);

        MenuNode* child8 = create_node("Modificar Duty Cycle del PWM", '1',8);
        add_child(child5, child8);

        MenuNode* child9 = create_node("Modificar frecuencia del PWM", '2',9);
        add_child(child5, child9);

        MenuNode* child10 = create_node("Modificar valor maximo del PWM", '3',10);
        add_child(child5, child10);

        MenuNode* child11 = create_node("Presione enter + el valor maximo de DutyCycle permitido", '1',11);
        add_child(child10, child11);

    MenuNode* child6 = create_node("Modo de funcionamiento", '4',6);
    add_child(root, child6);

        MenuNode* child7 = create_node("Presiona enter + y para pasar a modo SEND DATA o enter + n para desactivar", '1',7);
        add_child(child6, child7);


    MenuNode* child12 = create_node("Fecha y hora", '5',12);
    add_child(root, child12);

    MenuNode* child13 = create_node("Curva de carga", '6',13);
    add_child(root, child13);

        MenuNode* child14 = create_node("Crear curva de carga", '1',14);
        add_child(child13, child14);

            MenuNode* child17 = create_node("Agregar Pin", '1',17);
            add_child(child14, child17);

            MenuNode* child18 = create_node("Agregar puntos", '2',18);
            add_child(child14, child18);
                MenuNode* child19 = create_node("Seleccionar curva", '1',19);
                add_child(child18, child19);

                MenuNode* child20 = create_node("Agregar punto a la curva", '2',20);
                add_child(child18, child20);

        MenuNode* child15 = create_node("Ver curvas", '2',15);
        add_child(child13, child15);

        MenuNode* child16 = create_node("Activar curvas", '3',16);
        add_child(child13, child16);

    return root;
}



void menuUpdate(char caracter, MenuNode **current) {
    if (current == nullptr || *current == nullptr) {  
        return;  // Verifica que 'current' y '*current' no sean nulos
    }

    if (caracter == GO_BACK && (*current)->parent) {  // Si es ESC, sube al nodo padre
        *current = (*current)->parent;
    } else {
        // Buscar si el carácter ingresado corresponde a un hijo
        for (int i = 0; i < (*current)->child_count; i++) {
            if ((*current)->children[i] != nullptr && (*current)->children[i]->key == caracter) {
                *current = (*current)->children[i];  // Cambia al nodo hijo
                return;
            }
        }
    }
}
void printNode(MenuNode *node) {
    if (node == nullptr) {
        return;
    }

    // Imprime el título del nodo padre
    writeSerialComln(String(node->title));

    // Imprime los hijos con su respectiva clave y título
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i] != nullptr) {
            writeSerialComln(String("\t"));  // Tabulación para los hijos
            writeSerialComln(String(node->children[i]->key));
            writeSerialComln(String(" -> "));
            writeSerialComln(String(node->children[i]->title));
        }
    }
}



void printFullMenu(MenuNode *root) {
    if (root == nullptr) {
        return;
    }

    std::queue<std::pair<MenuNode*, int>> q;  // Cola para BFS (nodo, nivel)
    q.push({root, 0});

    while (!q.empty()) {
        MenuNode* current = q.front().first;
        int level = q.front().second;
        q.pop();

        // Imprime la indentación según el nivel del nodo
        for (int i = 0; i < level; i++) {
            writeSerialComln("\t");
        }

        // Imprime el nodo actual
        writeSerialComln(current->title);

        // Agrega los hijos a la cola con el siguiente nivel
        for (int i = 0; i < current->child_count; i++) {
            if (current->children[i] != nullptr) {
                q.push({current->children[i], level + 1});
            }
        }
    }
}


bool hasChildWithKey(MenuNode *node, char key) {
    if (node == nullptr) {
        return false;
    }

    // Verifica todos los hijos directos del nodo
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i] != nullptr && node->children[i]->key == key) {
            return true;
        }
    }

    return false;
}

void freeMenu(MenuNode *node) {
    if (node == nullptr) {
        return;  // Si el nodo es nulo, no hace nada
    }

    // Libera los nodos hijos recursivamente
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i] != nullptr) {
            freeMenu(node->children[i]);  // Llamada recursiva
        }
    }

    // Después de liberar los hijos, libera el nodo actual
    free(node);
}

char* get_title(MenuNode* menu){
    if(menu == nullptr){
        return nullptr;
    }
    return menu->title;
}

