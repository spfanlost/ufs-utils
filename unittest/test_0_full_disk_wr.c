/**
 * @file test_0_full_disk_wr.c
 * @author yumeng (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2019-10-08
 *
 * @copyright Copyright (c) 2019
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>

#include "../ufs.h"

#include "common.h"
#include "unittest.h"
#include "test.h"
#include "test_send_scsi.h"
#include "test_send_cmd.h"
#include "test_send_uic.h"
#include "test_case_all.h"

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_write_read_verify(void);
static dword_t sub_case_write_read(void);

static SubCaseHeader_t sub_case_header = {
    "test_0_full_disk_wr",
    "This case will tests write/read cmd, then compare data",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_read_verify, "send write/read cmd, then compare data"),
    SUB_CASE(sub_case_write_read, "send write/read cmd"),
};

static int file_desc = 0;
static qword_t lu_cap = 0;
static dword_t wr_slba = 0;
static dword_t wr_nlb = 1;
static byte_t sg_type = SG4_TYPE;

int test_0_full_disk_wr(void)
{
    int test_flag = SUCCEED;
    uint32_t round_idx = 0;
    uint32_t test_loop = 1;

    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        LOG_INFO("loop cnt: %d\n", round_idx);
        test_flag = sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (test_flag)
            return FAILED;
    }
    return SUCCEED;
}

static dword_t sub_case_pre(void)
{
    int test_flag = SUCCEED;
    char path[PATH_MAX];
    byte_t lun = rand() % 8;
    sg_type = BYTE_RAND() % 2;
    sprintf(path, sg_type ? "/dev/sg%d" : "/dev/bsg/0:0:0:%d", sg_type ? lun + 3 : lun);
    file_desc = open(path, O_RDWR);
    if (file_desc < 0)
    {
        LOG_ERROR("open");
    }
    send_test_unit_ready(file_desc, sg_type);
    lu_cap = send_read_capacity(file_desc, sg_type);
    LOG_INFO("  lu%d_path:%s\n", lun, path);
    LOG_INFO("  lu%d_cap:%#lx\n", lun, lu_cap);
    return test_flag;
}
static dword_t sub_case_end(void)
{
    int test_flag = SUCCEED;
    close(file_desc);
    return test_flag;
}

static dword_t sub_case_write_read_verify(void)
{
    int test_flag = SUCCEED, cmp_fg;
    dword_t rand_dw = DWORD_RAND();

    if (lu_cap > 0x24)
        return SKIPED;
    wr_slba = rand_dw % (lu_cap / 2);
    wr_nlb = (rand_dw >> 4) % 0x10 + 1;

    for (size_t i = 0; i < wr_nlb * LBA_DAT_SIZE; i++)
    {
        MEM8_GET(wr_buf + i) = (byte_t)rand_dw;
    }
    memset(rd_buf, 0, wr_nlb * LBA_DAT_SIZE);

    if ((wr_slba + wr_nlb) < lu_cap)
    {
        send_scsi_write(file_desc, sg_type, wr_slba, wr_nlb, wr_buf);
        send_scsi_read(file_desc, sg_type, wr_slba, wr_nlb, rd_buf);
        cmp_fg = memcmp(wr_buf, rd_buf, wr_nlb * LBA_DAT_SIZE);
        if (cmp_fg != SUCCEED)
        {
            LOG_INFO("[E]wr_slba:%x,wr_nlb:%x\n", wr_slba, wr_nlb);
            LOG_INFO("\nwrite_buffer Data:\n");
            mem_disp(wr_buf, wr_nlb * LBA_DAT_SIZE);
            LOG_INFO("\nRead_buffer Data:\n");
            mem_disp(rd_buf, wr_nlb * LBA_DAT_SIZE);
        }
        else
        {
            LOG_INFO("  wr/rd check ok!\n");
        }
    }
    return test_flag;
}

static dword_t sub_case_write_read(void)
{
    int test_flag = SUCCEED;
    dword_t rand_dw = DWORD_RAND();
    if ((wr_slba + wr_nlb) < lu_cap)
    {
        wr_slba = rand_dw % (lu_cap / 2);
        wr_nlb = (rand_dw >> 4) % 0x80 + 1;
        send_scsi_write(file_desc, sg_type, wr_slba, wr_nlb, wr_buf);
        wr_slba = rand_dw > 1 % (lu_cap / 2);
        wr_nlb = (rand_dw >> 2) % 0x80 + 1;
        send_scsi_read(file_desc, sg_type, wr_slba, wr_nlb, rd_buf);
    }
    return test_flag;
}
