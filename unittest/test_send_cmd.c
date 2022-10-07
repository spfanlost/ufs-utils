/**
 * @file test_send_cmd.c
 * @author yumeng (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2019-10-08
 *
 * @copyright Copyright (c) 2019
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>

#include "../ufs_cmds.h"
#include "../ufs.h"
#include "../options.h"
#include "../ioctl.h"

#include "common.h"
#include "test.h"
#include "test_send_scsi.h"
#include "test_send_uic.h"
#include "test_send_cmd.h"

extern int do_read_desc(int fd, struct ufs_bsg_request *bsg_req,
                        struct ufs_bsg_reply *bsg_rsp, __u8 idn, __u8 index,
                        __u16 desc_buf_len, __u8 *data_buf);
extern int do_query_rq(int fd, struct ufs_bsg_request *bsg_req,
                       struct ufs_bsg_reply *bsg_rsp, __u8 query_req_func,
                       __u8 opcode, __u8 idn, __u8 index, __u8 sel,
                       __u16 req_buf_len, __u16 res_buf_len, __u8 *data_buf);
extern void print_descriptors(char *desc_str, __u8 *desc_buf,
                              struct desc_field_offset *desc_array, int arr_size);

#define CONFIG_HEADER_OFFSET_EXT 0x16
#define CONFIG_LUN_OFFSET_EXT 0x1A

#define UPIU_HEADER_DWORD_EXT(byte3, byte2, byte1, byte0) \
    htobe32((byte3 << 24) | (byte2 << 16) |               \
            (byte1 << 8) | (byte0))

#define ARRAY_SIZE_EXT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ATTR_RSRV_EXT() "Reserved", BYTE, ACC_INVALID, MODE_INVALID, LEVEL_INVALID

struct desc_field_offset device_desc_field_name_ext[] = {
    {"bLength", 0x00, BYTE},
    {"bDescriptorType", 0x01, BYTE},
    {"bDevice", 0x02, BYTE},
    {"bDeviceClass", 0x03, BYTE},
    {"bDeviceSubClass", 0x04, BYTE},
    {"bProtocol", 0x05, BYTE},
    {"bNumberLU", 0x06, BYTE},
    {"bNumberWLU", 0x07, BYTE},
    {"bBootEnable", 0x08, BYTE},
    {"bDescrAccessEn", 0x09, BYTE},
    {"bInitPowerMode", 0x0A, BYTE},
    {"bHighPriorityLUN", 0x0B, BYTE},
    {"bSecureRemovalType", 0x0C, BYTE},
    {"bSecurityLU", 0x0D, BYTE},
    {"bBackgroundOpsTermLat", 0x0E, BYTE},
    {"bInitActiveICCLevel", 0x0F, BYTE},
    {"wSpecVersion", 0x10, WORD},
    {"wManufactureDate", 0x12, WORD},
    {"iManufactureName", 0x14, BYTE},
    {"iProductName", 0x15, BYTE},
    {"iSerialNumber", 0x16, BYTE},
    {"iOemID", 0x17, BYTE},
    {"ManufacturerID", 0x18, WORD},
    {"bUD0BaseOffset", 0x1A, BYTE},
    {"bUDConfigPLength", 0x1B, BYTE},
    {"bDeviceRTTCap", 0x1C, BYTE},
    {"wPeriodicRTCUpdate", 0x1D, WORD},
    {"bUFSFeaturesSupport", 0x1F, BYTE},
    {"bFFUTimeout", 0x20, BYTE},
    {"bQueueDepth", 0x21, BYTE},
    {"wDeviceVersion", 0x22, WORD},
    {"bNumSecureWPArea", 0x24, BYTE},
    {"dPSAMaxDataSize", 0x25, DWORD},
    {"bPSAStateTimeout", 0x29, BYTE},
    {"iProductRevisionLevel", 0x2A, BYTE},
    {"Reserved1", 0x2B, BYTE},
    {"Reserved2", 0x2C, DWORD},
    {"Reserved3", 0x30, DWORD},
    {"Reserved4", 0x34, DWORD},
    {"Reserved5", 0x38, DWORD},
    {"Reserved6", 0x3c, DWORD},
    {"wHPBVersion", 0x40, WORD},
    {"bHPBControl", 0x42, BYTE},
    {"Reserved8", 0x43, DWORD},
    {"Reserved9", 0x47, DDWORD},
    {"dExtendedUFSFeaturesSupport", 0x4F, DWORD},
    {"bWriteBoosterBufferPreserveUserSpaceEn", 0x53, BYTE},
    {"bWriteBoosterBufferType", 0x54, BYTE},
    {"dNumSharedWriteBoosterBufferAllocUnits", 0x55, DWORD}};

struct desc_field_offset device_config_unit_desc_field_name_ext[] = {
    {"bLUEnable", 0x00, BYTE},
    {"bBootLunID", 0x01, BYTE},
    {"bLUWriteProtect", 0x02, BYTE},
    {"bMemoryType", 0x03, BYTE},
    {"dNumAllocUnits", 0x04, DWORD},
    {"bDataReliability", 0x08, BYTE},
    {"bLogicalBlockSize", 0x09, BYTE},
    {"bProvisioningType", 0x0A, BYTE},
    {"wContextCapabilities", 0x0B, WORD},
    {"wLUMaxActiveHPBRegions", 0x10, WORD},
    {"wHPBPinnedRegionStartIdx", 0x12, WORD},
    {"wNumHPBPinnedRegions", 0x14, WORD},
    {"dLUNumWriteBoosterBufferAllocUnits", 0x16, DWORD}};

struct desc_field_offset device_config_desc_field_name_ext[] = {
    {"bLength", 0x00, BYTE},
    {"bDescriptorType", 0x01, BYTE},
    {"bConfDescContinue", 0x02, BYTE},
    {"bBootEnable", 0x03, BYTE},
    {"bDescrAccessEn", 0x04, BYTE},
    {"bInitPowerMode", 0x05, BYTE},
    {"bHighPriorityLUN", 0x06, BYTE},
    {"bSecureRemovalType", 0x07, BYTE},
    {"bInitActiveICCLevel", 0x08, BYTE},
    {"wPeriodicRTCUpdate", 0x09, WORD},
    {"bHPBControl", 0x0B, BYTE},
    {"bRPMBRegionEnable", 0x0C, BYTE},
    {"bRPMBRegion1Size", 0x0D, BYTE},
    {"bRPMBRegion2Size", 0x0E, BYTE},
    {"bRPMBRegion3Size", 0x0F, BYTE},
    {"bWriteBoosterBufferPreserveUserSpaceEn", 0x10, BYTE},
    {"bWriteBoosterBufferType", 0x11, BYTE},
    {"dNumSharedWriteBoosterBufferAllocUnits", 0x12, DWORD}};

struct attr_fields ufs_attrs_ext[] = {
    {"bBootLunEn", BYTE, (URD | UWRT), (READ_ONLY | WRITE_PRSIST), DEV},
    {"bMAX_DATA_SIZE_FOR_HPB_SINGLE_CMD", BYTE, URD, READ_ONLY, DEV},
    {"bCurrentPowerMode", BYTE, URD, READ_ONLY, DEV},
    {"bActiveICCLevel", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bOutOfOrderDataEn", BYTE, (URD | UWRT), (READ_NRML | WRITE_ONCE), DEV},
    {"bBackgroundOpStatus", BYTE, URD, READ_ONLY, DEV},
    {"bPurgeStatus", BYTE, URD, READ_ONLY, DEV},
    {"bMaxDataInSize", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bMaxDataOutSize", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"dDynCapNeeded", WORD, URD, READ_ONLY, ARRAY},
    {"bRefClkFreq", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bConfigDescrLock", BYTE, (URD | UWRT), (READ_NRML | WRITE_ONCE), DEV},
    {"bMaxNumOfRTT", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"wExceptionEventControl", WORD, URD, (READ_NRML | WRITE_VLT), DEV},
    {"wExceptionEventStatus", WORD, URD, READ_ONLY, DEV},
    {"dSecondsPassed", DWORD, UWRT, WRITE_ONLY, DEV},
    {"wContextConf", WORD, (URD | UWRT), (READ_NRML | WRITE_VLT), ARRAY},
    {"Reserved", BYTE, ACC_INVALID, MODE_INVALID, LEVEL_INVALID},
    {"Reserved", BYTE, ACC_INVALID, MODE_INVALID, LEVEL_INVALID},
    {"Reserved", BYTE, ACC_INVALID, MODE_INVALID, LEVEL_INVALID},
    {"bDeviceFFUStatus", BYTE, URD, READ_ONLY, DEV},
    {"bPSAState", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"dPSADataSize", DWORD, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bRefClkGatingWaitTime", BYTE, URD, READ_ONLY, DEV},
    {"bDeviceCaseRoughTemperaure", BYTE, URD, READ_ONLY, DEV},
    {"bDeviceTooHighTempBoundary", BYTE, URD, READ_ONLY, DEV},
    /*1A*/ {"bDeviceTooLowTempBoundary", BYTE, URD, READ_ONLY, DEV},
    /*1B*/ {"bThrottlingStatus", BYTE, URD, READ_ONLY, DEV},
    /*1C*/ {"bWBBufFlushStatus", BYTE, URD, READ_ONLY, DEV | ARRAY},
    /*1D*/ {"bAvailableWBBufSize", BYTE, URD, READ_ONLY, DEV | ARRAY},
    /*1E*/ {"bWBBufLifeTimeEst", BYTE, URD, READ_ONLY, DEV | ARRAY},
    /*1F*/ {"bCurrentWBBufSize", DWORD, URD, READ_ONLY, DEV | ARRAY},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    {ATTR_RSRV_EXT()},
    /*2C*/ {"bRefreshStatus", BYTE, URD, READ_ONLY, DEV},
    {"bRefreshFreq", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bRefreshUnit", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV},
    {"bRefreshMethod", BYTE, (URD | UWRT), (READ_NRML | WRITE_PRSIST), DEV}};

UFS_DEVICE_DESC device_desc = {0x00};
static UFS_CONFIG_DESC config_desc = {0x00};
byte_t current_pwr_mode = 0;

extern byte_t flag;
extern byte_t abort_flag;

int do_vendor_query(QUERY_VENDOR_FUNCTION vendor_func, QUERY_VENDOR_OPCODE vendor_opcode)
{
    int fd;
    int rc = OK;
    int oflag = O_RDONLY;
    word_t desc_buf_len = 0;
    byte_t data_buf[512] = {0x00};
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct tool_options opt =
        {
            .path = "/dev/bsg/ufs-bsg",
            .config_type_inx = DESC_TYPE,
            .opr = READ,
            .idn = QUERY_DESC_IDN_DEVICE,
        };

    fd = open(opt.path, oflag);
    if (fd < 0)
    {
        print_error("open");
        return ERROR;
    }
    rc = do_query_rq(fd, &bsg_req, &bsg_rsp,
                     vendor_func, vendor_opcode, opt.idn, 0, 0,
                     0, desc_buf_len, data_buf);
    // if (!rc)
    //	rc = check_read_desc_size(opt.idn, data_buf);
    if (rc)
    {
        printf("vendor fun execute err\r\n");
    }

    close(fd);
    return rc;
}

int read_device_desc_ext(UFS_DEVICE_DESC *desc_buff)
{
    int fd;
    int rc = OK;
    int oflag = O_RDONLY;
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    byte_t data_buf[QUERY_DESC_DEVICE_MAX_SIZE] = {0};
    struct tool_options opt =
        {
            .path = "/dev/bsg/ufs-bsg",
            .config_type_inx = DESC_TYPE,
            .opr = READ,
            .idn = QUERY_DESC_IDN_DEVICE,
        };

    fd = open(opt.path, oflag);
    if (fd < 0)
    {
        print_error("open");
        return ERROR;
    }

    rc = do_read_desc(fd, &bsg_req, &bsg_rsp,
                      QUERY_DESC_IDN_DEVICE, 0,
                      QUERY_DESC_DEVICE_MAX_SIZE, data_buf);
    if (rc)
    {
        print_error("Could not read device descriptor , error %d", rc);
        goto out;
    }
    print_descriptors("Device Descriptor", data_buf,
                      device_desc_field_name_ext, data_buf[0]);
    if (!desc_buff)
        ;
    else
        memcpy(desc_buff, data_buf, data_buf[0]);

out:
    close(fd);
    return rc;
}

// read one config desc
UFS_CONFIG_DESC *read_one_config_desc(byte_t cfg_index)
{
    int fd;
    int rc = OK;
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    word_t offset, i;

    // if(cfg_index > 3)
    //{
    //     printf("err cfg index:%d\r\n", cfg_index);
    //     return NULL;
    // }

    fd = open("/dev/bsg/ufs-bsg", O_RDONLY);
    if (fd < 0)
    {
        print_error("open");
        return NULL;
    }
    memset(&config_desc, 0x0, sizeof(UFS_CONFIG_DESC));
    printf("#################### UFS_CONFIG_DESC %d ####################\r\n", cfg_index);
    //
    rc = do_read_desc(fd, &bsg_req, &bsg_rsp, QUERY_DESC_IDN_CONFIGURAION, cfg_index,
                      QUERY_DESC_CONFIGURAION_MAX_SIZE, (byte_t *)&config_desc);
    if (rc)
    {
        printf("Coudn't read config descriptor error %d", rc);
        close(fd);
        return NULL;
    }
    else
    {
        print_descriptors("Config Device Descriptor:",
                          (byte_t *)&config_desc,
                          device_config_desc_field_name_ext,
                          CONFIG_HEADER_OFFSET_EXT);
        offset = CONFIG_HEADER_OFFSET_EXT;
        for (i = 0; i < 8; i++)
        {
            printf("Config %d Unit Descriptor:\n", i);
            print_descriptors("Config Descriptor:",
                              (byte_t *)&config_desc + offset,
                              device_config_unit_desc_field_name_ext,
                              CONFIG_LUN_OFFSET_EXT);
            offset = offset + CONFIG_LUN_OFFSET_EXT;
        }
        close(fd);
        return (&config_desc);
    }
}

// write one config desc
// idx:0~3
// data_buf:the expected write data
void write_one_config_desc(byte_t idx, byte_t *data_buf)
{
    int fd;
    int rc = OK;
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};

    printf("\r\n#################### Write %d Config Desc  ####################\r\n", idx);
    fd = open("/dev/bsg/ufs-bsg", O_RDWR);
    if (fd < 0)
    {
        print_error("open");
        return;
    }
    rc = do_query_rq(fd, &bsg_req, &bsg_rsp, UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST,
                     UPIU_QUERY_OPCODE_WRITE_DESC, QUERY_DESC_IDN_CONFIGURAION, idx, 0,
                     QUERY_DESC_CONFIGURAION_MAX_SIZE, 0, data_buf);

    if (!rc)
    {
        printf("req-dw0/1/2:0x%08x 0x%08x 0x%08x\r\n",
               be32toh(bsg_req.upiu_req.header.dword_0),
               be32toh(bsg_req.upiu_req.header.dword_1),
               be32toh(bsg_req.upiu_req.header.dword_2));

        printf("rsp-dw0/1/2:0x%08x 0x%08x 0x%08x\r\n",
               be32toh(bsg_rsp.upiu_rsp.header.dword_0),
               be32toh(bsg_rsp.upiu_rsp.header.dword_1),
               be32toh(bsg_rsp.upiu_rsp.header.dword_2));

        printf("write %d config desc success\n", idx);
    }
    else
    {
        printf("write %d config desc err\r\n", idx);
    }

    close(fd);
}

// read all config desc and printf
void read_all_config_descs(struct tool_options *opt)
{
    byte_t idx = 0;

    memset(opt, 0, sizeof(struct tool_options));
    strcpy(opt->path, "/dev/bsg/ufs-bsg");
    opt->config_type_inx = DESC_TYPE;
    opt->opr = READ;
    opt->idn = QUERY_DESC_IDN_CONFIGURAION;
    for (idx = 0; idx < 4; idx++)
    {
        opt->index = idx;
        printf("\r\n#################### UFS_CONFIG_DESC %d ####################\r\n", idx);
        do_desc(opt);
    }
}

#if 0
//modify config descs
//the num of modified config descs  num:1~4
//1:modify index0
//2:modify index0/1
//3:modify index0/1/2
//4:modify index0/1/2/3
void modify_config_descs(byte_t num)
{
    byte_t idx = 0;
    byte_t unit_idx = 0;
    UFS_CONFIG_DESC *pconfig_desc = NULL;

    if((num > 4) || (num < 1))
    {
        printf("err num:%d\r\n", num);
    }

    for(idx=0; idx<num; idx++)
    {
        //read->modify->write
        pconfig_desc = read_one_config_desc(idx);
        if(idx == (num-1))  //the last
        {
            pconfig_desc->bConfDescContinue = 0;
        }
        else
        {
            pconfig_desc->bConfDescContinue = 1;
        }
        pconfig_desc->bBootEnable = 0;
        pconfig_desc->bDescrAccessEn = 0;
        for(unit_idx = 0; unit_idx < CFG_UNIT_DESC_NUM; unit_idx++)
        {
            pconfig_desc->UnitConfig[unit_idx].bProvisioningType = 2;
            pconfig_desc->UnitConfig[unit_idx].bDataReliability = 0;
        }
        write_one_config_desc(idx, (byte_t *)pconfig_desc);
    }
}
#else
void modify_config_descs(byte_t num)
{
    byte_t idx = 0;
    byte_t unit_idx = 0;
    UFS_CONFIG_DESC *pconfig_desc = NULL;
    UFS_CONFIG_DESC config_desc[4] = {0x00};

    if ((num > 4) || (num < 1))
    {
        printf("err num:%d\r\n", num);
    }
    // read
    for (idx = 0; idx < num; idx++)
    {
        pconfig_desc = read_one_config_desc(idx);
        memcpy(&config_desc[idx], pconfig_desc, sizeof(UFS_CONFIG_DESC));
    }
    // modify and write
    for (idx = 0; idx < num; idx++)
    {
        if (idx == (num - 1)) // the last
        {
            config_desc[idx].bConfDescContinue = 0;
        }
        else
        {
            config_desc[idx].bConfDescContinue = 1;
        }
        config_desc[idx].bDescrAccessEn = 0;
        config_desc[idx].bInitActiveICCLevel = 3;
        config_desc[idx].wPeriodicRTCUpdate = be32toh(0x18);
        config_desc[idx].dNumSharedWriteBoosterBufferAllocUnits = be32toh(0x1);
        for (unit_idx = 0; unit_idx < CFG_UNIT_DESC_NUM; unit_idx++)
        {
            config_desc[idx].UnitConfig[unit_idx].bProvisioningType = 0x3;
            config_desc[idx].UnitConfig[unit_idx].bDataReliability = 1;
        }
        write_one_config_desc(idx, (byte_t *)&config_desc[idx]);
    }
}
#endif

void modify_config_descs_ext(void)
{
    byte_t idx = 0;
    byte_t unit_idx = 0;
    UFS_CONFIG_DESC *pconfig_desc = NULL;
    UFS_CONFIG_DESC config_desc[4] = {0x00};
    // read
    for (idx = 0; idx < 4; idx++)
    {
        pconfig_desc = read_one_config_desc(idx);
        memcpy(&config_desc[idx], pconfig_desc, sizeof(UFS_CONFIG_DESC));
    }
    // modify and write
    for (idx = 0; idx < 4; idx++)
    {
        config_desc[idx].bDescrAccessEn = 1;
        for (unit_idx = 0; unit_idx < CFG_UNIT_DESC_NUM; unit_idx++)
        {
            config_desc[idx].UnitConfig[unit_idx].bProvisioningType = 0x03;
            config_desc[idx].UnitConfig[unit_idx].bDataReliability = 1;
        }
    }

    config_desc[0].bConfDescContinue = 1;
    config_desc[1].bConfDescContinue = 0;
    config_desc[2].bConfDescContinue = 1;
    config_desc[3].bConfDescContinue = 1;

    for (idx = 0; idx < 4; idx++)
    {
        write_one_config_desc(idx, (byte_t *)&config_desc[idx]);
    }
}

// read one desc with rand idn
void read_one_desc_rand_idn(void)
{
    struct tool_options opt = {0x00};

    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.config_type_inx = DESC_TYPE;
    opt.opr = READ;
    opt.idn = BYTE_RAND();
    opt.index = 0;
    opt.selector = 0;
    printf("\r\n#################### Read one desc:Rand idn=0x%02x ####################\r\n", opt.idn);
    do_desc(&opt);
}

// read/write one desc with rand selector
void rdwr_one_desc_rand_idn(void)
{
    struct tool_options opt = {0x00};

    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.config_type_inx = DESC_TYPE;
    opt.opr = READ;
    opt.idn = BYTE_RAND();
    opt.index = 0;
    opt.selector = 0;
    printf("\r\n#################### Read one desc:Rand idn=0x%02x ####################\r\n", opt.idn);
    do_desc(&opt);

    opt.opr = WRITE;
    opt.idn = BYTE_RAND();
    printf("\r\n#################### Write one desc:Rand idn=0x%02x ####################\r\n", opt.idn);
    do_desc(&opt);
}

// read/write dev desc with rand selector
void rdwr_dev_desc_rand_index(void)
{
    struct tool_options opt = {0x00};

    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.config_type_inx = DESC_TYPE;
    opt.opr = READ;
    opt.idn = QUERY_DESC_IDN_DEVICE;
    opt.index = 0;
    opt.selector = 0;
    printf("\r\n#################### Read dev desc:Rand selector=0x%02x ####################\r\n", opt.selector);
    do_desc(&opt);

    opt.opr = WRITE;
    opt.index = 0;
    opt.selector = 0;
    printf("\r\n#################### Write dev desc:Rand selector=0x%02x ####################\r\n", opt.selector);
    do_desc(&opt);
}

// read/write cfg desc with rand selector
void rdwr_cfg_desc_rand_index(void)
{
    struct tool_options opt = {0x00};
    UFS_CONFIG_DESC *cfg_desc = NULL;

    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.config_type_inx = DESC_TYPE;
    opt.opr = READ;
    opt.idn = QUERY_DESC_IDN_CONFIGURAION;
    opt.index = 0;
    opt.selector = BYTE_RAND();
    printf("\r\n#################### Read cfg desc:Rand index=0x%02x ####################\r\n", opt.index);
    do_desc(&opt);

    cfg_desc = read_one_config_desc(0);
    opt.index = BYTE_RAND();
    printf("\r\n#################### Write cfg desc:Rand index=0x%02x ####################\r\n", opt.index);
    write_one_config_desc(opt.index, (byte_t *)cfg_desc);
}

/****************************************** tm drive****************************************/
static void prepare_upiu_ext(struct ufs_bsg_request_ext *bsg_req,
                             byte_t task_req_func, byte_t lun, byte_t ttg, byte_t iid)
{
    bsg_req->msgcode = UPIU_TRANSACTION_TASK_REQ;

    /* Fill UPIU header */
    bsg_req->upiu_req.header.dword_0 =
        UPIU_HEADER_DWORD_EXT(UPIU_TRANSACTION_TASK_REQ, 0, lun, ttg);
    bsg_req->upiu_req.header.dword_1 =
        UPIU_HEADER_DWORD_EXT(iid, task_req_func, 0, 0);
    bsg_req->upiu_req.header.dword_2 =
        UPIU_HEADER_DWORD_EXT(0, 0, 0, 0);

    /* Fill Transaction Specific Fields */
    bsg_req->upiu_req.tm.input_param1 = htobe32(lun);
    bsg_req->upiu_req.tm.input_param2 = htobe32(ttg);
    bsg_req->upiu_req.tm.input_param3 = htobe32(iid);
    bsg_req->upiu_req.tm.reserved[0] = 0;
    bsg_req->upiu_req.tm.reserved[1] = 0;
}

/**
 * send_bsg_scsi_trs - Utility function for SCSI transport cmd sending
 * @fd: ufs bsg driver file descriptor
 * @request_buff: pointer to the Query Request
 * @reply_buff: pointer to the Query Response
 * @req_buf_len: Query Request data length
 * @reply_buf_len: Query Response data length
 * @data_buf: pointer to the data buffer
 *
 * The function using ufs bsg infrastructure in linux kernel (/dev/ufs-bsg)
 * in order to send Query request command
 **/
static int send_bsg_scsi_tm(int fd, struct ufs_bsg_request_ext *request_buff,
                            struct ufs_bsg_reply_ext *reply_buff)
{
    int ret = OK;
    struct sg_io_v4 io_hdr_v4 = {0};

    io_hdr_v4.guard = 'Q';
    io_hdr_v4.protocol = BSG_PROTOCOL_SCSI;
    io_hdr_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_TRANSPORT;
    io_hdr_v4.response = (__u64)reply_buff;
    io_hdr_v4.request = (__u64)request_buff;
    io_hdr_v4.max_response_len = BSG_REPLY_SZ;
    io_hdr_v4.request_len = BSG_REQUEST_SZ;

    //	write_file_with_counter("bsg_reg_%d.bin",
    //				&request_buff->upiu_req,
    //				sizeof(struct utp_upiu_req));

    while (((ret = ioctl(fd, SG_IO, &io_hdr_v4)) < 0) &&
           ((errno == EINTR) || (errno == EAGAIN)))
        ;

    if (io_hdr_v4.info != 0)
    {
        print_error("Command fail with status %x ",
                    io_hdr_v4.info);
        ret = -EINVAL;
    }

    //	write_file_with_counter("bsg_rsp_%d.bin", reply_buff,
    //			BSG_REPLY_SZ);
    return ret;
}

int do_task_rq(int fd, byte_t task_req_func, byte_t lun, byte_t ttg, byte_t iid)
{
    int rc = OK;
    byte_t rsp = 0;
    struct ufs_bsg_request_ext bsg_req = {0};
    struct ufs_bsg_reply_ext bsg_rsp = {0};

    prepare_upiu_ext(&bsg_req, task_req_func, lun, ttg, iid);
    rc = send_bsg_scsi_tm(fd, &bsg_req, &bsg_rsp);

#if 0
    printf("req-dw0/1/2:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
           be32toh(bsg_req.upiu_req.header.dword_0),
           be32toh(bsg_req.upiu_req.header.dword_1),
           be32toh(bsg_req.upiu_req.header.dword_2),
           be32toh(bsg_req.upiu_req.tm.input_param1),
           be32toh(bsg_req.upiu_req.tm.input_param2),
           be32toh(bsg_req.upiu_req.tm.input_param3));
#endif
    printf("rsp-dw0/1/2:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
           be32toh(bsg_rsp.upiu_rsp.header.dword_0),
           be32toh(bsg_rsp.upiu_rsp.header.dword_1),
           be32toh(bsg_rsp.upiu_rsp.header.dword_2),
           be32toh(bsg_rsp.upiu_rsp.tm.input_param1),
           be32toh(bsg_rsp.upiu_rsp.tm.input_param2),
           be32toh(bsg_rsp.upiu_rsp.tm.input_param3));

    if (rc)
    {
        print_error("%s: task failed, status %d lun: %d, ttg: %d, iid: %d",
                    __func__, rc, lun, ttg, iid);
        rc = ERROR;
        goto out;
    }

    rsp = (be32toh(bsg_rsp.upiu_rsp.header.dword_1) >> 8) & 0xff;
    if (rsp)
    {
        printf("rsp err %d \r\n", rsp);
        rc = ERROR;
    }
    rsp = be32toh(bsg_rsp.upiu_rsp.tm.input_param1);
    printf("service rsp %d \r\n", be32toh(bsg_rsp.upiu_rsp.tm.input_param1));

    if ((task_req_func != QUERY_TASK_FUCNC) && (task_req_func != QUERY_TASK_SET_FUNC))
    {
        if (rsp == 0)
        {
            ufshcd_abt(fd, lun, ttg);
        }
    }

    if (rsp == 8)
    {
        flag = 1;
    }
out:
    return rc;
}

int abort_task(byte_t lun, byte_t ttg, byte_t iid)
{
    int fd = OK;
    int rc = OK;
    int oflag = O_RDWR;
    struct tool_options opt =
        {
            .path = "/dev/bsg/ufs-bsg",
        };

    fd = open(opt.path, oflag);
    if (fd < 0)
    {
        print_error("open");
        return ERROR;
    }
    rc = do_task_rq(fd, ABORT_TASK_FUNC, lun, ttg, iid);
    if (rc)
    {
        printf("abort task err %d\r\n", rc);
    }
    close(fd);
    return rc;
}

int do_task(byte_t task_func, byte_t lun, byte_t ttg, byte_t iid)
{
    int fd = OK;
    int rc = OK;
    int oflag = O_RDWR;
    struct tool_options opt =
        {
            .path = "/dev/bsg/ufs-bsg",
        };

    fd = open(opt.path, oflag);
    if (fd < 0)
    {
        print_error("open");
        return ERROR;
    }
    printf("task func=0x%02x lun=0x%02x ttg=0x%02x iid=0x%02x\r",
           task_func, lun, ttg, iid);
    rc = do_task_rq(fd, task_func, lun, ttg, iid);
    if (rc)
    {
        printf("task exec err %d\r\n", rc);
    }
    close(fd);
    return rc;
}

void get_currnet_pwr_mode(void)
{
    int fd;
    int rc = OK;
    int oflag = O_RDONLY;
    dword_t attr_value;
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct tool_options opt = {0x00};
    memset(&opt, 0, sizeof(struct tool_options));
    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.opr = READ;
    opt.config_type_inx = ATTR_TYPE;
    opt.idn = 0x02;
    opt.selector = 0;
    opt.index = 0;
    do_attributes(&opt);

    fd = open(opt.path, oflag);
    if (fd < 0)
    {
        perror("Device open");
        return;
    }

    rc = do_query_rq(fd, &bsg_req, &bsg_rsp,
                     UPIU_QUERY_FUNC_STANDARD_READ_REQUEST,
                     UPIU_QUERY_OPCODE_READ_ATTR, opt.idn,
                     opt.index, opt.selector, 0, 0, 0);
    if (rc == OK)
    {
        attr_value = be32toh(bsg_rsp.upiu_rsp.qr.value);
        current_pwr_mode = (byte_t)attr_value;
        printf("current_pwr_mode:0x%02x\r\n", current_pwr_mode);
    }
    close(fd);
}

void ufs_query_test(void)
{
    struct tool_options opt = {0x00};
    // UFS_CONFIG_DESC *cfg_desc = NULL;
    dword_t tmp = 0x03;

    printf("\r\n##############read all##############\r\n");
    strcpy(opt.path, "/dev/bsg/ufs-bsg");
    opt.opr = READ_ALL;
    opt.config_type_inx = DESC_TYPE;
    do_desc(&opt);
    opt.config_type_inx = ATTR_TYPE;
    do_attributes(&opt);
    opt.config_type_inx = FLAG_TYPE;
    do_flags(&opt);

    // cfg_desc = read_one_config_desc(0);
    // cfg_desc->bConfDescContinue = 0;
    // write_one_config_desc(0, (byte_t *)cfg_desc);

    printf("\r\n##############write attr##############\r\n");
    opt.opr = WRITE;
    opt.config_type_inx = ATTR_TYPE;
    opt.idn = 3;
    opt.index = 0;
    opt.selector = 0;
    tmp = 0;
    opt.data = &tmp;
    do_attributes(&opt);

    opt.opr = SET_FLAG;
    opt.config_type_inx = FLAG_TYPE;
    opt.idn = 0x04;
    opt.index = 0;
    opt.selector = 0;
    printf("\r\n##############set/clear/toggle flag##############\r\n");
    do_flags(&opt);
    opt.opr = CLEAR_FLAG;
    do_flags(&opt);
    opt.opr = TOGGLE_FLAG;
    do_flags(&opt);
}

void exe_task_on_cmd_going1(void)
{
    int fd;
    int oflag = O_RDWR;
    int i = 0;
    struct tool_options opt = {0x00};

    strcpy(opt.path, "/dev/bsg/0:0:0:0");
    fd = open(opt.path, oflag);
    LOG_INFO("Start : %s fd %d\n", __func__, fd);

    if (fd < 0)
    {
        print_error("open");
    }

    for (i = 0; i < 2000; i++)
    {
        printf("\r\n--- %s %d---\r\n", __func__, i);
        inquiry_test(fd, 0, 0);
        if (!(i % 10))
        {
            do_task(ABORT_TASK_SET_FUNC, 0, 0, 0);
            if (flag)
            {
                flag = 0;
                printf("query task succeed!\r\n");
                break;
            }
        }
    }
    close(fd);
}

void exe_task_on_cmd_going2(void)
{
    int fd;
    int oflag = O_RDWR;
    int i = 0;
    struct tool_options opt = {0x00};

    strcpy(opt.path, "/dev/bsg/0:0:0:0");
    fd = open(opt.path, oflag);
    LOG_INFO("Start : %s fd %d\n", __func__, fd);

    if (fd < 0)
    {
        print_error("open");
    }

    for (i = 0; i < 2000; i++)
    {
        printf("\r\n--- %s %d---\r\n", __func__, i);
        inquiry_test(fd, 0, 0);
        do_task(QUERY_TASK_FUCNC, 0, i, 0);
        if (flag)
        {
            flag = 0;
            printf("query task succeed!\r\n");
            break;
        }
    }
    close(fd);
}
