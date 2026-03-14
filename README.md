# KProx HID Automation & Proxy

KProx is a programmable BLE + USB keyboard and mouse with an encrypted REST API and an on-device credential store.

KProx runs on the
[M5Stack AtomS3 Lite ESP32S3](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit)
and the
[M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3).

It presents itself to a paired host as a Bluetooth or USB keyboard and mouse, then accepts input from a web interface or REST API, letting you type into, click on, and control any HID-capable device without installing software on it.

The primary use case is scripted automation via a small built-in DSL. Token strings are stored in numbered registers on-device. A single button press replays the active register. The language supports keyboard chords, mouse control, loops, conditionals, variables, math, random numbers, timed scheduling, and direct credential injection from the encrypted credential store.

Typical applications: unlocking unattended machines, mouse jiggling, canned text sequences, scripted UI testing, automated form entry on devices where you cannot install software — embedded terminals, locked-down kiosks, Android devices, or any machine that accepts a standard keyboard and mouse.

![KProx](web/kprox.png)

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
- [M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3)

---

## Flashing the Device

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)
- [Node.js](https://nodejs.org/) (for the build step that minifies web assets)

### First Flash (USB Cable)

The device must be in DFU mode to flash.

1. Connect via USB-C.
2. Enter DFU mode:
   - **AtomS3:** Hold the side button for 3–5 seconds until the LED turns green.
   - **Cardputer ADV:** Hold BtnG0, then press BtnRst.
3. Run:
   ```bash
   make build
   ```
   This builds and minifies web assets, uploads the SPIFFS filesystem image, then flashes the firmware over serial.

### OTA (Over-the-Air) Update

Once the device is on your network:

```bash
make ota
```

Or specify a hostname/IP:

```bash
make ota HOST=192.168.1.42
```

The OTA binary can also be uploaded via the web interface under **Settings → OTA Firmware Update**.

---

## Initial WiFi Setup

KProx does not host its own access point. On first boot it connects using stored credentials. Factory defaults:

| Setting  | Default     |
|----------|-------------|
| SSID     | `kprox`     |
| Password | `1337prox`  |

**Recommended first-boot flow:**

1. On your Android phone create a personal hotspot:
   - **Name:** `kprox`  **Password:** `1337prox`  **Band:** 2.4 GHz
2. Power on KProx. The LED blinks orange while connecting, flashes green on success.
3. Find the IP in your hotspot client list, or use mDNS: `http://kprox.local`
4. Open the web interface → **WiFi Settings**, enter your real network credentials, click **Connect**.
5. Disable the hotspot. Update the **API Endpoint** field to the new IP or mDNS hostname.

> ⚠️ KProx is designed to connect to a personal AP. Do **not** connect it to a shared network.

---

## Security Model

### API Encryption

All API endpoints require authentication. Requests are authenticated with an HMAC-SHA256 over a single-use nonce, and both request and response bodies are encrypted with AES-256-CTR + HMAC-SHA256 keyed from the API key. The nonce rotates on every request.

**Default API key:** `kprox1337` — change this immediately via the web interface sidebar.

### Credential Store

The credential store is a separate encrypted vault for secrets used in token strings (passwords, API keys, etc.). It uses AES-256-CTR + HMAC-SHA256 keyed from a user-supplied store key that is **never written to flash**. The store key lives only in volatile RAM and is cleared on lock or reboot.

The store has two states: **locked** (default after boot) and **unlocked**. Unlock via the web interface **Credential Store** tab or the `/api/credstore` API endpoint. Once unlocked, `{CREDSTORE label}` tokens in registers are substituted with the decrypted value at playback time.

> **Note:** Register contents themselves are stored in plaintext on flash. Do not store secrets directly in registers — use the credential store instead.

---

## Basic Operation

| Action (BtnG0) | Behavior |
|----------------|----------|
| **1 click** | Play active register / halt if running |
| **2 clicks** | Cycle to next register (LED blinks register number) |
| **Hold 5 s** | Delete all registers (emergency reset) |

---

## Cardputer ADV Apps

The Cardputer ADV has a display and keyboard. Use arrow keys to navigate the launcher; Enter to open an app; `fn+esc` or backtick to return to the launcher.

| App | Description |
|-----|-------------|
| **KProx** | Main register playback and status app |
| **CredStore** | Credential store status — shows lock state, credential count, and labels. Press `L` to lock the store. |
| **Keyboard HID** | Direct keyboard input forwarding |
| **Clock** | Current time display (requires NTP) |
| **Settings** | Per-page settings: Connectivity · WiFi · API Key · Device Identity |

![Splash](img/kprox_cardputer_adv_splash.png)
![WiFi connect](img/kprox_cardputer_adv_wifi_connect.png)
![KProx app](img/kprox_cardputer_adv_kprox_app.png)

---

## Web Interface

The web interface is served from SPIFFS and provides full device management.

![Web interface](img/web_interface_screenshot.png)

**Tabs:**
- **Registers** — create, edit, name, reorder, and play token string registers
- **Mouse** — trackpad for direct mouse control
- **Code Reference** — searchable table of all tokens, HID codes, and examples
- **Credential Store** — manage the encrypted credential vault; lock/unlock, add/delete/update credentials, change the store key

The sidebar provides API key management, WiFi status, Bluetooth toggle, LED control, keymap selection, mTLS configuration, and OTA firmware/SPIFFS update.

---

## DSL Overview

See [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) for complete documentation.

The token string DSL supports:

- **Keyboard output** — plain text, special keys, key chords, raw HID
- **Mouse control** — absolute/relative movement, clicks, press/release
- **Credential injection** — `{CREDSTORE label}` substitutes secrets from the encrypted store
- **Loops** — infinite, timed, counter (`LOOP`/`FOR`/`WHILE`) with `BREAK`
- **Variables** — `{SET varname expr}` with full expression evaluation
- **Conditionals** — `{IF left op right}..{ELSE}..{ENDIF}`
- **Math** — `{MATH expr}` with arithmetic, trig, floor/ceil/round, constants PI and E
- **Random** — `{RAND min max}`
- **Timing** — `{SLEEP ms}`, `{SCHEDULE HH:MM}`
- **Keymap** — `{KEYMAP id}` to switch keyboard layouts mid-string
- **System** — Bluetooth/USB toggle, halt/resume, WiFi connect

---

## Token String Examples

### Credential-based login
```
{CREDSTORE corp_username}{TAB}{CREDSTORE corp_password}{ENTER}
```

### FizzBuzz
```
{LOOP i 1 1 20}{SET fb {i}}{IF {MATH {i} % 15} == 0}{SET fb FizzBuzz}{ELSE}{IF {MATH {i} % 3} == 0}{SET fb Fizz}{ELSE}{IF {MATH {i} % 5} == 0}{SET fb Buzz}{ENDIF}{ENDIF}{ENDIF}{fb}{ENTER}{ENDLOOP}
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

## API

All endpoints require `X-Auth: <hmac-sha256(apiKey, nonce)>` obtained from `GET /api/nonce`. Request and response bodies are encrypted (AES-256-CTR + HMAC) when `X-Encrypted: 1` is set.

| Endpoint | Methods | Description |
|----------|---------|-------------|
| `/api/nonce` | GET | Fetch a fresh single-use nonce |
| `/api/status` | GET | Device status, connection state, credstore lock state |
| `/api/settings` | GET POST DELETE | Aggregate settings |
| `/api/registers` | GET POST DELETE | Register management |
| `/api/registers/export` | GET | Export all registers as JSON |
| `/api/registers/import` | POST | Import registers from JSON |
| `/api/credstore` | GET POST | Credential store — lock/unlock/get/set/delete |
| `/api/credstore/rekey` | POST | Re-encrypt all credentials with a new key |
| `/api/wifi` | GET POST | WiFi status and connect |
| `/api/bluetooth` | GET POST | Bluetooth enable/disable |
| `/api/usb` | GET POST | USB HID enable/disable |
| `/api/led` | GET POST | LED colour and enable/disable |
| `/api/keymap` | GET POST PUT DELETE | Keyboard layout management |
| `/api/mtls` | GET POST | mTLS settings |
| `/api/mtls/certs` | POST DELETE | Certificate upload/clear |
| `/api/device` | GET POST | Device identity (USB manufacturer/product) |
| `/api/ota` | POST | OTA firmware upload |
| `/api/ota/spiffs` | POST | OTA SPIFFS image upload |
| `/send/text` | POST | Send a token string immediately |
| `/send/mouse` | POST | Send a mouse action immediately |

### Credential Store API

```
# Get status (locked state + count; labels if unlocked)
GET /api/credstore

# Unlock
POST /api/credstore  {"action":"unlock","key":"<store_key>"}

# Lock
POST /api/credstore  {"action":"lock"}

# Add or update a credential (store must be unlocked)
POST /api/credstore  {"action":"set","label":"wifi_pass","value":"s3cr3t"}

# Delete a credential
POST /api/credstore  {"action":"delete","label":"wifi_pass"}

# Change the store key (re-encrypts all entries)
POST /api/credstore/rekey  {"old_key":"old","new_key":"newkey123"}
```

---

## External Data Input

Pipe token strings from a shell script:

```bash
echo "{SLEEP 1000}hello world{ENTER}" | bash kpipe.sh
```

---

## Architecture

- **Firmware** — `src/` Arduino on ESP32S3, WebServer on port 80
- **Web assets** — `web/` HTML/CSS/JS minified by `build.js` and stored in SPIFFS
- **Android companion app** — `android/` discovery, register management, and credential store control
