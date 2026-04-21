Este proyecto consiste en un sistema de adquicion de datos.


Para ejecutar pruebas :

    pio test -e esp32doit-devkit-v1

caso de uso:modo manual

-Se necesita que el sistema este en modo manual (curve mode =OFF menu 6-4)

-Poner un valor entre 0 y 1000 en el menu 3-1

Nota importante:

    Cada carga electronica tiene una relimentacion local que obtine los valores de los ina219. Esto significa que si el PID no tiene un valor integral igualmente tendra error estacionario 0. Siempre y cuando el PID para esa salida este activo.
