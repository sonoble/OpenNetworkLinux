/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/psui.h>
#include <onlplib/mmap.h>
#include <onlplib/file.h>
#include <stdio.h>
#include <string.h>
#include "platform_lib.h"

#define PSU_STATUS_PRESENT    1
#define PSU_STATUS_POWER_GOOD 1

#define PSU_NODE_MAX_INT_LEN  8
#define PSU_NODE_MAX_PATH_LEN 64

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_PSU(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

static int
psu_status_info_get(int id, char *node, int *value)
{
    int ret = 0;
    char buf[PSU_NODE_MAX_INT_LEN + 1] = {0};
    char node_path[PSU_NODE_MAX_PATH_LEN] = {0};

    *value = 0;

    sprintf(node_path, PSU_AC_HWMON_PREFIX, id, node);
    ret = deviceNodeReadString(node_path, buf, sizeof(buf), 0);
    if (ret == 0) {
        *value = atoi(buf);
    }

    return ret;
}

static int
psu_ym2651y_pmbus_info_get(int id, char *node, int *value)
{
    int  ret = 0;
    char buf[PSU_NODE_MAX_INT_LEN + 1]    = {0};
    char node_path[PSU_NODE_MAX_PATH_LEN] = {0};

    *value = 0;

    sprintf(node_path, PSU_AC_PMBUS_PREFIX, id, node);
    ret = deviceNodeReadString(node_path, buf, sizeof(buf), 0);
    if (ret == 0) {
        *value = atoi(buf);
    }

    return ret;
}

int
onlp_psui_init(void)
{
    return ONLP_STATUS_OK;
}

static int
_psu_read_eeprom(int psu_index, onlp_psu_info_t* info)
{
    char path[PSU_NODE_MAX_PATH_LEN] = {0};
    const char sanity_check[]   = "MLNX";
    const uint8_t serial_offset = 4;
    const uint8_t serial_len    = 24;
    const uint8_t part_offset   = 28;
    const uint8_t part_len      = 20;
    uint8_t data[256] = {0};
    int rv  = 0;
    int len = 0;

    snprintf(path, sizeof(path), IDPROM_PATH, "psu", psu_index);
    rv = onlp_file_read(data, sizeof(data), &len, path);
    if (rv < 0) {
        return ONLP_STATUS_E_INTERNAL;
    }

    /* Sanity checker */
    if (strcmp(sanity_check, (char*)&data[0])) {
        return ONLP_STATUS_E_INVALID;
    }

    /* Serial number */
    strncpy(info->serial, (char *)&data[serial_offset], serial_len);
    info->serial[serial_len + 1] = '\0';

    /* Part number */
    strncpy(info->model, (char *)&data[part_offset], part_len);
    info->serial[part_len + 1] = '\0';

    return ONLP_STATUS_OK;
}

static int
psu_ym2651y_info_get(onlp_psu_info_t* info)
{
    int val   = 0;
    int index = ONLP_OID_ID_GET(info->hdr.id);

    /* Set capability
     */
    info->caps = ONLP_PSU_CAPS_AC;

    if (info->status & ONLP_PSU_STATUS_FAILED) {
        return ONLP_STATUS_OK;
    }

    /* Set the associated oid_table */
    info->hdr.coids[0] = ONLP_FAN_ID_CREATE(index + CHASSIS_FAN_COUNT);
    info->hdr.coids[1] = ONLP_THERMAL_ID_CREATE(index + CHASSIS_THERMAL_COUNT);

    /* Read voltage, current and power */
    if (psu_ym2651y_pmbus_info_get(index, "12_aux", &val) == 0) {
        info->mvout = val;
        info->caps |= ONLP_PSU_CAPS_VIN;
    }

    if (PSU2_ID == index) {
        if (psu_ym2651y_pmbus_info_get(index, "curr", &val) == 0) {
            info->miout = val;
            info->caps |= ONLP_PSU_CAPS_IOUT;
        }

        if (psu_ym2651y_pmbus_info_get(index, "in", &val) == 0) {
            info->miout = val;
            info->caps |= ONLP_PSU_CAPS_IIN;
        }

        if (psu_ym2651y_pmbus_info_get(index, "power", &val) == 0) {
            info->mpout = val;
            info->caps |= ONLP_PSU_CAPS_POUT;
        }

        if (psu_ym2651y_pmbus_info_get(index, "power_in", &val) == 0) {
            info->mpout = val;
            info->caps |= ONLP_PSU_CAPS_PIN;
        }
    }

    return _psu_read_eeprom(index, info);
}

/*
 * Get all information about the given PSU oid.
 */
static onlp_psu_info_t pinfo[] =
{
    { }, /* Not used */
    {
        { ONLP_PSU_ID_CREATE(PSU1_ID), "PSU-1", 0 },
    },
    {
        { ONLP_PSU_ID_CREATE(PSU2_ID), "PSU-2", 0 },
    }
};

int
onlp_psui_info_get(onlp_oid_t id, onlp_psu_info_t* info)
{
    int val   = 0;
    int ret   = ONLP_STATUS_OK;
    int index = ONLP_OID_ID_GET(id);

    VALIDATE(id);

    memset(info, 0, sizeof(onlp_psu_info_t));
    *info = pinfo[index]; /* Set the onlp_oid_hdr_t */

    /* Get the present state */
    if (psu_status_info_get(index, "status", &val) != 0) {
        printf("Unable to read PSU(%d) node(psu_present)\r\n", index);
    }

    if (val != PSU_STATUS_PRESENT) {
        info->status &= ~ONLP_PSU_STATUS_PRESENT;
        return ONLP_STATUS_OK;
    }
    info->status |= ONLP_PSU_STATUS_PRESENT;

    ret = psu_ym2651y_info_get(info);

    return ret;
}

int
onlp_psui_ioctl(onlp_oid_t pid, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

