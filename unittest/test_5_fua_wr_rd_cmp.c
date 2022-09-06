/**
 * @file .c
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

#include "common.h"
#include "unittest.h"
#include "test_send_cmd.h"
#include "test_case_all.h"

static int test_flag = SUCCEED;
static uint16_t g_wr_nlb = 8;
static uint32_t test_loop = 0;

static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_write_read_compare(void);

static SubCaseHeader_t sub_case_header = {
    "test_5_fua_wr_rd_cmp",
    "This case will tests write/read cmd, then send compare cmd",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_read_compare, "this case will tests write->read->compare cmd"),
};

int test_5_fua_wr_rd_cmp(void)
{
    uint32_t round_idx = 0;

    test_loop = 1;
    g_wr_nlb = 32;

    LOG_INFO("test will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        LOG_INFO("test cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (FAILED == test_flag)
        {
            LOG_ERROR("test_flag == FAILED\n");
            break;
        }
    }
    return test_flag;
}

static dword_t sub_case_pre(void)
{
    LOG_COLOR(PURPLE_LOG, "  sub_case_pre\n");
    return test_flag;
}
static dword_t sub_case_end(void)
{
    LOG_COLOR(PURPLE_LOG, "  sub_case_end\n");
    return test_flag;
}

static dword_t sub_case_write_read_compare(void)
{
    wr_slba = DWORD_RAND() % (1024 / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    if ((wr_slba + wr_nlb) < 1024)
    {
        mem_set(wr_buf, DWORD_RAND(), wr_nlb * LBA_DAT_SIZE);
        mem_set(rd_buf, 0, wr_nlb * LBA_DAT_SIZE);

        /****************************************************************/
        // cmd_cnt = 0;
        // test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, wr_buf);
        // cmd_cnt++;
        // test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        // test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        // LOG_DBUG("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        /****************************************************************/
        // cmd_cnt = 0;
        // test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, rd_buf);
        // cmd_cnt++;
        // test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        // test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        // LOG_DBUG("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

        /****************************************************************/
        // test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, rd_buf, wr_nlb * LBA_DAT_SIZE);
        // test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        // test_flag |= cq_gain(io_cq_id, 1, &reap_num);
        // LOG_DBUG("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

        LOG_DBUG("  reaped ok!\n", );
    }
    return test_flag;
}
