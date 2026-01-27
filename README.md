Este proyecto consiste en un sistema de adquicion de datos.

Los datos obtenidos por la placa de desarrollo ESP32 DEVKITV1 son enviados por WiFi a una base de datos InfluxDB Cloud.
Luego utilizando Grafana se pueden visualizar los datos en tiempo real.

Para ejecutar pruebas :

    pio test -e esp32doit-devkit-v1




caso de uso:modo manual

-Se necesita que el sistema este en modo manual (curve mode =OFF menu 6-4)

-Poner un valor entre 0 y 1000 en el menu 3-1
