# KProx HID Automation

KProx is a programmable BLE + USB keyboard and mouse with an encrypted REST API, an on-device credential store, and a community gadget library.

KProx runs on the
[M5Stack AtomS3 Lite ESP32S3](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit)
and the
[M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3).

It presents itself to a paired host as a Bluetooth or USB keyboard and mouse, then accepts input from a web interface or REST API, letting you type into, click on, and control any HID-capable device without installing software on it.

The primary use case is scripted automation via a small built-in DSL. Token strings are stored in numbered registers on-device. A single button press replays the active register. The language supports keyboard chords, mouse control, loops, conditionals, variables, math, random numbers, timed scheduling, and direct credential injection from the encrypted credential store.

Typical applications: unlocking unattended machines, mouse jiggling, canned text sequences, scripted UI testing, automated form entry on devices where you cannot install software — embedded terminals, locked-down kiosks, Android devices, or any machine that accepts a standard keyboard and mouse.

![KProx](web/kprox.png)

---

> ### ⚠️ Ethical Use Warning
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

> ### ⚠️ Unverified Cryptographic Security
>
> **This project is a toy.** The cryptographic implementations and security protocols contained within this repository 
> have **not been audited or verified** by security professionals. 
>
> It is intended for educational and hobbyist purposes only. Do not use this software to store, transmit, or protect 
> sensitive data, or in any production environment.
>
> **USE AT YOUR OWN RISK.** The authors and contributors assume no liability for data loss, security breaches, or any 
> other damages resulting from the use of this software.


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

The credential store is a separate encrypted vault for secrets used in token strings (passwords, API keys, etc.). It uses AES-256-CTR + HMAC-SHA256 keyed from a user-supplied store key that is **never written to flash**. The store key lives only in volatile RAM and is cleared on lock or reboot. The store itself is persisted to NVS (survives firmware flashes).

The store has two states: **locked** (default after boot) and **unlocked**. Unlock via the web interface **Credential Store** tab or the `/api/credstore` API endpoint. Once unlocked, `{CREDSTORE label}` tokens in registers are substituted with the decrypted value at playback time.

> **Note:** Register contents are stored in plaintext on flash. Do not store secrets directly in registers — use the credential store instead.

---

## Basic Operation

| Action (BtnG0) | Behavior |
|----------------|----------|
| **1 click** | Play active register / halt if running |
| **2 clicks** | Cycle to next register (LED blinks register number) |
| **Hold 5 s** | Delete all registers (emergency reset) |

---

## Cardputer ADV Apps

The Cardputer ADV has a display and keyboard. Use arrow keys to navigate the launcher; Enter to open an app; backtick or `fn+esc` to return to the launcher.

| App | Description |
|-----|-------------|
| **KProx** | Main register playback and status. Shows active register, content preview, IP, SSID, hostname, and credential store lock state. |
| **CredStore** | 3-page credential store manager: Status/Unlock · Change Key · Wipe Store. Type your key directly to unlock. |
| **Gadgets** | Browse and install community gadgets from GitHub. Requires WiFi. ENTER installs to a new register. |
| **Keyboard HID** | Direct keyboard input forwarding |
| **Clock** | Current time display (requires NTP) |
| **Settings** | Per-page settings: Connectivity · WiFi Settings · API Key · Device Identity |

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
- **Credential Store** — manage the encrypted credential vault; lock/unlock, add/delete/update credentials, change the store key, wipe the store
- **Gadgets** — browse and install community gadgets from GitHub directly into registers

---

## Gadgets

Gadgets are pre-built token string scripts stored as JSON files in the [`gadgets/`](gadgets/) directory of this repository. They can be browsed and installed:

- **Web interface** — open the **Gadgets** tab, click **Load Gadgets from GitHub**, then click **Install** on any gadget to add it as a new register on the connected device.
- **Cardputer ADV** — open the **Gadgets** app. It fetches the gadget list over WiFi, then lets you page through them with the arrow keys. Press ENTER to install the current gadget as a new register.

### Gadget Format

Each gadget is a single JSON file in `gadgets/`:

```json
{
  "gadget": {
    "name": "Random Mouse Walk",
    "description": "Randomly moves the mouse around the screen forever.",
    "content": "{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}"
  }
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `name` | yes | Short display name shown in the app and web interface |
| `description` | yes | One or two sentences describing what the gadget does |
| `content` | yes | The token string that will be loaded into the register |

### Contributing a Gadget

If you've written a useful token string, contributing it as a gadget makes it available to all KProx users directly from the device — no copy-pasting required.

1. **Fork** this repository.
2. Create a new file in `gadgets/` named descriptively in snake_case, e.g. `my_gadget.json`.
3. Use the format above. Keep `name` under 30 characters (display width on the Cardputer). Keep `description` to 1–2 sentences.
4. Test your gadget on a real device or verify the token string logic against the [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md).
5. Open a **Pull Request** with a brief description of what the gadget does and what device/OS it targets (if applicable).

Good gadgets to contribute: OS-specific shortcuts, useful automation sequences, fun demos, diagnostic tools, or anything that would have been handy to have pre-loaded.

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

### Mouse jiggler (infinite, random)
```
{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP {RAND 1000 3000}}{ENDLOOP}
```

### Lock Windows
```
{CHORD GUI+L}
```

### Linux Magic SysRq REISUB (safe emergency reboot)
```
{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}
```

---

## Cardputer Apps

The Cardputer launcher displays all enabled apps as scrollable icons. Navigate with **`<`** / **`>`**, select with **`ENTER`**. Press **`BtnG0`** (the orange side button) from any app to play the active register. The active app, order, and visibility are configurable from [Settings → App Layout](#settings) or the web interface.

---

### KProx

The main register player and device status screen. Shows the active register index and a content preview alongside the device IP, SSID, and hostname. Navigate registers with **`<`** / **`>`**, play the active register with **`ENTER`** or a single **`BtnG0`** press. Type a number then **`ENTER`** to jump directly to that register. Double-press **`BtnG0`** to advance to the next register. Hold **`BtnG0`** for 2 seconds to toggle halt/resume — halting stops all in-progress HID playback. The credential-store lock state is shown in the header.

---

### FuzzyProx

A fuzzy-search register picker. Type any part of a register name or content to filter the list in real time; matches are ranked by consecutive character runs. Navigate results with **`↑`** / **`↓`**. Press **`ENTER`** on a result to set it as the active register; press **`ENTER`** again (when it is already active) to play it immediately. Press **`BtnG0`** to play the current active register from anywhere in the list. Hold **`BtnG0`** for 2 seconds to halt/resume. The active register is marked with `*`.

---

### RegEdit

A full-screen register editor. Browse registers with **`<`** / **`>`**. Press **`E`** to edit the content of the selected register — the editor supports a visible block cursor, arrow-key navigation (with **`fn`** held), newline insertion with **`ENTER`**, and backspace with **`DEL`**. Exit edit mode with **`fn+``** (backtick); you will be prompted to save or discard. Press **`R`** to rename a register, **`N`** to create a new empty one, **`M`** to enter move mode (reorder with **`<`** / **`>`**, confirm with **`ENTER`**), and **`DEL`** to delete the current register after confirmation. **`fn+X`** deletes all registers after a second confirmation.

---

### CredStore

The on-device encrypted credential store. Credentials are key/value pairs (e.g. `admin_pass → s3cr3t`) accessible inline via the `{CREDSTORE label}` token. The store must be unlocked with its PIN before credentials can be read. From the app you can unlock/lock the store, add new entries, view and delete existing ones, and change the PIN. When locked, `{CREDSTORE …}` tokens resolve to an empty string. The lock state is shown in the header of KProx and FuzzyProx.

---

### Gadgets

A curated library of pre-built token-string automations fetched from the KProx GitHub repository over WiFi. Each gadget has a name, description, and a token-string payload. Browse with **`<`** / **`>`**, press **`ENTER`** to load a gadget into the active register, or **`P`** to play it immediately without overwriting the register. Requires an active WiFi connection to fetch the gadget list; the list is cached in memory for the session. Press **`R`** to refresh from the repository.

---

### SinkProx

Visualises the sink buffer — a write-only staging area that any HTTP client can POST raw text or token strings to without authentication (`POST /api/sink`). The app shows the current buffer size, a preview of the contents, and the device endpoints. Press **`ENTER`** or **`BtnG0`** to flush the buffer, replaying it as HID keystrokes and clearing it. Press **`D`** to delete the buffer without flushing. Press **`H`** or **`?`** to toggle the built-in help page. Hold **`BtnG0`** for 2 seconds to halt/resume playback.

---

### Keyboard

Passes every key typed on the Cardputer keyboard directly to the connected host as a raw HID keystroke — effectively turning the Cardputer into a standalone USB/BLE keyboard. Special keys (arrows, function keys, modifiers) are forwarded transparently. Press **`BtnG0`** to return to the launcher.

---

### Clock

Displays the current time and date synchronised via NTP (requires WiFi). The time is shown in large text at the centre of the screen. Press **`↑`** / **`↓`** to cycle through timezone presets (UTC−12 through UTC+14); the selected offset is persisted and applied to all scheduled task comparisons. If no NTP sync has occurred, dashes are shown in place of the time. The current IP address is shown in the header when connected.

---

### QRProx

Displays a QR code encoding the device's web interface URL (`http://<ip>`). Scan the code with a phone camera to open the web interface without typing the address. The right panel shows the URL, IP, mDNS hostname, and current SSID as text. If WiFi is not connected, a "No WiFi" message is shown instead. Press **`BtnG0`** to type the URL (`http://<ip>`) as HID keystrokes into the currently focused host application. Press any keyboard key to refresh.

---

### SchedProx

Creates and manages scheduled tasks — token strings that fire automatically when the device clock matches a configured date/time. Each task has a label, a datetime pattern (year/month/day/hour/minute/second, where `0` matches any value), a payload (any valid token string), a repeat flag (re-fires every matching second rather than once), and an enabled toggle.

The add form uses an inline datetime editor: navigate fields with **`←`** / **`→`**, adjust values with **`↑`** / **`↓`** or type digits directly. Navigate between sections (label, datetime, payload, options, save) with **`↑`** / **`↓`** and confirm each with **`ENTER`**. From the task list, press **`D`** to delete the selected task and **`E`** from the detail view to toggle it on or off. Requires WiFi + NTP for time comparison.

---

### Settings

Device configuration, cycled with **`BtnG0`** or **`<`** / **`>`**:

| Page | Contents |
|------|----------|
| **WiFi** | Enable/disable WiFi, connect to a new SSID/password |
| **Bluetooth** | Enable BLE, toggle BLE keyboard and mouse sub-devices, reconnect |
| **USB HID** | Enable USB, toggle USB keyboard, mouse, and FIDO2/CTAP2 |
| **API Key** | View (masked) and replace the HMAC API key |
| **Device Identity** | USB manufacturer, product name, mDNS hostname, USB serial number |
| **Sink Config** | Set the maximum sink buffer size in bytes (0 = unlimited) |
| **HID Timing 1/2** | Key press delay, key release delay, between-keys delay |
| **HID Timing 2/2** | Send-text delay, special/media key delay, token execution delay |
| **Startup App** | Choose which app launches automatically on boot |
| **App Layout** | Reorder apps in the launcher with **`←`** / **`>`**, toggle visibility with **`ENTER`** or **`H`**. Settings cannot be hidden. |

On timing pages, press **`BtnG0`** to play the active register as a live test of the current timing values.


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
| `/api/credstore` | GET POST | Credential store — lock/unlock/get/set/delete/wipe |
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
- **Gadgets** — `gadgets/` community token string scripts in JSON format
