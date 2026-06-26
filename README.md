Este proyecto consiste en un sistema de adquicion de datos.

Para ejecutar pruebas :

    pio test -e esp32doit-devkit-v1

caso de uso:modo manual

-Se necesita que el sistema este en modo manual (curve mode =OFF menu 6-4)

-Poner un valor entre 0 y 1000 en el menu 3-1

Nota importante:

    Cada carga electronica tiene una relimentacion local que obtine los valores de los ina219. Esto significa que si el PID no tiene un valor integral igualmente tendra error estacionario 0. Siempre y cuando el PID para esa salida este activo.






# Identificación de Planta

## ¿Para qué sirve?

La identificación de planta genera automáticamente una tabla de correspondencia entre el **duty cycle aplicado** y la **corriente resultante** medida en una carga electrónica. Con esta tabla es posible conocer el comportamiento real del canal sin necesidad de calibrarlo manualmente.

El proceso aplica una secuencia de duty cycles crecientes, mide la corriente en cada paso y guarda los resultados en memoria no volátil (NVS).

---

## Menú

### `Identificar canal` → `Ver identificacion de los canales` (opción 1)

Muestra por consola la última identificación guardada de un canal.

**Parámetro:** `<pin>`

* `pin`: número del canal a consultar (0–4)

**Ejemplo:** `1` → muestra la identificación guardada del canal 1.

---

### `Identificar canal` → `Identificar canal` (opción 2)

Inicia el proceso de identificación en un canal. El canal se pone automáticamente en modo manual (PID apagado, feedforward apagado) y comienza la secuencia de barrido.

**Parámetros:** `<pin>,<numPuntos>,<tiempo_por_punto_s>`

| Parámetro             | Descripción                                        | Rango típico |
| ---------------------- | --------------------------------------------------- | ------------- |
| `pin`                | Canal a identificar                                 | 0–4          |
| `numPuntos`          | Cantidad de escalones de duty cycle entre 10% y 95% | ≥ 2          |
| `tiempo_por_punto_s` | Segundos que dura cada escalón                     | ≥ 1          |

**Ejemplo:** `1,10,3` → identifica el canal 1 con 10 puntos y 3 segundos por punto.

---

## Secuencia de ejecución

1. **Standby (10 segundos):** el canal se pone en duty=0 antes de comenzar. Este período no se mide.
2. **Barrido:** se aplican `numPuntos` escalones de duty cycle entre 10% y 95% (incluyendo ambos extremos), cada uno durante `tiempo_por_punto_s` segundos.
3. **Guardado:** al terminar, los resultados se guardan automáticamente en NVS y se imprimen por consola.

---

## Ejemplo de salida por consola

```
==============================
IDENTIFICACION CANAL 1
Timestamp: 1718000000
Puntos: 5
Muestras/punto: 30
duty(%) -> corriente(mA)
------------------------------
  10.0% ->  45.20 mA
  31.2% -> 312.50 mA
  52.5% -> 598.10 mA
  73.7% -> 891.30 mA
  95.0% -> 1150.00 mA
==============================
```

---

## Diagrama duty vs tiempo

![Diagrama duty vs tiempo](https://claude.ai/chat/duty_vs_tiempo.png)

> *Ejemplo con 5 puntos y 3 segundos por punto. La zona gris representa el standby inicial (10s) donde no se toman mediciones.*
