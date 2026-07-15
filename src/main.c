/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>
#include <assert.h>
#include <zephyr/spinlock.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/bluetooth/services/bas.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/dis.h>
#include <dk_buttons_and_leds.h>

#include "app_nfc.h"

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101

#define OUTPUT_REPORT_MAX_LEN            1
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x02
#define INPUT_REP_KEYS_REF_ID            0
#define OUTPUT_REP_KEYS_REF_ID           0
#define MODIFIER_KEY_POS                 0
#define SHIFT_KEY_CODE                   0x02
#define SCAN_CODE_POS                    2
#define KEYS_MAX_LEN                    (INPUT_REPORT_KEYS_MAX_LEN - \
					SCAN_CODE_POS)

#define ADV_LED_BLINK_INTERVAL  1000

#define NUM_BT_IDS 3

/* IDs 1..NUM_BT_IDS are used for advertising; ID 0 is the default identity
 * that the Bluetooth stack always creates and is left unused here.
 */
static uint8_t current_bt_id = 1;
static uint8_t bond_delete_index = 0;

#define ADV_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define LED_CAPS_LOCK  DK_LED3
#define NFC_LED	       DK_LED4
#define KEY_TEXT_MASK  DK_BTN1_MSK
#define KEY_SHIFT_MASK DK_BTN2_MSK
#define KEY_SWITCH_ID_MASK DK_BTN3_MSK
#define KEY_DELETE_BOND_MASK DK_BTN4_MSK
#define KEY_ADV_MASK DK_BTN4_MSK
/* Key used to accept or reject passkey value */
#define KEY_PAIRING_ACCEPT DK_BTN1_MSK
#define KEY_PAIRING_REJECT DK_BTN2_MSK

/* HIDs queue elements. */
#define HIDS_QUEUE_SIZE 10

/* ********************* */
/* Buttons configuration */

/* Note: The configuration below is the same as BOOT mode configuration
 * This simplifies the code as the BOOT mode is the same as REPORT mode.
 * Changing this configuration would require separate implementation of
 * BOOT mode report generation.
 */
#define KEY_CTRL_CODE_MIN 224 /* Control key codes - required 8 of them */
#define KEY_CTRL_CODE_MAX 231 /* Control key codes - required 8 of them */
#define KEY_CODE_MIN      0   /* Normal key codes */
#define KEY_CODE_MAX      101 /* Normal key codes */
#define KEY_PRESS_MAX     6   /* Maximum number of non-control keys
			       * pressed simultaneously
			       */

/* Number of bytes in key report
 *
 * 1B - control keys
 * 1B - reserved
 * rest - non-control keys
 */
#define INPUT_REPORT_KEYS_MAX_LEN (1 + 1 + KEY_PRESS_MAX)

/* Current report map construction requires exactly 8 buttons */
BUILD_ASSERT((KEY_CTRL_CODE_MAX - KEY_CTRL_CODE_MIN) + 1 == 8);

/* OUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	OUTPUT_REP_KEYS_IDX = 0
};

/* INPUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	INPUT_REP_KEYS_IDX = 0
};

/* HIDS instance. */
BT_HIDS_DEF(hids_obj,
	    OUTPUT_REPORT_MAX_LEN,
	    INPUT_REPORT_KEYS_MAX_LEN);

static volatile bool is_adv;
/* Set while waiting for active connections to drop before completing a
 * bt_id switch. Advertising (and the directed-adv bond snapshot) must not
 * restart until conn_mode[] is actually empty, otherwise the peer being
 * disconnected is still seen as "connected" and gets filtered out of the
 * directed advertising candidate list.
 */
static bool pending_id_switch;

/* Microsoft Swift Pair Beacon data */
#define MSFT_VENDOR_ID 0x0006  /* Microsoft Vendor ID */
#define MSFT_BEACON_ID 0x03    /* Beacon ID for Swift Pair */
#define MSFT_BEACON_SUB_SCENARIO 0x00  /* No specific sub-scenario */
#define MSFT_RSSI_BYTE 0x80    /* Reserved RSSI byte */

/* Swift Pair manufacturer data with display name */
static uint8_t msft_swift_pair_data[] = {
	MSFT_VENDOR_ID & 0xff, (MSFT_VENDOR_ID >> 8) & 0xff,  /* Microsoft Vendor ID */
	MSFT_BEACON_ID,           /* Beacon ID */
	MSFT_BEACON_SUB_SCENARIO, /* Sub Scenario */
	MSFT_RSSI_BYTE,           /* Reserved RSSI byte */
	/* Display name section (optional, for showing name in Swift Pair notification) */
	0x01,                     /* Display name type */
	0x05,                     /* Length of display name (5 for "swift") */
	/* Device name */
	's', 'w', 'i', 'f', 't'
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	/* Microsoft Swift Pair */
	BT_DATA(BT_DATA_MANUFACTURER_DATA, msft_swift_pair_data, sizeof(msft_swift_pair_data)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
				  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

static const uint8_t hello_world_str[] = {
	0x0b,	/* Key h */
	0x08,	/* Key e */
	0x0f,	/* Key l */
	0x0f,	/* Key l */
	0x12,	/* Key o */
	0x28,	/* Key Return */
};

static const uint8_t shift_key[] = { 225 };

/* Current report status
 */
static struct keyboard_state {
	uint8_t ctrl_keys_state; /* Current keys state */
	uint8_t keys_state[KEY_PRESS_MAX];
} hid_keyboard_state;

#if CONFIG_NFC_OOB_PAIRING
static struct k_work adv_work;
#endif

static struct k_work pairing_work;
struct pairing_data_mitm {
	struct bt_conn *conn;
	unsigned int passkey;
};

K_MSGQ_DEFINE(mitm_queue,
	      sizeof(struct pairing_data_mitm),
	      CONFIG_BT_HIDS_MAX_CLIENT_COUNT,
	      4);

static struct bt_bond_info bonds[CONFIG_BT_MAX_PAIRED];
static size_t bond_count;

static void bond_cb(const struct bt_bond_info *info, void *user_data)
{
	size_t *count = user_data;
	if (*count < ARRAY_SIZE(bonds)) {
		bonds[*count] = *info;
		(*count)++;
	}
}

#if CONFIG_BT_DIRECTED_ADVERTISING
/* Bonded addresses of the current identity, queued for directed advertising. */
K_MSGQ_DEFINE(dir_adv_bonds_queue,
	      sizeof(bt_addr_le_t),
	      CONFIG_BT_MAX_PAIRED,
	      4);

static bt_addr_le_t current_dir_adv_addr;
static uint8_t dir_adv_retry_count;
#define DIR_ADV_MAX_RETRIES 3
#define DIR_ADV_FALLBACK_TIMEOUT K_SECONDS(30)
static struct k_work_delayable directed_adv_timeout_work;

/* bt_le_adv_start() for a directed peer can transiently return -EINVAL: the
 * Bluetooth host still holds the last reference of that peer's previous
 * bt_conn object until right after our connected()/disconnected() callback
 * returns (see bt_conn_exists_le() in conn.c), so it briefly looks "still
 * connected" to a fresh directed-adv request. Retry shortly instead of
 * giving up outright.
 */
#define DIR_ADV_START_RETRY_DELAY K_MSEC(100)
static struct k_work_delayable dir_adv_start_retry_work;

static void dir_adv_bond_find(const struct bt_bond_info *info, void *user_data)
{
	int err;

	/* Skip peers that are already connected. */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn) {
			const bt_addr_le_t *dst = bt_conn_get_dst(conn_mode[i].conn);

			if (!bt_addr_le_cmp(&info->addr, dst)) {
				return;
			}
		}
	}

	err = k_msgq_put(&dir_adv_bonds_queue, (void *)&info->addr, K_NO_WAIT);
	if (err) {
		printk("No space in the queue for the bond.\n");
	}
}
#endif

static void advertising_continue(void)
{
	int err;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = CONFIG_BT_ID_MAX;
	char addr_str[BT_ADDR_LE_STR_LEN];

	/* Get and display current local address */
	bt_id_get(addrs, &count);
	if (current_bt_id < count) {
		bt_addr_le_to_str(&addrs[current_bt_id], addr_str, sizeof(addr_str));
		printk("Local address for bt_id %d: %s\n", current_bt_id, addr_str);
	}

#if CONFIG_BT_DIRECTED_ADVERTISING
	bt_addr_le_t dir_addr;
	bool has_dir_addr = false;

	if (dir_adv_retry_count > 0 && dir_adv_retry_count < DIR_ADV_MAX_RETRIES) {
		dir_addr = current_dir_adv_addr;
		has_dir_addr = true;
		dir_adv_retry_count++;
		printk("Retrying directed advertising (%u/%u)\n",
		       dir_adv_retry_count, DIR_ADV_MAX_RETRIES);
	} else if (!k_msgq_get(&dir_adv_bonds_queue, &dir_addr, K_NO_WAIT)) {
		has_dir_addr = true;
		current_dir_adv_addr = dir_addr;
		dir_adv_retry_count = 1;
	} else if (dir_adv_retry_count >= DIR_ADV_MAX_RETRIES) {
		dir_adv_retry_count = 0;
	}

	if (has_dir_addr) {
		struct bt_le_adv_param dir_param;

		if (is_adv) {
			err = bt_le_adv_stop();
			if (err) {
				printk("Advertising failed to stop (err %d)\n", err);
				return;
			}
			is_adv = false;
		}

		dir_param = *BT_LE_ADV_CONN_DIR(&dir_addr);
		dir_param.id = current_bt_id;
		/* Use the fixed identity address instead of RPA so that central
		 * devices which fail to resolve RPAs (e.g. some Razer laptops)
		 * can still recognize and reconnect to this device.
		 */
		dir_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;

		err = bt_le_adv_start(&dir_param, NULL, 0, NULL, 0);
		if (err) {
			printk("Directed advertising failed to start (err %d), retrying shortly\n", err);
			k_work_reschedule(&dir_adv_start_retry_work, DIR_ADV_START_RETRY_DELAY);
			return;
		}

		bt_addr_le_to_str(&dir_addr, addr_str, sizeof(addr_str));
		printk("Directed advertising to %s started on bt_id %d\n",
		       addr_str, current_bt_id);

		k_work_reschedule(&directed_adv_timeout_work, DIR_ADV_FALLBACK_TIMEOUT);
		is_adv = true;
		return;
	}

	k_work_cancel_delayable(&directed_adv_timeout_work);
	k_work_cancel_delayable(&dir_adv_start_retry_work);
#endif

	if (is_adv) {
		return;
	}

	/* Check if there are bonded devices, for logging purposes only. */
	bond_count = 0;
	bt_foreach_bond(current_bt_id, bond_cb, &bond_count);

	if (bond_count > 0) {
		bt_addr_le_to_str(&bonds[0].addr, addr_str, sizeof(addr_str));
		printk("Found %d bonded device(s) for bt_id %d, first bonded: %s\n",
		       bond_count, current_bt_id, addr_str);
	}

	/* Use normal advertising for better compatibility and reliability */
	struct bt_le_adv_param normal_param = BT_LE_ADV_PARAM_INIT(
							BT_LE_ADV_OPT_CONN,
							BT_GAP_ADV_FAST_INT_MIN_1,
							BT_GAP_ADV_FAST_INT_MAX_1,
							NULL);
	normal_param.id = current_bt_id;
	normal_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;

	err = bt_le_adv_start(&normal_param, ad, ARRAY_SIZE(ad), sd,
				      ARRAY_SIZE(sd));
	if (err) {
		if (err == -EALREADY) {
			printk("Advertising continued\n");
		} else {
			printk("Advertising failed to start (err %d)\n", err);
		}

		return;
	}

	printk("Advertising started with bt_id %d (%s bonded devices)\n",
	       current_bt_id, bond_count > 0 ? "has" : "no");

	is_adv = true;
}

static void advertising_start(void)
{
#if CONFIG_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&dir_adv_bonds_queue);
	bt_foreach_bond(current_bt_id, dir_adv_bond_find, NULL);
	dir_adv_retry_count = 0;
#endif
	advertising_continue();
}

#if CONFIG_BT_DIRECTED_ADVERTISING
static void directed_adv_timeout_process(struct k_work *work)
{
	int err;

	if (is_adv) {
		err = bt_le_adv_stop();
		if (err) {
			printk("Directed advertising failed to stop (err %d)\n", err);
			return;
		}
		is_adv = false;
	}

	dir_adv_retry_count = DIR_ADV_MAX_RETRIES;
	printk("Directed advertising timed out, falling back to regular advertising\n");
	advertising_continue();
}

static void dir_adv_start_retry_process(struct k_work *work)
{
	advertising_continue();
}
#endif


static void create_bt_ids(void)
{
	int err;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = CONFIG_BT_ID_MAX;

	for (uint8_t i = 1; i <= NUM_BT_IDS; i++) {
		size_t id_count = 0xFF;

		/* Retrieve the number of currently configured identities. */
		bt_id_get(NULL, &id_count);

		/* Check if the identity has already been created. */
		if (i == id_count) {
			/* Creates a new identity, with a new random static address and random IRK. */
			err = bt_id_create(NULL, NULL);
			if (err < 0) {
				printk("Create id %d failed (%d)\n", i, err);
				return;
			}

			printk("New id created: %d\n", err);
		} else if (i < id_count) {
			printk("ID %d already exists (restored from settings)\n", i);
		}
	}

	/* Display address info for each identity */
	bt_id_get(addrs, &count);
	for (uint8_t i = 1; i <= NUM_BT_IDS && i < count; i++) {
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(&addrs[i], addr_str, sizeof(addr_str));
		printk("ID %d: %s (Each ID appears as independent device)\n", i, addr_str);
	}
}


static void disconnect_conn_cb(struct bt_conn *conn, void *user_data)
{
	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}


static void disconnect_all_connections(void)
{
	bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_conn_cb, NULL);
}


static void switch_bt_id_continue(void)
{
	current_bt_id = (current_bt_id % NUM_BT_IDS) + 1;
	printk("Switched to bt_id: %d\n", current_bt_id);

	// Restart advertising with new id
	advertising_start();
}


static void switch_bt_id(void)
{
	bool has_conn = false;

	// Stop advertising first
	if (is_adv) {
		bt_le_adv_stop();
		is_adv = false;
		printk("Advertising stopped for bt_id switch\n");
	}

#if CONFIG_BT_DIRECTED_ADVERTISING
	k_work_cancel_delayable(&directed_adv_timeout_work);
	k_work_cancel_delayable(&dir_adv_start_retry_work);
#endif

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn) {
			has_conn = true;
			break;
		}
	}

	if (!has_conn) {
		switch_bt_id_continue();
		return;
	}

	/* Defer the actual switch to disconnected(), once conn_mode[] is
	 * confirmed empty. Restarting advertising while the disconnect is
	 * still in flight would make the peer look "connected" and get it
	 * filtered out of the directed advertising candidate list.
	 */
	pending_id_switch = true;
	disconnect_all_connections();
	printk("Disconnecting... bt_id switch will continue once connections drop\n");
}


static void delete_bond_for_current_id(void)
{
	bond_count = 0;
	bt_foreach_bond(current_bt_id, bond_cb, &bond_count);

	if (bond_count == 0) {
		printk("No bonds for current bt_id %d\n", current_bt_id);
		return;
	}

	if (bond_delete_index >= bond_count) {
		bond_delete_index = 0;
	}

	bt_addr_le_t *addr = &bonds[bond_delete_index].addr;
	int err = bt_unpair(current_bt_id, addr);
	if (err) {
		printk("Failed to unpair device (err %d)\n", err);
	} else {
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
		printk("Unpaired device %s for bt_id %d\n", addr_str, current_bt_id);
	}

	bond_delete_index = (bond_delete_index + 1) % bond_count;
}


#if CONFIG_NFC_OOB_PAIRING
static void delayed_advertising_start(struct k_work *work)
{
	advertising_start();
}


void nfc_field_detected(void)
{
	dk_set_led_on(NFC_LED);

	for (int i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			k_work_submit(&adv_work);
			break;
		}
	}
}


void nfc_field_lost(void)
{
	dk_set_led_off(NFC_LED);
}
#endif









static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
#if CONFIG_BT_DIRECTED_ADVERTISING
		if (err == BT_HCI_ERR_ADV_TIMEOUT) {
			k_work_cancel_delayable(&directed_adv_timeout_work);
			k_work_cancel_delayable(&dir_adv_start_retry_work);
			is_adv = false;
			printk("Directed advertising to %s timed out\n", addr);
			advertising_continue();
			return;
		}
#endif
		printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));
		return;
	}

#if CONFIG_BT_DIRECTED_ADVERTISING
	k_work_cancel_delayable(&directed_adv_timeout_work);
	k_work_cancel_delayable(&dir_adv_start_retry_work);
#endif
	printk("Connected %s (bt_id: %d)\n", addr, current_bt_id);
	dk_set_led_on(CON_STATUS_LED);
	
	/* Update bond list for current bt_id */
	bond_count = 0;
	bt_foreach_bond(current_bt_id, bond_cb, &bond_count);
	
	/* Check if this is a bonded device by checking if address matches any bond */
	bool is_bonded = false;
	for (size_t i = 0; i < bond_count; i++) {
		if (bt_addr_le_cmp(bt_conn_get_dst(conn), &bonds[i].addr) == 0) {
			is_bonded = true;
			break;
		}
	}
	
	if (is_bonded) {
		printk("Reconnected to bonded device\n");
	} else {
		printk("New device connection, pairing may be required\n");
	}

	err = bt_hids_connected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about connection\n");
		return;
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			conn_mode[i].conn = conn;
			conn_mode[i].in_boot_mode = false;
			break;
		}
	}

#if CONFIG_NFC_OOB_PAIRING == 0
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			advertising_start();
			return;
		}
	}
#endif
	is_adv = false;
}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	bool is_any_dev_connected = false;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	err = bt_hids_disconnected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			conn_mode[i].conn = NULL;
		} else {
			if (conn_mode[i].conn) {
				is_any_dev_connected = true;
			}
		}
	}

	if (!is_any_dev_connected) {
		dk_set_led_off(CON_STATUS_LED);
	}

	if (pending_id_switch) {
		if (!is_any_dev_connected) {
			pending_id_switch = false;
			switch_bt_id_continue();
		}
		return;
	}

#if CONFIG_NFC_OOB_PAIRING
	if (is_adv) {
		printk("Advertising stopped after disconnect\n");
		bt_le_adv_stop();
		is_adv = false;
	}
#else
	/* Small delay before restarting advertising to ensure clean state */
	if (!is_adv) {
		printk("Restarting advertising in 100ms...\n");
		k_sleep(K_MSEC(100));
		advertising_start();
	}
#endif
}


static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err,
		       bt_security_err_to_str(err));
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};


static void caps_lock_handler(const struct bt_hids_rep *rep)
{
	uint8_t report_val = ((*rep->data) & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) ?
			  1 : 0;
	dk_set_led(LED_CAPS_LOCK, report_val);
}


static void hids_outp_rep_handler(struct bt_hids_rep *rep,
				  struct bt_conn *conn,
				  bool write)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}


static void hids_boot_kb_outp_rep_handler(struct bt_hids_rep *rep,
					  struct bt_conn *conn,
					  bool write)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Boot Keyboard Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}


static void hids_pm_evt_handler(enum bt_hids_pm_evt evt,
				struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	size_t i;

	for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			break;
		}
	}

	if (i >= CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		printk("Cannot find connection handle when processing PM");
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = false;
		break;

	default:
		break;
	}
}


static void hid_init(void)
{
	int err;
	struct bt_hids_init_param    hids_init_obj = { 0 };
	struct bt_hids_inp_rep       *hids_inp_rep;
	struct bt_hids_outp_feat_rep *hids_outp_rep;

	static const uint8_t report_map[] = {
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x06,       /* Usage (Keyboard) */
		0xA1, 0x01,       /* Collection (Application) */

		/* Keys */
#if INPUT_REP_KEYS_REF_ID
		0x85, INPUT_REP_KEYS_REF_ID,
#endif
		0x05, 0x07,       /* Usage Page (Key Codes) */
		0x19, 0xe0,       /* Usage Minimum (224) */
		0x29, 0xe7,       /* Usage Maximum (231) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x08,       /* Report Count (8) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */

		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x08,       /* Report Size (8) */
		0x81, 0x01,       /* Input (Constant) reserved byte(1) */

		0x95, 0x06,       /* Report Count (6) */
		0x75, 0x08,       /* Report Size (8) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x65,       /* Logical Maximum (101) */
		0x05, 0x07,       /* Usage Page (Key codes) */
		0x19, 0x00,       /* Usage Minimum (0) */
		0x29, 0x65,       /* Usage Maximum (101) */
		0x81, 0x00,       /* Input (Data, Array) Key array(6 bytes) */

		/* LED */
#if OUTPUT_REP_KEYS_REF_ID
		0x85, OUTPUT_REP_KEYS_REF_ID,
#endif
		0x95, 0x05,       /* Report Count (5) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, 0x08,       /* Usage Page (Page# for LEDs) */
		0x19, 0x01,       /* Usage Minimum (1) */
		0x29, 0x05,       /* Usage Maximum (5) */
		0x91, 0x02,       /* Output (Data, Variable, Absolute), */
				  /* Led report */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) */
		0x91, 0x01,       /* Output (Data, Variable, Absolute), */
				  /* Led report padding */

		0xC0              /* End Collection (Application) */
	};

	hids_init_obj.rep_map.data = report_map;
	hids_init_obj.rep_map.size = sizeof(report_map);

	hids_init_obj.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_obj.info.b_country_code = 0x00;
	hids_init_obj.info.flags = (BT_HIDS_REMOTE_WAKE |
				    BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep =
		&hids_init_obj.inp_rep_group_init.reports[INPUT_REP_KEYS_IDX];
	hids_inp_rep->size = INPUT_REPORT_KEYS_MAX_LEN;
	hids_inp_rep->id = INPUT_REP_KEYS_REF_ID;
	hids_init_obj.inp_rep_group_init.cnt++;

	hids_outp_rep =
		&hids_init_obj.outp_rep_group_init.reports[OUTPUT_REP_KEYS_IDX];
	hids_outp_rep->size = OUTPUT_REPORT_MAX_LEN;
	hids_outp_rep->id = OUTPUT_REP_KEYS_REF_ID;
	hids_outp_rep->handler = hids_outp_rep_handler;
	hids_init_obj.outp_rep_group_init.cnt++;

	hids_init_obj.is_kb = true;
	hids_init_obj.boot_kb_outp_rep_handler = hids_boot_kb_outp_rep_handler;
	hids_init_obj.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_obj);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

#if CONFIG_NFC_OOB_PAIRING
static void auth_oob_data_request(struct bt_conn *conn,
				  struct bt_conn_oob_info *info)
{
	int err;
	struct bt_le_oob *oob_local = app_nfc_oob_data_get();

	printk("LESC OOB data requested\n");

	if (info->type != BT_CONN_OOB_LE_SC) {
		printk("Only LESC pairing supported\n");
		return;
	}

	if (info->lesc.oob_config != BT_CONN_OOB_LOCAL_ONLY) {
		printk("LESC OOB config not supported\n");
		return;
	}

	/* Pass only local OOB data. */
	err = bt_le_oob_set_sc_data(conn, &oob_local->le_sc_data, NULL);
	if (err) {
		printk("Error while setting OOB data: %d\n", err);
	} else {
		printk("Successfully provided LESC OOB data\n");
	}
}
#endif


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct pairing_data_mitm pairing_data;

	if (k_msgq_peek(&mitm_queue, &pairing_data) != 0) {
		return;
	}

	if (pairing_data.conn == conn) {
		bt_conn_unref(pairing_data.conn);
		k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT);
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d %s\n", addr, reason,
	       bt_security_err_to_str(reason));
}

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};


/** @brief Function process keyboard state and sends it
 *
 *  @param pstate     The state to be sent
 *  @param boot_mode  Information if boot mode protocol is selected.
 *  @param conn       Connection handler
 *
 *  @return 0 on success or negative error code.
 */
static int key_report_con_send(const struct keyboard_state *state,
			bool boot_mode,
			struct bt_conn *conn)
{
	int err = 0;
	uint8_t  data[INPUT_REPORT_KEYS_MAX_LEN];
	uint8_t *key_data;
	const uint8_t *key_state;
	size_t n;

	data[0] = state->ctrl_keys_state;
	data[1] = 0;
	key_data = &data[2];
	key_state = state->keys_state;

	for (n = 0; n < KEY_PRESS_MAX; ++n) {
		*key_data++ = *key_state++;
	}
	if (boot_mode) {
		err = bt_hids_boot_kb_inp_rep_send(&hids_obj, conn, data,
							sizeof(data), NULL);
	} else {
		err = bt_hids_inp_rep_send(&hids_obj, conn,
						INPUT_REP_KEYS_IDX, data,
						sizeof(data), NULL);
	}
	return err;
}

/** @brief Function process and send keyboard state to all active connections
 *
 * Function process global keyboard state and send it to all connected
 * clients.
 *
 * @return 0 on success or negative error code.
 */
static int key_report_send(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn) {
			int err;

			err = key_report_con_send(&hid_keyboard_state,
						  conn_mode[i].in_boot_mode,
						  conn_mode[i].conn);
			if (err) {
				printk("Key report send error: %d\n", err);
				return err;
			}
		}
	}
	return 0;
}

/** @brief Change key code to ctrl code mask
 *
 *  Function changes the key code to the mask in the control code
 *  field inside the raport.
 *  Returns 0 if key code is not a control key.
 *
 *  @param key Key code
 *
 *  @return Mask of the control key or 0.
 */
static uint8_t button_ctrl_code(uint8_t key)
{
	if (KEY_CTRL_CODE_MIN <= key && key <= KEY_CTRL_CODE_MAX) {
		return (uint8_t)(1U << (key - KEY_CTRL_CODE_MIN));
	}
	return 0;
}


static int hid_kbd_state_key_set(uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code(key);

	if (ctrl_mask) {
		hid_keyboard_state.ctrl_keys_state |= ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < KEY_PRESS_MAX; ++i) {
		if (hid_keyboard_state.keys_state[i] == 0) {
			hid_keyboard_state.keys_state[i] = key;
			return 0;
		}
	}
	/* All slots busy */
	return -EBUSY;
}


static int hid_kbd_state_key_clear(uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code(key);

	if (ctrl_mask) {
		hid_keyboard_state.ctrl_keys_state &= ~ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < KEY_PRESS_MAX; ++i) {
		if (hid_keyboard_state.keys_state[i] == key) {
			hid_keyboard_state.keys_state[i] = 0;
			return 0;
		}
	}
	/* Key not found */
	return -EINVAL;
}

/** @brief Press a button and send report
 *
 *  @note Functions to manipulate hid state are not reentrant
 *  @param keys
 *  @param cnt
 *
 *  @return 0 on success or negative error code.
 */
static int hid_buttons_press(const uint8_t *keys, size_t cnt)
{
	while (cnt--) {
		int err;

		err = hid_kbd_state_key_set(*keys++);
		if (err) {
			printk("Cannot set selected key.\n");
			return err;
		}
	}

	return key_report_send();
}

/** @brief Release the button and send report
 *
 *  @note Functions to manipulate hid state are not reentrant
 *  @param keys
 *  @param cnt
 *
 *  @return 0 on success or negative error code.
 */
static int hid_buttons_release(const uint8_t *keys, size_t cnt)
{
	while (cnt--) {
		int err;

		err = hid_kbd_state_key_clear(*keys++);
		if (err) {
			printk("Cannot clear selected key.\n");
			return err;
		}
	}

	return key_report_send();
}


static void button_text_changed(bool down)
{
	static const uint8_t *chr = hello_world_str;

	if (down) {
		hid_buttons_press(chr, 1);
	} else {
		hid_buttons_release(chr, 1);
		if (++chr == (hello_world_str + sizeof(hello_world_str))) {
			chr = hello_world_str;
		}
	}
}


static void button_shift_changed(bool down)
{
	if (down) {
		hid_buttons_press(shift_key, 1);
	} else {
		hid_buttons_release(shift_key, 1);
	}
}


static void num_comp_reply(bool accept)
{
	struct pairing_data_mitm pairing_data;
	struct bt_conn *conn;

	if (k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT) != 0) {
		return;
	}

	conn = pairing_data.conn;

	if (accept) {
		bt_conn_auth_passkey_confirm(conn);
		printk("Numeric Match, conn %p\n", conn);
	} else {
		bt_conn_auth_cancel(conn);
		printk("Numeric Reject, conn %p\n", conn);
	}

	bt_conn_unref(pairing_data.conn);

	if (k_msgq_num_used_get(&mitm_queue)) {
		k_work_submit(&pairing_work);
	}
}


static void delete_all_bonds(void)
{
	int err;

	err = bt_unpair(BT_ADDR_LE_PUBLIC, NULL);
	if (err) {
		printk("Failed to unpair public addresses: %d\n", err);
	}

	err = bt_unpair(BT_ADDR_LE_RANDOM, NULL);
	if (err) {
		printk("Failed to unpair random addresses: %d\n", err);
	}

	printk("All bonds deleted\n");
}


static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	static bool pairing_button_pressed;

	uint32_t buttons = button_state & has_changed;

	if (k_msgq_num_used_get(&mitm_queue)) {
		if (buttons & KEY_PAIRING_ACCEPT) {
			pairing_button_pressed = true;
			num_comp_reply(true);

			return;
		}

		if (buttons & KEY_PAIRING_REJECT) {
			pairing_button_pressed = true;
			num_comp_reply(false);

			return;
		}
	}

	/* Do not take any action if the pairing button is released. */
	if (pairing_button_pressed &&
	    (has_changed & (KEY_PAIRING_ACCEPT | KEY_PAIRING_REJECT))) {
		pairing_button_pressed = false;

		return;
	}

	if (has_changed & KEY_TEXT_MASK) {
		button_text_changed((button_state & KEY_TEXT_MASK) != 0);
	}
	if (has_changed & KEY_SHIFT_MASK) {
		button_shift_changed((button_state & KEY_SHIFT_MASK) != 0);
	}
	if (has_changed & KEY_SWITCH_ID_MASK) {
		if (button_state & KEY_SWITCH_ID_MASK) {
			switch_bt_id();
		}
	}
	if (has_changed & KEY_DELETE_BOND_MASK) {
		if (button_state & KEY_DELETE_BOND_MASK) {
			delete_bond_for_current_id();
		}
	}
#if CONFIG_NFC_OOB_PAIRING
	if (has_changed & KEY_ADV_MASK) {
		size_t i;

		for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
			if (!conn_mode[i].conn) {
				advertising_start();
				return;
			}
		}

		printk("Cannot start advertising, all connections slots are"
		       " taken\n");
	}
#endif
}


static void configure_gpio(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	err = dk_leds_init();
	if (err) {
		printk("Cannot init LEDs (err: %d)\n", err);
	}
}


static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}


int main(void)
{
	int err;
	int blink_status = 0;

	printk("Starting Bluetooth Peripheral HIDS keyboard sample\n");

	configure_gpio();

	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err) {
		printk("Failed to register authorization info callbacks.\n");
		return 0;
	}

	hid_init();

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	/* Load settings first to restore saved identities */
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		printk("Loading settings...\n");
		settings_load();
	}

	/* Create IDs after loading settings to avoid overwriting saved addresses */
	create_bt_ids();

#if CONFIG_BT_DIRECTED_ADVERTISING
	k_work_init_delayable(&directed_adv_timeout_work, directed_adv_timeout_process);
	k_work_init_delayable(&dir_adv_start_retry_work, dir_adv_start_retry_process);
#endif

#if CONFIG_NFC_OOB_PAIRING
	k_work_init(&adv_work, delayed_advertising_start);
	app_nfc_init();
#else
	advertising_start();
#endif


	for (;;) {
		static int status_counter = 0;
		
		if (is_adv) {
			dk_set_led(ADV_STATUS_LED, (++blink_status) % 2);
		} else {
			dk_set_led_off(ADV_STATUS_LED);
		}
		
		/* Print connection status every 10 seconds */
		if (++status_counter >= 10) {
			status_counter = 0;
			int connected_count = 0;
			for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
				if (conn_mode[i].conn) {
					connected_count++;
				}
			}
			if (is_adv && connected_count == 0) {
				printk("Status: Advertising (bt_id: %d, %d bonded), waiting for connection...\n", 
				       current_bt_id, bond_count);
			}
		}
		
		k_sleep(K_MSEC(ADV_LED_BLINK_INTERVAL));
		/* Battery level simulation */
		bas_notify();
	}
}
