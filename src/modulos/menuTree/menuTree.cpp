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
        Serial.println("Error: Invalid parent or child node");
        return;
    }

    // Verificar si ya existe un hijo con la misma tecla
    if (hasChildWithKey(parent, child->key)) {
        Serial.println("Error: Child with this key already exists");
        return;
    }

    // Buscar un espacio disponible en el array de hijos
    if (parent->child_count < 5) {
        int i = 0;
        while (i < 5 && parent->children[i] != nullptr) {
            i++;
        }

        // Asegurarse de que no se excede el límite
        if (i < 5) {
            parent->children[i] = child;
            child->parent = parent;  // Se asigna el nodo padre
            parent->child_count++;
        } else {
            Serial.println("Error: No space available for new child");
        }
    }
}

MenuNode* menuInit() {
    MenuNode* root = create_node("Bienvenido al menu de configuracion", 'a',0);
    if (root == nullptr) {
        Serial.println("Error: Failed to create root node");
        return nullptr;
    }

    MenuNode* child1 = create_node("Entradas analogicas", '1',1);
    add_child(root, child1);
    MenuNode* child2 = create_node("Configuracion de WiFi", '2',2);
    add_child(root, child2);
    MenuNode* child4 = create_node("Entre constraseña", '1',3);
    add_child(child2, child4);
    MenuNode* child3 = create_node("Entre SSID", '2',4);
    add_child(child2, child3);
    MenuNode* child5 = create_node("Configuracion adicional", '3',5);
    add_child(root, child5);
    return root;
}



void menuUpdate(char caracter, MenuNode **current) {
    if (current == nullptr || *current == nullptr) {  
        return;  // Verifica que 'current' y '*current' no sean nulos
    }

    if (caracter == 27 && (*current)->parent) {  // Si es ESC, sube al nodo padre
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
    Serial.println(node->title);

    // Imprime los hijos con su respectiva clave y título
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i] != nullptr) {
            Serial.print("\t");  // Tabulación para los hijos
            Serial.print(node->children[i]->key);
            Serial.print(" -> ");
            Serial.println(node->children[i]->title);
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
            Serial.print("\t");
        }

        // Imprime el nodo actual
        Serial.println(current->title);

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

