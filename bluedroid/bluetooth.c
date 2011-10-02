/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * NOTE: This file has been modified by Sony Ericsson Mobile Communications AB.
 * Modifications are licensed under the License.
 */

#define LOG_TAG "bluedroid"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <bluedroid/bluetooth.h>

#ifndef HCI_DEV_ID
#define HCI_DEV_ID 0
#endif

#define HCID_START_DELAY_SEC   3
#define HCID_STOP_DELAY_USEC 500000
#define HCIA_START_ATTEMPTS 300		// 15 sec per attempt

#define MIN(x,y) (((x)<(y))?(x):(y))


static int rfkill_id = -1;
static char *rfkill_state_path = NULL;


static int init_rfkill() {
    char path[64];
    char buf[16];
    int fd;
    int sz;
    int id;
    for (id = 0; ; id++) {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            LOGW("open(%s) failed: %s (%d)\n", path, strerror(errno), errno);
            return -1;
        }
        sz = read(fd, &buf, sizeof(buf));
        close(fd);
        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0) {
            rfkill_id = id;
            break;
        }
    }

    asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkill_id);
    return 0;
}

static int check_bluetooth_power() {
    int sz;
    int fd = -1;
    int ret = -1;
    char buffer;

    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_RDONLY);
    if (fd < 0) {
        LOGE("open(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    sz = read(fd, &buffer, 1);
    if (sz != 1) {
        LOGE("read(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }

    switch (buffer) {
    case '1':
        ret = 1;
        break;
    case '0':
        ret = 0;
        break;
    }

out:
    if (fd >= 0) close(fd);
    return ret;
}

static int set_bluetooth_power(int on) {
    int sz;
    int fd = -1;
    int ret = -1;
    const char buffer = (on ? '1' : '0');

    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_WRONLY);
    if (fd < 0) {
        LOGE("open(%s) for write failed: %s (%d)", rfkill_state_path,
             strerror(errno), errno);
        goto out;
    }
    sz = write(fd, &buffer, 1);
    if (sz < 0) {
        LOGE("write(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    ret = 0;

out:
    if (fd >= 0) close(fd);
    return ret;
}

static inline int create_hci_sock() {
    int sk = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (sk < 0) {
        LOGE("Failed to create bluetooth hci socket: %s (%d)",
             strerror(errno), errno);
    }
    return sk;
}


static int ll_chip_enable(int reenable) {
    LOGV(__FUNCTION__);
    int ret = -1;

    if (reenable) {
        LOGI("Stopping hciattach daemon");
        if (property_set("ctl.stop", "hciattach") < 0) {
            LOGE("Failed to stop hciattach");
            goto out;
        }
        usleep(1000*1000);
        if (set_bluetooth_power(0) < 0) {
            LOGE("Failed to turn off bluetooth power");
            goto out;
        }
        usleep(10000);
    }
    if (set_bluetooth_power(1) < 0) {
        LOGE("Failed to turn on bluetooth power");
        goto out;
    }
    usleep(10000);
    LOGI("Starting hciattach daemon");
    if (property_set("ctl.start", "hciattach") < 0) {
        LOGE("Failed to start hciattach");
        goto out;
    }
    ret = 0;

out:
    return ret;
}

static int do_chip_enable()
{
    LOGV(__FUNCTION__);

    int ret = -1;
    int hci_sock = -1;
    int attempt;

    if (ll_chip_enable(0) < 0)
        goto out;

    // Wait for the HCI socket to be up; this can only succeed once hciattach
    // has sent the FW and then turned on hci device via HCIUARTSETPROTO ioctl
    for (attempt = HCIA_START_ATTEMPTS; attempt > 0;  attempt--) {
        hci_sock = create_hci_sock();
        if (hci_sock < 0) goto out;

        if (!ioctl(hci_sock, HCIDEVUP, HCI_DEV_ID)) {
            break;
        }
        close(hci_sock);
        usleep(100000);  // 100 ms retry delay
        if (!((attempt-1) % (HCIA_START_ATTEMPTS / 2)))
            if (ll_chip_enable(1) < 0)
                goto out;
    }
    if (attempt == 0) {
        LOGE("%s: Timeout waiting for HCI device to come up", __FUNCTION__);
        goto out;
    }

    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

static int do_btd_enable()
{
    LOGV(__FUNCTION__);

    int ret = -1;

    LOGI("Starting bluetoothd deamon");
    if (property_set("ctl.start", "bluetoothd") < 0) {
        LOGE("Failed to start bluetoothd");
        goto out;
    }
    sleep(HCID_START_DELAY_SEC);

    ret = 0;

out:
    return ret;
}

static int do_chip_disable()
{
    LOGV(__FUNCTION__);

    int ret = -1;
    int hci_sock = -1;

    LOGI("Stopping hciattach deamon");
    hci_sock = create_hci_sock();
    if (hci_sock < 0) goto out;
    ioctl(hci_sock, HCIDEVDOWN, HCI_DEV_ID);

     LOGI("Stopping hciattach deamon");
    if (property_set("ctl.stop", "hciattach") < 0) {
        LOGE("Error stopping hciattach");
        goto out;
    }

    if (set_bluetooth_power(0) < 0) {
        goto out;
    }

    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

static int do_btd_disable()
{
    LOGV(__FUNCTION__);

    int ret = -1;

    LOGI("Stopping bluetoothd deamon");
    if (property_set("ctl.stop", "bluetoothd") < 0) {
        LOGE("Error stopping bluetoothd");
        goto out;
    }
    usleep(HCID_STOP_DELAY_USEC);

    ret = 0;

out:
    return ret;
}

static pthread_mutex_t chip_users_mutex = PTHREAD_MUTEX_INITIALIZER;
static int chip_users = 0;

int bt_chip_enable() {

    int ret = 0;

    pthread_mutex_lock(&chip_users_mutex);

    if (chip_users != 0 || (ret = do_chip_enable()) == 0)
        chip_users++;

    pthread_mutex_unlock(&chip_users_mutex);

    return ret;
}

int bt_chip_disable() {

    int ret = 0;

    pthread_mutex_lock(&chip_users_mutex);

    if (chip_users == 0) {
        LOGE("%s: trying to call before enabling\n", __func__);
        ret = -1;
        goto out;
    }

    if (chip_users != 1 || (ret = do_chip_disable()) == 0)
        chip_users--;

out:
    pthread_mutex_unlock(&chip_users_mutex);
    return ret;
}

int bt_enable() {
    LOGV(__FUNCTION__);

    int ret = bt_chip_enable();

    if (ret)
        goto out;

    if ((ret = do_btd_enable()) != 0)
        bt_chip_disable();

out:
    return ret;
}

int bt_disable() {
    LOGV(__FUNCTION__);

    int ret;

    do_btd_disable();

    /* even if do_btd_disable fails, we'll try to disable the chip anyway */
    ret = bt_chip_disable();
out:
    return ret;
}

int bt_is_enabled() {
    LOGV(__FUNCTION__);

    int hci_sock = -1;
    int ret = -1;
    struct hci_dev_info dev_info;


    // Check power first
    ret = check_bluetooth_power();
    if (ret == -1 || ret == 0) goto out;

    ret = -1;

    // Power is on, now check if the HCI interface is up
    hci_sock = create_hci_sock();
    if (hci_sock < 0) goto out;

    dev_info.dev_id = HCI_DEV_ID;
    if (ioctl(hci_sock, HCIGETDEVINFO, (void *)&dev_info) < 0) {
        ret = 0;
        goto out;
    }

    ret = hci_test_bit(HCI_UP, &dev_info.flags);

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int ba2str(const bdaddr_t *ba, char *str) {
    return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);
}

int str2ba(const char *str, bdaddr_t *ba) {
    int i;
    for (i = 5; i >= 0; i--) {
        ba->b[i] = (uint8_t) strtoul(str, &str, 16);
        str++;
    }
    return 0;
}
