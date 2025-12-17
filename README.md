# Multiple Bluetooth Identity HID Keyboard with Swift Pair

A Bluetooth Low Energy (BLE) HID keyboard implementation supporting multiple Bluetooth identities, allowing the same device to pair with multiple hosts or switch between different device profiles.

## Features

- **Multiple Bluetooth Identities**: Support for 3 independent Bluetooth identities (bt_id 0, 1, 2)
- **Microsoft Swift Pair**: Quick pairing support for Windows 10/11
- **Automatic Reconnection**: Auto-reconnects to previously paired hosts using Privacy (RPA)
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

- **nRF Connect SDK**: v3.2.0 or later
- **Zephyr OS**: v4.2.99 or later
- **Toolchain**: As provided with nRF Connect SDK
- **VS Code**: With nRF Connect extension (recommended)

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
| **Button 3** | Switch Identity | Switches to next Bluetooth identity (0→1→2→0) |
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
1. Power on → Uses bt_id 0
2. Pair with PC-1 (Office)
3. Press Button 3 → Switches to bt_id 1
4. Pair with PC-2 (Home)
5. Press Button 3 → Switches to bt_id 2
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
CONFIG_BT_ID_MAX=3                    # Number of identities
CONFIG_BT_MAX_PAIRED=6                # Max paired devices (2 per identity)

# Privacy for Auto-Reconnection
CONFIG_BT_PRIVACY=y                   # Enable RPA for auto-reconnect
CONFIG_BT_RPA_TIMEOUT=900             # RPA timeout (15 minutes)

# Settings Storage
CONFIG_BT_SETTINGS=y                  # Save bonds to flash
CONFIG_SETTINGS=y
CONFIG_FLASH=y
```

### Customization

**Change Device Name**:
```ini
CONFIG_BT_DEVICE_NAME="YourDeviceName"
```

**Adjust Number of Identities**:
```c
// In main.c
#define NUM_BT_IDS 3  // Change to desired number (1-3)
```

```ini
// In prj.conf
CONFIG_BT_ID_MAX=3    // Must match NUM_BT_IDS
```

## Troubleshooting

### Device Not Reconnecting After Reset

**Symptom**: Device advertises but doesn't auto-reconnect to Windows

**Solution**:
- Verify `CONFIG_BT_PRIVACY=y` is enabled in prj.conf
- On Windows, manually click "Connect" for first reconnection after pairing
- Check that device identity address is consistent (shown in logs)

### Advertising Failed (err -12)

**Symptom**: `Advertising failed to start (err -12)` in logs

**Solution**:
- This occurs when trying to advertise while connected
- Wait for disconnection to complete before switching identities
- Code automatically handles this with 500ms delay

### Cannot Pair After Switching Identity

**Symptom**: "Refusing new pairing" error

**Solution**:
- Windows recognizes device by IRK when Privacy is enabled
- To pair different identity with same PC:
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

Each identity has:
- **Unique Random Static Address**: Stored in flash
- **Independent IRK (Identity Resolving Key)**: For privacy
- **Separate Bond Storage**: Up to 2 devices per identity
- **Independent Advertisement**: Can advertise with any identity

### Privacy and Reconnection

With `CONFIG_BT_PRIVACY=y`:
- Device uses **Resolvable Private Address (RPA)**
- RPA changes every 15 minutes (configurable)
- Windows uses stored IRK to identify device
- Enables automatic reconnection

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
1. Iterate from 0 to NUM_BT_IDS-1
2. Check if current identity is already created
3. If not created: call `bt_id_create()` to generate new identity
4. If exists: restore from settings
5. Print address information for all identities

---

#### `advertising_start()`
```c
static void advertising_start(void)
```
**Purpose**: Start BLE advertising using current Bluetooth identity

**Details**:
- Retrieves and displays current identity's local address
- Checks if current identity has bonded devices
- Configures advertising parameters (using fast advertising interval)
- Includes Swift Pair beacon data
- Starts advertising and updates `is_adv` status

**Advertising Parameters**:
- Interval: BT_GAP_ADV_FAST_INT_MIN_1 ~ BT_GAP_ADV_FAST_INT_MAX_1 (30-60ms)
- Options: BT_LE_ADV_OPT_CONN (connectable)
- Advertising data: device name, appearance, Swift Pair beacon
- Scan response data: complete device name, service UUIDs

**Called When**:
- After application startup
- After disconnection (if NFC OOB not disabled)
- After switching identity

---

#### `switch_bt_id()`
```c
static void switch_bt_id(void)
```
**Purpose**: Switch to the next Bluetooth identity

**Details**:
- Stops current advertising (if advertising)
- Disconnects all existing connections
- Waits 500ms to ensure cleanup completes
- Cycles to next identity (0→1→2→0)
- Restarts advertising with new identity

**State Transition**:
```
Current State    Action           New State
------------------------------------------------
Advertising  → Stop advertising  → Idle
Connected    → Disconnect        → Idle
Idle         → Switch bt_id     → Advertising new ID
```

**Important Notes**:
- Brief service interruption during switch (~500ms)
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
1. Check if connection was established successfully
2. Print connected device address and current bt_id
3. Update bond list, check if it's a bonded device
4. Notify HIDS service that connection is established
5. Save connection to `conn_mode[]` array
6. Turn on connection status LED
7. Continue advertising if there are free connection slots

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
6. Restart advertising after 100ms delay

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

- **v1.0** (Dec 2025): Initial implementation
  - Multiple Bluetooth identity support
  - Swift Pair integration
  - Auto-reconnection with Privacy
  - Per-identity bond management
