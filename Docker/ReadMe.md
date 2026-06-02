# ESP-IDF Docker Workflow

## Create the Image

```
docker build -t esp-idf .
```

## Add the USB Device

Verify first that the USB serial device is available and has enough privileges:

```bash
ls /dev/ttyS* /dev/ttyUSB* /dev/ttyACM*
sudo chmod 666 /dev/ttyACM0
```

Run the container with the project mounted and the USB serial device shared:

```bash
sudo docker run --rm -it \
  --device=/dev/ttyACM0 \
  -v $PWD:/project \
  esp-idf
```

## Run the Container Without USB

Use this only when you want to build without flashing:

```bash
sudo docker run --rm -it -v $PWD:/project esp-idf
```

## Build 

Set the target:

```bash
idf.py set-target esp32c3
```

Configure WiFi and Wake-on-LAN settings:

```bash
idf.py menuconfig
```

Open `ESP32 Wake-on-LAN Configuration` and set:

- `WiFi SSID`
- `WiFi password`
- `Wake-on-LAN broadcast IP`
- `Target computer MAC address`

```bash
idf.py build
```

For a classic ESP32 board, use:

```bash
idf.py set-target esp32
idf.py build
```

## Flash and Monitor

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

## Troubleshooting Flash Connection

If flashing fails with:

```text
Failed to connect to ESP32-C3: No serial data received.
```

Check the serial device on the host:

```bash
ls /dev/ttyS* /dev/ttyUSB* /dev/ttyACM*
```

Then make sure the same device is used in both commands:

```bash
sudo docker run --rm -it \
  --device=/dev/ttyACM0 \
  -v $PWD:/project \
  esp-idf

idf.py -p /dev/ttyACM0 flash monitor
```

If the board still does not respond:

- Hold `BOOT`, tap `RESET`, release `RESET`, then release `BOOT`.
- Try flashing at a lower baud rate:

```bash
idf.py -p /dev/ttyACM0 -b 115200 flash monitor
```

- Check that the USB cable supports data.
- If the board appears as `/dev/ttyS0` or `/dev/ttyUSB0`, replace `/dev/ttyACM0` in both commands.
