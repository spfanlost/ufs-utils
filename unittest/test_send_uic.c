/**
 * @file test_send_uic.c
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
#include "../unipro.h"

#include "common.h"
#include "test.h"
#include "unittest.h"
#include "test_send_cmd.h"
#include "test_send_uic.h"

#define UIC_BSG_PATH "/dev/bsg/ufs-bsg"

extern int do_uic(struct tool_options *opt);
extern int unipro_read(int fd, int idn, int id, __u8 all);
extern int unipro_write(int fd, int idn, int id, int mib_val,
                        int attr_set, int target);

int SetPowerMode(struct ufs_pwr_mode_info *pwr_info)
{
    int rt = OK;
    int fd;
    int oflag = O_RDWR;
    static const char *const names[] = {
        "INVALID MODE",
        "FAST MODE",
        "SLOW_MODE",
        "INVALID MODE",
        "FASTAUTO_MODE",
        "SLOWAUTO_MODE",
        "INVALID MODE",
    };
    fd = open(UIC_BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        rt = ERROR;
        goto done;
    }

    // LOG_INFO("GetPowerMode:\n");
    // unipro_read(fd, PHY_ADAPTER, PA_TxGear, 0);
    // unipro_read(fd, PHY_ADAPTER, PA_RxGear, 0);
    // unipro_read(fd, PHY_ADAPTER, PA_PWRMode, 0);
    // unipro_read(fd, PHY_ADAPTER, PA_ConnectedTxDataLanes, 0);
    // unipro_read(fd, PHY_ADAPTER, PA_ConnectedRxDataLanes, 0);

    LOG_COLOR(GREEN_LOG, "SetPowerMode:%s,gear:%x\n", names[pwr_info->pwr], pwr_info->gear);
    if (pwr_info->pwr == Fast || pwr_info->pwr == FastAuto)
    {
    }
    else if (pwr_info->pwr == Slow || pwr_info->pwr == SlowAuto)
    {
    }
    else
    {
    }

    unipro_write(fd, PHY_ADAPTER, PA_TxGear, pwr_info->gear, ATTR_SET_NOR, DME_LOCAL);
    unipro_write(fd, PHY_ADAPTER, PA_RxGear, pwr_info->gear, ATTR_SET_NOR, DME_LOCAL);

    unipro_write(fd, PHY_ADAPTER, PA_PWRMode, (pwr_info->pwr << 4 | pwr_info->pwr), ATTR_SET_NOR, DME_LOCAL);
done:
    close(fd);
    return rt;
}

int ufshcd_abt(int fd, int32_t lun, int32_t task_tag)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct ufs_bsg_adm_abort *adm_abort = (struct ufs_bsg_adm_abort *)&bsg_req.upiu_req.uc;
    int rt = OK;

    bsg_req.msgcode = UPIU_TRANSACTION_ADM_ABORT;

    adm_abort->lun = lun;
    adm_abort->task_tag = task_tag;
    rt = send_bsg_scsi_trs(fd, &bsg_req, &bsg_rsp, 0, 0, 0);
    if (rt)
    {
        print_error("%s: bsg request failed", __func__);
        rt = ERROR;
        goto out;
    }
out:
    return rt;
}

int ufshcd_dme_reset(dword_t reset_level)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct uic_command *uic_cmd = (struct uic_command *)&bsg_req.upiu_req.uc;
    int fd;
    int oflag = O_RDWR;
    fd = open(UIC_BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto out;
    }

    int rt = OK;
    LOG_WARN("%s %d...\n", __func__, reset_level);

    uic_cmd->command = UIC_CMD_DME_RESET;
    uic_cmd->argument1 = reset_level;
    bsg_req.msgcode = UPIU_TRANSACTION_UIC_CMD;

    rt = send_bsg_scsi_trs(fd, &bsg_req, &bsg_rsp, 0, 0, 0);
    if (rt)
    {
        print_error("%s: bsg request failed", __func__);
        rt = ERROR;
        goto out;
    }
out:
    close(fd);
    return rt;
}

int ufshcd_dme_cmd(enum uic_cmd_dme uic_cmd_dme)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct uic_command *uic_cmd =
        (struct uic_command *)&bsg_req.upiu_req.uc;
    int fd;
    int oflag = O_RDWR;
    fd = open(UIC_BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto out;
    }

    int rt = OK;
    LOG_WARN("%s...\n", __func__);

    uic_cmd->command = uic_cmd_dme;
    bsg_req.msgcode = UPIU_TRANSACTION_UIC_CMD;

    rt = send_bsg_scsi_trs(fd, &bsg_req, &bsg_rsp, 0, 0, 0);
    if (rt)
    {
        print_error("%s: bsg request failed", __func__);
        rt = ERROR;
        goto out;
    }

out:
    close(fd);
    return rt;
}

int ufshcd_link_recovery(void)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    int rt = OK;
    int fd;
    int oflag = O_RDWR;
    fd = open(UIC_BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto out;
    }

    bsg_req.msgcode = UPIU_TRANSACTION_ADM_LINK_RECOVERY;

    rt = send_bsg_scsi_trs(fd, &bsg_req, &bsg_rsp, 0, 0, 0);
    if (rt)
    {
        print_error("%s: bsg request failed", __func__);
        rt = ERROR;
        goto out;
    }

out:
    close(fd);
    return rt;
}

int ufshcd_reset_scsi(void)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    int rt = OK;
    int fd;
    int oflag = O_RDWR;
    fd = open(UIC_BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto out;
    }

    bsg_req.msgcode = UPIU_TRANSACTION_ADM_RESET_SCSI;

    rt = send_bsg_scsi_trs(fd, &bsg_req, &bsg_rsp, 0, 0, 0);
    if (rt)
    {
        print_error("%s: bsg request failed", __func__);
        rt = ERROR;
        goto out;
    }

out:
    close(fd);
    return rt;
}
////////////////////////////////////////////////////////////////////////

dword_t get_unipro_mphy_info(void)
{
    struct tool_options opt = {0};
    strcpy(opt.path, UIC_BSG_PATH);
    opt.opr = READ_ALL;
    opt.idn = MPHY;
    do_uic(&opt);
    opt.idn = PHY_ADAPTER;
    do_uic(&opt);
    opt.idn = DME_QOS;
    do_uic(&opt);
    return SUCCEED;
}

dword_t ufs_pwr_rand(void)
{
    struct ufs_pwr_mode_info pwr_info = {0};
    pwr_info.pwr = BYTE_RAND() % 2 ? Fast : Slow;
    pwr_info.gear = (pwr_info.pwr == Slow) ? RAND_RANGE(1, 4) : RAND_RANGE(1, 2);
    SetPowerMode(&pwr_info);
    usleep(50000);
    return SUCCEED;
}

static dword_t sub_case_do_uic(void)
{
    get_unipro_mphy_info();
    ufs_pwr_rand();
    return SUCCEED;
}

static dword_t sub_case_do_reset(void)
{
    struct tool_options opt = {0};
    ufshcd_dme_cmd(UIC_CMD_DME_END_PT_RST);
    usleep(50000);
    ufshcd_link_recovery();
    usleep(20000);
    strcpy(opt.path, UIC_BSG_PATH);
    opt.opr = READ_ALL;
    do_desc(&opt);
    ufshcd_dme_reset(Cold_Reset);
    usleep(20000);
    ufshcd_link_recovery();
    usleep(20000);
    ufshcd_dme_reset(Warm_Reset);
    usleep(20000);
    ufshcd_link_recovery();
    usleep(20000);

    // ufshcd_dme_reset(Cold_Reset);
    // ufshcd_dme_reset(Warm_Reset);
    // ufshcd_dme_cmd(UIC_CMD_DME_END_PT_RST);
    // ufshcd_dme_cmd(UIC_CMD_DME_POWEROFF);
    // ufshcd_dme_cmd(UIC_CMD_DME_POWERON);
    // ufshcd_dme_cmd(UIC_CMD_DME_ENABLE);
    // ufshcd_dme_cmd(UIC_CMD_DME_LINK_STARTUP);
    // ufshcd_dme_cmd(UIC_CMD_DME_HIBER_ENTER);
    // ufshcd_dme_cmd(UIC_CMD_DME_HIBER_EXIT);
    return SUCCEED;
}

////////////////////////////////////////////////////////////////////////
static dword_t sub_case_do_uic(void);
static dword_t sub_case_do_reset(void);

static SubCaseHeader_t sub_case_header = {
    "ufs_uic_test",
    "This case will tests uic",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_do_uic, "do_uic"),
    SUB_CASE(sub_case_do_reset, "do_reset"),
};

void ufs_uic_test(void)
{
    int test_flag = SUCCEED;
    dword_t round_idx = 0;
    dword_t test_loop = 1;
    dword_t uic_case;
    LOG_COLOR(SKBLU_LOG, "pls enter uic case:");
    fflush(stdout);
    scanf("%d", &uic_case);
    LOG_COLOR(SKBLU_LOG, "pls enter uic loop:");
    fflush(stdout);
    scanf("%d", &test_loop);

    LOG_INFO("test will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        LOG_INFO("loop cnt: %d\n", round_idx);
        switch (uic_case)
        {
        case 1:
            sub_case_do_uic();
            break;
        case 2:
            sub_case_do_reset();
            break;
        case 3:
            ufshcd_reset_scsi();
            break;
        default:
            test_flag = sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
            if (test_flag)
                return;
            break;
        }
    }
}
