/**
 * @file test_send_scsi.h
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef _TEST_SEND_SCSI_H_
#define _TEST_SEND_SCSI_H_
#include <linux/types.h>
#include "../options.h"

/*
#define UFS_CDB_SIZE	16

#define BUFFER_VENDOR_MODE 0x01
#define BUFFER_DATA_MODE 0x02
#define BUFFER_FFU_MODE 0x0E
#define BUFFER_EHS_MODE 0x1C

#define SG_DXFER_NONE -1        // e.g. a SCSI Test Unit Ready command 
#define SG_DXFER_TO_DEV -2      // e.g. a SCSI WRITE command 
#define SG_DXFER_FROM_DEV -3    // e.g. a SCSI READ command 

#define SENSE_BUFF_LEN	(18)
#define WRITE_BUF_CMDLEN (10)
#define READ_BUF_CMDLEN (10)
*/
#define MODE_SELECT_CMDLEN (10)
#define MODE_SENSE_CMDLEN (10)
#define INQUIRY_CMDLEN (6)
#define READ_CAPACITY10_CMDLEN	(10)
#define READ_CAPACITY16_CMDLEN	(16)
#define START_STOP_UNIT_CMDLEN	(6)
#define TEST_UNIT_READY_CMDLEN	(6)
#define REPORT_LUNS_CMDLEN	(12)
#define VERIFY_CMDLEN	(10)
#define REQUEST_SENSE_CMDLEN	(6)
#define FORMAT_CMDLEN	(6)
#define PRE_FETCH10_CMDLEN	(10)
#define PRE_FETCH16_CMDLEN	(16)
#define SEND_DIAGNOSTIC_CMDLEN  (6)
#define SYNCHRONIZE_CACHE10_CMDLEN (10)
#define SYNCHRONIZE_CACHE16_CMDLEN (16)
#define UNMAP_CMDLEN (10)
#define READ6_LEN (6)
#define READ10_LEN (10)
#define READ16_LEN (16)
#define WRITE6_LEN (6)
#define WRITE10_LEN (10)
#define WRITE16_LEN (16)
#define HPB_READ_BUF_CMDLEN (10)
#define HPB_WRITE_BUF_CMDLEN (10)
/*
#define SEC_PROTOCOL_TIMEOUT_MSEC	(1000)
#define SEC_PROTOCOL_CMD_SIZE		(12)
#define SEC_PROTOCOL_UFS		(0xEC)
#define SEC_SPECIFIC_UFS_RPMB		(0x0001)

#define WRITE_BUFFER_CMD (0x3B)
#define READ_BUFFER_CMD (0x3c)
*/
#define	INQUIRY_CMD	(0x12)
#define MODE_SELECT_CMD (0x55)
#define MODE_SENSE_CMD	(0x5a)
#define READ_CAPACITY10_CMD	(0x25)
#define READ_CAPACITY16_CMD	(0x9e)
#define READ_CAPACITY10_CMD	(0x25)
#define START_STOP_UNIT_CMD	(0x1b)
#define TEST_UNIT_READY_CMD	(0x0)
#define REPORT_LUNS_CMD	(0xa0)
#define VERIFY_CMD	(0x2f)
#define REQUEST_SENSE_CMD	(0x03)
#define	FORMAT_CMD	(0x04)
#define PRE_FETCH10_CMD	(0x34)
#define PRE_FETCH16_CMD	(0x90)
//#define SECURITY_PROTOCOL_IN  (0xa2)
//#define SECURITY_PROTOCOL_OUT (0xb5)
#define SEND_DIAGNOSTIC_CMD  (0x1d)
#define SYNCHRONIZE_CACHE10_CMD (0x35)
#define SYNCHRONIZE_CACHE16_CMD (0x91)
#define UNMAP_CMD (0x42)
#define READ6_CMD (0x08)
#define READ10_CMD (0x28)
#define READ16_CMD (0x88)
#define WRITE6_CMD (0x0a)
#define WRITE10_CMD (0x2a)
#define WRITE16_CMD (0x8a)
#define HPB_READ_BUF_CMD (0xf9)
#define HPB_WRITE_BUF_CMD (0xfa)

#define SEC_SPEC_OFFSET 2
#define SEC_TRANS_LEN_OFFSET 6
/*struct rpmb_frame {
	__u8  stuff[196];
	__u8  key_mac[32];
	__u8  data[256];
	__u8  nonce[16];
	__u32 write_counter;
	__u16 addr;
	__u16 block_count;
	__u16 result;
	__u16 req_resp;
};

struct inquiry_standard_rsp
{
	__u8	peripheral;
	__u8	rmb;
	__u8	version;
	__u8	rsp_data_format;
	__u8	addition_len;
	__u8	rsvd[2];
	__u8	cmdque;
	__CHAR_BIT__	vendor_idfy;
	__CHAR_BIT__	product_idfy;
	__CHAR_BIT__	product_rvsn_level;
};*/
enum page_code
{
	RW_ERR_RECOVERY = 0x01,
	CACHING			= 0x08,
	CONTROL	 		= 0x0A,
	ALL_PAGE		= 0x3F,
};

typedef struct _sense_data
{
	/* data */
	__u8	sense_key;
	__u8	asc;
	__u8	ascq;
}sense_data_t;

int send_scsi_cmd_maxio(int fd, const __u8 *cdb, void *buf, __u8 cmd_len,
		__u32 byte_cnt, int dir, __u8 sg_type);
void test_send_scsi(struct tool_options *opt);
__u64 read_capacity_for_cap(__u8 fd);
int scsi_write(int fd, __u8 *write_rsp, __u8 opcode, __u8 dpo, __u8 fua, __u8 grp_num,
 			__u64 lba, __u32 transfer_len,__u32 byte_count, __u8 sg_type);
#endif
