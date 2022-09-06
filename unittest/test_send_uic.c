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

// #include "auto_header.h"

#include "common.h"
#include "test_send_uic.h"

#define BSG_PATH "/dev/bsg/ufs-bsg"

extern int unipro_read(int fd, int idn, int id, __u8 all);
extern int unipro_write(int fd, int idn, int id, int mib_val,
                        int attr_set, int target);

int SetPowerMode(enum Mode mod, byte_t ger)
{
    int rt = OK;
    int fd;
    int oflag = O_RDWR;
    fd = open(BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto done;
    }

    LOG_INFO("GetPowerMode:\n");
    unipro_read(fd, PHY_ADAPTER, PA_TxGear, 0);
    unipro_read(fd, PHY_ADAPTER, PA_RxGear, 0);
    unipro_read(fd, PHY_ADAPTER, PA_PWRMode, 0);
    unipro_read(fd, PHY_ADAPTER, PA_AvailTxDataLanes, 0);
    unipro_read(fd, PHY_ADAPTER, PA_AvailRxDataLanes, 0);
    unipro_read(fd, PHY_ADAPTER, PA_ConnectedTxDataLanes, 0);
    unipro_read(fd, PHY_ADAPTER, PA_ConnectedRxDataLanes, 0);

    LOG_INFO("SetPowerMode:\n");
    if (mod == Fast || mod == FastAuto)
    {
    }
    else if (mod == Slow || mod == SlowAuto)
    {
    }
    else
    {
    }
    unipro_write(fd, PHY_ADAPTER, PA_TxGear, ger, ATTR_SET_NOR, DME_LOCAL);
    unipro_write(fd, PHY_ADAPTER, PA_RxGear, ger, ATTR_SET_NOR, DME_LOCAL);

    unipro_write(fd, PHY_ADAPTER, PA_PWRMode, (mod << 4 | mod), ATTR_SET_NOR, DME_LOCAL);
    return rt;
done:
    close(fd);
    return ERROR;
}

int ufshcd_abt(int fd, int32_t lun, int32_t task_tag)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct ufs_bsg_adm_abort *adm_abort = (struct ufs_bsg_adm_abort *)&bsg_req.upiu_req.uc;
    struct uic_command uic_rsq = {0};
    int rt = OK;
    __u8 res_code;

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

    memcpy(&uic_rsq, &bsg_rsp.upiu_rsp.uc, UIC_CMD_SIZE);
    res_code = uic_rsq.argument2 & MASK_UIC_COMMAND_RESULT;

    if (res_code)
    {
        print_error("%s: unkonw error code %d", __func__, res_code);
        rt = ERROR;
    }

out:
    return rt;
}

int ufshcd_dme_reset(__u32 reset_level)
{
    struct ufs_bsg_request bsg_req = {0};
    struct ufs_bsg_reply bsg_rsp = {0};
    struct uic_command *uic_cmd = (struct uic_command *)&bsg_req.upiu_req.uc;
    int fd;
    int oflag = O_RDWR;
    fd = open(BSG_PATH, oflag);
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
    fd = open(BSG_PATH, oflag);
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
    fd = open(BSG_PATH, oflag);
    if (fd < 0)
    {
        perror("Device open");
        goto out;
    }

    bsg_req.msgcode = UPIU_TRANSACTION_LINK_RECOVERY;

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
