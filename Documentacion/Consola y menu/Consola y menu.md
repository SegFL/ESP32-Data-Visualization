# Módulo userInterface

### Documentación técnica y guía de uso

*Firmware ESP32 — Carga electrónica*

---

## 1. Descripción general

El módulo `userInterface` es responsable de toda la interacción con el operador a través del puerto serie (UART). Expone dos funciones públicas principales:

* `userInterfaceInit()` — inicialización: configura la UART, construye el árbol de menús, carga la configuración desde NVS y crea el timer de timeout para transferencia de curvas.
* `userInterfaceUpdate()` — debe llamarse periódicamente desde el loop principal. Consume un carácter por vez de la UART y avanza la máquina de estados de la interfaz.

El módulo  **no bloquea** . Cada llamada a `userInterfaceUpdate()` procesa como máximo un carácter y retorna inmediatamente.

---

## 2. Flujo principal — máquina de estados

Internamente, `userInterfaceUpdate()` es una máquina de estados simple basada en variables globales estáticas:

```
userInterfaceUpdate()  ← llamada periódica desde el loop
  │
  ├─ '@' recibido  →  activa APP_MODE, descarta buffer
  ├─ '#' recibido  →  desactiva APP_MODE, vuelve al menú
  │
  ├─ APP_MODE activo  →  handleAppMode(c)
  │       acumula en app_buffer hasta '\n'
  │       luego llama procesarComandoApp()
  │
  ├─ ESC / '<'  →  menuUpdate(GO_BACK)  →  sube al nodo padre
  │       onEnterNode()  →  acciones de entrada al nodo padre
  │
  ├─ aceptandoDatos == false  →  navegación de menú
  │       menuUpdate(c)  →  baja al hijo cuya key == c
  │       si el nodo cambió: onEnterNode()  →  prompt + acciones
  │
  └─ aceptandoDatos == true
          acumula c en data_buffer[]
          al recibir '\n'  →  procesarDatos(data_buffer)
          luego reactiva aceptandoDatos si el nodo sigue pidiéndolo
```

> **Variable clave: `aceptandoDatos`**
>
> * `true` → los caracteres se acumulan en `data_buffer` hasta recibir Enter.
> * `false` → los caracteres se interpretan como teclas de navegación del menú.
> * Se activa automáticamente en `onEnterNode()` cuando el nodo está en la lista `nodeRequiresInput()`.
> * Se reactiva tras cada Enter, permitiendo ingresar múltiples valores sin navegar.

---

## 3. Modos de operación

### 3.1 Modo consola (menú interactivo)

Modo por defecto. El operador navega el árbol de menús pulsando la tecla que corresponde a cada opción. Al llegar a un nodo hoja que requiere entrada, escribe el valor y pulsa Enter. El módulo valida y aplica el cambio.

### 3.2 Modo APP (`@...#`)

Se activa enviando el carácter `@` y se desactiva con `#`. En este modo la navegación de menús queda suspendida y la interfaz acepta comandos completos terminados en `\n`, pensados para ser enviados por un programa externo (PC, app móvil, etc.).

Protocolo: enviar `@` → recibir `APP_MODE_ON`. Desactivar: `#` → recibir `APP_MODE_OFF`.

El único conjunto de comandos implementado en APP_MODE es la transferencia de curvas por chunks (ver sección 6).

---

## 4. El árbol de menús (menuTree)

### 4.1 Estructura de un nodo

```c
typedef struct MenuNode {
    char   title[MAX_TITLE_LEN];    // texto que se muestra al usuario
    char   key;                     // tecla que activa este nodo desde su padre
    int    id;                      // identificador único, usado en procesarDatos()
    MenuNode* parent;               // puntero al nodo padre (NULL en la raíz)
    MenuNode* children[MAX_CHILDREN];
    int    child_count;
} MenuNode;
```

### 4.2 Navegación

`menuUpdate(char c, MenuNode** current)` implementa la navegación:

* Si `c == GO_BACK` y hay nodo padre → sube al padre.
* En cualquier otro caso → busca entre los hijos aquel cuyo campo `key` coincida con `c` y baja a él.
* Si no encuentra coincidencia → no hace nada (el nodo actual no cambia).

### 4.3 Restricción de keys

Dos hijos del mismo nodo no pueden tener la misma key. `add_child()` lo verifica con `hasChildWithKey()` y rechaza la inserción si hay colisión, imprimiendo un error por serie.

### 4.4 Mapa completo del árbol actual

| Nodo (título)                | Key   | ID | Función / descripción                                         | Hijos |
| ----------------------------- | ----- | -- | --------------------------------------------------------------- | ----- |
| Bienvenido al menú           | `a` | 0  | Raíz del árbol                                                | 5     |
| Entradas analógicas          | `1` | 1  | Muestra datos de sensores ADC en tiempo real (onUpdate)         | —    |
| Configuración de WiFi        | `2` | 2  | Sub-menú WiFi                                                  | 3     |
| Entre SSID                    | `1` | 3  | Pide el SSID por teclado y lo aplica                            | —    |
| Entre contraseña             | `2` | 4  | Pide la contraseña por teclado                                 | —    |
| Conectar a WiFi               | `3` | 33 | Intenta conectar en onEnter (llama `connectWiFi`)             | —    |
| Configuración manual         | `3` | 5  | Sub-menú ajustes manuales                                      | 4     |
| Modificar corriente           | `1` | 8  | Pide `index,mA`y llama `setCurrentReference_mA()`           | —    |
| Modificar frecuencia PWM      | `2` | 9  | Pide `index,Hz`y llama `PWMSetFrequency()`                  | —    |
| Modificar max duty cycle      | `3` | 10 | Pide `index,%`y llama `PWMSetMaxDC()`                       | —    |
| Modo de funcionamiento        | `4` | 6  | Activa/desactiva SEND_DATA (`y`/`n`)                        | —    |
| Fecha y hora                  | `5` | 12 | Sub-menú fecha/hora                                            | 2     |
| Ver fecha                     | `1` | 24 | Muestra fecha formateada en tiempo real (onUpdate)              | —    |
| Modificar fecha               | `2` | 25 | Pide `DD/MM/AAAA HH:MM`y llama `setDateTime()`              | —    |
| Curva de carga                | `6` | 13 | Sub-menú gestión de curvas                                    | 9     |
| Crear curva de carga          | `1` | 14 | Sub-menú creación                                             | 3     |
| Crear curva                   | `1` | 18 | Pide ID de pin y llama `createCurve()`                        | —    |
| Settear límites              | `2` | 19 | Pide `CurveId,Imin,Imax,Vmin,Vmax,Pmin,Pmax`                  | —    |
| Agregar punto                 | `3` | 20 | Pide `curva,tiempo,valor,tipo`y llama `addPointToCurve()`   | —    |
| Ver curvas                    | `2` | 15 | Imprime curvas en RAM (onEnter)                                 | —    |
| Asociar curva a pin           | `3` | 16 | Pide `curvaId,pin`y llama `asociarCurvaAPin()`              | —    |
| Activar/desactivar modo curva | `4` | 21 | Toggle ON/OFF de la curva por índice                           | —    |
| Guardar curva en flash        | `5` | 22 | Pide ID y llama `saveCurveNVS()`                              | —    |
| Cargar curva de flash         | `6` | 23 | Pide ID y llama `loadCurveNVS()`                              | —    |
| Ver curvas → pines           | `7` | 26 | Imprime mapa pin→curva en onEnter                              | —    |
| Eliminar curva (RAM)          | `8` | 28 | Pide ID y llama `deleteCurve()`                               | —    |
| Eliminar curva (flash)        | `9` | 29 | Pide ID y llama `deleteCurveNVS()`                            | —    |
| Configuración del PID        | `7` | 27 | Sub-menú PID                                                   | 5     |
| Cambiar modo control          | `1` | 30 | Pide `index,PID\|NONE`y llama `setControlMode()`             | —    |
| Resetear PID                  | `2` | 31 | Pide índice y llama `resetPID()`                             | —    |
| Modificar parámetros PID     | `3` | 32 | Pide `index,Kp,Ki,Kd`y llama `setPIDParams()`               | —    |
| Habilitar feedforward         | `4` | 34 | Pide `index,0\|1`y llama `setFeedforwardEnabled()`           | —    |
| Variable a estabilizar        | `5` | 35 | Pide `index,v\|i\|p`y llama `setPIDMode()`                    | —    |
| Identificar canal             | `8` | 36 | Sub-menú identificación                                       | 1     |
| Iniciar identificación       | `1` | 38 | Pide `pin,numPuntos,tiempo_s`y llama `identificacionInit()` | —    |

---

## 5. Funciones clave del sistema de menús

### 5.1 `nodeRequiresInput(int id)`

Tabla estática que indica si un nodo espera que el usuario escriba datos. Retorna `true` o `false`. Es puro `switch/case`, sin lógica adicional.

Cuando retorna `true`, `userInterfaceUpdate()` acumula los siguientes caracteres en `data_buffer[]` hasta recibir Enter, y entonces llama a `procesarDatos()`.

**Caracteres aceptados en el buffer:** dígitos `0-9`, letras `a-z / A-Z`, coma `,`, punto `.`, guion `-`, y espacios (para formatos como `DD/MM/AAAA HH:MM`).

### 5.2 `onEnterNode(MenuNode* n)`

Se llama una sola vez cada vez que el cursor de menú llega a un nodo nuevo. Realiza dos tareas en orden:

1. **Acciones inmediatas** (switch sobre `n->id`): muestra valores actuales, imprime listas, dispara conexiones, etc. Esto ocurre incluso si el nodo NO pide datos.
2. Si `nodeRequiresInput(n->id)` es `true`: activa `aceptandoDatos = true` y muestra el prompt explicando el formato esperado.

Ejemplo para el nodo ID 32 (modificar parámetros PID):

```cpp
// Bloque de acciones previas:
case 32:
    for(int i=0; i<NUMBER_OF_SENSORS; i++){
        getPIDParams(i, &kp, &ki, &kd);
        writeSerialComln("Kp: " + String(kp,3) + ...);
    }
    break;

// Bloque de prompts:
case 32: writeSerialComln("Ingrese index,Kp,Ki,Kd y presione ENTER"); break;
```

### 5.3 `onUpdateNode(MenuNode* n)`

Se llama en cada iteración de `userInterfaceUpdate()`. Sirve para refrescar información en tiempo real mientras el usuario permanece en un nodo.

Solo dos nodos tienen comportamiento activo actualmente:

* **ID 1** (Entradas analógicas): llama `printSensorData()` en cada iteración para mostrar valores ADC actualizados.
* **ID 24** (Ver fecha): limpia la pantalla y muestra `getFormattedDateTime()` constantemente.

Para la mayoría de los nodos, `onUpdateNode` no hace nada (rama `default` del switch).

### 5.4 `procesarDatos(String data)`

Se llama cuando el usuario presiona Enter y `aceptandoDatos` es `true`. Recibe el contenido del buffer como `String`. La función es un bloque de `if (menu->id == X)` que despacha la lógica según el nodo activo.

Patrón típico dentro de cada bloque:

```cpp
if (menu->id == X) {
    int a; float b;
    if (sscanf(data.c_str(), "%d,%f", &a, &b) == 2) {
        // validar rango
        // llamar función del módulo correspondiente
        writeSerialComln("OK: valor aplicado");
    } else {
        writeSerialComln("Error: formato invalido. Use <a>,<b>");
    }
}
```

---

## 6. Cómo agregar un nuevo nodo

Para agregar un nodo que pida un dato al usuario y ejecute una acción, seguir estos 5 pasos en orden:

### Paso 1 — Crear el nodo en `menuInit()` (menuTree.cpp)

```cpp
// Elegir un ID libre (ej: 99) y una key que no colisione en el nodo padre
MenuNode* child99 = create_node("Mi nueva funcion", '9', 99);
add_child(child13, child99);   // child13 = nodo padre deseado
```

### Paso 2 — Declarar que el nodo pide datos (`nodeRequiresInput`)

```cpp
static bool nodeRequiresInput(int id) {
    switch (id) {
        // ... casos existentes ...
        case 99:   // ← agregar aquí
            return true;
        default:
            return false;
    }
}
```

### Paso 3 — Mostrar valores actuales al entrar (`onEnterNode`, bloque de acciones)

```cpp
switch (n->id) {
    // ... casos existentes ...
    case 99:
        writeSerialComln("Valor actual: " + String(miGetterActual()));
        break;
}
```

### Paso 4 — Mostrar el prompt de ayuda (`onEnterNode`, bloque de prompts)

```cpp
// Dentro del if (nodeRequiresInput), al final de onEnterNode():
case 99: writeSerialComln("Ingrese <index>,<valor> y presione ENTER"); break;
```

### Paso 5 — Procesar el dato recibido (`procesarDatos`)

```cpp
if (menu->id == 99) {
    int index; float valor;
    if (sscanf(data.c_str(), "%d,%f", &index, &valor) != 2) {
        writeSerialComln("Error: formato invalido. Use <index>,<valor>");
        return;
    }
    if (valor < 0 || valor > 1000) {
        writeSerialComln("Error: valor fuera de rango (0..1000)");
        return;
    }
    miFuncion(index, valor);
    writeSerialComln("OK — valor aplicado: " + String(valor));
}
```

> **Nodo de solo visualización (sin input)**
> Si el nodo solo muestra información y no pide datos:
>
> * No agregar su ID a `nodeRequiresInput()`.
> * Poner la lógica de display en `onEnterNode()` para mostrar al entrar.
> * Si querés refresco continuo, agregar también un `case` en `onUpdateNode()`.
> * No es necesario tocar `procesarDatos()`.

---

## 7. Transferencia de curvas por chunks (APP_MODE)

Para curvas con muchos puntos, el protocolo las divide en bloques de máximo 10 puntos para evitar desbordar el buffer de la UART.

### 7.1 Formato de los paquetes

```
# Chunk inicial (siempre primero):
CURVE,<id>,<total>,<p1;p2;...pN>,<checksum_hex>

# Chunks de continuación (uno por cada grupo de 10 puntos adicionales):
CURVEC,<id>,<p1;p2;...pN>,<checksum_hex>

# Formato de cada punto dentro del chunk:
<tiempo_ms>,<valor_float>,<tipo>
  tipo: 0=STEP  1=LINEAR  2=S_CURVE

# Ejemplo (curva ID 2, 12 puntos en total, primer chunk con 10):
CURVE,2,12,0,0.0,0;500,1.5,1;1000,3.0,1;...,A3
CURVEC,2,1000,4.5,2;1500,5.0,0,B7
```

### 7.2 Respuestas del firmware

| Respuesta                   | Significado                                                                                                                          |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `ACK,<recibidos>,<total>` | Chunk procesado OK. Enviar el siguiente.                                                                                             |
| `OK,<id>`                 | Transferencia completa. Curva lista en RAM.                                                                                          |
| `ERROR,CHECKSUM`          | XOR del payload no coincide. El firmware re-envía ACK con el estado actual; reintentar el chunk.                                    |
| `ERROR,ABORT,<motivo>`    | Transferencia abortada. La curva parcial fue eliminada. Motivos:`TIMEOUT`,`NUEVA_CURVA`,`CANCELADO_POR_USUARIO`,`ADD_POINT`. |
| `ERROR,CREATE`            | No se pudo crear la curva con el ID solicitado.                                                                                      |
| `ERROR,FORMATO`           | Los puntos del chunk no tienen el formato esperado.                                                                                  |

### 7.3 Cálculo del checksum

XOR byte a byte de todos los caracteres del payload (todo el comando excepto la última coma y el checksum). El resultado se formatea como hexadecimal.

```python
# Ejemplo en Python (lado PC):
def checksum(payload: str) -> str:
    cs = 0
    for c in payload:
        cs ^= ord(c)
    return format(cs, '02X')

payload = 'CURVE,2,12,0,0.0,0;500,1.5,1'
cmd = payload + ',' + checksum(payload)   # → 'CURVE,2,12,...,A3'
```

### 7.4 Timeout

Si pasan más de 10 segundos sin recibir el siguiente chunk, el timer FreeRTOS `curveTimeoutTimer` expira, se llama a `abortarTransferenciaCurva("TIMEOUT")`, la curva parcial se elimina y se notifica `ERROR,ABORT,TIMEOUT`. El timer se resetea con cada chunk exitoso.

### 7.5 Flujo completo de transferencia

```
PC                             FIRMWARE
 │                                │
 │── '@' ────────────────────────>│  activa APP_MODE
 │<─── 'APP_MODE_ON' ─────────────│
 │                                │
 │── CURVE,2,12,<pts1>,CS ───────>│  crea curva, agrega pts 1-10
 │<─── ACK,10,12 ─────────────────│  timer iniciado
 │                                │
 │── CURVEC,2,<pts2>,CS ─────────>│  agrega pts 11-12
 │<─── ACK,12,12 ─────────────────│  timer reseteado
 │<─── OK,2 ──────────────────────│  curva completa en RAM
 │                                │
 │── '#' ────────────────────────>│  desactiva APP_MODE
 │<─── 'APP_MODE_OFF' ────────────│
```

---

## 8. Helpers de parseo

| Función                                           | Descripción                                                                                                 |
| -------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `parseStringToInts(str, &n1, &n2)`               | Parsea `"n1,n2"`→ dos `int`. Retorna `true`si OK.                                                     |
| `parseStringToFloats(str, &idx, &kp, &ki, &kd)`  | Parsea `"idx,f1,f2,f3"`→ un `int`y tres `float`.                                                      |
| `parseStringToPoint(str, &curve, &t, &v, &type)` | Parsea `"curve,tiempo,valor,tipo"`para agregar un punto a una curva.`tipo`: 0=STEP, 1=LINEAR, 2=S_CURVE. |

---

## 9. Referencia rápida — teclas de navegación

| Tecla                       | Acción                                               |
| --------------------------- | ----------------------------------------------------- |
| Letra/dígito de la opción | Navega al nodo hijo correspondiente                   |
| `ESC`o `<`              | Sube al nodo padre (GO_BACK)                          |
| `Enter`(`\n`)           | Confirma el dato ingresado en el buffer               |
| `@`                       | Activa APP_MODE (interfaz para aplicaciones externas) |
| `#`                       | Desactiva APP_MODE y vuelve al menú                  |
