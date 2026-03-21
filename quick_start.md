# KProx Quick Start

KProx is a programmable keyboard and mouse. Connect it to a host via USB or Bluetooth, then control it over
WiFi from your browser. The host sees it as a standard HID device and needs no software.

---

## Connect to the web interface

On first boot KProx connects to a WiFi AP named `kprox` with password `1337prox`. Create a phone hotspot
with those details, then power on the device.

To find the web interface URL:

- **Cardputer:** open the **QRProx** app from the launcher. It displays a QR code and the URL. Scan it with
  your phone or read the IP directly from the screen.
- **mDNS (most devices):** open `http://kprox.local` in a browser.
- **By IP:** find the device IP in your router or hotspot client list.

Once you are in, go to **Settings -> WiFi** and switch to your real network.

---

## Change the API key

The default API key is `kprox1337`. Change it in the **sidebar** before using KProx on any shared network.
Every API call is authenticated with this key.

---

## Type your first token string

In the **Registers** tab, click Register 1 and enter:

```
Hello from KProx!{ENTER}
```

Click **Play**. KProx types that text into whatever currently has keyboard focus on the paired host. Plain
text is typed verbatim. Anything inside `{}` is a command: `{ENTER}`, `{TAB}`, `{CHORD CTRL+C}`,
`{SLEEP 500}`, and so on.

See [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) for every available token.

---

## Store credentials

Open the **Credential Store** tab. Enter a passphrase to create your store, then add an entry with a label,
password, username, and optional notes.

Reference credentials in any token string:

```
{CREDSTORE username work}{TAB}{CREDSTORE work}{ENTER}
```

The store is a KeePass KDBX 3.1 file, encrypted at rest. It can be kept in built-in flash (default) or on
the SD card. The store must be unlocked after every reboot before credentials or TOTP codes can be used.

---

## Add a TOTP account

Open **TOTProx** (unlock the credential store first), then add an account with its Base32 secret from the
setup QR code.

```
{TOTP github}
```

The current 6-digit code is substituted at playback time. NTP sync is required.

---

## Write a script (SD card)

For multi-step automation, write a `.kps` file and save it to the SD card. Use the **KPScript Editor** web
tab:

```kps
set user {CREDSTORE username mysite}
set pass {CREDSTORE mysite}
set code {TOTP mysite}

echo "${user}{TAB}${pass}{ENTER}"
sleep 2000
echo "${code}{ENTER}"
```

Execute it with `{SD_EXEC /scripts/login.kps}` in any token string, or run it directly from the
**KPScript** app on the Cardputer.

See [KEYPROX_SCRIPT_REFERENCE.md](KEYPROX_SCRIPT_REFERENCE.md) for the full language reference.

---

## Cardputer at a glance

Arrow keys navigate the launcher. ENTER opens an app. BtnG0 always plays the active register.

| App         | What it does                                                       |
|-------------|--------------------------------------------------------------------|
| **KProx**   | Status, active register playback                                   |
| **CredStore**| Unlock and manage credentials                                     |
| **TOTProx** | View live TOTP codes, type them with BtnG0                         |
| **QRProx**  | Displays a QR code and URL for the web interface                   |
| **Files**   | Browse SD card, view files, dump to HID with `D`                   |
| **KPScript**| Select and run `.kps` scripts from the SD card                     |
| **Settings**| WiFi, Bluetooth, USB, app layout, security                         |

---

## What to do next

- Read [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) to see all available tokens.
- Read [KEYPROX_SCRIPT_REFERENCE.md](KEYPROX_SCRIPT_REFERENCE.md) to learn KProx Script.
- Read [README.md](README.md) for a full feature overview.
- Read [API_REFERENCE.md](API_REFERENCE.md) if you want to drive the device programmatically.
