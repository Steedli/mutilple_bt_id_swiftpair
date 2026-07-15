# Multiple Bluetooth Identity HID Keyboard with Swift Pair

A Bluetooth Low Energy (BLE) HID keyboard implementation supporting multiple Bluetooth identities, allowing the same device to pair with multiple hosts or switch between different device profiles.

## Features

- **Multiple Bluetooth Identities**: Support for 3 independent Bluetooth identities (bt_id 1, 2, 3; bt_id 0 is the default identity created by the stack and is left unused)
- **Microsoft Swift Pair**: Quick pairing support for Windows 10/11
- **Directed Advertising**: Tries fast, high duty-cycle directed advertising to bonded peers first, before falling back to regular Swift Pair advertising
- **Automatic Reconnection**: Auto-reconnects to previously paired hosts using the fixed identity address (RPA/Privacy disabled — see [RPA / Identity Address](#rpa--identity-address) below)
- **HID Keyboard**: Standard USB HID keyboard protocol support
- **Bond Management**: Per-identity bond storage and management
- **Battery Service**: BLE Battery Service (simulated)
- **Multi-host Support**: Each identity can pair with a different host

## Hardware Requirements

- **Supported Boards**:
  - nRF54H20 DK Application Core (nrf54h20dk_nrf54h20_cpuapp)
  - nRF54H20 DK Radio Core (nrf54h20dk_nrf54h20_cpurad)
  - nRF54L15 DK (nrf54l15dk_nrf54l15_cpuapp)
  - nRF54LM20 DK (nrf54lm20dk_nrf54lm20a_cpuapp)
  - Other nRF Connect SDK compatible boards

- **Peripherals**:
  - 4 buttons (DK buttons 1-4)
  - 4 LEDs (DK LEDs 1-4)

## Software Requirements

- **nRF Connect SDK**: v3.2.0 or later (validated on v3.3.1)
- **Zephyr OS**: v4.2.99 or later
- **Toolchain**: As provided with nRF Connect SDK
- **VS Code**: With nRF Connect extension (recommended)

> Built, flashed and reconnect-tested on an **nRF54L15 DK** with NCS v3.3.1.

## Building and Flashing

### Using VS Code with nRF Connect Extension

1. Open the project folder in VS Code
2. Select your target board in nRF Connect panel
3. Click **Build** button
4. Click **Flash** button

### Using Command Line

```bash
# Navigate to project directory
cd d:\workspace\mutilple_bt_id_swiftpair

# Build for your board
west build -b nrf54h20dk/nrf54h20/cpurad

# Flash to device
west flash
```

## Usage Guide

### Initial Setup

1. **Power on the device** - LED1 will start blinking (advertising)
2. **On Windows PC**:
   - Open Settings → Bluetooth & devices
   - Click "Add device" → Bluetooth
   - Select "swift" from the list
   - Follow pairing instructions if prompted
3. **Connection established** - LED2 turns on solid

### Button Functions

| Button | Function | Description |
|--------|----------|-------------|
| **Button 1** | Text Input | Cycles through "hello\n" characters |
| **Button 2** | Shift Key | Acts as Shift modifier key |
| **Button 3** | Switch Identity | Switches to next Bluetooth identity (1→2→3→1) |
| **Button 4** | Delete Bond | Removes bond for current identity |

### LED Indicators

| LED | Status | Meaning |
|-----|--------|---------|
| **LED 1** | Blinking | Advertising / Waiting for connection |
| **LED 1** | Off | Connected |
| **LED 2** | On | Device connected |
| **LED 2** | Off | Device disconnected |
| **LED 3** | On | Caps Lock active |
| **LED 3** | Off | Caps Lock inactive |

### Multi-Identity Workflow

#### Scenario 1: Different PCs for Each Identity

```
1. Power on → Uses bt_id 1
2. Pair with PC-1 (Office)
3. Press Button 3 → Switches to bt_id 2
4. Pair with PC-2 (Home)
5. Press Button 3 → Switches to bt_id 3
6. Pair with PC-3 (Laptop)

Now you can:
- Reset device → Auto-reconnects to last used identity
- Switch between identities to connect to different PCs
```

#### Scenario 2: Auto-Reconnection

```
1. Pair with PC
2. Reset device → Automatically reconnects ✓
3. Turn off PC Bluetooth → Device advertises
4. Turn on PC Bluetooth → Automatically reconnects ✓
```

## Configuration

### Key Configuration Options (`prj.conf`)

```ini
# Bluetooth Identity Support
CONFIG_BT_ID_MAX=4                    # bt_id 0 (unused, default) + 3 usable identities
CONFIG_BT_MAX_PAIRED=6                # Max paired devices (2 per identity)

# RPA (Privacy) disabled — see "RPA / Identity Address" below
CONFIG_BT_PRIVACY=n

# Settings Storage
CONFIG_BT_SETTINGS=y                  # Save bonds to flash
CONFIG_SETTINGS=y
CONFIG_FLASH=y
```

```ini
# Kconfig (this sample's own option, see Kconfig)
CONFIG_BT_DIRECTED_ADVERTISING=y      # Try directed advertising before falling back to Swift Pair
```

### Customization

**Change Device Name**:
```ini
CONFIG_BT_DEVICE_NAME="YourDeviceName"
```

**Adjust Number of Identities**:
```c
// In main.c
#define NUM_BT_IDS 3  // Change to desired number of usable identities (1-3); bt_id 0 stays unused
```

```ini
// In prj.conf
CONFIG_BT_ID_MAX=4    // Must be NUM_BT_IDS + 1 (bt_id 0 is always created too)
```

## Troubleshooting

### Device Not Reconnecting After Reset

**Symptom**: Device advertises but doesn't auto-reconnect to Windows

**Solution**:
- Check that the identity address printed in the logs (`Local address for bt_id N: ...`) is the same address across resets — since RPA/Privacy is disabled, this address is fixed and must not change
- On Windows, manually click "Connect" for first reconnection after pairing
- Confirm the peer address shows up in `Found N bonded device(s) for bt_id N: ...` at boot

### Directed Advertising Failed to Start (err -22)

**Symptom**: `Directed advertising failed to start (err -22), retrying shortly` in logs, sometimes right after `Directed advertising to ... timed out`

**Solution**:
- This is expected and self-recovers. The Bluetooth host briefly still holds the last reference of the peer's previous connection object (released right after the `connected()`/`disconnected()` callback returns), so an immediate retry of directed advertising to that address returns `-EINVAL`. The app automatically retries after 100 ms (`dir_adv_start_retry_work`) — no action needed unless it keeps failing well beyond that.

### Advertising Failed (err -12)

**Symptom**: `Advertising failed to start (err -12)` in logs

**Solution**:
- This occurs when trying to advertise while still connected
- Switching identity (Button 3) now waits for the `disconnected()` callback before restarting advertising with the new identity — it is event-driven, not a fixed delay, so this should no longer happen during a normal identity switch

### Cannot Pair After Switching Identity

**Symptom**: "Refusing new pairing" error

**Solution**:
- Each bt_id has its own fixed identity address and its own bond storage, so a given PC is bonded to at most one bt_id at a time
- To pair a different identity with the same PC:
  1. Switch identity (Button 3)
  2. On Windows: Remove/Forget device
  3. Add device again

### Bond Storage Full

**Symptom**: Pairing fails after multiple bonds

**Solution**:
- Use Button 4 to delete bonds for current identity
- Or modify `CONFIG_BT_MAX_PAIRED` value in prj.conf

## Technical Details

### Bluetooth Identity System

Each identity (bt_id 1, 2, 3 — bt_id 0 is the default identity and stays unused) has:
- **Unique Random Static Address**: Fixed, stored in flash
- **Separate Bond Storage**: Up to 2 devices per identity
- **Independent Advertisement**: Can advertise with any identity

### RPA / Identity Address

`CONFIG_BT_PRIVACY=n`: the device advertises using each identity's **fixed Random Static address** instead of a rotating Resolvable Private Address (RPA), and every `bt_le_adv_start()` call (both directed and regular Swift Pair advertising) sets `BT_LE_ADV_OPT_USE_IDENTITY`.

**Why RPA is disabled**: some Bluetooth controllers (observed on certain Razer laptops) fail to resolve this device's RPA via IRK — the scan response shows up as "Unknown BD_ADDR" and the host refuses to reconnect. Advertising the fixed identity address instead removes the dependency on the central's IRK-resolution database, at the cost of the device's address no longer rotating for privacy. Since this is a fixed peripheral with per-identity bonding already isolating hosts from each other, this trade-off is acceptable.

### Directed Advertising

`CONFIG_BT_DIRECTED_ADVERTISING=y` (default): before falling back to regular Swift Pair advertising, the device tries fast, high duty-cycle directed advertising (`BT_LE_ADV_CONN_DIR`) to bonded peers of the *current* identity:

1. On `advertising_start()`, the bonded addresses of the current bt_id are snapshotted into a queue (`dir_adv_bonds_queue`), skipping any peer that is still shown as connected in `conn_mode[]`.
2. Each candidate address gets up to 3 directed-advertising attempts (`DIR_ADV_MAX_RETRIES`); each attempt uses a high duty cycle and times out after ~1.28s per the Bluetooth spec (reported via `connected()` with `BT_HCI_ERR_ADV_TIMEOUT`).
3. If `bt_le_adv_start()` itself returns `-EINVAL` (see [Troubleshooting](#directed-advertising-failed-to-start-err--22)), it is retried after 100 ms rather than treated as a hard failure.
4. A 30 s software fallback timer (`directed_adv_timeout_work`) guarantees the device eventually falls back to regular advertising even if something else goes wrong.
5. Once all candidates are exhausted (or there are none), advertising falls back to regular Swift Pair advertising.

**Identity switching and directed advertising**: switching bt_id (Button 3) must not rebuild the directed-advertising candidate queue while the previous connection is still tearing down, otherwise the very peer you want to reconnect to gets filtered out as "still connected." `switch_bt_id()` therefore does not use a fixed delay — if a connection is active, it requests disconnection and defers the actual identity switch + advertising restart to the `disconnected()` callback (via a `pending_id_switch` flag), which only proceeds once `conn_mode[]` is confirmed empty.

### Microsoft Swift Pair

Swift Pair beacon includes:
- Microsoft Vendor ID: 0x0006
- Beacon ID: 0x03
- Device name: "swift"
- Appears as Windows 10/11 notification for quick pairing

### Bond Management

Bonds are stored in flash memory:
- Survives device resets
- Per-identity storage
- Can be erased individually or all at once

## API Reference

### Key Functions

#### `create_bt_ids()`
```c
static void create_bt_ids(void)
```
**Purpose**: Create and initialize multiple Bluetooth identities

**Details**:
- Checks if each identity already exists (restored from Flash)
- Creates new random static address and IRK if identity doesn't exist
- Displays address information for each identity
- Called once during application startup

**Workflow**:
1. Iterate bt_id 1 to NUM_BT_IDS (bt_id 0, the default identity, is left untouched)
2. Check if current identity is already created
3. If not created: call `bt_id_create()` to generate new identity
4. If exists: restore from settings
5. Print address information for identities 1..NUM_BT_IDS

---

#### `advertising_start()` / `advertising_continue()`
```c
static void advertising_start(void)
static void advertising_continue(void)
```
**Purpose**: Start BLE advertising using the current Bluetooth identity, trying directed advertising to bonded peers first

**Details**:
- `advertising_start()` (re)builds the directed-advertising candidate queue for the current bt_id (`dir_adv_bonds_queue`, skipping already-connected peers) and resets the retry counter, then calls `advertising_continue()`
- `advertising_continue()` retrieves and displays the current identity's local address, then:
  - If a directed-advertising candidate is available (new or a retry of the current one), starts high duty-cycle directed advertising to it (`BT_LE_ADV_CONN_DIR` + `BT_LE_ADV_OPT_USE_IDENTITY`) and arms the 30s fallback timer
  - Otherwise, starts regular advertising with Swift Pair beacon data (`BT_LE_ADV_OPT_CONN` + `BT_LE_ADV_OPT_USE_IDENTITY`, `BT_GAP_ADV_FAST_INT_MIN_1` ~ `MAX_1`, 30–60ms)
- `advertising_continue()` is also re-entered directly (bypassing the queue rebuild) by the directed-advertising timeout/retry work items, so an in-progress retry sequence isn't reset

**Called When**:
- After application startup
- After disconnection (if NFC OOB not disabled, and no bt_id switch is pending)
- After a bt_id switch completes (via `disconnected()`, see `switch_bt_id()` below)
- Indirectly, by the directed-advertising retry/timeout work items

---

#### `switch_bt_id()` / `switch_bt_id_continue()`
```c
static void switch_bt_id(void)
static void switch_bt_id_continue(void)
```
**Purpose**: Switch to the next Bluetooth identity

**Details**:
- `switch_bt_id()` stops current advertising (if advertising) and cancels the directed-advertising work items
- If there is no active connection, it calls `switch_bt_id_continue()` immediately
- If there is an active connection, it sets `pending_id_switch = true`, requests disconnection, and returns — it does **not** use a fixed delay
- `disconnected()` checks `pending_id_switch`; once `conn_mode[]` is confirmed empty, it clears the flag and calls `switch_bt_id_continue()`
- `switch_bt_id_continue()` cycles to the next identity (1→2→3→1) and calls `advertising_start()`

**Why event-driven instead of a fixed delay**: restarting advertising (and rebuilding the directed-advertising candidate queue) before the previous connection has actually finished tearing down would make `dir_adv_bond_find()` see the peer as "still connected" and filter it out — the device would then fall back to slow, regular advertising instead of fast directed advertising. Waiting for the real `disconnected()` event avoids this race regardless of how long the disconnect takes.

**State Transition**:
```
Current State    Action                        New State
--------------------------------------------------------------------
Advertising  → Stop advertising                → Idle
Connected    → Request disconnect, set flag     → Waiting for disconnected()
disconnected() with flag set → conn_mode[] empty → Switch bt_id, advertise new ID
```

**Important Notes**:
- Service interruption during switch lasts until the actual disconnect completes (no fixed delay)
- Paired devices need to reconnect
- Bond information is preserved per identity

---

#### `delete_bond_for_current_id()`
```c
static void delete_bond_for_current_id(void)
```
**Purpose**: Delete one bonded device for current identity

**Details**:
- Enumerates all bonded devices for current identity
- Uses cycling index to select bond to delete
- Calls `bt_unpair()` to remove selected bond
- Automatically moves to next index, allowing multiple deletions

**Use Cases**:
- Bond storage is full, need to free space
- No longer using a paired host
- Resolving pairing conflicts

**Example**:
```
Current identity has 2 bonds: [PC-1, PC-2]
Press 1: Delete PC-1
Press 2: Delete PC-2
Press 3: No bonds to delete
```

### Connection Callbacks

#### `connected()`
```c
static void connected(struct bt_conn *conn, uint8_t err)
```
**Purpose**: Callback when device connection is established

**Parameters**:
- `conn`: Connection handle
- `err`: Error code (0 means success)

**Execution Flow**:
1. If `err == BT_HCI_ERR_ADV_TIMEOUT` (directed advertising to this peer timed out): cancel the directed-adv work items, clear `is_adv`, and call `advertising_continue()` directly to retry the next candidate or fall back — then return
2. If `err` is some other failure: log it and return
3. On success: cancel the directed-adv work items, print connected device address and current bt_id
4. Update bond list, check if it's a bonded device
5. Notify HIDS service that connection is established
6. Save connection to `conn_mode[]` array
7. Turn on connection status LED
8. Continue advertising if there are free connection slots

**State Changes**:
- LED2 turns on (connection status)
- LED1 stops blinking (stops advertising if all slots full)
- `is_adv` set to false (if advertising stops)

---

#### `disconnected()`
```c
static void disconnected(struct bt_conn *conn, uint8_t reason)
```
**Purpose**: Callback when device disconnects

**Parameters**:
- `conn`: Disconnected connection handle
- `reason`: Disconnection reason code (e.g., 0x13 = remote user terminated)

**Execution Flow**:
1. Print disconnection info and reason
2. Notify HIDS service that connection is lost
3. Remove connection from `conn_mode[]` array
4. Check if any other connections remain active
5. Turn off connection LED if no active connections
6. If `pending_id_switch` is set and no connections remain: clear the flag and call `switch_bt_id_continue()` (see `switch_bt_id()`), then return
7. Otherwise, restart advertising after a short delay

**Common Disconnection Reasons**:
- `0x08`: Connection timeout
- `0x13`: Remote user terminated connection
- `0x16`: Local host terminated connection
- `0x3d`: Connection failed to be established

---

#### `security_changed()`
```c
static void security_changed(struct bt_conn *conn, 
                            bt_security_t level,
                            enum bt_security_err err)
```
**Purpose**: Callback when security level changes

**Parameters**:
- `conn`: Connection handle
- `level`: New security level (1-4)
- `err`: Error code (0 means success)

**Security Levels**:
- **Level 1**: No security (no encryption, no authentication)
- **Level 2**: Encryption, no MITM protection
- **Level 3**: Encryption + MITM protection
- **Level 4**: Encryption + MITM + Secure Connections

**Typical Flow**:
```
Connection established → Level 1 (no security)
  ↓
Pairing starts → Level 2 (encryption)
  ↓
Pairing complete → Level 3 (encryption + MITM)
```

**Log Example**:
```
Security changed: E8:B0:C5:83:6D:EA (public) level 2
```

## License

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

Copyright (c) 2018-2025 Nordic Semiconductor ASA

## Support

For issues and questions:
- Check [nRF Connect SDK Documentation](https://docs.nordicsemi.com)
- Visit [DevZone](https://devzone.nordicsemi.com)
- Review logs via serial terminal (115200 8N1)

## Version History

- **v1.1**: Ported to NCS v3.3.1, validated on nRF54L15 DK
  - Disabled RPA/Privacy (`CONFIG_BT_PRIVACY=n` + `BT_LE_ADV_OPT_USE_IDENTITY`) for compatibility with central Bluetooth controllers that fail to resolve this device's RPA
  - Added directed advertising (`CONFIG_BT_DIRECTED_ADVERTISING`) with per-candidate retries, a 30s fallback timer, and a short retry on transient `-EINVAL` from `bt_le_adv_start()`
  - Renumbered usable identities to bt_id 1-3 (bt_id 0 left unused); `CONFIG_BT_ID_MAX` raised to 4
  - Made identity switching (`switch_bt_id()`) event-driven instead of a fixed 500ms delay, fixing a race where directed advertising was skipped right after switching identity
- **v1.0** (Dec 2025): Initial implementation
  - Multiple Bluetooth identity support
  - Swift Pair integration
  - Auto-reconnection with Privacy
  - Per-identity bond management
