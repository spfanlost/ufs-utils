/**
 * @file test.c
 * @author yumeng (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2019-10-08
 *
 * @copyright Copyright (c) 2018-2019 Maxio-Tech
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>

#include "../ufs_cmds.h"
#include "../options.h"
#include "../ufs.h"
#include "../ufs_err_hist.h"
#include "../unipro.h"
#include "../ufs_ffu.h"
#include "../ufs_vendor.h"
#include "../ufs_rpmb.h"
#include "../ufs_hmr.h"

#include "common.h"
#include "unittest.h"
#include "test_case_all.h"
#include "test_send_cmd.h"
#include "test_send_scsi.h"
#include "test_send_uic.h"

#include "test.h"

#define RANDOM_VAL 0

#define UFS_BSG_UTIL_VERSION	"3.12.3"

#define DISP_HELP "\033[32mtest case list:\033[0m\n" \
                  "case   0: Display bsg device\n"   \
                  "case   1: \n"                     \
                  "case 255: test case list exec\n"

#define PARAM_TYPE 1 // 0 Desc  1 Attr  2 Flag

int file_desc = 0;
void *rd_buf;
void *wr_buf;
// char *device_file_name = "/dev/bsg/0:0:0:0";

static TestCase_t TestCaseList[] = {
    TCD(test_0_full_disk_wr),
    TCD(test_5_fua_wr_rd_cmp),
};

struct tool_options opt =
    {
        //.path="/dev/bsg/0:0:0:49488",
        .path = "/dev/bsg/ufs-bsg",
        //.config_type_inx = DESC_TYPE,
        //.opr = READ,
        //.idn = QUERY_DESC_IDN_DEVICE,
};

byte_t read_only_attr_id[] = {0x02, 0x05, 0x06, 0x09, 0x0e, 0x14, 0x17, 0x18,
                              0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x2c};
byte_t index_buff[MAX_INDEX] = {0x00};
byte_t flag = 0;
byte_t abort_flag = 0;

void test_mem_alloc(void);
void test_mem_free(void);
extern int do_uic(struct tool_options *opt);

int main(int argc, char *argv[])
{
    int test_case, test_loop;
    UFS_CONFIG_DESC *cfg_desc = NULL;
    dword_t tmp = 0x03;
    LOG_INFO("\n****** ufs-utils unittest ver:%s ******\n", UFS_BSG_UTIL_VERSION);
    LOG_INFO("****** compile:%4d/%02d/%02d %s   ******\n", YEAR, MONTH, DAY, __TIME__);
    LOG_INFO("%s", DISP_HELP);
    static dword_t cnt = 0;
    byte_t IDN_buff[] = {0X15, 0X16, 0X1B, 0X2C, 0X2D, 0X2E, 0X2F};
    test_mem_alloc();
    RAND_INIT();
    do
    {
        LOG_COLOR(SKBLU_LOG, "%d%s>", argc, argv[0]);
        fflush(stdout);
        scanf("%d", &test_case);
        switch (test_case)
        {
        case 0:
            do_get_ufs_bsg_list(NULL);
            break;
        case 1:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            // opt.config_type_inx = DESC_TYPE;
            // opt.idn = QUERY_DESC_IDN_CONFIGURAION;/*  */
            // opt.index = 1;
            do_desc(&opt);
            break;
        case 2:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/0:0:0:0");
            test_send_scsi(&opt);
            break;
        case 3:
            // strcpy(opt.path,"/dev/bsg/0:0:0:49476");
            // strcpy(opt.path, "/dev/sg1");
            // opt.idn = READ_WRITE_COUNTER;
            // opt.sg_type = SG3_TYPE;
            // opt.region = 0;
            // do_rpmb(&opt);

            // ufshcd_dme_reset(Cold_Reset);
            // ufshcd_dme_reset(Warm_Reset);
            for (int idx = 0; idx < 1000000000; idx++)
            {
                LOG_COLOR(RED_LOG, "cnt:%d\n", idx);
                ufshcd_dme_cmd(UIC_CMD_DME_END_PT_RST);
                sleep(2);
                ufshcd_link_recovery();
                sleep(2);
                memset(&opt, 0, sizeof(struct tool_options));
                strcpy(opt.path, "/dev/bsg/ufs-bsg");
                opt.opr = READ_ALL;
                do_desc(&opt);
                sleep(2);
            }
            ufshcd_link_recovery();
            // ufshcd_dme_cmd(UIC_CMD_DME_END_PT_RST);
            // ufshcd_dme_cmd(UIC_CMD_DME_POWEROFF);
            // ufshcd_dme_cmd(UIC_CMD_DME_POWERON);
            // ufshcd_dme_cmd(UIC_CMD_DME_ENABLE);
            // ufshcd_dme_cmd(UIC_CMD_DME_LINK_STARTUP);
            // ufshcd_dme_cmd(UIC_CMD_DME_HIBER_ENTER);
            // ufshcd_dme_cmd(UIC_CMD_DME_HIBER_EXIT);

            break;
        case 4:
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            opt.idn = MPHY;
            do_uic(&opt);
            opt.idn = PHY_ADAPTER;
            do_uic(&opt);
            opt.idn = DME_QOS;
            do_uic(&opt);

            // SetPowerMode(Fast, 2);
            break;
            /******************************************descriptor case********************************************/
        case 5: // read all unit desc
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ;
            opt.config_type_inx = DESC_TYPE;
            opt.idn = QUERY_DESC_IDN_UNIT;
            for (int i = 0; i < 33; i++)
            {
                printf("\r\n-------------------lu%d unit desc-------------------\r\n", i);
                opt.index = i;
                opt.selector = 0;
                do_desc(&opt);
            }
            break;
        case 6: // read all desc loop 1000
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            opt.config_type_inx = DESC_TYPE;
            printf("test 1000 cnt begain>>>>>>>>>>>>>>>>>>>>>\r\n");
            for (int i = 0; i < 1; i++)
            {
                do_desc(&opt);
                printf("test cnt%d\r\n", (i + 1));
            }
            printf("test 1000 cnt end<<<<<<<<<<<<<<<<<<<<<<<<<\r\n");
            break;
        case 7: // read device desc value and get the index of string desc
            read_device_desc_ext(&device_desc);
            printf("device_desc.iManufactureName = %d\r\n", device_desc.iManufactureName);
            printf("device_desc.iProductName = %d\r\n", device_desc.iProductName);
            printf("device_desc.iSerialNumber = %d\r\n", device_desc.iSerialNumber);
            printf("device_desc.iOemID = %d\r\n", device_desc.iOemID);
            printf("device_desc.iProductRevisionLevel = %d\r\n", device_desc.iProductRevisionLevel);
            printf("device_desc.dExtendedUFSFeaturesSupport = 0x%02x\r\n", be32toh(device_desc.dExtendedUFSFeaturesSupport));
            index_buff[MANU_NAME_INDEX] = device_desc.iManufactureName;
            index_buff[PROD_NAME_INDEX] = device_desc.iProductName;
            index_buff[SERI_NUMB_INDEX] = device_desc.iSerialNumber;
            index_buff[OEMID_INDEX] = device_desc.iOemID;
            index_buff[PROD_REVI_INDEX] = device_desc.iProductRevisionLevel;
            break;
        case 8: // according the index from case7,read the string desc and write OEMID string
            opt.opr = READ;
            opt.config_type_inx = DESC_TYPE;
            opt.idn = QUERY_DESC_IDN_STRING;
            for (int i = 0; i < MAX_INDEX; i++)
            {
                opt.index = index_buff[i];
                do_desc(&opt);
            }
            opt.opr = WRITE;
            char *str = "112233445566";
            opt.data = str;
            for (int i = 0; i < MAX_INDEX; i++)
            {
                opt.index = index_buff[i];
                do_desc(&opt);
            }
            break;
        case 9: // vendor query
            do_vendor_query(UPIU_QUERY_VENDOR_FUNC_0, QUERY_VENDOR_OPCODE_0);
            break;

            /************************************read all desc/attr/flag case**************************************/
        case 10:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            opt.config_type_inx = DESC_TYPE;
            do_desc(&opt);
            break;
        case 11:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            opt.config_type_inx = ATTR_TYPE;
            do_attributes(&opt);
            break;
        case 12:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            opt.config_type_inx = FLAG_TYPE;
            do_flags(&opt);
            break;

            /******************************************descriptor case********************************************/
        case 13:
            read_all_config_descs(&opt);
            break;
        case 14: // read one config desc
            printf("pls enter an index(0~3) to read one config desc:");
            int idx1 = 0;
            scanf("%d", &idx1);
            read_one_config_desc(idx1);
            break;
        case 15: // modify one config desc(can test rand value)
            cfg_desc = read_one_config_desc(0);
            cfg_desc->bConfDescContinue = 0;
            cfg_desc->bHPBControl = 1;
            cfg_desc->wPeriodicRTCUpdate = be16toh(0x0);
            cfg_desc->dNumSharedWriteBoosterBufferAllocUnits = be32toh(0x0);
            // cfg_desc->bInitPowerMode = BYTE_RAND();
            // printf("the rand power value:0x%02x\r\n", cfg_desc->bInitPowerMode);
            // cfg_desc->bWriteBoosterBufferType = 0;
            // cfg_desc->bDescrAccessEn = 0;
            // cfg_desc->bInitActiveICCLevel = 5;
            for (int i = 0; i < 8; i++)
            {
                cfg_desc->UnitConfig[i].bDataReliability = 0;
                cfg_desc->UnitConfig[i].bProvisioningType = 0;
            }
            // printf("the rand bBootEnable/bDescrAccessEn/bInitPowerMode/bProvisioningType = 0x%02x/0x%02x/0x%02x/0x%02x\r\n",
            //         cfg_desc->bBootEnable, cfg_desc->bDescrAccessEn, cfg_desc->bInitPowerMode, randvalue);
            // cfg_desc->UnitConfig[1].dLUNumWriteBoosterBufferAllocUnits = be32toh(0x400);
            // cfg_desc->UnitConfig[0].wContextCapabilities = be16toh(0x1f);
            // cfg_desc->UnitConfig[0].bDataReliability = 76;
            // cfg_desc->UnitConfig[0].bLUEnable = 1;
            // cfg_desc->UnitConfig[0].wLUMaxActiveHPBRegions = 0;
            write_one_config_desc(0, (byte_t *)cfg_desc);
            break;
        case 16: // write 1~4 config descs
            printf("pls enter the num(1~4) expected to write cinfg descs:");
            int idx2 = 0;
            scanf("%d", &idx2);
            modify_config_descs(idx2);
            break;
        case 17:
            modify_config_descs_ext();
            break;
        case 18:
            rdwr_one_desc_rand_idn();
            break;
        case 19:
            rdwr_dev_desc_rand_index();
            break;
        case 20:
            rdwr_cfg_desc_rand_index();
            break;

            /******************************************flag case********************************************/
        case 21:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = SET_FLAG;
            opt.config_type_inx = FLAG_TYPE;
            opt.idn = 0x01;
            opt.index = 0;
            opt.selector = 0;
            do_flags(&opt);
            // opt.idn = 0x04;
            // printf("2 the rand index=0x%02x\r\n", opt.index);
            // do_flags(&opt);
            // opt.idn = 0x0e;
            // opt.index = 0;
            // opt.selector = 0;
            // printf("3 the rand index=0x%02x\r\n", opt.index);
            // do_flags(&opt);
            break;
        case 22:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = SET_FLAG;
            opt.config_type_inx = FLAG_TYPE;
            opt.idn = 0x0e;
            opt.index = 0;
            opt.selector = 0;
            printf("set flag\r\n");
            do_flags(&opt);
            opt.opr = CLEAR_FLAG;
            printf("clear flag\r\n");
            do_flags(&opt);
            opt.opr = TOGGLE_FLAG;
            printf("toggle flag\r\n");
            do_flags(&opt);
            opt.opr = READ;
            printf("read flag\r\n");
            do_flags(&opt);
            break;
        case 23:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = CLEAR_FLAG;
            opt.config_type_inx = FLAG_TYPE;
            opt.idn = BYTE_RAND();
            opt.index = 0;
            opt.selector = 0;
            printf("the idn=0x%02x\r\n", opt.idn);
            do_flags(&opt);
            opt.opr = READ;
            do_flags(&opt);
            break;

            /******************************************attribute case********************************************/
        case 24:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ;
            opt.config_type_inx = ATTR_TYPE;
            opt.idn = 0;
            opt.index = 0;
            opt.selector = 0;
            for (int i = 0; i < sizeof(IDN_buff) / sizeof(IDN_buff[0]); i++)
            {
                // tmp = WORD_RAND();
                // printf("the rand value:0x%02x\r\n", tmp);
                // opt.data = &tmp;
                opt.idn = IDN_buff[i];
                printf("the IDN:0x%02x\r\n", opt.idn);
                do_attributes(&opt);
            }
            break;
        case 25:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = WRITE;
            opt.config_type_inx = ATTR_TYPE;
            opt.idn = 3;
            opt.index = 0;
            opt.selector = 0;
            tmp = 0;
            printf("idn = 0x%02x  rand value = 0x%02x\r\n", opt.idn, tmp);
            opt.data = &tmp;
            do_attributes(&opt);
            tmp = 0xf;
            printf("idn = 0x%02x  rand value = 0x%02x\r\n", opt.idn, tmp);
            opt.data = &tmp;
            do_attributes(&opt);

            tmp = BYTE_RAND();
            printf("idn = 0x%02x  rand value = 0x%02x\r\n", opt.idn, tmp);
            opt.data = &tmp;
            do_attributes(&opt);
            break;
        case 26:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ;
            opt.config_type_inx = ATTR_TYPE;
            opt.idn = 0x10;
            opt.index = 0;
            for (int i = 1; i <= 0xf; i++)
            {
                opt.selector = i;
                do_attributes(&opt);
            }
            break;
        case 27:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ;
            opt.config_type_inx = ATTR_TYPE;
            opt.idn = 0x1c;
            opt.selector = 0;
            opt.index = BYTE_RAND();
            printf("RAND INDEX = 0x%02x\r\n", opt.index);
            do_attributes(&opt);
            opt.index = 0;
            opt.selector = BYTE_RAND();
            printf("RAND selector = 0x%02x\r\n", opt.selector);
            do_attributes(&opt);
            break;
        case 28:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            dword_t tmpdate = 0x0f;
            opt.opr = WRITE;
            opt.config_type_inx = ATTR_TYPE;
            // opt.idn = 3;
            opt.index = 0;
            opt.selector = 0;
            opt.data = &tmpdate;
            printf("opt.data = 0x%02x\r\n", *(dword_t *)opt.data);
            for (int i = 0; i < sizeof(read_only_attr_id) / sizeof(byte_t); i++)
            {
                opt.idn = read_only_attr_id[i];
                printf("write attr-idn=0x%02x\r\n", opt.idn);
                do_attributes(&opt);
            }
            break;
        case 29:
            memset(&opt, 0, sizeof(struct tool_options));
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ;
            opt.config_type_inx = ATTR_TYPE;
            opt.idn = 0x02;
            opt.selector = 0;
            opt.index = 0;
            do_attributes(&opt);
            break;
#if 0
            case 30:
                //get_currnet_pwr_mode();
                while(1)
                {
                    printf("\r\n---------------------query test loop:%d---------------------\r\n", query_cnt);
                    ufs_query_test();
                    query_cnt++;
                }

                break;
#endif
            /******************************************task case********************************************/
        case 31:
            while (1)
            {
                if (do_task(ABORT_TASK_FUNC, 0, 0, 0))
                {
                    printf("abort fail break\r");
                    break;
                }
            }
            break;
        case 32:
            while (1)
            {
                do_task(ABORT_TASK_SET_FUNC, 0, 0, 0);
            }
            break;
        case 33:
            while (1)
            {
                do_task(CLEAR_TASK_SET_FUNC, 0, 0, 0);
            }
            break;
        case 34:
            while (1)
            {
                do_task(LU_RESET_FUNC, 0, 0, 0);
            }
            break;
        case 35:
            flag = 0;
            while (1)
            {
                do_task(QUERY_TASK_FUCNC, 0, 0, 0);
                if (flag)
                {
                    flag = 0;
                    printf("query task succeed!\r\n");
                    break;
                }
            }
            break;
        case 36:
            flag = 0;
            while (1)
            {
                do_task(QUERY_TASK_FUCNC, 0, 0, 0);
                if (flag)
                {
                    flag = 0;
                    printf("query task succeed!\r\n");
                    break;
                }
            }
            break;

        case 41:
            do_task(ABORT_TASK_FUNC, 0, 0, 0);
            break;
        case 42:
            do_task(ABORT_TASK_SET_FUNC, 0, 0, 0);
            break;
        case 43:
            do_task(CLEAR_TASK_SET_FUNC, 0, 0, 0);
            break;
        case 44:
            do_task(LU_RESET_FUNC, 0, 0, 0);
            break;
        case 50:
            exe_task_on_cmd_going1();
            break;
        case 51:
            get_currnet_pwr_mode();
            break;
        case 37:
            while (1)
            {
                printf("\r\n------------------loop:%d------------------\r\n", cnt++);
                ufs_query_test();
                do_task(ABORT_TASK_FUNC, 0, 0, 0);
                do_task(ABORT_TASK_SET_FUNC, 0, 0, 0);
                do_task(CLEAR_TASK_SET_FUNC, 0, 0, 0);
                do_task(LU_RESET_FUNC, 0, 0, 0);
                do_task(QUERY_TASK_FUCNC, 0, 0, 0);
                do_task(QUERY_TASK_SET_FUNC, 0, 0, 0);
                do_task(BYTE_RAND(), 0, 0, 0);
            }
            break;
        case 60:
            while (1)
            {
                printf("\r\n------------------loop:%d------------------\r\n", cnt++);
                do_task(QUERY_TASK_FUCNC, 0, 0, 0);
                do_task(QUERY_TASK_SET_FUNC, 0, 0, 0);
            }
            break;

        case 255:
            LOG_COLOR(SKBLU_LOG, "pls enter auto loop cnt:");
            fflush(stdout);
            int loop = 0;
            scanf("%d", &test_loop);
            while (test_loop--)
            {
                loop++;
                LOG_COLOR(SKBLU_LOG, "auto_case_loop_cnt:%d\r\n", loop);
                if (test_list_exe(TestCaseList, ARRAY_SIZE(TestCaseList)))
                {
                    break;
                }
                random_list(TestCaseList, ARRAY_SIZE(TestCaseList));
            }
            LOG_COLOR(SKBLU_LOG, "auto_case_run_loop:%d\r\n", loop);

            break;
        default:
            if (test_case < 256)
                LOG_ERROR("Error case number! please try again:\n %s", DISP_HELP);
            break;
        }
    } while (test_case < 256);
    LOG_INFO("test_case num should < 256, Now test will Exiting...\n");
    test_mem_free();
    LOG_INFO("\n****** END OF TEST ******\n");
    // close(file_desc);
    return 0;
}

void test_mem_alloc(void)
{
/* Allocating buffer for Read & write*/
#ifdef RW_BUF_4K_ALN_EN
    if ((posix_memalign(&rd_buf, 4096, RW_BUFFER_SIZE)) ||
        (posix_memalign(&wr_buf, 4096, RW_BUFFER_SIZE)))
    {
        LOG_ERROR("Memalign Failed\n");
    }
#else
    rd_buf = malloc(RW_BUFFER_SIZE);
    wr_buf = malloc(RW_BUFFER_SIZE);
    if ((wr_buf == NULL) || (rd_buf == NULL))
    {
        LOG_ERROR("Malloc Failed\n");
    }
#endif
    for (size_t i = 0; i < RW_BUFFER_SIZE; i++)
    {
        MEM8_GET(wr_buf + i) = BYTE_RAND();
        // MEM8_GET(wr_buf+i) = BYTE_RAND();
        // LOG_INFO("%X",MEM8_GET(wr_buf));
    }
}

void test_mem_free(void)
{
    free(rd_buf);
    free(wr_buf);
}
