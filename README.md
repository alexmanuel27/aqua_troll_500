# Aqua TROLL 500 con Arduino Nano clásico

Lector SDI-12 para **Nano de 5 V con ATmega328P**. Lee todos los canales
configurados, comprueba CRC, muestra los valores por USB y permite guardarlos
en el ordenador. No requiere una licencia del software de In-Situ para esta lectura.

**Estado:** comunicación comprobada con la sonda `AT500V250`, serie `1364600`,
y rondas de 20 canales recibidas en aire y agua del grifo. La nueva tabla está
compilada para Nano y comprobada con una simulación del sketch usando tus datos;
queda probar su presentación al cargarla en la placa. El mapa de parámetros
sigue siendo provisional y esta prueba no certifica calibración.

## 1. Reúne el material

- Arduino Nano clásico y cable USB **de datos**.
- Aqua TROLL 500 con los sensores instalados y sus puertos cerrados correctamente.
- Cable compatible RuggedCable/Twist-Lock con terminación de **hilos libres**
  (*flying leads / stripped-and-tinned*), o su adaptador correspondiente.
- Fuente de **12 V DC regulada**; sugiero 1 A de capacidad para tener margen.
- Resistencia de 1 kΩ en serie con la señal, bornes y cables para la prueba de mesa.
- Ordenador con Arduino IDE 2. Para guardar archivos automáticamente: Python 3.

Si tienes solamente un cable terminado en otro conector Twist-Lock, necesitas
el adaptador de hilos libres. No adivines el pinout ni cortes el cable de la sonda.
Un Wireless TROLL Com es otro accesorio: no conectes sus salidas USB a D7.

La sonda necesita alimentación externa: **el USB del Nano no la alimenta**.
El manual especifica 8–36 V DC; aquí usamos 12 V. [Manual de In-Situ,
alimentación y diagramas](https://documents.thermofisher.com/TFS-Assets/ANZ/manuals/aqua-troll-500-manual.pdf).

## 2. Conecta con todo apagado

Para el cable oficial de hilos libres de la Aqua TROLL 500:

| Hilo de la sonda | Función | Conexión en este montaje |
| --- | --- | --- |
| Rojo | Alimentación externa | Positivo de la fuente de 12 V |
| Negro | Masa / retorno | Negativo de la fuente **y GND del Nano** |
| Blanco | SDI-12 | Resistencia de 1 kΩ y después **D7** del Nano |
| Azul | RS485 (+) | Masa común, como indica el diagrama SDI-12 del fabricante |
| Verde | RS485 (−) | Masa común, como indica el diagrama SDI-12 del fabricante |
| Marrón, si existe | No utilizado en este cable | Sin conectar; aislar |

Confirma que tu cable coincide con esta leyenda. La leyenda y el diagrama SDI-12
están en las páginas 39–40 de su [manual actual de la Aqua TROLL 500](https://media.in-situ.com/FDUAXN8W/as/qtmfswf6x5shvn86rbxb2t74/Aqua_TROLL_500_Manual).
No uses el esquema de una **Level TROLL 500**, que es otro instrumento.

```text
Fuente 12 V (+) ───────────────────── rojo de la sonda
Fuente 12 V (−) ───────────┬───────── negro de la sonda
                          ├───────── azul y verde de la sonda
                          └───────── GND del Nano
Nano D7 ─────── [1 kΩ] ────────────── blanco de la sonda
Ordenador USB ────────────────────── USB del Nano
```

**Nunca conectes 12 V a D7 ni al pin 5V.** En este montaje, el Nano se alimenta
por su USB y la sonda por su fuente. La resistencia es una precaución para la
prueba de mesa, no un convertidor de tensión. No añadas MAX485 ni resistencias
de terminación de 120 Ω a esta línea SDI-12.
Este cableado es exclusivamente SDI-12: no conectes simultáneamente un maestro
RS485 a los hilos azul y verde puestos a masa.

Empieza con conexiones cortas, secas y firmes. Para un cable largo o instalación
exterior, hará falta revisar protección eléctrica, tierra y el interfaz físico.

## 3. Carga el programa

1. Instala [Arduino IDE](https://www.arduino.cc/en/software/).
2. Conecta únicamente el Nano al ordenador mientras cargas el programa.
3. En el gestor de placas, instala **Arduino AVR Boards**.
4. En el gestor de bibliotecas, instala **SDI-12**, de Kevin M. Smith, Shannon Hicks
   y Sara Damiano / EnviroDIY. Se ha verificado con **2.3.4**.
5. Abre `AquaTroll500/AquaTroll500.ino`. Mantén `protocol.h` en la misma carpeta.
6. Selecciona **Arduino Nano** y su puerto USB.
7. Selecciona procesador **ATmega328P**. Si falla la carga y es un clon antiguo,
   prueba **ATmega328P (Old Bootloader)**. El programa sirve para ambos.
8. Pulsa **Subir**. No necesitas modificar la biblioteca.
9. Apaga, realiza el cableado del paso 2, enciende la fuente y reconecta el USB.
10. Abre el monitor serie a **115200 baudios**. Puedes seleccionar nueva línea.

La [biblioteca EnviroDIY](https://github.com/EnviroDIY/Arduino-SDI-12) implementa
la comunicación SDI-12 y deja libre el puerto USB del Nano. Los 115200 baudios
corresponden al ordenador; la comunicación con la sonda es distinta y la maneja
la biblioteca.

## 4. Comprueba que responde

Después del arranque verás una línea `# ID ...` con la identificación real.
No tiene por qué coincidir exactamente con un ejemplo: depende del firmware.
El lector consulta la dirección con `?!`; no presupone que siga siendo `0`.
Conecta **una sola sonda** a esta línea durante la prueba.

Escribe cada comando y pulsa Enviar:

| Comando | Acción |
| --- | --- |
| `i` | Buscar la sonda y mostrar identificación; detiene la repetición |
| `v` | Consultar los estados del dispositivo y conexiones |
| `m` | Medir una ronda completa y mostrar una tabla debajo de `# SCAN_END` |
| `r` | Medir con tabla y repetir, dejando 60 s entre rondas |
| `s` | Detener la repetición al terminar la ronda en curso |
| `a` | Recrear la lista SDI-12 a partir de los sensores instalados; queda en pausa |

Primero envía `v` y después `m`. El programa espera el tiempo que anuncie la sonda
para cada grupo; varios grupos tardan más. No desconectes al ver una pausa.

Si no hay canales, o necesitas incluir los sensores que acabas de instalar,
envía **`a` una sola vez** y espera `# CONFIGURED channels=...`. Después envía
`m`. Este comando **reemplaza la lista y el orden de salida SDI-12 existentes**;
anota cualquier correspondencia previa antes de usarlo. No es una calibración.
No lo uses repetidamente en cada arranque.

La especificación documenta `XAC` para incluir parámetros por orden de puertos,
con un máximo de 30. El lector consulta `MC`, `MC1`, etc. para superar el límite
de nueve valores del primer grupo, y solicita los bloques `D0`, `D1`, etc. con
verificación CRC. En la respuesta observada de `XAC`, el resultado de `D0`
también lleva CRC, que el lector verifica. [Especificación de In-Situ, secciones
SDI-12 y Auto Configure](https://assets.thermofisher.com/TFS-Assets/ANZ/Handbooks/aqua-troll-500-interface-specification.pdf).

## 5. Entiende los datos antes de usarlos

Ejemplo **inventado únicamente para explicar el formato**:

```text
uptime_ms,address,group,channel,value,status
12000,0,0,1,+21.12345,RECEIVED
12000,0,0,2,-99999,MISSING_SENSOR
```

- `uptime_ms`: contador del Nano al iniciar la petición de ese grupo. No es una
  fecha ni un reloj preciso; se reinicia con el Nano y desborda tras unos 49 días.
- `address`: dirección SDI-12 detectada.
- `group`: 0 para `MC`, 1 para `MC1`, etc.
- `channel`: posición de salida, contando nueve posiciones por grupo.
- `value`: texto numérico original enviado por la sonda, sin convertir unidades.
- `RECEIVED`: respuesta completa con CRC correcto. **No certifica calibración
  ni calidad metrológica**.
- `MISSING_SENSOR`: la sonda devolvió su centinela −99999. Trátalo como dato
  ausente, nunca como una concentración u otra medida real.

Después de `# SCAN_END`, tanto `m` como `r` muestran una tabla con los valores
originales de esa ronda, sin redondeo ni conversión de unidades. La tabla está en
inglés y solo incluye los canales identificados (1–16). Todas sus líneas
empiezan por `#`. El registrador copia sus filas al CSV con los mismos nombres,
valores y unidades, y conserva toda la salida en el `.log`. Ejemplo de algunas
filas con tu tercera medición:

```text
# Ch | Parameter                      | Value      | Unit
#  2 | Actual conductivity            | +707.20    | uS/cm
#  8 | pH                             | +8.00      | pH
# 11 | Dissolved oxygen               | +9.32      | mg/L
```

**Perfil provisional de esta instalación:** puerto 1 conductividad/temperatura,
puerto 2 pH/ORP, puerto 3 vacío con tapón y puerto 4 RDO. El mapa se deduce del
orden por puertos, los sensores y su respuesta en aire y agua; no se ha leído la
configuración de nombres/unidades mediante Modbus o el software del fabricante.
Las unidades de la tabla son las predeterminadas documentadas, no unidades
consultadas a la sonda. La relación entre conductividad y resistividad respalda
los canales 2 y 5: `1 000 000 / 707.20 ≈ 1414.0 ohm·cm`.

| Canal | Etiqueta provisional | Unidad supuesta |
| --- | --- | --- |
| 1 | Temperatura del sensor de conductividad | °C |
| 2 | Conductividad actual | µS/cm |
| 3 | Conductividad específica | µS/cm |
| 4 | Salinidad | PSU |
| 5 | Resistividad | ohm·cm |
| 6 | Densidad del agua | g/cm³ |
| 7 | Sólidos disueltos totales (TDS) | ppt |
| 8 | pH | pH |
| 9 | Potencial del electrodo de pH | mV |
| 10 | ORP | mV |
| 11 | Oxígeno disuelto | mg/L |
| 12 | Saturación de oxígeno | % |
| 13 | Presión parcial de oxígeno | torr |
| 14 | Temperatura del RDO | °C |
| 15 | Voltaje de alimentación | V |
| 16 | Presión barométrica | mmHg |

En el monitor se usa ASCII: `degC` = °C, `uS/cm` = µS/cm, `ohm*cm` = ohm·cm
y `g/cm3` = g/cm³. Un centinela `−99999` aparece como `NO_SENSOR` en la tabla;
se conserva el valor original y `MISSING_SENSOR` en el `.log`. El valor positivo
`+99999` se conserva tal cual: no es el centinela de ausencia documentado.

Solo se muestra la tabla si la ronda termina correctamente y contiene los
20 canales esperados; con otra cantidad aparece `TABLE_SKIPPED`. Un fallo de
lectura conserva las filas crudas anteriores, pero no genera una tabla de ronda
completa. Cada ronda limpia el resumen anterior. Tener 20 canales no demuestra
por sí solo que el mapa siga siendo el mismo: si cambias sensores, su posición,
las unidades o la lista SDI-12, revisa las etiquetas en `printSummary()`.

Para investigación, verifica canal → parámetro → unidad con VuSitu/Win-Situ o
mediante Modbus y la configuración real. Los canales 17–20 siguen en las filas
crudas del `.log`, pero se omiten de la tabla y del CSV hasta identificarlos.

La 500 admite cuatro sensores intercambiables, además de opciones internas según
el modelo; no todos los tipos comercializados caben simultáneamente. Un sensor
puede producir varios parámetros. Si cambias sensores o ejecutas `a`, vuelve a
verificar la correspondencia.

`v` devuelve los campos crudos `device_low`, `device_high` y `sensor_connections`.
Son campos de bits para diagnóstico, no valores de calidad del agua: una conexión
puede corresponder a un tapón, y un estado distinto de cero no equivale siempre
a un fallo. Conserva su salida para interpretarla con la especificación del firmware.

Las líneas que empiezan por `#` son diagnósticos. Si una respuesta falta, está
incompleta o falla el CRC, no se emite ese grupo como si fuera una medida válida.
Los grupos anteriores de la ronda pueden haberse recibido: busca `SCAN_FAILED`
para reconocer una ronda parcial. El lector no presenta datos de una ronda vieja
como si acabaran de medirse.

## 6. Guarda automáticamente en CSV

Cierra el monitor serie y cualquier otro programa que use ese puerto.
En una terminal dentro de esta carpeta, ejecuta:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install pyserial==3.5
python registrar.py --list
```

En Windows, usa `py -m venv .venv` y `.venv\Scripts\activate` para los dos
primeros pasos. Los comandos siguientes funcionan igual.

Usa el puerto USB que aparezca en tu ordenador. Para iniciar una ronda cada
**10 minutos** (600 segundos), por ejemplo:

```sh
python registrar.py --port /dev/cu.usbserial-XXXX --interval 600 --output experimento_01.csv
```

En Windows, el puerto suele tener un nombre como `COM5`:

```sh
python registrar.py --port COM5 --interval 600 --output experimento_01.csv
```

Abrir el puerto suele reiniciar el Nano. El registrador espera unos 15–20 s,
envía `s` y espera su confirmación para cancelar una repetición anterior. Consulta
el estado y envía `m` para cada ronda. La primera empieza al terminar esa
preparación; las siguientes se solicitan cada 600 segundos, aproximadamente de
inicio a inicio. El intervalo predeterminado también es 600 s. Cambia
`--interval` para elegir otro, sin volver a cargar el Nano. No se solapan rondas:
si una tarda más que el intervalo, la siguiente empieza cuando termina.

Este intervalo lo controla el ordenador; el comando manual `r` del Nano mantiene
su pausa de 60 s tras cada ronda. **No se ejecuta `a`** ni se cambian sensores,
calibraciones o unidades. Verás las lecturas y las tablas en pantalla.

Se guardan dos archivos, sin sobrescribir experimentos anteriores:

- `experimento_01.csv`: la tabla legible en inglés, con columnas `Ch`, `Parameter`,
  `Value`, `Unit` y `Date/time (Europe/Brussels)`. Son 16 filas por ronda, una por
  parámetro identificado, conservando signos y decimales. `NO_SENSOR` significa
  dato ausente. La fecha y hora local corresponden a la recepción de `SCAN_BEGIN`;
  no implica que todos los parámetros se midieran simultáneamente.
- `experimento_01.csv.log`: toda la salida original con fecha y hora UTC de
  recepción de cada línea: los 20 canales crudos, tablas, identificación y errores.

Las filas del CSV se copian de la tabla que ya emite el Nano; no se mantiene
otro mapa de nombres y unidades en Python. Si no aparece tabla, por ejemplo por
`SCAN_FAILED` o `TABLE_SKIPPED`, las lecturas recibidas se conservan en el log y
no se presentan como una ronda completa en el CSV. El formato antiguo de CSV
con `uptime_ms,address,group,channel,value,status` ya no se utiliza para archivos
nuevos. Si el registrador estaba abierto al actualizarlo, ciérralo con Ctrl+C
y vuelve a arrancarlo para aplicar el formato nuevo.

Si omites `--output`, se genera un nombre con fecha y hora automáticamente.
Termina con **Ctrl+C**, preferiblemente después de `# TABLE_END` para conservar
completa la última ronda. Se envía `s` antes de cerrar. Los archivos se actualizan
después de cada línea y se sincronizan con disco al terminar una ronda o cerrar
el programa. Si el Nano se reinicia y vuelve a emitir su cabecera, se repite la
preparación y se reanuda el registro. Si el USB se desconecta, el programa termina
con un error: reconecta y vuelve a arrancarlo con un archivo nuevo. Si no llega
la confirmación inicial en 90 s o una ronda no termina en 180 s, se detiene con
un mensaje, conservando lo recibido.

**Mantén la tapa abierta y evita la suspensión del portátil.** En macOS puedes
impedir la suspensión por inactividad durante el registro con:

```sh
caffeinate -i python registrar.py --port /dev/cu.usbserial-XXXX --interval 600
```

La fuente externa de la sonda también debe permanecer alimentada. Consulta
[la guía para el lago](LAGO.md) para los pasos de esta salida.

Las fechas del PC son **horas de recepción**, no horas exactas de adquisición.
Los grupos se miden en secuencia; no los trates como una muestra simultánea.
Sin el ordenador y el registrador activos, este montaje no conserva datos.

## 7. Si algo falla

| Síntoma | Comprobación |
| --- | --- |
| No aparece ningún puerto USB | Cable de datos, conexión USB y controlador del chip del clon |
| No sube el programa | Puerto correcto, monitor cerrado y opción Old Bootloader si procede |
| `no_probe` | 12 V medidos entre rojo y negro, masa común, cable adecuado, blanco a D7 |
| `unexpected_identification` | Guarda la respuesta; revisa firmware y que sea un equipo In-Situ compatible |
| No hay canales | Revisa instalación y envía `a` si quieres recrear la lista |
| `invalid_autoconfigure_ack` | Guarda el log; puede faltar soporte del comando en ese firmware |
| `response_timeout_overflow_or_crc` | Bornes, masa común, alimentación estable y longitud de conexión |
| `MISSING_SENSOR` | Sensor esperado ausente: revisa la instalación y la configuración de canales |
| Valores plausibles pero incorrectos | Verifica parámetro, unidad, calibración, estabilización y compensaciones |
| CSV vacío | Mira el `.csv.log`; el archivo vacío de lecturas no demuestra que la sonda funcione |

No pruebes un restablecimiento de fábrica para resolver un timeout: puede borrar
calibraciones. El programa no incluye ese comando. Si falla CRC de forma
persistente, corrige la conexión; no desactives la comprobación.

## 8. Antes de medir para la tesis

1. Identifica sensores, unidades y correspondencia de todos los canales.
2. Calibra y verifica con patrones según el sensor, usando el procedimiento de
   In-Situ y el acceso de configuración correspondiente. Este lector **no
   implementa los procedimientos de calibración**.
3. Comprueba compensaciones relevantes: salinidad/presión para oxígeno,
   referencia/compensación barométrica para profundidad, etc., según tu modelo.
4. Haz una prueba de al menos 10 minutos, conserva el log y confirma que los
   cambios de la muestra se reflejan en los canales esperados.
5. Deja estabilizar los sensores. Una lectura cada pocos segundos no garantiza
   que la respuesta química del sensor se haya estabilizado.
6. Mantén y almacena cada sensor según su manual; especialmente pH/ISE y la
   tapa óptica RDO. No uses el montaje de mesa sin protección como instalación de campo.

Para calibrar/configurar, In-Situ documenta Win-Situ y VuSitu. Comprueba si ya
tienes un TROLL Com o un enlace compatible antes de comprar accesorios.
La [guía oficial de Win-Situ](https://in-situ.com/pub/media/support/documents/Win-Situ_5_QSG.pdf)
indica cómo descargar el software y conectarlo; no confundas el programa con el
accesorio físico necesario para acceder a la sonda.

## Comprobaciones del proyecto

```sh
c++ -std=c++11 -Wall -Wextra -pedantic tests/check_protocol.cpp -o /tmp/aquatroll-check
/tmp/aquatroll-check
c++ -std=c++11 -Wall -Wextra -pedantic -Itests/stubs tests/check_summary.cpp -o /tmp/aquatroll-summary
/tmp/aquatroll-summary
python3 tests/check_registrar.py
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old AquaTroll500
```

Las pruebas comprueban el analizador de protocolo y ejecutan el sketch con
respuestas simuladas de tu tercera ronda: tabla debajo de `SCAN_END`, precisión
original, comandos `m`/`r`, sensor ausente, limpieza entre rondas, otra cantidad
de canales y fallo de un grupo posterior. Los sustitutos de Arduino/SDI12 solo
se usan en el ordenador; no simulan CRC ni temporización eléctrica. La
compilación y las pruebas no sustituyen cargar la nueva versión en el Nano.

La prueba del registrador usa un Nano simulado y archivos reales temporales:
intervalos de 10 minutos sin esperar ese tiempo real, CSV y log, ronda parcial,
reinicio, prevención de solapamientos, límite de espera y parada al salir. No
comprueba la conexión USB ni la alimentación del montaje de campo.

Los ajustes de pin, pausa entre rondas y pulso de activación están al principio
del sketch. No añadas otras bibliotecas que usen las mismas interrupciones o
temporizadores sin revisar su compatibilidad con SDI-12.
