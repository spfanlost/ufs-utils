/**
 * @file test_case_all.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-12-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef _TEST_CASE_ALL_H_
#define _TEST_CASE_ALL_H_

#define LBA_DAT_SIZE 4096

extern void *rd_buf;
extern void *wr_buf;

//***new unittest framework**************************************

int test_0_full_disk_wr(void);
int test_5_fua_wr_rd_cmp(void);

#endif

