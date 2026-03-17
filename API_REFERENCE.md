# KProx API Reference

## Overview

The KProx HTTP API runs on port 80. All endpoints return JSON unless otherwise stated.

Requests must be authenticated using HMAC-SHA256 unless marked as **open** (no auth required).

---

## Authentication

KProx uses a challenge-response HMAC scheme. Every authenticated request requires a fresh `X-Auth` header.

### Flow

1. `GET /api/nonce` — fetch a single-use nonce *(open, no auth)*
2. Compute `X-Auth = HMAC-SHA256(apiKey, nonce)`
3. Send the computed `X-Auth` header with your request
4. The nonce is consumed — fetch a new one for each request

**Header:** `X-Auth: <hex-encoded HMAC>`

Nonces expire after use. Sending the same nonce twice returns 401.

### Encryption (optional)

Responses that contain sensitive data are AES-256-encrypted when the client is set up for it. Use `X-Encrypted: 1` on requests to send an encrypted body. Encrypted responses carry `X-Encrypted: 1` in their headers.

---

## Authentication Endpoint

### `GET /api/nonce` — open, no auth

Returns a single-use nonce string used to compute `X-Auth`.

**Response**
```json
{ "nonce": "a3f1b2c4..." }
```

---

## Status & Discovery

### `GET /api/status` — auth required

Returns current device state including connection status, active register, and version info.

**Response**
```json
{
  "connected": true,
  "connections": {
    "bluetooth": {
      "enabled": true,
      "initialized": true,
      "connected": false,
      "keyboard_enabled": true,
      "mouse_enabled": true
    },
    "wifi": {
      "ssid": "MyNetwork",
      "connected": true,
      "rssi": -62
    },
    "usb": {
      "supported": true,
      "enabled": true,
      "initialized": true,
      "connected": true,
      "keyboard_ready": true,
      "mouse_ready": true
    }
  },
  "activeRegister": 0,
  "registers": ["content0", "content1", "..."],
  "ip": "192.168.1.42",
  "hostname": "kprox"
}
```

---

### `GET /api/discovery` — auth required

Returns full device discovery payload including capabilities and network details.

**Response**
```json
{
  "protocol_version": "1.0",
  "device_type": "KProx_HID_Controller",
  "device_id": "AA:BB:CC:DD:EE:FF",
  "device_name": "KProx",
  "hostname": "kprox",
  "ip": "192.168.1.42",
  "gateway": "192.168.1.1",
  "subnet_mask": "255.255.255.0",
  "services": { "http": 80 },
  "capabilities": { "hid": true, "keyboard": true, "mouse": true, "bluetooth": true, "usb": false },
  "status": { "bluetooth_connected": false }
}
```

---

### `GET /api/network` — auth required

Returns detailed network information.

**Response**
```json
{
  "wifi_connected": true,
  "wifi_ssid": "MyNetwork",
  "wifi_rssi": -62,
  "ip_address": "192.168.1.42",
  "gateway": "192.168.1.1",
  "subnet_mask": "255.255.255.0",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "hostname": "kprox",
  "broadcast_address": "192.168.1.255",
  "mdns_enabled": true
}
```

---

## HID Output

### `POST /send/text` — auth required

Types a string through the active HID connection using the token parser. Supports all token expressions (`{ENTER}`, `{TAB}`, `{CREDSTORE label}`, etc.).

**Request**
```json
{ "text": "username{TAB}password{ENTER}" }
```

**Response**
```json
{ "status": "ok" }
```

---

### `POST /send/mouse` — auth required

Sends mouse movement or button events.

**Request**
```json
{
  "x": 10,
  "y": -5,
  "action": "click",
  "button": 1
}
```

Fields are all optional. `action` values: `click`, `double`, `press`, `release`. `button`: `1`=left, `2`=right, `4`=middle.

**Response**
```json
{ "status": "ok" }
```

---

## Registers

### `GET /api/registers` — auth required

Returns all registers, their names, and the active register index.

**Response**
```json
{
  "activeRegister": 0,
  "registers": [
    { "number": 0, "content": "token string", "name": "My Register" },
    { "number": 1, "content": "", "name": "" }
  ]
}
```

---

### `POST /api/registers` — auth required

Update register content, name, active register, or trigger play/delete actions.

**Request — set active register**
```json
{ "activeRegister": 2 }
```

**Request — update content**
```json
{ "action": "update", "number": 0, "content": "{ENTER}", "name": "Enter key" }
```

**Request — play a register**
```json
{ "action": "play", "number": 0 }
```

**Request — delete a register**
```json
{ "action": "delete", "number": 3 }
```

**Response**
```json
{ "status": "ok" }
```

---

### `DELETE /api/registers` — auth required

Deletes all registers.

**Response**
```json
{ "status": "ok" }
```

---

### `GET /api/registers/export` — auth required

Returns all registers as a downloadable JSON attachment (`kprox_registers.json`).

---

### `POST /api/registers/import` — auth required

Replaces all registers from a JSON export.

**Request** — same format as the export response.

---

## Sink Buffer

The sink is a write-only text accumulator. `POST` is intentionally unauthenticated so any device on the network can push text. `GET`, flush, size, and delete all require auth.

### `POST /api/sink` — **open, no auth required**

Appends text to the sink buffer. Supports cleartext and encrypted payloads.

**Request — raw body**
```
hello world
```

**Request — JSON body**
```json
{ "text": "hello world" }
```

**Request — encrypted (set header `X-Encrypted: 1`)**  
Body is the AES-encrypted form of either of the above.

**Response**
```json
{ "status": "ok", "size": 11 }
```

Errors: `400` empty body, `413` sink full.

---

### `GET /api/sink` — auth required

Returns sink status and a 120-character preview.

**Response**
```json
{ "size": 512, "preview": "hello wor...", "max_size": 4096 }
```

---

### `GET /api/sink_size` — auth required

Returns just the current byte count and limit.

**Response**
```json
{ "size": 512, "max_size": 4096 }
```

---

### `POST /api/flush` — auth required

Types the entire sink buffer through HID and clears it.

**Response**
```json
{ "status": "ok", "flushed": 512 }
```

---

### `POST /api/sink_delete` — auth required

Discards the sink buffer without typing it.

**Response**
```json
{ "status": "ok", "deleted": 512 }
```

---

## Settings

### `GET /api/settings` — auth required

Returns all device settings.

**Response**
```json
{
  "bluetooth": { "enabled": true },
  "led": { "enabled": true, "color": { "r": 0, "g": 0, "b": 255 } },
  "wifi": { "ssid": "MyNetwork", "password": "********" },
  "utcOffset": 0,
  "device": {
    "manufacturer": "KProx",
    "product": "HID Controller",
    "hostname": "kprox",
    "usb_serial": "KPROX001"
  },
  "defaultApp": 1,
  "maxSinkSize": 4096,
  "appOrder": [1,2,3,4,5,6,7,8,9,10,11,12],
  "appHidden": [false,false,false,false,false,false,false,false,false,false,false,false]
}
```

---

### `POST /api/settings` — auth required

Updates one or more settings. All fields optional — only supplied fields are changed.

**Request**
```json
{
  "utcOffset": -18000,
  "defaultApp": 2,
  "maxSinkSize": 8192,
  "device": { "hostname": "mykprox" },
  "led": { "enabled": false },
  "appOrder": [1,2,3,4,5,6,7,8,9,10,11,12],
  "appHidden": [false,true,false,false,false,false,false,false,false,false,false,false]
}
```

**Response**
```json
{ "status": "ok" }
```

---

### `DELETE /api/settings` — auth required

Wipes all settings (does not affect registers or credentials).

**Response**
```json
{ "status": "ok" }
```

---

### `POST /api/wipe-settings` — auth required

Alias for settings wipe. Responds with a status message.

**Response**
```json
{ "status": "success", "message": "All settings wiped. Device restart required." }
```

---

### `POST /api/wipe-everything` — auth required

Wipes settings AND all registers. Does not touch credentials or the KDBX store.

**Response**
```json
{ "status": "success", "message": "Everything wiped. Device restart required." }
```

---

## Bluetooth

### `GET /api/bluetooth` — auth required

**Response**
```json
{ "enabled": true, "initialized": true, "connected": false }
```

---

### `POST /api/bluetooth` — auth required

**Request**
```json
{ "enabled": true }
```

Optionally include sub-device enable flags: `"keyboard_enabled"`, `"mouse_enabled"`.

---

## USB

### `GET /api/usb` — auth required

**Response**
```json
{
  "enabled": true,
  "initialized": true,
  "connected": true,
  "keyboardReady": true,
  "mouseReady": true,
  "manufacturer": "KProx",
  "product": "HID Controller"
}
```

---

### `POST /api/usb` — auth required

**Request**
```json
{ "enabled": true, "manufacturer": "Acme", "product": "KB" }
```

---

## Device

### `GET /api/device` — auth required

Returns manufacturer, product string, and hostname.

---

### `POST /api/device` — auth required

**Request**
```json
{ "manufacturer": "Acme", "product": "KB" }
```

---

## LED

### `GET /api/led` — auth required

**Response**
```json
{ "enabled": true, "color": { "r": 0, "g": 0, "b": 255 } }
```

---

### `POST /api/led` — auth required

**Request**
```json
{ "enabled": true, "color": { "r": 255, "g": 0, "b": 0 } }
```

---

## WiFi

### `GET /api/wifi` — auth required

**Response**
```json
{
  "ssid": "MyNetwork",
  "password": "********",
  "connected": true,
  "ip": "192.168.1.42",
  "rssi": -62,
  "hostname": "kprox"
}
```

---

### `POST /api/wifi` — auth required

**Request**
```json
{ "ssid": "NewNetwork", "password": "passphrase" }
```

**Response**
```json
{ "status": "connected", "message": "WiFi connection successful" }
```

---

## Keymap

### `GET /api/keymap` — auth required

Without query params: returns list of available keymaps.

**Response**
```json
{
  "active": "en",
  "maps": [
    { "id": "en", "name": "English (built-in)" },
    { "id": "de", "name": "German" }
  ]
}
```

With `?id=<id>`: returns the full keymap JSON for that id.

---

### `POST /api/keymap` — auth required

Sets the active keymap.

**Request**
```json
{ "active": "de" }
```

---

### `PUT /api/keymap` — auth required

Uploads a new custom keymap.

**Request** — raw keymap JSON with `id`, `name`, and `map` array.

---

### `DELETE /api/keymap` — auth required

Deletes a custom keymap.

**Request**
```json
{ "id": "de" }
```

---

## Credential Store

The credential store is a KeePass KDBX 3.1 file (`/kprox.kdbx` in SPIFFS). All credential metadata including labels and count is inside the AES-256-CBC encrypted payload. Nothing is readable at rest without the key.

### `GET /api/credstore` — auth required

Returns lock state and credential list if unlocked.

**Response (locked)**
```json
{ "locked": true, "count": 0 }
```

**Response (unlocked)**
```json
{
  "locked": false,
  "count": 3,
  "labels": ["github", "email", "vpn"]
}
```

---

### `POST /api/credstore` — auth required

All credential operations are dispatched via the `action` field.

#### `action: "unlock"`

**Request (key-only mode)**
```json
{ "action": "unlock", "key": "mypassword" }
```

**Request (key + TOTP mode)**
```json
{ "action": "unlock", "key": "mypin", "totp_code": "123456" }
```

**Request (TOTP-only mode)**
```json
{ "action": "unlock", "totp_code": "123456" }
```

**Response**
```json
{ "status": "ok", "locked": false }
```
Errors: `400` missing/short key, `401` invalid credentials.

---

#### `action: "lock"`

```json
{ "action": "lock" }
```

**Response**
```json
{ "status": "ok", "locked": true }
```

---

#### `action: "set"`

Add or update a credential. Store must be unlocked.

```json
{ "action": "set", "label": "github", "value": "mysecrettoken" }
```

**Response**
```json
{ "status": "ok" }
```

---

#### `action: "get"`

Retrieve a credential value. Store must be unlocked.

```json
{ "action": "get", "label": "github" }
```

**Response**
```json
{ "status": "ok", "value": "mysecrettoken" }
```

---

#### `action: "delete"`

Delete a credential. Store must be unlocked.

```json
{ "action": "delete", "label": "github" }
```

---

#### `action: "wipe"`

Deletes all credentials and resets the KDBX file. Cannot be undone.

```json
{ "action": "wipe" }
```

---

### `POST /api/credstore/rekey` — auth required

Re-encrypts the KDBX database with a new password. The old password must be provided.

**Request**
```json
{ "old_key": "currentpassword", "new_key": "newpassword" }
```

Minimum key length: 8 chars (key-only mode), 4 chars (key+TOTP mode).

**Response**
```json
{ "status": "ok" }
```
Error `401` if old key is wrong.

---

## TOTP / Authenticator

### `GET /api/totp` — auth required

Returns all TOTP accounts with current codes (if NTP is synced) and gate configuration.

**Response**
```json
{
  "gate_mode": 0,
  "time_ready": true,
  "time_epoch": 1700000000,
  "cs_locked": false,
  "accounts": [
    {
      "name": "github",
      "digits": 6,
      "period": 30,
      "code": "123456",
      "seconds_remaining": 14
    }
  ]
}
```

`gate_mode`: `0`=off (key only), `1`=key+TOTP, `2`=TOTP only.

TOTP secrets are encrypted inside the KDBX credential store. Accounts are only returned when the credential store is unlocked.

---

### `POST /api/totp` — auth required

All TOTP operations are dispatched via the `action` field.

#### `action: "add"`

Adds or updates a TOTP account. Credential store must be unlocked (secrets are encrypted with it).

```json
{ "action": "add", "name": "github", "secret": "JBSWY3DPEHPK3PXP", "digits": 6, "period": 30 }
```

`secret` must be a valid Base32 string, minimum 16 characters.

---

#### `action: "delete"`

```json
{ "action": "delete", "name": "github" }
```

---

#### `action: "set_gate"`

Sets the credential store gate mode. Mode `2` (TOTP-only) requires the store to be unlocked first (credentials are rekeyed to the gate secret automatically).

**Switching to TOTP or TOTP-only:**
```json
{ "action": "set_gate", "gate_mode": 1, "gate_secret": "JBSWY3DPEHPK3PXP" }
```

**Switching from TOTP-only back to key-based (requires `new_key`):**
```json
{ "action": "set_gate", "gate_mode": 0, "new_key": "mynewpassword" }
```

`gate_secret` can be omitted to keep the existing secret.

---

#### `action: "set_cs_key"`

Sets or replaces the credential store encryption key. If the store is unlocked all credentials are rekeyed immediately.

```json
{ "action": "set_cs_key", "new_key": "mynewpassword" }
```

---

#### `action: "wipe"`

Wipes all TOTP accounts and gate settings.

```json
{ "action": "wipe" }
```

---

## Scheduled Tasks

### `GET /api/schedtasks` — auth required

Returns all scheduled tasks.

**Response**
```json
{
  "count": 2,
  "tasks": [
    {
      "id": 0,
      "label": "Daily login",
      "year": 0, "month": 0, "day": 0,
      "hour": 9, "minute": 0, "second": 0,
      "payload": "{CREDSTORE username}{TAB}{CREDSTORE password}{ENTER}",
      "enabled": true,
      "repeat": true
    }
  ]
}
```

Year/month/day of `0` means "any" (wildcard match).

---

### `POST /api/schedtasks` — auth required

Add, update, or toggle a scheduled task.

**Request — add/update**
```json
{
  "label": "Daily login",
  "year": 0, "month": 0, "day": 0,
  "hour": 9, "minute": 0, "second": 0,
  "payload": "{CREDSTORE username}{TAB}{ENTER}",
  "enabled": true,
  "repeat": true
}
```

**Request — toggle enabled**
```json
{ "id": 0, "enabled": false }
```

---

### `DELETE /api/schedtasks` — auth required

Delete a task by id.

**Request**
```json
{ "id": 0 }
```

---

## mTLS / Certificates

### `GET /api/mtls` — auth required

**Response**
```json
{
  "enabled": false,
  "has_server_cert": false,
  "has_server_key": false,
  "has_ca_cert": false
}
```

---

### `POST /api/mtls` — auth required

**Request**
```json
{ "enabled": true }
```

---

### `POST /api/mtls/certs` — auth required

Upload PEM certificates.

**Request**
```json
{
  "server_cert": "-----BEGIN CERTIFICATE-----\n...",
  "server_key":  "-----BEGIN PRIVATE KEY-----\n...",
  "ca_cert":     "-----BEGIN CERTIFICATE-----\n..."
}
```

All fields optional — supply only what you want to update.

---

### `DELETE /api/mtls/certs` — auth required

Removes all stored certificates.

---

## OTA Firmware Update

### `POST /api/ota` — auth required (multipart)

Upload firmware binary. The `X-Auth` header must be present in the multipart request.

```bash
curl -X POST http://kprox.local/api/ota \
  -H "X-Auth: $(compute_hmac $NONCE $API_KEY)" \
  -F "firmware=@firmware.bin"
```

**Response**
```json
{ "status": "ok", "message": "Firmware OTA complete, restarting" }
```

---

### `POST /api/ota/spiffs` — auth required (multipart)

Upload SPIFFS filesystem image.

```bash
curl -X POST http://kprox.local/api/ota/spiffs \
  -H "X-Auth: $(compute_hmac $NONCE $API_KEY)" \
  -F "spiffs=@spiffs.bin"
```

---

## Documentation

### `GET /api/docs` — open, no auth

Returns the TOKEN_REFERENCE.md file as `text/markdown`. Used by the web app Reference tab.

---

## Error Responses

All errors return an appropriate HTTP status code and a JSON body:

```json
{ "error": "Human-readable error message" }
```

| Code | Meaning |
|------|---------|
| 400  | Bad request — missing or invalid fields |
| 401  | Unauthorized — missing, invalid, or replayed nonce/HMAC |
| 403  | Forbidden — operation not permitted in current state |
| 405  | Method not allowed |
| 409  | Conflict — store must be unlocked before this operation |
| 413  | Payload too large — sink buffer full |
| 500  | Internal error |

---

## Notes

- All responses from authenticated endpoints may be AES-256 encrypted. Check for the `X-Encrypted: 1` response header.
- The nonce is single-use and rotates on every authenticated request (including failed ones).
- The sink POST endpoint is the only endpoint without authentication by design — it is a write-only accumulator.
- `/api/nonce`, static file serving, and `/api/docs` are the only other open endpoints.
