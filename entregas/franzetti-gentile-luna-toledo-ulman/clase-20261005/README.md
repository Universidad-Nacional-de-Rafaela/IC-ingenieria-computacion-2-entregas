# Grupo 4 - Nodo publicador (potenciómetro + botón)

Programa para una **ESP32 WROOM** que lee un potenciómetro y un botón y envía sus valores por una red **painlessMesh** al gateway del Grupo 1. El gateway los publica en MQTT (Mosquitto, Grupo 2) para que el Grupo 5 controle el ventilador y la luz.

Recorrido de los datos:

- Grupo 4 (este nodo) → painlessMesh → Grupo 1 (gateway)
- Grupo 1 → WiFi / MQTT → Grupo 2 (Mosquitto)
- Grupo 2 → WiFi / MQTT → Grupo 3 (gateway)
- Grupo 3 → painlessMesh → Grupo 5 (ventilador / LED)

Archivo principal: `nodo_mesh.ino`

---

## Qué hace el código

### Potenciómetro (potencia del ventilador)

- Topic MQTT: `casa/ventilador/`
- Valor enviado: número de 0 a 100 (porcentaje).
- Se lee cada 100 ms. Cada lectura es el promedio de 10 muestras del ADC para reducir el ruido.
- El valor del ADC (0 a 4095) se convierte a porcentaje.
- Solo se envía si el valor cambió 2 puntos o más desde el último envío.

### Botón (luz prendida / apagada)

- Topic MQTT: `casa/luz/`
- Valor enviado: `1` (prendida) o `0` (apagada).
- Se lee cada 20 ms, con antirrebote de 50 ms.
- Actúa solo al **apretar** (no al soltar): cada pulsación invierte el estado de la luz y envía el nuevo estado.
- El LED integrado de la placa (GPIO2) refleja el estado. Sirve para verificar el funcionamiento sin el resto del sistema.

### Reenvío periódico

- Cada 5 segundos se reenvían los dos valores actuales, por si algún mensaje se perdió o el gateway se reinició.

### Envío

- Los mensajes se mandan por **broadcast** a toda la mesh, así que no hace falta conocer el ID del gateway.

---

## Conexionado

### Potenciómetro

- Extremo 1 → **3V3**
- Pata central → **GPIO34**
- Extremo 2 → **GND**

### Botón

- Terminal 1 → **GPIO25**
- Terminal 2 → **GND**

### LED

- Se usa el LED integrado de la placa, en **GPIO2**. No requiere conexión.

### Notas

- El botón usa la resistencia pull-up interna (`INPUT_PULLUP`): **no necesita resistencia externa**.
- Si el botón es de 4 patas, usar dos patas **en diagonal**. Las patas del mismo lado están unidas internamente.
- El potenciómetro va a **GPIO34** (ADC1). Los pines ADC2 no funcionan con el WiFi activo, y la mesh lo usa.
- No conectar el potenciómetro a 5V: la ESP32 trabaja a 3,3V.

---

## Librerías y entorno

- **IDE**: Arduino IDE 2.x
- **Placa**: ESP32 Dev Module
- **Core de ESP32 (Espressif)**: versión **2.0.17**. Con el core 3.x el programa se reinicia en bucle con `assert failed: tcp_alloc`.
- **painlessMesh**: se instala desde el Gestor de librerías.
- **ArduinoJson**: dependencia de painlessMesh. Si da errores de compilación, usar la versión 6.x.
- **TaskScheduler**: dependencia de painlessMesh.
- **AsyncTCP**: dependencia de painlessMesh. Si el programa se reinicia en bucle, probar una versión 1.1.x.

URL para agregar el soporte de ESP32 (Preferencias → URLs adicionales de gestor de placas):

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

---

## Parámetros de la mesh

Tienen que ser **idénticos** a los del gateway del Grupo 1:

- `MESH_PREFIX`: `GRUPO1_MESH`
- `MESH_PASSWORD`: se la pasa el Grupo 1. **No se sube al repositorio**: va en `nodo_mesh/secrets.h` (ver `secrets.h.example`).
- `MESH_PORT`: `5555`
- `MESH_CANAL`: `1`

El nodo usa `mesh.setContainsRoot(true)`, porque en la mesh hay un gateway (root) conectado al router. Si el gateway no usa `setRoot(true)`, comentar esa línea.

---

## Formato de los mensajes

Cada dato se manda como un **mensaje JSON de texto** con dos campos:

- `t`: topic MQTT al que debe publicar el gateway. Ejemplo: `"casa/ventilador/"`.
- `v`: valor, un número entero. Ejemplo: `75`.

Ejemplos de mensajes:

```
{"t":"casa/ventilador/","v":75}
{"t":"casa/luz/","v":1}
```

La mesh no sabe nada de topics: el topic viaja **dentro del mensaje**, y el gateway lo lee y lo usa al publicar en Mosquitto (`10.108.142.122:1883`).

El mensaje se arma en la función `publicar(topic, valor)`:

```cpp
snprintf(msg, sizeof(msg), "{\"t\":\"%s\",\"v\":%d}", topic, valor);
mesh.sendBroadcast(msg);
```

En MQTT, `casa/luz/` y `casa/luz` son topics **distintos**. Se usa la barra final, tal como los definió el Grupo 2.

---

## Cómo cargarlo

1. Instalar el core **esp32 2.0.17** y las librerías indicadas arriba.
2. En la carpeta `nodo_mesh`, copiar `secrets.h.example` como `secrets.h` y poner la contraseña real de la mesh.
3. Abrir `nodo_mesh.ino` (la carpeta tiene que llamarse `nodo_mesh`).
4. Elegir **ESP32 Dev Module** y el puerto COM.
5. Subir el sketch. Si se queda en "Connecting...", mantener apretado el botón **BOOT** de la placa.
6. Abrir el Serial Monitor a **115200 baudios**.

---

## Salida por Serial

- `Nodo listo. ID de este nodo: ...`: el programa arrancó. Ese número es el Node ID en la mesh.
- `Nodos conectados en la mesh: N`: si `N` es 1 o más, el nodo se unió a la mesh.
- `TX {...} (ok)`: el mensaje salió hacia la mesh.
- `TX {...} (error)`: no hay nodos conectados, la mesh no se formó.
- `RX de <id>: ...`: se recibió un mensaje de otro nodo.

---

## Problemas frecuentes

**Se reinicia en bucle con `assert failed: tcp_alloc`**
Core de ESP32 3.x. Bajar a la versión 2.0.17.

**`TX ... (error)` y `Nodos conectados: 0`**
Prefix, contraseña, puerto o canal distintos a los del gateway, o gateway apagado.

**Caracteres raros en el monitor**
La velocidad no es 115200.

**El potenciómetro salta o no llega a 100**
Mal contacto. Revisar 3V3, GND y la pata central en GPIO34.

**El botón siempre figura apretado**
Falta conectar el otro terminal a GND.

**El botón cambia varias veces con una sola pulsación**
Rebote del botón. Ajustar `TIEMPO_REBOTE` en el código.

---

## Pruebas por partes

- **Solo hardware**: `test_pot_boton.ino` imprime por Serial el potenciómetro y el botón, sin mesh.
- **Mesh y MQTT**: desde una PC en la misma red, `mosquitto_sub -h 10.108.142.122 -t "casa/#" -v` muestra lo que llega al broker.
