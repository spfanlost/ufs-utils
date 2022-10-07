
#ifndef _TEST_H_
#define _TEST_H_

#include "auto_header.h"
#include "test_case_all.h"

enum
{
    MANU_NAME_INDEX = 0,
    PROD_NAME_INDEX = 1,
    SERI_NUMB_INDEX = 2,
    OEMID_INDEX = 3,
    PROD_REVI_INDEX = 4,
    MAX_INDEX,
};

#define LBA_DAT_SIZE 4096

extern void *rd_buf;
extern void *wr_buf;

#endif
