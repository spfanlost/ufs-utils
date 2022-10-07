/**
 * @file common_define.h
 * @author meng_yu (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2020-06-06
 *
 * @copyright imyumeng@qq.com Copyright (c) 2020
 *
 */
#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

// #define FAILED (0)
// #define SUCCEED (1)
#define FAILED (-1)
#define SUCCEED (0)

#define DISABLE (0)
#define ENABLE (1)

#define TRUE (1)
#define FALSE (0)

#ifndef NULL
#define NULL (0)
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MEM32_GET(n) (*(volatile dword_t *)(n))
#define MEM16_GET(n) (*(volatile word_t *)(n))
#define MEM8_GET(n) (*(volatile byte_t *)(n))

#define RAND_INIT() (srand(time(NULL)))
#define BYTE_RAND() ((byte_t)rand())
#define WORD_RAND() ((word_t)rand())
#define DWORD_RAND() ((dword_t)rand())
#define RAND_RANGE(s, e) (rand() % ((e) - (s) + 1) + (s))

#define BYTE_MASK (0xFF)
#define WORD_MASK (0xFFFF)
#define DWORD_MASK (0xFFFFFFFF)
#define QWORD_MASK (0xFFFFFFFFFFFFFFFF)

#define BYTE2DWORD(byte3, byte2, byte1, byte0) \
    ((dword_t)(byte3) << 24 | (dword_t)(byte2) << 16 | (dword_t)(byte1) << 8 | (dword_t)(byte0))

#define BYTE2QWORD(byte7, byte6, byte5, byte4, byte3, byte2, byte1, byte0)                               \
    ((qword_t)(byte7) << 56 | (qword_t)(byte6) << 48 | (qword_t)(byte5) << 40 | (qword_t)(byte4) << 32 | \
     (qword_t)(byte3) << 24 | (qword_t)(byte2) << 16 | (qword_t)(byte1) << 8 | (qword_t)(byte0))

#define PRINT_IF printf

#define _VAL(x) #x
#define _STR(x) _VAL(x)

#define YEAR ((((__DATE__[7] - '0') * 10 + (__DATE__[8] - '0')) * 10 + (__DATE__[9] - '0')) * 10 + (__DATE__[10] - '0'))

#define MONTH (__DATE__[2] == 'n'   ? (__DATE__[1] == 'a' ? 1 : 6) \
               : __DATE__[2] == 'b' ? 2                            \
               : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4) \
               : __DATE__[2] == 'y' ? 5                            \
               : __DATE__[2] == 'l' ? 7                            \
               : __DATE__[2] == 'g' ? 8                            \
               : __DATE__[2] == 'p' ? 9                            \
               : __DATE__[2] == 't' ? 10                           \
               : __DATE__[2] == 'v' ? 11                           \
                                    : 12)

#define DAY ((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))

#endif
