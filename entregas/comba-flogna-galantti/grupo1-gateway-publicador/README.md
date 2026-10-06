Grupo 1 – Gateway ESP32
El Grupo 1 se encargó de desarrollar la gateway encargada de conectar un ESP32 a una red WiFi y permitir la comunicación con un broker MQTT.
Primero se configuró el ESP32 para conectarse a la red WiFi UNRaf_Libre. Durante las pruebas se verificó correctamente la conexión y se obtuvieron los siguientes datos del dispositivo:
IP asignada al ESP32: 10.103.11.158
MAC del ESP32: 6C:C8:40:34:46:BC
Canal WiFi utilizado: 1
Luego se configuró la conexión MQTT utilizando el broker Mosquitto ubicado en la dirección 10.103.11.11 y el puerto 1883.
Se comprobó correctamente la comunicación entre el ESP32 y Mosquitto. Para realizar esta prueba inicialmente se enviaron valores simulados al broker. Una vez verificado el funcionamiento, estos valores fueron eliminados para que la gateway no publique información ficticia.
La gateway quedó configurada para mantenerse conectada al broker MQTT y posteriormente recibir información mediante una red Mesh.
Para la red Mesh se definieron los siguientes parámetros:
MESH_PREFIX: GRUPO1_MESH
MESH_PASSWORD: grupo12026
MESH_PORT: 5555
MESH_CANAL: 1
El canal 1 fue seleccionado porque es el mismo canal utilizado por la red WiFi UNRaf_Libre. Esto es necesario debido a que el ESP32 utiliza la misma interfaz WiFi para trabajar con la red Mesh y para conectarse a la red externa.
Para trabajar con Mesh se utilizarán las librerías painlessMesh, ArduinoJson, TaskScheduler y AsyncTCP. Para la comunicación MQTT se utiliza PubSubClient. El proyecto se configuró utilizando ESP32 versión 2.0.17 y la placa ESP32 Dev Module.
La gateway no tiene definidos de forma fija los topics MQTT. El objetivo es que el topic llegue dentro del propio mensaje recibido por Mesh.
El formato acordado para los mensajes es:
{"t":"casa/ventilador/","v":75}
o, por ejemplo:
{"t":"casa/luz/","v":1}
El campo "t" contiene el topic MQTT y el campo "v" contiene el valor que debe publicarse.
Cuando la gateway reciba un mensaje de este tipo, deberá interpretar el JSON, extraer el topic y el valor y publicar ese valor en Mosquitto utilizando el topic recibido.
Por ejemplo, si la gateway recibe:
{"t":"casa/ventilador/","v":75}
interpretará:
topic = casa/ventilador/
valor = 75
y publicará el valor 75 en ese topic MQTT.
De esta manera la gateway queda preparada para trabajar con distintos topics sin necesidad de modificarlos manualmente dentro del código.
Hasta el momento se verificó correctamente la conexión del ESP32 a la red WiFi, la identificación del canal WiFi y la conexión del ESP32 al broker MQTT. También quedaron definidos los parámetros que se utilizarán para integrar la comunicación Mesh.