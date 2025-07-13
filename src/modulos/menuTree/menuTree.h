

#include <stdlib.h>
#include <Arduino.h>
#include <cstring>  // Para strncpy
#include <queue>

#define MAX_CHILDREN 10 // Número máximo de hijos por nodo (ajustable)
// Definición del nodo del árbol del menú


typedef struct MenuNode {
    char title[70];                     // Nombre del menú (ej: "MENU1")
    char key;                         // Teclas para acceder a los hijos
    struct MenuNode *parent;         // Nodo padre (para volver atrás)
    struct MenuNode *children[MAX_CHILDREN];    // Punteros a hijos (máx MAX_CHILDREN, ajustable)
    int child_count;                 // Número de hijos
    int id;                         // Identificador unico del nodo
} MenuNode;

void add_child(MenuNode *parent, MenuNode *child);
MenuNode* create_node(String title,char key,int id);
MenuNode* menuInit() ;

void menuUpdate(char caracter, MenuNode **current);
char* get_title(MenuNode* menu);


void freeMenu(MenuNode *node);
void printNode(MenuNode *node);
void printFullMenu(MenuNode *node) ;
