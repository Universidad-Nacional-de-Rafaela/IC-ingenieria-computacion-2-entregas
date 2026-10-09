# Uso y Configuración de Eclipse Mosquitto (MQTT)

Instalación, configuración, apertura de puertos y pruebas de publicación/suscripción con **Eclipse Mosquitto** en sistemas **Linux** y **Windows**, utilizando entornos nativos y entornos en contenedores.

---

## 1. Conceptos Clave y Ejemplos de Tópicos

* **Broker (Servidor):** Centraliza y distribuye los mensajes entre los clientes.
* **Suscriptor (Subscriber):** Escucha uno o varios tópicos en espera de datos.
* **Publicador (Publisher):** Envía un mensaje a un tópico específico.
* **Tópicos de ejemplo utilizados:**
  * `casa/luz/living`
  * `casa/luz/cocina`
  * `casa/ventilador/dormitorio`
  * `casa/ventilador/comedor`

> **Nota importante sobre Mosquitto 2.0+:**  
> Por defecto, las versiones 2.0 o superiores se ejecutan en *modo local restringido* (*Starting in local only mode*). Para permitir la conexión de clientes externos y de la red local, **siempre se debe especificar un archivo de configuración (`mosquitto.conf`)** con las siguientes directivas básicas:
>
> ```ini
> listener 1883
> allow_anonymous true
> ```

---

## 2. Instalación y Uso SIN Docker

### A. En Linux (Ubuntu / Debian / Raspberry Pi OS)

#### 1. Instalación

Abre una terminal y ejecuta:

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

#### 2. Configurar el archivo `mosquitto.conf`

Crea o edita la configuración para habilitar el puerto 1883:

```bash
sudo nano /etc/mosquitto/conf.d/default.conf
```

Agrega estas dos líneas:

```ini
listener 1883
allow_anonymous true
```

Guarda con `Ctrl + O` y sale con `Ctrl + X`.

#### 3. Habilitar el Puerto en el Firewall (Linux - UFW)

Si tienes el firewall de Ubuntu activado (`ufw`), permite el tráfico en el puerto 1883:

```bash
sudo ufw allow 1883/tcp
sudo ufw reload
```

#### 4. Levantar / Reiniciar el Servicio

```bash
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto
```

Verifica que esté corriendo con:

```bash
sudo systemctl status mosquitto
```

#### 5. Probar Suscripción y Publicación

* **Terminal 1 (Suscribirse a todas las luces viendo el tópico exacto `-v`):**

  ```bash
  mosquitto_sub -h localhost -t "casa/luz/+" -v
  ```

* **Terminal 2 (Publicar un mensaje):**

  ```bash
  mosquitto_pub -h localhost -t "casa/luz/living" -m "ON"
  mosquitto_pub -h localhost -t "casa/luz/cocina" -m "OFF"
  ```

---

### B. En Windows (CMD / PowerShell)

#### 1. Instalación

Descarga el instalador oficial `.exe` desde [mosquitto.org/download](https://mosquitto.org/download/) o instálalo vía PowerShell/WinGet:

```powershell
winget install --id EclipseFoundation.Mosquitto -e
```

#### 2. Configurar el archivo `mosquitto.conf`

Abre CMD como Administrador y navega a la carpeta de instalación:

```cmd
cd "C:\Program Files\mosquitto"
```

Crea el archivo de configuración necesario:

```cmd
echo listener 1883 > mosquitto.conf && echo allow_anonymous true >> mosquitto.conf
```

#### 3. Habilitar el Puerto en el Firewall de Windows

Ejecuta el siguiente comando en **PowerShell como Administrador** para permitir el tráfico entrante en el puerto 1883:

```powershell
New-NetFirewallRule -DisplayName "Mosquitto MQTT (1883)" -Direction Inbound -LocalPort 1883 -Protocol TCP -Action Allow
```

#### 4. Levantar el Servidor

Puedes arrancarlo directamente por consola o como servicio de fondo:

* **Opción Consola (Modo detallado):**

  ```cmd
  mosquitto.exe -c mosquitto.conf -v
  ```

  *Deberías ver la línea: `Opening ipv4 listen socket on port 1883.`*

* **Opción Servicio de Windows:**

  ```powershell
  Start-Service mosquitto
  ```

#### 5. Probar Suscripción y Publicación

Abre dos ventanas adicionales de CMD o PowerShell dentro de `C:\Program Files\mosquitto`:

* **Ventana 1 (Suscriptor para ventiladores):**

  ```cmd
  mosquitto_sub.exe -h localhost -t "casa/ventilador/#" -v
  ```

* **Ventana 2 (Publicador):**

  ```cmd
  mosquitto_pub.exe -h localhost -t "casa/ventilador/dormitorio" -m "VELOCIDAD_3"
  ```

---

## 3. Instalación y Uso CON Docker

El uso de Docker simplifica la instalación y garantiza la portabilidad del entorno en cualquier sistema operativo.

### Preparación del archivo de configuración local

Antes de ejecutar el contenedor, crea un archivo llamado `mosquitto.conf` en tu directorio de trabajo actual con este contenido:

```ini
listener 1883
allow_anonymous true
```

---

### A. Uso directo con `docker run`

#### 1. Levantar el contenedor

Ejecuta el contenedor montando el archivo de configuración y mapeando el puerto `1883`:

* **Linux / macOS:**

  ```bash
  docker run -d \
    --name broker-mosquitto \
    -p 1883:1883 \
    -v $(pwd)/mosquitto.conf:/mosquitto/config/mosquitto.conf \
    eclipse-mosquitto:2
  ```

* **Windows (PowerShell):**

  ```powershell
  docker run -d `
    --name broker-mosquitto `
    -p 1883:1883 `
    -v ${PWD}/mosquitto.conf:/mosquitto/config/mosquitto.conf `
    eclipse-mosquitto:2
  ```

#### 2. Apertura de Puertos con Docker

Al publicar el puerto con `-p 1883:1883`, Docker gestiona automáticamente el redireccionamiento. Asegúrate únicamente de que el Firewall del Host (UFW en Linux o Windows Firewall) no bloquee las conexiones entrantes al puerto `1883`.

#### 3. Publicar y Suscribirse desde fuera o dentro del contenedor

* **Usando los clientes locales del Host (si están instalados):**

  ```bash
  mosquitto_sub -h localhost -t "casa/#" -v
  mosquitto_pub -h localhost -t "casa/luz/living" -m "OFF"
  ```

* **Usando `docker exec` (sin clientes instalados en el host):**

  * **Suscribirse:**

    ```bash
    docker exec -it broker-mosquitto mosquitto_sub -t "casa/#" -v
    ```
  * **Publicar:**

    ```bash
    docker exec -it broker-mosquitto mosquitto_pub -t "casa/ventilador/comedor" -m "ENCENDIDO"
    ```

---

### B. Uso con Docker Compose

Es la mejor opción para integrar el broker con otras aplicaciones (APIs, bases de datos, etc.).

#### 1. Estructura de archivos

```text
proyecto/
├── docker-compose.yml
└── mosquitto.conf
```

#### 2. Archivo `docker-compose.yml`

```yaml
version: '3.8'

services:
  mosquitto:
    image: eclipse-mosquitto:2
    container_name: broker-mqtt
    restart: always
    ports:
      - "1883:1883"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf
```

#### 3. Comandos principales

* **Levantar el servicio:**

  ```bash
  docker compose up -d
  ```
* **Ver los registros en tiempo real:**

  ```bash
  docker compose logs -f mosquitto
  ```
* **Detener el servicio:**

  ```bash
  docker compose down
  ```

---

## 4. Patrones de Suscripción y Comandos Útiles

| Comando / Sintaxis | Descripción                                                                                              | Ejemplo de Uso                                    |
|:------------------ |:-------------------------------------------------------------------------------------------------------- |:------------------------------------------------- |
| `casa/luz/living`  | Suscripción a un tópico exacto.                                                                          | `mosquitto_sub -t "casa/luz/living"`              |
| `casa/luz/+`       | Comodín **`+`**: Reemplaza **un único nivel** de la jerarquía (ej. living, cocina).                      | `mosquitto_sub -t "casa/luz/+"`                   |
| `casa/#`           | Comodín **`#`**: Reemplaza **múltiples niveles** desde ese punto hacia abajo.                            | `mosquitto_sub -t "casa/#"`                       |
| **`-v`**           | Muestra el **nombre del tópico** junto con el contenido del mensaje.                                     | `mosquitto_sub -t "casa/#" -v`                    |
| **`-r`**           | Mensaje **Retenido** (Retained): El broker guarda el último valor para entregarlo a nuevos suscriptores. | `mosquitto_pub -t "casa/luz/living" -m "ON" -r`   |
| **`-q 1`**         | **Quality of Service (QoS):** Define el nivel de garantía de entrega (0, 1 o 2).                         | `mosquitto_pub -t "casa/luz/living" -m "ON" -q 1` |