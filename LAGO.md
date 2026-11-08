# Lago: una ronda cada 10 minutos con el portátil

El Nano conserva su programa actual. El portátil solicita una ronda con `m`,
guarda los resultados y solicita la siguiente unos 10 minutos después del
inicio anterior. La primera ronda comienza al terminar la preparación inicial;
no espera 10 minutos. Cada ronda de 20 canales tarda aproximadamente 50–55 s.

## Antes de salir

- Lleva el portátil con batería suficiente, USB de datos y alimentación externa
  de 12 V para la sonda, con el cableado que ya probaste. El USB solo alimenta
  el Nano en este montaje.
- Comprueba los tapones y retira el protector de almacenamiento del pH para
  medir. Mantén el Nano, el portátil y las conexiones secos.
- Haz hoy una prueba de al menos 12 minutos para comprobar que se guardan
  dos rondas completas y que el portátil permanece despierto.
- Los valores se leen con la calibración que tenga la sonda. El registrador no
  calibra ni certifica la calidad de las medidas.

## Arrancar en tu Mac

1. Conecta la sonda, su alimentación externa y el Nano por USB. Cierra el monitor
   serie de Arduino y otros programas que usen ese puerto.
2. Abre Terminal y ejecuta:

   ```sh
   cd '/Users/alex/Nube /repos/aqua_troll_500'
   .venv/bin/python registrar.py --list
   ```

   El entorno `.venv` y `pyserial` ya quedaron instalados en este Mac, para poder
   trabajar sin internet. Busca el puerto del Nano: suele incluir `usbserial`,
   `wchusbserial` o `usbmodem`. No elijas `Bluetooth-Incoming-Port` ni
   `debug-console`. Si solo aparecen esos dos, revisa la conexión USB del Nano.

3. Sustituye el puerto del ejemplo por el que aparezca y ejecuta:

   ```sh
   caffeinate -i .venv/bin/python registrar.py --port /dev/cu.usbserial-XXXX --interval 600
   ```

   Mantén la tapa abierta. `caffeinate` impide la suspensión por inactividad
   mientras este programa se ejecuta. Deja encendidas la fuente de la sonda y
   la conexión USB durante toda la medición.

4. Comprueba la primera ronda: tras la preparación inicial aparecerán los
   canales, `# SCAN_END` y la tabla en inglés hasta `# TABLE_END`. Entre rondas
   habrá varios minutos sin datos nuevos; es normal. No necesitas enviar `r`
   ni otros comandos.

## Archivos que quedan guardados

En la carpeta del proyecto se crean automáticamente:

- `aqua_troll_FECHA_HORA.csv`: las columnas **Ch, Parameter, Value, Unit** de la
  tabla del Nano, más **Date/time (Europe/Brussels)**. Son 16 filas por ronda,
  con los parámetros identificados, nombres en inglés y unidades. La fecha es
  la hora local de recepción del inicio de la ronda.
- `aqua_troll_FECHA_HORA.csv.log`: toda la salida original, incluidos los 20
  canales crudos, tablas, identificación, estados y errores. Cada línea conserva
  su fecha y hora de recepción en UTC.

Se conservan signos y decimales originales. Los archivos anteriores no se
sobrescriben. Para elegir un nombre añade, por ejemplo, `--output lago_01.csv`.

Si aparece `# SCAN_FAILED`, la ronda es parcial: las lecturas recibidas quedan
en el log y no se genera una tabla para el CSV. `NO_SENSOR` en la tabla/CSV
significa dato ausente. En los datos crudos del log se conserva `MISSING_SENSOR`.
`RECEIVED` solo confirma la recepción con CRC correcto. Las etiquetas y unidades
del perfil siguen siendo provisionales. Si se interrumpe el USB durante la
transmisión de una tabla, el CSV puede contener solo parte de esa tabla.

El reloj del ordenador debe estar bien ajustado; el CSV usa la hora de Bruselas
y el log usa UTC. Los grupos se adquieren en secuencia, no simultáneamente.

Si el registrador ya estaba ejecutándose al actualizarlo, termina con Ctrl+C
y vuelve a lanzar el mismo comando para que los nuevos CSV usen este formato.

## Terminar o recuperar una interrupción

Pulsa **Ctrl+C**, preferiblemente después de `# TABLE_END`. Se guardará lo
recibido y se enviará la parada al Nano. Copia ambos archivos al terminar.

Si se desconecta el USB o aparece un mensaje de tiempo de espera agotado,
comprueba las conexiones y vuelve a ejecutar el mismo comando: se creará otro
par de archivos. El programa no recupera datos del período desconectado.

Sin el portátil ejecutando el registrador, este montaje no guarda datos. Usa
este procedimiento aunque ya hayas iniciado `r` desde Arduino: al arrancar,
el registrador cancela ese modo y pasa a controlar las rondas de 10 minutos.
