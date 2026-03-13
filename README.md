# KProx HID Automation

KProx is a 1-key programmable BLE + USB keyboard and mouse with API.

KProx is a wireless HID proxy device built on the 
[M5Stack AtomS3 Lite ESP32S3](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit) 
and the
[M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3). 

It presents itself to a paired host as a Bluetooth or USB keyboard and mouse, 
then accepts input from a web interface, REST API; letting you 
type into, click on, and control any HID-capable device without installing 
software on it.

The primary use case is scripted automation via a DSL. KProx stores sequences 
called *token strings* in numbered registers on-device. A single button press replays 
the active register. Token strings are written in a small built-in scripting 
language that supports keyboard chords, mouse control, loops, conditionals, 
variables, math, random numbers, timed scheduling and more.

Typical applications include unlocking unattended machines, mouse jiggling, 
sending canned text sequences, scripted UI testing, and automated form entry 
on devices where you cannot install software; embedded terminals, locked-down 
kiosks, Android devices, or any machine that accepts a standard keyboard and 
mouse.

Another use case is for sending credentials from keepass to a host device. 

![Alt text](web/kprox.png)

> ⚠️ **Ethical Use Warning**
>
> This software, firmware, source code, and all associated artifacts are 
> intended for personal use only, on systems and devices you own or have 
> explicit authorization to operate. By using this project you accept full 
> responsibility for how it is used.
>
> This software is provided **as-is** with no warranty of any kind. The creator
> assumes no liability for damages, data loss, legal consequences, or any other 
> harm arising from its use or misuse.
>
> **The license granted by this project is automatically and irrevocably 
> revoked for any use that is illegal, unethical, or unauthorized.** This 
> includes but is not limited to: use on systems you do not own or have 
> permission to access, use in any form of surveillance, coercion, or 
> harassment, and use in violation of any applicable law or regulation.


## Hardware

- [M5Stack AtomS3 Lite ESP32S3 Dev Kit](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit)
- [M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3). 

---

## Flashing the Device

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)
- [Node.js](https://nodejs.org/) (for the build step that minifies web assets)

### First Flash (USB Cable): Device must be in DFU mode to flash!

1. Connect the AtomS3 or Cardputer ADV via USB-C.
2. Hold the side button for 3–5 seconds until the LED turns green(Atom S3) or 
   hold down BtnG0 and then press BtnRst(Cardputer ADV); this enters programming/DFU mode.
3. Run:
   ```bash
   make build
   ```
   This builds and minifies web assets, uploads the SPIFFS filesystem image, then flashes the firmware over serial.

### OTA (Over-the-Air) Update

Once the device is connected to your PRIVATE and personal AP:

```bash
make ota
```

Or specify a different hostname/IP:

```bash
make ota HOST=192.168.1.42
```

The OTA binary can also be uploaded via the web interface under **Settings → OTA Firmware Update**.

---

## Initial WiFi Setup

KProx does not host its own access point. On first boot it tries to connect using stored credentials. The factory defaults are:

| Setting  | Default     |
|----------|-------------|
| SSID     | `kprox`     |
| Password | `1337prox`  |

**Recommended first-boot flow:**

1. On your Android phone open **Settings → Network → Hotspot** and create a personal hotspot with:
   - **Name:** `kprox`
   - **Password:** `1337prox`
   - **Band:** 2.4 GHz (ESP32 does not support 5 GHz)

2. Power on the KProx device. The LED blinks orange while connecting and flashes green on success.

3. Find the device's IP in your Android hotspot client list, or use mDNS:
   ```
   http://kproxk.local
   ```

4. Open the web interface. Go to **Settings → WiFi**, enter your real network 
SSID and password, and click **Connect**. The new credentials are saved to 
flash and the device reconnects.

5. Disable the Android hotspot. Update the **API Endpoint** field in the 
sidebar to the new IP or mDNS hostname.


> ⚠️ **WARNING: ** KProx is designed it connect to a personal AP. It is NOT 
> recommended NOT to connect the device to a shared network!

---

## Security

All API endpoints are encrypted using AES256 + nonce (keystrokes, mouse, 
registers, settings) require an API key sent as the `X-API-Key` HTTP header.
The API key can be changed via the web interface and it is recommended
you change it on first boot.

**Default key:** `kprox1337`

Change it in the web interface sidebar under **API Key** — enter a new key 
(minimum 8 characters) and click **Save API Key**. The key is persisted to 
flash and takes effect immediately.


> ⚠️ **WARNING: ** Register contents is not encrypted on the device. It is 
> recommended NOT to store secrets on the device!

---

## Basic Operation

| Action (BtnG0) | Behavior |
|--------|----------|
| **1 click** | Play/output content of active register |
| **2 clicks** | Cycle to next register (LED blinks register number) |
| **Hold 2 s** | Toggle halt/resume all operations |
| **Hold 5 s** | Delete all registers (emergency reset) |

---

### Cardputer ADV
KProx provides a simple user interface that takes advantage of the Cardputer ADV
display. 

- use the arrow keys to navigate
- When in the KProx "app" all of the previous operaitons defined above still apply
- To exit to the menu use "fn + esc", enter/return to accept


![Alt text](img/kprox_cardputer_adv_splash.png)
![Alt text](img/kprox_cardputer_adv_wifi_connect.png)
![Alt text](img/kprox_cardputer_adv_kprox_app.png)

## Web Interface

The KProx web interface prides settings configuration and register management
features.

![Alt text](img/web_interface_screenshot.png)

---

## DSL Overview
The KProx firmware has a simple DSL similar to DuckScript referred to as "token
strings"

The token string DSL supports:

- **Keyboard output** — plain text, special keys, key chords, raw HID
- **Mouse control** — absolute/relative movement, clicks, press/release, drag
- **Loops** — if, while, infinite, timed, and counter loops with `{LOOP}..{ENDLOOP}`
- **Variables** — `{SET varname expr}` assigns any evaluated expression to a named variable; `{varname}` outputs it anywhere
- **Conditionals** — `{IF left op right}..{ELSE}..{ENDIF}` with operators `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Math** — `{MATH expr}` with arithmetic, trig, floor/ceil/round, modulo, PI, E
- **Random** — `{RAND min max}`
- **Timing** — `{SLEEP ms}`, `{SCHEDULE HH:MM}`
- **System** — Bluetooth/USB toggle, halt/resume, WiFi connect

See [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) for a complete documentation and examples.

---

## Token String Examples

### FizzBuzz
```
{LOOP i 1 1 20}{SET fb {i}}{IF {MATH {i} % 15} == 0}{SET fb FizzBuzz}{ELSE}{IF {MATH {i} % 3} == 0}{SET fb Fizz}{ELSE}{IF {MATH {i} % 5} == 0}{SET fb Buzz}{ENDIF}{ENDIF}{ENDIF}{fb}{ENTER}{ENDLOOP}
```

### Aligned 0–9 multiplication table
```
 x |{LOOP j 0 1 9}  {j}{ENDLOOP}{ENTER}---+{LOOP j 0 1 9}---{ENDLOOP}{ENTER}{LOOP i 0 1 9} {i} |{LOOP j 0 1 9}{SET p {MATH {i} * {j}}}{IF {p} < 10}  {p}{ELSE} {p}{ENDIF}{ENDLOOP}{ENTER}{ENDLOOP}
```

### Accumulate a running sum
```
{SET total 0}{LOOP i 1 1 100}{SET total {MATH {total} + {i}}}{ENDLOOP}Sum 1-100 = {total}{ENTER}
```
Types: `Sum 1-100 = 5050`

### Mouse jiggler (infinite, random movement)
```
{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP {RAND 1000 3000}}{ENDLOOP}
```

### Unlock Android
```
{MOUSEMOVE 10 0}{ENTER}{SLEEP 100}mysecurepassword{SLEEP 300}{ENTER}
```

### Unlock Windows
```
{LEFT}{SLEEP 1000}mysecurepassword{ENTER}
```

### Linux Magic SysRq REISUB (safe emergency reboot)
```
{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}
```

---

## External Data Input

Pipe token strings from a shell script:

```bash
echo "{SLEEP 1000}hello world{ENTER}" | bash kpipe.sh
```

---

## Architecture

- **Firmware** — `src` Arduino on ESP32S3, WebServer on port 80
- **Web assets** — `web` Minified by build.js HTML/CSS/JS stored in SPIFFS
- **Android companion app** — `android` Android companion application for KProx
