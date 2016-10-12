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
#include <unistd.h>
#include <fcntl.h>

#include <onlplib/file.h>
#include <onlp/platformi/sysi.h>
#include <onlp/platformi/ledi.h>
#include <onlp/platformi/thermali.h>
#include <onlp/platformi/fani.h>
#include <onlp/platformi/psui.h>

#include "x86_64_mlnx_x86_r5_0_1404_int.h"
#include "x86_64_mlnx_x86_r5_0_1404_log.h"

#include "platform_lib.h"

#define NUM_OF_THERMAL_ON_MAIN_BROAD  CHASSIS_THERMAL_COUNT
#define NUM_OF_FAN_ON_MAIN_BROAD      CHASSIS_FAN_COUNT
#define NUM_OF_PSU_ON_MAIN_BROAD      2
#define NUM_OF_LED_ON_MAIN_BROAD      6

/* MODIFY */
#define PREFIX_PATH_ON_CPLD_DEV          "/bsp/cpld"
#define NUM_OF_CPLD                      3
static char arr_cplddev_name[NUM_OF_CPLD][30] =
{
    "cpld_brd_version",
    "cpld_mgmt_version",
    "cpld_port_version"
};

const char*
onlp_sysi_platform_get(void)
{
    return "x86-64-mlnx-x86-r5-0-1404";
}

int
onlp_sysi_onie_data_get(uint8_t** data, int* size)
{
    uint8_t* rdata = aim_zmalloc(256);
    if(onlp_file_read(rdata, 256, size, IDPROM_PATH) == ONLP_STATUS_OK) {
        if(*size == 256) {
            *data = rdata;
            return ONLP_STATUS_OK;
        }
    }

    aim_free(rdata);
    *size = 0;
    return ONLP_STATUS_E_INTERNAL;
}

int
onlp_sysi_platform_info_get(onlp_platform_info_t* pi)
{
    int   i, v[NUM_OF_CPLD]={0};

    for (i=0; i < NUM_OF_CPLD; i++) {
        v[i] = 0;
        if(onlp_file_read_int(v+i, "%s/%s", PREFIX_PATH_ON_CPLD_DEV, arr_cplddev_name[i]) < 0) {
            return ONLP_STATUS_E_INTERNAL;
        }
    }
    pi->cpld_versions = aim_fstrdup("%d.%d.%d", v[0], v[1], v[2]);

    return ONLP_STATUS_OK;
}

void
onlp_sysi_platform_info_free(onlp_platform_info_t* pi)
{
    aim_free(pi->cpld_versions);
}


int
onlp_sysi_oids_get(onlp_oid_t* table, int max)
{
    int i;
    onlp_oid_t* e = table;
    memset(table, 0, max*sizeof(onlp_oid_t));

    /* 8 Thermal sensors on the chassis */
    for (i = 1; i <= NUM_OF_THERMAL_ON_MAIN_BROAD; i++)
    {
        *e++ = ONLP_THERMAL_ID_CREATE(i);
    }

    /* 6 LEDs on the chassis */
    for (i = 1; i <= NUM_OF_LED_ON_MAIN_BROAD; i++)
    {
        *e++ = ONLP_LED_ID_CREATE(i);
    }

    /* 2 PSUs on the chassis */
    for (i = 1; i <= NUM_OF_PSU_ON_MAIN_BROAD; i++)
    {
        *e++ = ONLP_PSU_ID_CREATE(i);
    }

    /* 8 Fans and 2 PSU fans on the chassis */
    for (i = 1; i <= NUM_OF_FAN_ON_MAIN_BROAD; i++)
    {
        *e++ = ONLP_FAN_ID_CREATE(i);
    }

    return 0;
}

static int
_onlp_sysi_grep_output(char value[256], const char *attr, const char *tmp_file)
{
    int value_offset = 30;
    char buffer[256]  = {0};
    char command[256] = {0};
    int v = 0;
    FILE *fp = NULL;

    /* Open the command for reading. */
    snprintf(command, sizeof(command), "cat '%s' | grep '%s'", tmp_file, attr);
    fp = popen(command, "r");
    if (NULL == fp) {
        DEBUG_PRINT("[Debug][%s][%d]Failed to run command '%s'\n",
                    __FUNCTION__, __LINE__, command);
        return ONLP_STATUS_E_INTERNAL;
    }

    /* Read the output */
    if (fgets(buffer, sizeof(buffer)-1, fp) == NULL) {
        DEBUG_PRINT("[Debug][%s][%d]Failed to read output of command '%s'\n",
                    __FUNCTION__, __LINE__, command);
        pclose(fp);
        return ONLP_STATUS_E_INTERNAL;
    }
    /* close */
    pclose(fp);

    /* Reading value from buffer with command output */
    while (buffer[value_offset] != '\n' &&
           buffer[value_offset] != '\r' &&
           buffer[value_offset] != '\0') {
        value[v] = buffer[value_offset];
        v++;
        value_offset++;
    }
    value[v] = '\0';

    DEBUG_PRINT("[Debug][%s][%d]Value for sytem attribute '%s' is '%s' \n",
                __FUNCTION__, __LINE__, attr, value);

    return ONLP_STATUS_OK;
}

int
onlp_sysi_onie_info_get(onlp_onie_info_t* onie)
{
    const char tmp_file[] = "/tmp/onie-shell.log";
    const char onie_command[] = "onie-shell -c onie-syseeprom > /tmp/onie-shell.log";
    char value[256] = {0};
    int rc = 0;

    /* We must initialize this otherwise crash occurs while free memory */
    list_init(&onie->vx_list);

    system(onie_command);
    rc = _onlp_sysi_grep_output(value, "Product Name", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->product_name = aim_strdup(value);
    rc = _onlp_sysi_grep_output(value, "Part Number", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->part_number = aim_strdup(value);
    rc = _onlp_sysi_grep_output(value, "Serial Number", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->serial_number = aim_strdup(value);
    rc = _onlp_sysi_grep_output(value, "Base MAC Address", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    strncpy((char*)onie->mac, value, sizeof(onie->mac));
    rc = _onlp_sysi_grep_output(value, "Manufacture Date", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->manufacture_date = aim_strdup(value);
    rc = _onlp_sysi_grep_output(value, "Device Version", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->device_version = atoi(value);
    rc = _onlp_sysi_grep_output(value, "Manufacturer", tmp_file);
    if (ONLP_STATUS_OK != rc) {
        return rc;
    }
    onie->manufacturer = aim_strdup(value);

    return ONLP_STATUS_OK;
}

