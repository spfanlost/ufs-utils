/**
 * @file test_send_uic.h
 * @author yumeng (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2019-10-08
 *
 * @copyright Copyright (c) 2019
 *
 */
#ifndef _TEST_SEND_UIC_H_
#define _TEST_SEND_UIC_H_
#include <linux/types.h>
#include "common_types.h"
#include "../unipro.h"

#define PA_TxGear 0x1568
#define PA_RxGear 0x1583
#define PA_PWRMode 0x1571
#define PA_AvailTxDataLanes 0x1520
#define PA_AvailRxDataLanes 0x1540
#define PA_ConnectedTxDataLanes 0x1561
#define PA_ConnectedRxDataLanes 0x1581

#define UPIU_TRANSACTION_ADM_ABORT 0x8000
#define UPIU_TRANSACTION_LINK_RECOVERY 0x8001

#define Cold_Reset 0x00
#define Warm_Reset 0x01

struct ufs_bsg_adm_abort
{
    int32_t lun;
    int32_t task_tag;
};

enum Mode
{
    Fast = 1,
    Slow = 2,
    FastAuto = 4,
    SlowAuto = 5,
    Unchanged = 7
};

int SetPowerMode(enum Mode mod, byte_t ger);
int ufshcd_abt(int fd, int32_t lun, int32_t task_tag);
int ufshcd_abt(int fd, int32_t lun, int32_t task_tag);
int ufshcd_dme_reset(__u32 reset_level);
int ufshcd_dme_cmd(enum uic_cmd_dme uic_cmd_dme);
int ufshcd_link_recovery(void);

#endif
