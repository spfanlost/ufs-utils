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

#include "common.h"
#include "unittest.h"
#include "test_send_cmd.h"
#include "test_case_all.h"

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_write_order(void);
static dword_t sub_case_write_random(void);
static dword_t sub_case_read_order(void);
static dword_t sub_case_read_random(void);
static dword_t sub_case_write_read_verify(void);

static SubCaseHeader_t sub_case_header = {
    "test_0_full_disk_wr",
    "This case will tests write/read cmd, then compare data",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_order, "send write cmd by order"),
    SUB_CASE(sub_case_write_random, "random slba/nlb send write cmd"),
    SUB_CASE(sub_case_read_order, "send read cmd by order"),
    SUB_CASE(sub_case_read_random, "random slba/nlb send read cmd"),
    SUB_CASE(sub_case_write_read_verify, "send write/read cmd, then compare data"),
};

int test_0_full_disk_wr(void)
{
    int test_flag = SUCCEED;
    uint32_t round_idx = 0;
    uint32_t test_loop = 1;

    LOG_INFO("test will loop number: %d\n", test_loop);
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
    return test_flag;
}
static dword_t sub_case_end(void)
{
    int test_flag = SUCCEED;
    return test_flag;
}

static dword_t sub_case_write_order(void)
{
    int test_flag = SUCCEED;
    return test_flag;
}

static dword_t sub_case_write_random(void)
{
    int test_flag = SUCCEED;
    return test_flag;
}

static dword_t sub_case_read_order(void)
{
    int test_flag = SUCCEED;
    return test_flag;
}

static dword_t sub_case_read_random(void)
{
    int test_flag = SUCCEED;
    return test_flag;
}

static dword_t sub_case_write_read_verify(void)
{
    int test_flag = SUCCEED;
#if 0
    static dword_t patcnt;
    // memset(write_buffer, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
    // memset(read_buffer, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));
    for (uint32_t i = 0; i < 16; i++)
    {
        for (dword_t idx = 0; idx < (16*1024); idx += 4)
        {
            *((dword_t *)(write_buffer + idx + (16*1024*i))) = ((idx<<16)|patcnt);//DWORD_RAND();
        }
        patcnt++;
    }
    memset(read_buffer, 0, 16*1024*16);

    wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//WORD_RAND() % 255 + 1;

    cmd_cnt = 0;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer+(16*1024*i));
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, cmd_cnt);
    if(test_flag<0)
        goto OUT;
    // LOG_INFO("  cq:%d wr cnt:%d\n", io_cq_id, cmd_cnt);
    
    cmd_cnt = 0;
    wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//WORD_RAND() % 255 + 1;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer+(16*1024*i));
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, cmd_cnt);
    if(test_flag<0)
        goto OUT;
    // LOG_INFO("  cq:%d rd cnt:%d\n", io_cq_id, cmd_cnt);

    cmp_fg = memcmp(write_buffer, read_buffer, 16*1024*16);
    test_flag |= cmp_fg;
    if (cmp_fg != SUCCEED)
    {
        LOG_INFO("\nwrite_buffer Data:\n");
        mem_disp(write_buffer, 16*1024*16);
        LOG_INFO("\nRead_buffer Data:\n");
        mem_disp(read_buffer, 16*1024*16);
        //break;
    }
    //#else
    for (uint32_t i = 0; i < 16; i++)
    {
        wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = 32;//WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            cmd_cnt++;
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;
            
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;

            cmp_fg = memcmp(write_buffer, read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
            test_flag |= cmp_fg;
            if (cmp_fg != SUCCEED)
            {
                LOG_INFO("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
                LOG_INFO("\nwrite_buffer Data:\n");
                mem_disp(write_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                LOG_INFO("\nRead_buffer Data:\n");
                mem_disp(read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                //break;
            }
        }
    }
    LOG_INFO("  cq:%d wr/rd check ok! cnt:%d\n", io_cq_id, cmd_cnt);
#endif
    return test_flag;
}