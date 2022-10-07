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
#include "test_tdm.h"
#include "test_case_all.h"
#include "test_send_cmd.h"
#include "test_send_scsi.h"
#include "test_send_uic.h"

#include "test.h"

#define UFS_BSG_UTIL_VERSION "3.12.3"

#define DISP_HELP "\033[32mtest case list:\033[0m\n"                     \
                  "case   0: Display bsg device, and do link-recovery\n" \
                  "case   1: for uic test\n"                             \
                  "case   2: for scsi cmd test\n"                        \
                  "case   3: for tm & dm test\n"                         \
                  "case   10: for test_0_full_disk_wr\n"                 \
                  "case 255: test case list exec\n"

void *rd_buf;
void *wr_buf;

static TestCase_t TestCaseList[] = {
    TCD(test_0_full_disk_wr),
};

void test_mem_alloc(void)
{
/* Allocating buffer for Read & write*/
#ifdef RW_BUF_4K_ALN_EN
    posix_memalign(&rd_buf, 4096, UFS_RW_BUFFER_SIZE);
    posix_memalign(&wr_buf, 4096, UFS_RW_BUFFER_SIZE);
#else
    rd_buf = malloc(UFS_RW_BUFFER_SIZE);
    wr_buf = malloc(UFS_RW_BUFFER_SIZE);
#endif
    if ((wr_buf == NULL) || (rd_buf == NULL))
    {
        LOG_ERROR("Malloc Failed\n");
    }
    for (size_t i = 0; i < UFS_RW_BUFFER_SIZE; i++)
    {
        MEM8_GET(wr_buf + i) = BYTE_RAND();
    }
}

void test_mem_free(void)
{
    free(rd_buf);
    free(wr_buf);
}

int main(int argc, char *argv[])
{
    int test_case, test_loop;
    struct tool_options opt = {0};

    LOG_INFO("\n****** ufs-utils unittest ver:%s ******\n", UFS_BSG_UTIL_VERSION);
    LOG_INFO("****** compile:%4d/%02d/%02d %s   ******\n", YEAR, MONTH, DAY, __TIME__);
    LOG_INFO("%s", DISP_HELP);
    if (do_get_ufs_bsg_list(NULL))
        return -1;
    RAND_INIT();
    test_mem_alloc();
    do
    {
        LOG_COLOR(SKBLU_LOG, "%d%s>", argc, argv[0]);
        fflush(stdout);
        scanf("%d", &test_case);
        switch (test_case)
        {
        case 0:
            do_get_ufs_bsg_list(NULL);
            strcpy(opt.path, "/dev/bsg/ufs-bsg");
            opt.opr = READ_ALL;
            do_get_ufs_spec_ver(&opt);
            ufshcd_link_recovery();
            break;
        case 1:
            ufs_uic_test();
            break;
        case 2:
            strcpy(opt.path, "/dev/bsg/0:0:0:0");
            test_send_scsi(&opt);
            break;
        case 3:
            ufs_test_tdm();
            break;
        case 4:
        {
            int nlb = 1;
            int file_desc = open("/dev/bsg/0:0:0:0", O_RDWR);
            send_test_unit_ready(file_desc, SG4_TYPE);
            close(file_desc);

            file_desc = open("/dev/sg3", O_RDWR);
            fflush(stdout);
            scanf("%x", &nlb);
            LOG_INFO("nlb:%x\n", nlb);
            send_scsi_write(file_desc, SG3_TYPE, 0, nlb, wr_buf);
            send_scsi_read(file_desc, SG3_TYPE, 0, nlb, rd_buf);
            close(file_desc);
            break;
        }
        case 10:
            test_0_full_disk_wr();
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
                ufs_pwr_rand();
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
    return 0;
}
