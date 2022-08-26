/**
 * @file test_send_scsi.c
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

#include "common.h"
#include "test.h"
#include "../ioctl.h"
#include "../ufs.h"
#include "../ufs_rpmb.h"
#include "../options.h"
// #include "auto_header.h"
#include "test_send_scsi.h"

static const char *const snstext[] = {
	"No Sense",	    /* 0: There is no sense information */
	"Recovered Error",  /* 1: The last command completed successfully
				  but used error correction */
	"Not Ready",	    /* 2: The addressed target is not ready */
	"Medium Error",	    /* 3: Data error detected on the medium */
	"Hardware Error",   /* 4: Controller or device failure */
	"Illegal Request",  /* 5: Error in request */
	"Unit Attention",   /* 6: Removable medium was changed, or
				  the target has been reset, or ... */
	"Data Protect",	    /* 7: Access to the data is blocked */
	"Blank Check",	    /* 8: Reached unexpected written or unwritten
				  region of the medium */
	"Vendor Specific",
	"Copy Aborted",	    /* A: COPY or COMPARE was aborted */
	"Aborted Command",  /* B: The target aborted the command */
	"Equal",	    /* C: A SEARCH DATA command found data equal */
	"Volume Overflow",  /* D: Medium full with still data to be written */
	"Miscompare",	    /* E: Source data and data on the medium
				  do not agree */
};

sense_data_t g_sense_data = {0};

static inline void put_unaligned_be16(__u8 val, void *p)
{
	((__u8 *)p)[0] = (val >> 8) & 0xff;
	((__u8 *)p)[1] = val & 0xff;
}

static inline void put_unaligned_be24(__u32 val, void *p)
{
	((__u8 *)p)[0] = (val >> 16) & 0xff;
	((__u8 *)p)[1] = (val >> 8) & 0xff;
	((__u8 *)p)[2] = val & 0xff;
}

static inline void put_unaligned_be32(__u32 val, void *p)
{
	((__u8 *)p)[0] = (val >> 24) & 0xff;
	((__u8 *)p)[1] = (val >> 16) & 0xff;
	((__u8 *)p)[2] = (val >> 8) & 0xff;
	((__u8 *)p)[3] = val & 0xff;
}

static inline void put_unaligned_be64(__u64 val, void *p)
{
	((__u8 *)p)[0] = (val >> 56) & 0xff;
	((__u8 *)p)[1] = (val >> 48) & 0xff;
	((__u8 *)p)[2] = (val >> 40) & 0xff;
	((__u8 *)p)[3] = (val >> 32) & 0xff;
	((__u8 *)p)[4] = (val >> 24) & 0xff; 
	((__u8 *)p)[5] = (val >> 16) & 0xff;
	((__u8 *)p)[6] = (val >> 8) & 0xff;
	((__u8 *)p)[7] = val & 0xff;
}

static const char *sense_key_string(__u8 key)
{
	if (key <= 0xE)
		return snstext[key];

	return NULL;
}
/**
 * send_scsi_cmd_maxio - Utility function for SCSI command sending
 * @fd: bsg driver file descriptor
 * @cdb: pointer to SCSI cmd cdb buffer
 * @buf: pointer to the SCSI cmd data buffer
 * @cmd_len: SCSI command length
 * @byte_cnt: SCSI data length
 * @dir: The cmd direction
 *
 **/
int send_scsi_cmd_maxio(int fd, const __u8 *cdb, void *buf, __u8 cmd_len,
		__u32 byte_cnt, int dir, __u8 sg_type)
{
	int ret;
	void *sg_struct;
	struct sg_io_v4 io_hdr_v4 = { 0 };
	struct sg_io_hdr io_hdr_v3 = { 0 };
	__u8 sense_buffer[SENSE_BUFF_LEN] = { 0 };

	if ((byte_cnt && buf == NULL) || cdb == NULL) {
		print_error("send_scsi_cmd_maxio: wrong parameters");
		return -EINVAL;
	}

	if (sg_type == SG4_TYPE) {
		io_hdr_v4.guard = 'Q';
		io_hdr_v4.protocol = BSG_PROTOCOL_SCSI;
		io_hdr_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
		io_hdr_v4.response = (__u64)sense_buffer;
		io_hdr_v4.max_response_len = SENSE_BUFF_LEN;
		io_hdr_v4.request_len = cmd_len;
		if (dir == SG_DXFER_FROM_DEV) {
			io_hdr_v4.din_xfer_len = (__u32)byte_cnt;
			io_hdr_v4.din_xferp = (__u64)buf;
		} else {
			io_hdr_v4.dout_xfer_len = (__u32)byte_cnt;
			io_hdr_v4.dout_xferp = (__u64)buf;
		}
			io_hdr_v4.request = (__u64)cdb;
		sg_struct = &io_hdr_v4;
	}
	else {
		io_hdr_v3.interface_id = 'S';
		io_hdr_v3.cmd_len = cmd_len;
		io_hdr_v3.mx_sb_len = SENSE_BUFF_LEN;
		io_hdr_v3.dxfer_direction = dir;
		io_hdr_v3.dxfer_len = byte_cnt;
		io_hdr_v3.dxferp = buf;
		/* pointer to command buf (rbufCmdBlk) */
		io_hdr_v3.cmdp = (unsigned char *)cdb;
		io_hdr_v3.sbp = sense_buffer;
		io_hdr_v3.timeout = DEF_TIMEOUT_MSEC;
		sg_struct = &io_hdr_v3;
	}
	WRITE_LOG("Start : %s cmd = %x len %d sg_type %d\n", __func__, cdb[0],
			byte_cnt, sg_type);

	// write_file_with_counter("scsi_cmd_cdb_%d.bin",
	// 		cdb, cmd_len);
	while (((ret = ioctl(fd, SG_IO, sg_struct)) < 0) &&
		((errno == EINTR) || (errno == EAGAIN)));
	g_sense_data.sense_key = 0; 
	g_sense_data.asc = 0;
	g_sense_data.ascq = 0;
	if (sg_type == SG4_TYPE) {
		if (io_hdr_v4.info != 0) {
			print_error("Command fail with status %x , senseKey %s, asc 0x%02x, ascq 0x%02x",
				    io_hdr_v4.info,
				    sense_key_string(sense_buffer[2]),
				    sense_buffer[12],
				    sense_buffer[13]);
			g_sense_data.sense_key = sense_buffer[2]; 
			g_sense_data.asc = sense_buffer[12];
			g_sense_data.ascq = sense_buffer[13];
			ret = -EINVAL;
		}
	}
	else {
		if (io_hdr_v3.status) {
			print_error("Command fail with status %x , senseKey %s, asc 0x%02x, ascq 0x%02x",
				    io_hdr_v3.status,
				    sense_key_string(sense_buffer[2]),
				    sense_buffer[12],
				    sense_buffer[13]);
			g_sense_data.sense_key = sense_buffer[2]; 
			g_sense_data.asc = sense_buffer[12];
			g_sense_data.ascq = sense_buffer[13];
			ret = -EINVAL;
		}

	}

	return ret;
}

int format_unit(int fd, __u8 *format_rsp, __u8 longlist,
 			__u8 cmplst,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char format_cmd [FORMAT_CMDLEN] = {
		FORMAT_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0) {
		perror("scsi format cmd: wrong parameters\n");
		return -EINVAL;
	}
	format_cmd[1] = ((0x20 & (longlist << 5)) || (0x8 & (cmplst << 3)));
	for(int i = 0; i< FORMAT_CMDLEN; i++)
	{
		LOG_INFO("format_cmd[%d]: %x\n", i, format_cmd[i]);
	}
	LOG_INFO("Start : %s longlist %d , cmplst %d\n", __func__, longlist, cmplst);
	ret = send_scsi_cmd_maxio(fd, format_cmd, format_rsp, FORMAT_CMDLEN, byte_count,
			SG_DXFER_NONE, sg_type);	

	if (ret < 0) 
	{
		if(((0 != longlist) || (0 != cmplst)) && (-22 == ret))
		{
			//print_error("SG_IO format data error ret %d\n", ret);
			// ret = 1;
		}
		else{
			print_error("SG_IO format data error ret %d\n", ret);
		}
	}
	return ret;	
}

void format_unit_test(__u8 fd)
{
	__u32 i = 0;
	int error_status = 0, ret = 1;
	__u8 longlist = 0;
 	__u8 cmplst = 0;
	__u8 randdata = 0;
	__u8 format_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		randdata = (rand() % 0x10);
		if(0x9 == randdata)
		{
			do
			{
				longlist = (rand() % 0x2);
				cmplst = (rand() % 0x2);
			}
			while((0 == longlist) && (0 == cmplst));
		}
		else
		{
			longlist = 0;
			cmplst = 0;
		}
		LOG_INFO("%s i %x randdata %d\n", __func__, i, randdata);
		error_status = format_unit(fd, format_rsp, longlist,
 			cmplst, 0, SG4_TYPE);
		if((0 != longlist) || (0 != cmplst))
		{
			 if(-22 != error_status)
			{
				LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(error_status < 0)
			{
				LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int mode_select(int fd, __u8 *mode_select_req, __u8 sp, __u8 pf, __u16 para_list_len, 
				__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char mode_select_cmd [MODE_SELECT_CMDLEN] = {
		MODE_SELECT_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		perror("scsi mode_select cmd: wrong parameters\n");
		return -EINVAL;
	}
	mode_select_cmd[1] = ((sp & 0x1) | ((pf & 0x1) << 4));
	put_unaligned_be16(para_list_len, (mode_select_cmd + 7));
	for(int i = 0; i< MODE_SELECT_CMDLEN; i++)
	{
		LOG_INFO("mode_select_cmd[%d]: %x\n", i, mode_select_cmd[i]);
	}
		LOG_INFO("\n%s sp %d para_list_len %x\n", __func__, sp, para_list_len);
	ret = send_scsi_cmd_maxio(fd, mode_select_cmd, mode_select_req,
			WRITE_BUF_CMDLEN, byte_count, SG_DXFER_TO_DEV, sg_type);
	if (ret < 0) {
		print_error("SG_IO mode_select data error ret %d\n", ret);
	}
	return ret;
}

void mode_select_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1;
	__u8 sp = 0, pf = 0, page_code = 0, para_list_len = 0;
	__u8 randdata = 0;
	__u32 byte_count = 0;
	__u8 mode_select_req[100] = {0};
	LOG_INFO("\nStart : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d \n", __func__, i);
		sp = (rand() % 0x2);
		page_code = (rand() % 0x4);
		pf = 1;
		//page_code = 2;
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(0 == page_code)
		{
			para_list_len = 0x14;
			byte_count = 0x14;
			for(int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (RW_ERR_RECOVERY & 0x3f);//rw_err_recovery
			mode_select_req[9] = 0x0a;
			mode_select_req[10] = 0x80;
			mode_select_req[11] = 0x03;
			mode_select_req[16] = 0x03;
			mode_select_req[18] = 0xff;
			mode_select_req[19] = 0xff;
		}
		else if(1 == page_code)
		{
			para_list_len = 0x1c;
			byte_count = 0x1c;
			for(int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (CACHING & 0x3f);//caching
			mode_select_req[9] = 0x12;
			mode_select_req[10] = 0x4;
		}
		else if(2 == page_code)
		{
			para_list_len = 0x14;
			byte_count = 0x14;
			for(int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (CONTROL & 0x3f);//control
			mode_select_req[9] = 0x0a;
			mode_select_req[11] = 0x10;
			mode_select_req[16] = 0xff;
			mode_select_req[17] = 0xff;
		}
		else
		{
			para_list_len = 0xFF;
			byte_count = 0x34;
			for(int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			// mode_select_req[8] = (ALL_PAGE & 0x3f);//all_page
			// mode_select_req[9] = 0x26;
			mode_select_req[8] = (RW_ERR_RECOVERY & 0x3f);//rw_err_recovery
			mode_select_req[9] = 0x0a;
			mode_select_req[10] = 0x80;
			mode_select_req[11] = 0x03;
			mode_select_req[16] = 0x03;
			mode_select_req[18] = 0xff;
			mode_select_req[19] = 0xff;
			mode_select_req[20] = (CACHING & 0x3f);//caching
			mode_select_req[21] = 0x12;
			mode_select_req[22] = 0x5;
			mode_select_req[40] = (CONTROL & 0x3f);//control
			mode_select_req[41] = 0x0a;
			mode_select_req[43] = 0x10;
			mode_select_req[48] = 0xff;
			mode_select_req[49] = 0xff;
		}
		if(9 == randdata)
		{
			while((mode_select_req[8] == (RW_ERR_RECOVERY & 0x3f)) || (mode_select_req[8] == (CACHING & 0x3f)) \
			|| (mode_select_req[8] == (CONTROL & 0x3f)))
			{
				mode_select_req[8] = (rand() % 0x20) & 0x3f;
			}
			LOG_INFO("%s rand9 opcode: %x\n", __func__, mode_select_req[8]);
		}
		else if(8 == randdata)
		{
			while((para_list_len == 0x14) || (para_list_len == 0x1c) || (para_list_len == 0xff))
			{
				para_list_len = (rand() % 0x30);
				byte_count = para_list_len;
			}
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, para_list_len);
		}
		else if(7 == randdata)
		{
			while((mode_select_req[9] == 0xa) || (mode_select_req[9] == 0x12))
			{
				mode_select_req[9] = (rand() % 0x20);
				LOG_INFO("%s rand7 page_length: %x\n", __func__, mode_select_req[9]);
			}
		}
		else if(6 == randdata)
		{
			pf = 0;
			LOG_INFO("%s rand7 pf: %x\n", __func__, pf);
		}
		else{
			;
		}
		// LOG_INFO("\nsuccess:\n");
		for(int i = 0; i< byte_count; i++)
		{
			LOG_INFO("mode_select_req[%d]: %x\n", i, mode_select_req[i]);
		}
		LOG_INFO("\n");

		error_status = mode_select(fd, mode_select_req, sp, pf, para_list_len, 
				byte_count, SG4_TYPE);
		if(1 == err_event)
		{
			if(9 == randdata || 7 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x26 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/7 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9/7 sense_data pass\n", __func__);
				}
			}
			else if(8 == randdata)
			{
				LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
			}
			else if(6 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata6 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata6 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}

		}
		else
		{
			if(error_status < 0)
			{
				LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int mode_sense(int fd, __u8 *mode_sense_rsp, __u8 page_code, __u8 pc, 
				__u8 subpage_code, __u16 alloc_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char mode_sense_cmd [MODE_SENSE_CMDLEN] = {
		MODE_SENSE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0) {
		perror("scsi mode_sense cmd: wrong parameters\n");
		return -EINVAL;
	}
	mode_sense_cmd[1] = 0x8;
	mode_sense_cmd[2] = (0xc0 & (pc << 6 )) | (0x3f & page_code);
	put_unaligned_be16(alloc_len, (mode_sense_cmd + 7));
	for(int i = 0; i< 10; i++)
	{
		LOG_INFO("mode_sense_cmd[%d]: %x\n", i, mode_sense_cmd[i]);
	}
	LOG_INFO("Start : %s pc %d page_code %x subpage_code %x alloc_len %x\n", __func__, pc, page_code, subpage_code, alloc_len);
	ret = send_scsi_cmd_maxio(fd, mode_sense_cmd, mode_sense_rsp, MODE_SENSE_CMDLEN, byte_count,
			SG_DXFER_FROM_DEV, sg_type);	

	if (ret < 0) {
		print_error("SG_IO mode_sense data error ret %d\n", ret);
	}
	return ret;	
}

void mode_sense_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	__u8 page_code = 0, pc = 0, subpage_code = 0;
	__u16 alloc_len = 0;
	__u32 byte_count = 0;
	__u8 randdata = 0;
	//__u8 mode_sense_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		page_code = (rand() % 0x4);
		pc = (rand() % 0x4);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(0 == page_code)
		{
			page_code = RW_ERR_RECOVERY;
			alloc_len = 0x14;
			byte_count = 0x14;
		}
		else if(1 == page_code)
		{
			page_code = CACHING;
			alloc_len = 0x1c;
			byte_count = 0x1c;
		}
		else if(2 == page_code)
		{
			page_code = CONTROL;
			alloc_len = 0x14;
			byte_count = 0x14;
		}
		else 
		{
			page_code = ALL_PAGE;
			alloc_len = 0xFF;
			byte_count = 0xff;
		}
		if(9 == randdata)
		{
			while((page_code == RW_ERR_RECOVERY) || (page_code == CACHING) \
			|| (page_code == CONTROL) || (page_code == ALL_PAGE))
			{
				page_code = (rand() % 0x40);
			}
			LOG_INFO("%s rand9 page_code: %x\n", __func__, page_code);
		}
		else if(8 == randdata)
		{
			while((alloc_len == 0x14) || (alloc_len == 0x1c) || (alloc_len == 0xff))
			{
				alloc_len = (rand() % 0x30);
				byte_count = alloc_len;
			}
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
		}
		else{
			;
		}
		error_status = mode_sense(fd, rd_buf, page_code, pc, 
				subpage_code, alloc_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else if(8 == randdata)
			{
				LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		LOG_INFO("\nsuccess:\n");
		for(int i = 0; i< byte_count; i++)
		{
			LOG_INFO("mode_sense_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
		}
		LOG_INFO("\n");

	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int prefetch(int fd, __u8 *prefetch_rsp, __u8 opcode, __u8 immed,
 			__u64 lba, __u32 prefetch_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 prefetch_cmd_len = 0;
	unsigned char prefetch_cmd10 [PRE_FETCH10_CMDLEN] = {
		PRE_FETCH10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char prefetch_cmd16 [PRE_FETCH16_CMDLEN] = {
		PRE_FETCH16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;	
	if(PRE_FETCH10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (prefetch_cmd10 + 2));
		put_unaligned_be16((uint16_t)prefetch_len, (prefetch_cmd10 + 7));
		prefetch_cmd_len = PRE_FETCH10_CMDLEN;
		prefetch_cmd10[1] = (immed & 0x1) << 1;
		cmd_addr = prefetch_cmd10;
		for(int i = 0; i< PRE_FETCH10_CMDLEN; i++)
		{
			LOG_INFO("prefetch_cmd10[%d]: %x\n", i, prefetch_cmd10[i]);
		}
	}
	else if(PRE_FETCH16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (prefetch_cmd16 + 2));
		put_unaligned_be32((uint32_t)prefetch_len, (prefetch_cmd16 + 10));
		prefetch_cmd_len = PRE_FETCH16_CMDLEN;
		prefetch_cmd16[1] = (immed & 0x1) << 1;
		cmd_addr = prefetch_cmd16;
		for(int i = 0; i< PRE_FETCH16_CMDLEN; i++)
		{
			LOG_INFO("prefetch_cmd16[%d]: %x\n", i, prefetch_cmd16[i]);
		}
	}
	else
	{
		perror("scsi prefetch cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	if (fd < 0 || byte_count < 0) {
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	LOG_INFO("Start : %s opcode %d, immed %d, lba %lld, prefetch_len %d\n", 
			__func__, opcode, immed, lba, prefetch_len);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, prefetch_rsp, prefetch_cmd_len, byte_count,
			SG_DXFER_NONE, sg_type);	

	if (ret < 0) {
		print_error("SG_IO prefetch data error ret %d\n", ret);
	}
	return ret;	
}

void prefetch_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;;
	__u8 immed = 0, opcode = 0;
	__u64 lba = 0;
	__u32 prefetch_len = 0;
	__u32 byte_count = 0;
	__u64 cap = 0;
	__u8 randdata = 0;
	__u8 prefetch_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		immed = (rand() % 0x2);
		opcode = (rand() % 0x2) + 1;
		lba = (rand() % 0x100);
		prefetch_len = (rand() % 0x100);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(1 == opcode)
		{
			opcode = PRE_FETCH10_CMD;
		}
		else if(2 == opcode)
		{
			opcode = PRE_FETCH16_CMD;
		}
		else
		{
			opcode = 0xff;
		}

		if(9 == randdata)
		{
			cap = read_capacity_for_cap(fd);
			lba = cap + 1;
			prefetch_len = 0;
			LOG_INFO("%s rand9 lba: %llx\n", __func__, lba);
		}
		else if(8 == randdata)
		{
			cap = read_capacity_for_cap(fd);
			while((lba + prefetch_len ) <= (cap + 1))
			{
				lba = cap - (rand() % 0x50);
				prefetch_len = (rand() % 0x100);
			}
			LOG_INFO("%s rand8 lba: %llx prefetch_len: %x\n", __func__, lba, prefetch_len);
		}
		else{
			;
		}
		error_status = prefetch(fd, prefetch_rsp, opcode, immed,
 			lba, prefetch_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/8 sense_data err randdata %d\n", __func__, randdata);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}



int scsi_read_buffer(int fd, __u8 *buf, __u8 mode, __u8 buf_id,
	__u32 buf_offset,__u32 byte_count, __u8 sg_type)
{

	int ret;
	unsigned char read_buf_cmd[READ_BUF_CMDLEN] = {READ_BUFFER_CMD,
		0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi read cmd: wrong parameters\n");
		return -EINVAL;
	}

	read_buf_cmd[1] = (mode & 0x1f);
	read_buf_cmd[2] = buf_id;
	put_unaligned_be24((__u32)buf_offset, read_buf_cmd + 3);
	put_unaligned_be24((__u32)byte_count, read_buf_cmd + 6);
	for(int i = 0; i< READ_BUF_CMDLEN; i++)
	{
		LOG_INFO("read_buf_cmd[%d]: %x\n", i, read_buf_cmd[i]);
	}
	LOG_INFO("Start : %s buf_offset %x buf_id %x byte_count %x\n", \
			__func__, buf_offset, buf_id, byte_count);
	ret = send_scsi_cmd_maxio(fd, read_buf_cmd, buf,
			READ_BUF_CMDLEN, byte_count,
			SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO READ BUFFER data error ret %d\n", ret);
	}

	return ret;
}

void read_buffer_test(__u8 fd)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j =0;
	__u8 mode = 0xff, buf_id = 0;
	__u8 buf_id_max = 0, buf_id_flag = 0;
	__u32 buf_offset = 0;
	__u32 byte_count = 0;
	// __u8 read_buffer_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		if(0xff == mode)
		{
			mode = (rand() % 0x2);//expect for vendor mode
		}
		mode = 1;
		if(0 == mode)
		{
			mode = BUFFER_EHS_MODE;//error history
			buf_id = (rand() % 0x2);
			if(0 == buf_id)
			{
				buf_offset = 0;
			}
			else if(1 == buf_id)
			{
				buf_id = (rand() % 0xe0) + 0x10;
				buf_offset = (rand() % 0xff);
			}
			byte_count = 5;
		}
		else if(1 == mode)
		{
			mode = BUFFER_DATA_MODE;//data
			buf_offset = (rand() % 0x30);

			if(0 == buf_id_flag)
			{
				buf_id = (rand() % 0x2);
				if(0 == buf_id)
				{
					buf_id_max = (rand() % 0x2) + 1;
				}
			}
			if(buf_id_max > 1)
			{
				buf_id_flag = 1;
				if((buf_id == (buf_id_max - 1)) || (buf_id > 1))
				{
					buf_id_flag = 0;
					buf_id = (rand() % 0x2);
				}
				else
				{
					buf_id++;
				}
			}
			else
			{
				buf_id_flag = 0;
			}
			byte_count = (rand() % 0x50) + 1;
		}
		else if(2 == mode)
		{
			mode = BUFFER_VENDOR_MODE;//vendor specific
			LOG_ERROR("stop %s: expect for vendor mode\n", __func__);
			// buf_id = ;
			// buf_offset = ;
			// byte_count = ;
		}
		else
		{
			;
		}
		error_status = scsi_read_buffer(fd, rd_buf, mode, buf_id,
			buf_offset, byte_count, SG4_TYPE);
		if(error_status < 0)
		{
			LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
			ret = -1;
			break;
		}
		else
		{
			LOG_INFO("\nsuccess:\n");
			for(j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("rd_buf[%d]:", (j * 10));
				for(int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if((byte_count % 10) > 0)
			{
				LOG_INFO("rd_buf[%d]:", (j * 10));
				for(int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
					j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}



int read_capacity(int fd, __u8 *read_capacity_rsp, __u8 opcode,
 			__u32 lba, __u32 alloc_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 read_capacity_cmd_len = 0;
	unsigned char read_capacity_cmd10 [READ_CAPACITY10_CMDLEN] = {
		READ_CAPACITY10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char read_capacity_cmd16 [READ_CAPACITY16_CMDLEN] = {
		READ_CAPACITY16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
	unsigned char *cmd_addr;	
	LOG_INFO("Start : %s opcode %x, lba %x, alloc_len %x\n", 
			__func__, opcode, lba, alloc_len);
	if(READ_CAPACITY10_CMD == opcode)
	{
		put_unaligned_be16((uint16_t)lba, (read_capacity_cmd10 + 2));
		read_capacity_cmd_len = READ_CAPACITY10_CMDLEN;
		cmd_addr = read_capacity_cmd10;
		// for(int i = 0; i< READ_CAPACITY10_CMDLEN; i++)
		// {
		// 	LOG_INFO("read_capacity_cmd10[%d]: %x\n", i, read_capacity_cmd10[i]);
		// }
	}
	else if(READ_CAPACITY16_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (read_capacity_cmd16 + 2));
		put_unaligned_be32((uint32_t)alloc_len, (read_capacity_cmd16 + 10));
		read_capacity_cmd_len = READ_CAPACITY16_CMDLEN;
		cmd_addr = read_capacity_cmd16;
		// for(int i = 0; i< READ_CAPACITY16_CMDLEN; i++)
		// {
		// 	LOG_INFO("read_capacity_cmd16[%d]: %x\n", i, read_capacity_cmd16[i]);
		// }
	}
	else
	{
		perror("scsi read_capacity cmd: wrong parameters opcode\n");
		return -EINVAL;
	}
	// for(int i = 0; i< read_capacity_cmd_len; i++)
	// {
	// 	LOG_INFO("read_capacity_cmd%d[%d]: %x\n", read_capacity_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	// }
	if (fd < 0 || byte_count < 0) {
		perror("scsi read_capacity cmd: wrong parameters\n");
		return -EINVAL;
	}
	ret = send_scsi_cmd_maxio(fd, cmd_addr, read_capacity_rsp, read_capacity_cmd_len,
			byte_count,SG_DXFER_FROM_DEV, sg_type);	

	if (ret < 0) {
		print_error("SG_IO read_capacity data error ret %d\n", ret);
	}
	return ret;	
}

void read_capacity_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, randdata = 0;
	__u8 opcode = 0;
	__u32 alloc_len = 0;
	__u64 lba = 0;
	__u32 byte_count = 0;
	// __u64 cap = 0;
	//__u8 read_capacity_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		opcode = (rand() % 0x2);
		// lba = (rand() % 0x100);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}

		if(0 == opcode)
		{
			opcode = READ_CAPACITY10_CMD;
			byte_count = 0x8;
		}
		else if(1 == opcode)
		{
			opcode = READ_CAPACITY16_CMD;
			alloc_len = 0x20;//ufs spec 194
			byte_count = 0x20;
		}
		else
		{
			;		
		}
		
		if(9 == randdata)
		{
			if(READ_CAPACITY16_CMD == opcode)
			{
				while((0 == alloc_len) || (0x20 == alloc_len))
				{
					alloc_len = (rand() % 0x40);
					byte_count = alloc_len;
				}
			}
			LOG_INFO("%s rand9 page_code: %x\n", __func__, alloc_len);
		}
		else{
			;
		}

		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = read_capacity(fd, rd_buf, opcode,
 			lba, alloc_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)	
			{
				LOG_INFO("stop : %s randdata9 response panding check\n", __func__);
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		if(error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for(int i = 0; i< byte_count; i++)
			{
				LOG_INFO("read_capacity_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
			}
			LOG_INFO("\n");
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
/* 		if(READ_CAPACITY10_CMD == opcode)
		{
			cap = ((__u32)((__u8 *)rd_buf)[0] << 24) + ((__u32)((__u8 *)rd_buf)[1] << 16) + ((__u32)((__u8 *)rd_buf)[2] << 8) + ((__u8 *)rd_buf)[3];
		}
		else if(READ_CAPACITY16_CMD == opcode)
		{
			cap = ((__u64)((__u8 *)rd_buf)[0] << 56) + ((__u64)((__u8 *)rd_buf)[1] << 48) + ((__u64)((__u8 *)rd_buf)[2] << 40) + ((__u64)((__u8 *)rd_buf)[3] << 32) +\
					((__u64)((__u8 *)rd_buf)[4] << 24) + ((__u64)((__u8 *)rd_buf)[5] << 16) + ((__u64)((__u8 *)rd_buf)[6] << 8) + ((__u8 *)rd_buf)[7];
		}
		LOG_INFO("stop : %s result is 1 cap:%llx\n", __func__, cap);
 */		LOG_INFO("stop : %s result is 1 \n", __func__);
	}
}

__u64 read_capacity_for_cap(__u8 fd)
{
	int error_status = 0, ret = 0;
	__u8 opcode = 0;
	__u32 alloc_len = 0;
	__u64 lba = 0;
	__u32 byte_count = 0;
	 __u64 cap = 0;
	//__u8 read_capacity_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
		opcode = 1;
		if(0 == opcode)
		{
			opcode = READ_CAPACITY10_CMD;
			byte_count = 0x8;
		}
		else if(1 == opcode)
		{
			opcode = READ_CAPACITY16_CMD;
			alloc_len = 0x20;//ufs spec 194
			byte_count = 0x20;
		}
		else
		{
			;		
		}
		

		error_status = read_capacity(fd, rd_buf, opcode,
 			lba, alloc_len, byte_count, SG4_TYPE);
		if(error_status < 0)
		{
			LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
			ret = -1;
		}
		else
		{
			LOG_INFO("\nsuccess:\n");
			// for(int i = 0; i< byte_count; i++)
			// {
			// 	LOG_INFO("read_capacity_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
			// }
			// LOG_INFO("\n");
		}
	
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		if(READ_CAPACITY10_CMD == opcode)
		{
			cap = ((__u32)((__u8 *)rd_buf)[0] << 24) + ((__u32)((__u8 *)rd_buf)[1] << 16) + ((__u32)((__u8 *)rd_buf)[2] << 8) + ((__u8 *)rd_buf)[3];
		}
		else if(READ_CAPACITY16_CMD == opcode)
		{
			cap = ((__u64)((__u8 *)rd_buf)[0] << 56) + ((__u64)((__u8 *)rd_buf)[1] << 48) + ((__u64)((__u8 *)rd_buf)[2] << 40) + ((__u64)((__u8 *)rd_buf)[3] << 32) +\
					((__u64)((__u8 *)rd_buf)[4] << 24) + ((__u64)((__u8 *)rd_buf)[5] << 16) + ((__u64)((__u8 *)rd_buf)[6] << 8) + ((__u8 *)rd_buf)[7];
		}
		LOG_INFO("stop : %s result is 1 cap:%llx\n", __func__, cap);
		LOG_INFO("stop : %s result is 1 \n", __func__);
	}
	return cap;
}

int report_lun(int fd, __u8 *report_lun_rsp, __u8 select_report, 
	__u16 alloc_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char report_lun_cmd[REPORT_LUNS_CMDLEN] = {REPORT_LUNS_CMD,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi report_lun cmd: wrong parameters\n");
		return -EINVAL;
	}

	report_lun_cmd[2] = select_report;
	put_unaligned_be16((__u16)alloc_len, report_lun_cmd + 6);
	for(int i = 0; i< REPORT_LUNS_CMDLEN; i++)
	{
		LOG_INFO("report_lun_cmd[%d]: %x\n", i, report_lun_cmd[i]);
	}
	LOG_INFO("Start : %s select_report %d, alloc_len %d\n", 
			__func__, select_report, alloc_len);
	ret = send_scsi_cmd_maxio(fd, report_lun_cmd, report_lun_rsp,
			REPORT_LUNS_CMDLEN, byte_count,
			SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO report_lun data error ret %d\n", ret);
	}

	return ret;
}

void report_lun_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1, randdata = 0, j = 0;
	__u32 byte_count = 0;
	__u8 select_report = 0;
 	__u16 alloc_len = 0;
	//__u8 report_lun_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		select_report = (rand() % 0x3);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}

		if(0 == select_report)
		{
			alloc_len = 0x20;
		}
		else if(1 == select_report)
		{
			alloc_len = 0x28;	
		}
		else if(2 == select_report)
		{
			alloc_len = 0x40;	
		}
		else{
			;
		}
		byte_count = alloc_len;	
		randdata = 9;
		if(9 == randdata)
		{
			select_report = ((rand() % 0x5) + 0x3) & 0x3f;
			LOG_INFO("%s rand9 select_report: %x\n", __func__, select_report);
		}
		else if(8 == randdata)
		{
			if(0 == select_report)
			{
				while(0x20 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x30;
				}
			}
			else if(1 == select_report)
			{
				while(0x28 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x50;
				}
			}
			else if(2 == select_report)
			{
				while(0x40 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x80;
				}
			}
			else{
				;
			}
			byte_count = alloc_len;	
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
		}
		else{
			;
		}

		error_status = report_lun(fd, rd_buf, select_report, 
			alloc_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else if(8 == randdata)
			{
				LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		LOG_INFO("\nsuccess:\n");
		// for(int i = 0; i< byte_count; i++)
		// {
		// 	LOG_INFO("report_lun_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
		// }
		LOG_INFO("\n");
		LOG_INFO("\nsuccess:\n");
		for(j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("report_lun_rsp[%d]:", (j * 10));
			for(int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if((byte_count % 10) > 0)
		{
			LOG_INFO("report_lun_rsp[%d]:", (j * 10));
			for(int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
				j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}


int request_sense(int fd, __u8 *request_sense_rsp, __u8 alloc_len, 
		__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char request_sense_cmd[REQUEST_SENSE_CMDLEN] = {
		REQUEST_SENSE_CMD, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi request_sense cmd: wrong parameters\n");
		return -EINVAL;
	}

	request_sense_cmd[4] = alloc_len;

	for(int i = 0; i< REQUEST_SENSE_CMDLEN; i++)
	{
		LOG_INFO("request_sense_cmd[%d]: %x\n", i, request_sense_cmd[i]);
	}
	LOG_INFO("Start : %s  alloc_len %d\n", 
			__func__, alloc_len);
	ret = send_scsi_cmd_maxio(fd, request_sense_cmd, request_sense_rsp,
			REQUEST_SENSE_CMDLEN, byte_count, SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO request_sense data error ret %d\n", ret);
	}

	return ret;
}

void request_sense_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1;
	__u32 byte_count = 0;
	__u8 alloc_len = 0;
	__u8 randdata = 0;
	__u8 request_sense_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		if(1 == err_event)
		{
			randdata = (rand() % 0xa);
		}
		alloc_len = 0x12;
		byte_count = alloc_len;	
		if(9 == randdata)
		{
			while(0x12 == alloc_len)
			{
				alloc_len = (rand() % 0x25) + 1;
			}
			byte_count = alloc_len;
		}
		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = request_sense(fd, request_sense_rsp, alloc_len, 
			byte_count, SG4_TYPE);

		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)
			{
				LOG_INFO("stop : %s randdata9 response panding check\n", __func__);
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		if(error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for(int i = 0; i< byte_count; i++)
			{
				LOG_INFO("request_sense_rsp[%d]: %x\n", i, request_sense_rsp[i]);
			}
			LOG_INFO("\n");
		}
		
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

/*
int scsi_security_in(int fd, struct rpmb_frame *frame, int cnt, __u8 region,
		__u8 sg_type)
{
	int ret;
	__u32 trans_len = cnt * sizeof(struct rpmb_frame);
	__u16 sec_spec = (region << 8) | SEC_SPECIFIC_UFS_RPMB;
	unsigned char sec_in_cmd[SEC_PROTOCOL_CMD_SIZE] = {
			SECURITY_PROTOCOL_IN, SEC_PROTOCOL_UFS,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	LOG_INFO("Start : %s\n", __func__);
	if (fd < 0 || frame == NULL || cnt <= 0) {
		print_error("scsi sec_in cmd: wrong parameters\n");
		return ERROR;
	}

	*(__u16 *)(sec_in_cmd + SEC_SPEC_OFFSET) = htobe16(sec_spec);
	*(__u32 *)(sec_in_cmd + SEC_TRANS_LEN_OFFSET) = htobe32(trans_len);

	ret = send_scsi_cmd_maxio(fd, sec_in_cmd, frame, SEC_PROTOCOL_CMD_SIZE,
		trans_len, SG_DXFER_FROM_DEV, sg_type);

	return ret;
}

int scsi_security_out(int fd, struct rpmb_frame *frame_in,
		unsigned int cnt, __u8 region, __u8 sg_type)
{
	int ret;
	__u32 trans_len = cnt * sizeof(struct rpmb_frame);
	__u16 sec_spec = (region << 8) | SEC_SPECIFIC_UFS_RPMB;
	unsigned char sec_out_cmd[SEC_PROTOCOL_CMD_SIZE] = {
			SECURITY_PROTOCOL_OUT, SEC_PROTOCOL_UFS,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || frame_in == NULL || cnt <= 0) {
		print_error("scsi sec_out cmd: wrong parameters\n");
		return ERROR;
	}
	*(__u16 *)(sec_out_cmd + SEC_SPEC_OFFSET) = htobe16(sec_spec);
	*(__u32 *)(sec_out_cmd + SEC_TRANS_LEN_OFFSET) = htobe32(trans_len);
	ret = send_scsi_cmd_maxio(fd, sec_out_cmd, frame_in,
			SEC_PROTOCOL_CMD_SIZE, trans_len,
			SG_DXFER_TO_DEV, sg_type);

	return ret;
}
*/
int send_diagnostic(int fd, __u8 *send_diagnostic_rsp, __u8 self_test_code, __u8 pf, __u8 self_test,
 			__u8 devoffl, __u8 unitoffl, __u16 para_list_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char send_diagnostic_cmd [SEND_DIAGNOSTIC_CMDLEN] = {
		SEND_DIAGNOSTIC_CMD, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		perror("scsi send_diagnostic_cmd cmd: wrong parameters\n");
		return -EINVAL;
	}
	send_diagnostic_cmd[1] = ((self_test_code & 0x7) << 5) | ((pf & 0x1) << 4) | ((self_test & 0x1) << 2) | \
								((devoffl & 0x1) << 1) | (unitoffl & 0x1);
	put_unaligned_be16((uint16_t)para_list_len, (send_diagnostic_cmd + 3));
	for(int i = 0; i< SEND_DIAGNOSTIC_CMDLEN; i++)
	{
		LOG_INFO("send_diagnostic_cmd[%d]: %x\n", i, send_diagnostic_cmd[i]);
	}
	LOG_INFO("Start : %s self_test_code %d, self_test %d, pf %d devoffl %d, unitoffl %d, para_list_len %d\n", 
			__func__, self_test_code, self_test, pf, devoffl, unitoffl, para_list_len);
	ret = send_scsi_cmd_maxio(fd, send_diagnostic_cmd, send_diagnostic_rsp, SEND_DIAGNOSTIC_CMDLEN, byte_count,
			SG_DXFER_NONE, sg_type);	

	if (ret < 0) {
		print_error("SG_IO send_diagnostic_cmd data error ret %d\n", ret);
	}
	return ret;	
}

void send_diagnostic_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1, randdata = 0;
	__u32 byte_count = 0;
	__u8 self_test_code = 0, self_test = 0, devoffl = 0, unitoffl = 0, pf =0;
	__u16 para_list_len	= 0;
	__u8 send_diagnostic_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		self_test = (rand() % 0x2);
		devoffl = (rand() % 0x2);
		unitoffl = (rand() % 0x2);
		// pf = (rand() % 0x2);
		pf = (rand() % 0x1);
		para_list_len=0;
		if(1 == self_test)
		{
			self_test_code = 0;
		}
		else
		{
			do
			{
				self_test_code = (rand() % 0x7);
			}
			while(0x3 == self_test_code || 0x4 == self_test_code);
		}
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(9 == randdata)
		{
			para_list_len = (rand() % 0xf) + 1;
			LOG_INFO("%s rand9 para_list_len: %x\n", __func__, para_list_len);
		}
		else if(8 == randdata)
		{
			self_test = 1;
			self_test_code = (rand() % 0x6) + 1;
			LOG_INFO("%s rand8 para_list_len: %x\n", __func__, self_test_code);
		}
		else
		{
			;
		}
		error_status = send_diagnostic(fd, send_diagnostic_rsp, self_test_code, pf, self_test,
 			devoffl, unitoffl, para_list_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/8 sense_data err randdata %d\n", __func__, randdata);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9/8 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int sync_cache(int fd, __u8 *sync_cache_rsp, __u8 opcode, __u8 immed,
 			__u64 lba, __u32 nlb,__u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 sync_cache_cmd_len = 0;
	unsigned char sync_cache_cmd10 [PRE_FETCH10_CMDLEN] = {
		SYNCHRONIZE_CACHE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char sync_cache_cmd16 [PRE_FETCH16_CMDLEN] = {
		SYNCHRONIZE_CACHE16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;
	if(SYNCHRONIZE_CACHE10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (sync_cache_cmd10 + 2));
		put_unaligned_be16((uint16_t)nlb, (sync_cache_cmd10 + 7));
		sync_cache_cmd_len = PRE_FETCH10_CMDLEN;
		sync_cache_cmd10[1] = (immed & 0x1) << 1;
		cmd_addr = sync_cache_cmd10;
	}
	else if(SYNCHRONIZE_CACHE16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (sync_cache_cmd16 + 2));
		put_unaligned_be32((uint32_t)nlb, (sync_cache_cmd16 + 10));
		sync_cache_cmd_len = PRE_FETCH16_CMDLEN;
		sync_cache_cmd16[1] = (immed & 0x1) << 1;
		cmd_addr = sync_cache_cmd16;
	}
	else
	{
		perror("scsi synchronize_cache cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	if (fd < 0 || byte_count < 0) {
		perror("scsi synchronize_cache cmd: wrong parameters\n");
		return -EINVAL;
	}
	for(int i = 0; i< sync_cache_cmd_len; i++)
	{
		LOG_INFO("sync_cache_cmd%d[%d]: %x\n", sync_cache_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %d, immed %d, lba %llx, nlb %x\n", 
			__func__, opcode, immed, lba, nlb);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, sync_cache_rsp, sync_cache_cmd_len, byte_count,
			SG_DXFER_NONE, sg_type);	

	if (ret < 0) {
		print_error("SG_IO synchronize_cache data error ret %d\n", ret);
	}
	return ret;	
}

void sync_cache_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	__u32 byte_count = 0;
	__u8 immed = 0, opcode = 0;
	__u64 lba = 0;
	__u32 nlb = 0;
	__u64 cap = 0;
	__u8 randdata = 0;
	__u8 sync_cache_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		immed = (rand() % 0x2);
		opcode = (rand() % 0x2) + 1;
		lba = (rand() % 0x10);
		nlb = (rand() % 0x10);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(1 == opcode)
		{
			opcode = SYNCHRONIZE_CACHE10_CMD;
		}
		else if(2 == opcode)
		{
			opcode = SYNCHRONIZE_CACHE16_CMD;
		}
		else
		{
			opcode = 0xff;
		}
		
		if(9 == randdata)
		{
			cap = read_capacity_for_cap(fd);
			lba = cap + 1;
			nlb = 0;
			LOG_INFO("%s rand9 lba: %llx\n", __func__, lba);
		}
		else if(8 == randdata)
		{
			cap = read_capacity_for_cap(fd);
			while((lba + nlb ) <= (cap + 1))
			{
				lba = cap - (rand() % 0x50);
				nlb = (rand() % 0x100);
			}
			LOG_INFO("%s rand8 lba: %llx nlb: %x\n", __func__, lba, nlb);
		}
		else{
			;
		}
		error_status = sync_cache(fd, sync_cache_rsp, opcode, immed,
 			lba, nlb, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int inquiry(int fd, __u8 *inquiry_rsp, __u8 evpd, __u8 page_code, 
			__u16 alloc_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char inquiry_cmd [INQUIRY_CMDLEN] = {
		INQUIRY_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0) {
		perror("scsi inquiry cmd: wrong parameters\n");
		return -EINVAL;
	}
	inquiry_cmd[1] = evpd;
	inquiry_cmd[2] = page_code;
	inquiry_cmd[3] = (alloc_len >> 8) & 0xff;
	inquiry_cmd[4] = (alloc_len) & 0xff;

	for(int i = 0; i< INQUIRY_CMDLEN; i++)
	{
		LOG_INFO("inquiry_cmd[%d]: %x\n", i, inquiry_cmd[i]);
	}
	LOG_INFO("Start : %s evpd %d , page_code %d\n", __func__, evpd, page_code);
	ret = send_scsi_cmd_maxio(fd, inquiry_cmd, inquiry_rsp, INQUIRY_CMDLEN, byte_count,
			SG_DXFER_FROM_DEV, sg_type);	

	if (ret < 0) {
		print_error("SG_IO inquiry data error ret %d\n", ret);
	}
	return ret;	
}

void inquiry_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	__u32 byte_count = 0;
	__u8 evpd = 0, page_code = 0;
	__u8 randdata = 0;
	__u16 alloc_len = 0;
	//__u8 inquiry_rsp[100] = {0};
	LOG_INFO("Start : %s err_event%d\n", __func__, err_event);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		evpd = (rand() % 0x2);
		// lba = (rand() % 0x100);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if(0 == evpd)
		{
			page_code = 0;
			alloc_len = 0x24;
			byte_count = 0x24;
		}
		else if(1 == evpd)
		{
			page_code = (rand() % 0x2);
			if(0 == page_code)
			{			
				page_code = 0;
				alloc_len = 0x6;
				byte_count = alloc_len;
			}
			else
			{
				page_code = 0x87;
				alloc_len = 0x16;
				byte_count = alloc_len;
			}
		}
		//randdata = 9;
		if(9 == randdata)
		{
			page_code = (rand() % 0x20) + 1;
			LOG_INFO("%s rand9 page_code: %x\n", __func__, page_code);
		}
		else if (8 == randdata)
		{
			while((alloc_len == 0x24) || (alloc_len == 0x6) || (alloc_len == 0x16))
			{
				alloc_len = (rand() % 0x30) + 1;
				byte_count = alloc_len;
				LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
			}
		}
		else if(7 == randdata)
		{
			//device hardware reset to establish UAC
		}
		else{
			;
		}
		error_status = inquiry(fd, rd_buf, evpd, page_code, 
			alloc_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		if(error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for(int i = 0; i< byte_count; i++)
			{
				LOG_INFO("inquiry_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
			}
			LOG_INFO("\n");
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int test_unit_ready(int fd, __u8 *test_unit_ready_rsp,
			__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char test_unit_ready_cmd [TEST_UNIT_READY_CMDLEN] = {
		TEST_UNIT_READY_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0) {
		perror("scsi test_unit_ready cmd: wrong parameters\n");
		return -EINVAL;
	}
	LOG_INFO("Start : %s \n", __func__);
	for(int i = 0; i < TEST_UNIT_READY_CMDLEN; i++)
	{
		LOG_INFO("test_unit_ready_cmd[%d]: %x\n", i, test_unit_ready_cmd[i]);
	}
	ret = send_scsi_cmd_maxio(fd, test_unit_ready_cmd, test_unit_ready_rsp, 
			TEST_UNIT_READY_CMDLEN, byte_count, SG_DXFER_NONE, sg_type);	

	if (ret < 0) {
		print_error("SG_IO test_unit_ready data error ret %d\n", ret);
	}
	return ret;	
}

void test_unit_ready_test(__u8 fd)
{
	__u32 i = 0;
	int error_status = 0;
	__u32 byte_count = 0;
	__u8 test_unit_ready_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = test_unit_ready(fd, test_unit_ready_rsp,
			byte_count, SG4_TYPE);
		if(error_status < 0)
		{
			LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
			break;
		}
	}
	if(error_status < 0)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int unmap(int fd, __u8 *unmap_req, __u8 anchor, __u16 para_list_len,
		__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char unmap_cmd [UNMAP_CMDLEN] = {
		UNMAP_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		perror("scsi unmap cmd: wrong parameters\n");
		return -EINVAL;
	}
	unmap_cmd[1] = anchor & 0x1;
	put_unaligned_be16((uint16_t)para_list_len, unmap_cmd + 7);

	for(int i = 0; i< UNMAP_CMDLEN; i++)
	{
		LOG_INFO("unmap_cmd[%d]: %x\n", i, unmap_cmd[i]);
	}

	LOG_INFO("Start : %s para_list_len %x byte_count%x\n", __func__, para_list_len, byte_count);
	ret = send_scsi_cmd_maxio(fd, unmap_cmd, unmap_req,
			UNMAP_CMDLEN, byte_count,
			SG_DXFER_TO_DEV, sg_type);
	if (ret < 0) {
		print_error("SG_IO unmap data error ret %d\n", ret);
	}
	return ret;
}

void unmap_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j =0;
	__u32 byte_count = 0;
	__u64 cap = 0;
	__u16 para_list_len = 0;
	__u8 anchor = 0;
	__u8 unmap_list = 0, randdata = 0;
	// __u8 unmap_req[100] = {0};
	__u64 unmap_lba[10] = {0};
	__u32 unmap_nlb[10] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		unmap_list = (rand() % 10) + 1;
		para_list_len = 8 + 16 * unmap_list;
		anchor = 0;
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		randdata = 7;
		if(9 == randdata)
		{
			anchor = 1;
			LOG_INFO("%s rand9 anchor: %x\n", __func__, anchor);
		}
		// else if(8 == randdata)
		// {
		// 	para_list_len = 0;
		// 	LOG_INFO("%s rand8 para_list_len: 0\n", __func__);
		// }
		else if(7 == randdata)
		{
			para_list_len = 8 + ((rand() % 0x8) + 12) * unmap_list;
			LOG_INFO("%s rand7 para_list_len: %x\n", __func__, para_list_len);
		}
		else
		{
			para_list_len = 8 + 16 * unmap_list;
			LOG_INFO("%s rand%d\n", __func__, randdata);
		}
		//para_list_len = 16;
		if(para_list_len > 0)
		{
			byte_count = para_list_len;
		}
		else
		{
			byte_count = 8;
		}
		for (size_t j = 0; j < byte_count; j++)
		{
			MEM8_GET(wr_buf+j) = 0;
		}
		if(para_list_len > 7)
		{
			put_unaligned_be16((uint16_t)(para_list_len - 2), (__u8 *)wr_buf);
			put_unaligned_be16((uint16_t)(para_list_len - 8), (__u8 *)wr_buf + 2);
		}
		else if(para_list_len < 7 && para_list_len >1)
		{
			put_unaligned_be16((uint16_t)(para_list_len - 2), (__u8 *)wr_buf);
		}
		else
		{
			;
		}
		for(j = 0; j < unmap_list; j++)
		{
			unmap_lba[j] = rand()% 0x1f + 1;
			unmap_nlb[j] = rand()% 0x1f + 1;
			put_unaligned_be64((__u64)unmap_lba[j], (__u8 *)wr_buf + 8 + 16 * j);
			put_unaligned_be32((__u32)unmap_nlb[j], (__u8 *)wr_buf + 16 + 16 * j);
			if(5 == randdata)
			{
				cap = read_capacity_for_cap(fd);
				put_unaligned_be64((cap), (__u8 *)wr_buf + 8 + 16 * j);
				put_unaligned_be32(((__u32)unmap_nlb[j] + 1), (__u8 *)wr_buf + 16 + 16 * j);
			}
			else if(4 == randdata)
			{
				cap = read_capacity_for_cap(fd);
				put_unaligned_be64((cap + 1), (__u8 *)wr_buf + 8 + 16 * j);
			}
		}
		// for(int i = 0; i < byte_count; i++)
		// {
		// 	LOG_INFO("unmap_cmd_para[%d]: %x\n", i, ((__u8 *)wr_buf)[i]);
		// }
		for(j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("unmap_cmd_para[%d]:", (j * 10));
			for(int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if((byte_count % 10) > 0)
		{
			LOG_INFO("unmap_cmd_para[%d]:", (j * 10));
			for(int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");

		error_status = unmap(fd, wr_buf, anchor,para_list_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x26 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else if(5 == randdata || 4 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9 sense_data pass\n", __func__);
				}
			}
			else if(7 == randdata)
			{
				if(para_list_len < 8)
				{
					if((0x5 != g_sense_data.sense_key) || (0x26 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata7 sense_data err\n", __func__);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata7 sense_data pass\n", __func__);
					}
				}
				else
				{
/* 					if ((((para_list_len - 8) % 16) > 0 && ((para_list_len - 8) % 16) < 8) ||\
						(((para_list_len - 8) % 16) > 8 && ((para_list_len - 8) % 16) < 12) ||\
						(((para_list_len - 8) % 16) > 12 && ((para_list_len - 8) % 16) < 16))
 */					if((((para_list_len - 8) % 16) % 4) > 0)
					{
						if((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
						{
							LOG_INFO("stop : %s randdata7 sense_data err para_list_len %x\n", __func__, (para_list_len - 8) % 16);
							ret = -1;
							break;
						}
						else
						{
							LOG_INFO("stop : %s randdata7 sense_data pass\n", __func__);
						}
					}
					else
					{
						if(0x0 != g_sense_data.sense_key && 0x0 != g_sense_data.asc && 0x0 != g_sense_data.ascq)
						{
							LOG_INFO("stop : %s randdata7 sense_data err para_list_len %x\n", __func__, (para_list_len - 8) % 16);
							ret = -1;
							break;
						}
						else
						{
							LOG_INFO("stop : %s randdata7 sense_data pass\n", __func__);
						}
					}
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}

		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int verify(int fd, __u8 *verify_rsp, __u32 lba, __u16 verify_len,
			__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char verify_cmd [VERIFY_CMDLEN] = {
		VERIFY_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0) {
		perror("scsi verify cmd: wrong parameters\n");
		return -EINVAL;
	}
	put_unaligned_be32((uint32_t)lba, verify_cmd + 2);
	put_unaligned_be16((uint16_t)verify_len, verify_cmd + 7);
	for(int i = 0; i < VERIFY_CMDLEN; i++)
	{
		LOG_INFO("verify_cmd[%d]: %x\n", i, verify_cmd[i]);
	}
	LOG_INFO("Start : %s lba %x, verify_len %x\n", __func__, lba, verify_len);
	ret = send_scsi_cmd_maxio(fd, verify_cmd, verify_rsp, 
			VERIFY_CMDLEN, byte_count, SG_DXFER_NONE, sg_type);	

	if (ret < 0) {
		print_error("SG_IO verify data error ret %d\n", ret);
	}
	return ret;	
}

void verify_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, randdata = 0;
	__u32 byte_count = 0;
	__u32 lba = 0;
	__u16 verify_len = 0;	
	__u8 verify_rsp[100] = {0};
	__u64 cap = 0;
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);

		lba = rand()% 0x1f;
		verify_len = rand()% 0x1f;
		if(1 == err_event)
		{
			randdata = rand()% 0xf;
		}
		if(9 == randdata)
		{
			cap = read_capacity_for_cap(fd);
			lba = (__u32)cap + 1;
			LOG_INFO("%s rand9 lba: %x\n", __func__, lba);
		}
		else if(8 == randdata) 
		{
			cap = read_capacity_for_cap(fd);
			lba = (__u32)cap;
			verify_len = 2;
			LOG_INFO("%s rand8 lba: %x\n", __func__, lba);
		}
		else{
			;
		}
		error_status = verify(fd, verify_rsp, lba, verify_len,
			byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/8 sense_data err\n", __func__);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9/8 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int hpb_read_buffer(int fd, __u8 *hpb_read_buffer_rsp, __u8 buf_id, __u16 hpb_region,
	__u16 hpb_subregion, __u32 alloc_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char hpb_read_buf_cmd[HPB_READ_BUF_CMDLEN] = {HPB_READ_BUF_CMD,
		0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi hpb_read_buffer cmd: wrong parameters\n");
		return -EINVAL;
	}

	hpb_read_buf_cmd[1] = buf_id;
	put_unaligned_be16((__u32)hpb_region, hpb_read_buf_cmd + 2);
	put_unaligned_be16((__u32)hpb_subregion, hpb_read_buf_cmd + 4);
	put_unaligned_be24((__u32)alloc_len, hpb_read_buf_cmd + 6);
	for(int i = 0; i< HPB_READ_BUF_CMDLEN; i++)
	{
		LOG_INFO("hpb_read_buf_cmd[%d]: %x\n", i, hpb_read_buf_cmd[i]);
	}
	LOG_INFO("Start : %s\n buf_id %d, hpb_region %d, hpb_subregion %d, alloc_len %d\n", 
				__func__, buf_id, hpb_region, hpb_subregion, alloc_len);
	ret = send_scsi_cmd_maxio(fd, hpb_read_buf_cmd, hpb_read_buffer_rsp,
			HPB_READ_BUF_CMD, byte_count,
			SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO hpb_read_buffer data error ret %d\n", ret);
	}
	return ret;	
}

int hpb_write_buffer(int fd, __u8 *hpb_write_buffer_rsp, __u8 buf_id, __u16 hpb_region,
	__u32 lba, __u8 hpb_read_id, __u32 para_list_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char hpb_write_buf_cmd[HPB_WRITE_BUF_CMDLEN] = {HPB_WRITE_BUF_CMD,
		buf_id, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi hpb_read_buffer cmd: wrong parameters\n");
		return -EINVAL;
	}
	if(0x01 == buf_id)
	{
		put_unaligned_be16((uint16_t)hpb_region, (hpb_write_buf_cmd + 2));
	}
	else if(0x02 == buf_id)
	{
		put_unaligned_be64((uint32_t)lba, (hpb_write_buf_cmd + 2));
		put_unaligned_be32((uint16_t)para_list_len, (hpb_write_buf_cmd + 7));
		hpb_write_buf_cmd[6] = hpb_read_id;
	}
	else if(0x03 == buf_id)
	{
		//none
	}
	else
	{
		perror("scsi hpb_write_buffer cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	LOG_INFO("Start : %s\n buf_id %d, hpb_region %d, lba %d, hpb_read_id %d, para_list_len %d\n", 
				__func__, buf_id, hpb_region, lba, hpb_read_id, para_list_len);
	ret = send_scsi_cmd_maxio(fd, hpb_write_buf_cmd, hpb_write_buffer_rsp,
			HPB_WRITE_BUF_CMDLEN, byte_count, SG_DXFER_TO_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO hpb_write_buffer data error ret %d\n", ret);
	}
	return ret;	
}

int scsi_write_buffer(int fd, __u8 *buf, __u8 mode, __u8 buf_id, __u32 buf_offset,
		__u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char write_buf_cmd [WRITE_BUF_CMDLEN] = {
		WRITE_BUFFER_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		perror("scsi write cmd: wrong parameters\n");
		return -EINVAL;
	}

	write_buf_cmd[1] = mode;
	write_buf_cmd[2] = buf_id;
	put_unaligned_be24((uint32_t)buf_offset, write_buf_cmd + 3);
	put_unaligned_be24(byte_count, write_buf_cmd + 6);

	for(int i = 0; i< WRITE_BUF_CMDLEN; i++)
	{
		LOG_INFO("write_buf_cmd[%d]: %x\n", i, write_buf_cmd[i]);
	}

	LOG_INFO("Start : %s mode %d , buf_id %d\n", __func__, mode, buf_id);
	ret = send_scsi_cmd_maxio(fd, write_buf_cmd, buf,
			WRITE_BUF_CMDLEN, byte_count,
			SG_DXFER_TO_DEV, sg_type);
	if (ret < 0) {
		print_error("SG_IO WRITE BUFFER data error ret %d\n", ret);
	}
	return ret;
}


void write_buffer_test(__u8 fd)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j =0;
	__u8 mode = 0xff, buf_id = 0;
	__u32 buf_offset = 0;
	__u32 byte_count = 0;
	// __u8 write_buffer_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{		
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		if(0xff == mode)
		{
			mode = (rand() % 0x2);//expect for vendor mode
		}
		mode = 2;
		if(0 == mode)
		{
			mode = BUFFER_FFU_MODE;//download microcode mode
			byte_count = (rand() % 0x50) + 1;
			buf_offset = (rand() % 0x5);
			buf_id = 0;
		}
		else if(1 == mode)
		{
			mode = BUFFER_DATA_MODE;//data
			buf_offset = (rand() % 0x5);
			buf_id = (rand() % 0x2);
			byte_count = (rand() % 0x50) + 1;
		}
		else if(2 == mode)
		{
			mode = BUFFER_VENDOR_MODE;//vendor specific
			LOG_ERROR("stop %s: expect for vendor mode\n", __func__);
			// buf_id = ;
			// buf_offset = ;
			// byte_count = ;
		}
		else
		{
			;
		}
		LOG_INFO("\nsuccess:\n");
		for(j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("wr_buf[%d]:", (j * 10));
			for(int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
			}
			LOG_INFO("\n");
		}
		if((byte_count % 10) > 0)
		{
			LOG_INFO("wr_buf[%d]:", (j * 10));
			for(int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");

		error_status = scsi_write_buffer(fd, wr_buf, mode, buf_id,
			buf_offset, byte_count, SG4_TYPE);
		if(error_status < 0 && fd > 0)
		{
			LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
			ret = -1;
			break;
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_read(int fd, __u8 *read_rsp, __u8 opcode, __u8 dpo, __u8 fua, __u8 grp_num,
 			__u64 lba, __u32 transfer_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 read_cmd_len = 0;
	unsigned char read6 [READ6_LEN] = {
		READ6_CMD, 0, 0, 0, 0, 0};
	unsigned char read10 [READ10_LEN] = {
		READ10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char read16 [READ16_LEN] = {
		READ16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;	
	if(READ6_CMD == opcode)
	{
		//put_unaligned_be32((uint32_t)lba, (read6 + 1));
		read6[1] = ((uint32_t)lba >> 16) & 0x1f;
		read6[2] = ((uint32_t)lba >> 8) & 0xff;
		read6[3] = ((uint32_t)lba) & 0xff;
		read6[4] = transfer_len;
		read_cmd_len = READ6_LEN;
		cmd_addr = read6;
		// for(int i = 0; i< READ6_LEN; i++)
		// {
		// 	LOG_INFO("read6[%d]: %x\n", i, read6[i]);
		// }
	}
	else if(READ10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (read10 + 2));
		put_unaligned_be16((uint16_t)transfer_len, (read10 + 7));
		read10[1] = ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		read10[6] = (grp_num & 0x1f);
		read_cmd_len = READ10_LEN;
		cmd_addr = read10;
		// for(int i = 0; i< READ10_LEN; i++)
		// {
		// 	LOG_INFO("read10[%d]: %x\n", i, read10[i]);
		// }
	}
	else if(READ16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (read16 + 2));
		put_unaligned_be32((uint32_t)transfer_len, (read16 + 10));
		read16[1] = ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		read16[14] = (grp_num & 0x1f);
		read_cmd_len = READ16_LEN;
		cmd_addr = read16;
		// for(int i = 0; i< READ16_LEN; i++)
		// {
		// 	LOG_INFO("read16[%d]: %x\n", i, read16[i]);
		// }
	}
	else
	{
		perror("scsi prefetch cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	if (fd < 0 || byte_count < 0) {
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	for(int i = 0; i< read_cmd_len; i++)
	{
		LOG_INFO("read%d[%d]: %x\n", read_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %x dpo %d fua %d, lba %llx transfer_len %x grp_num %x, byte_count %x\n", 
			__func__, opcode, dpo, fua, lba, transfer_len, grp_num, byte_count);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, read_rsp, read_cmd_len, byte_count,
			SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO read data error ret %d\n", ret);
	}
	return ret;	
}

void scsi_read_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0;
	__u32 byte_count = 0;
	__u8 opcode = 0, dpo = 0, fua = 0, grp_num = 0;
	__u8 len = 0;
 	__u64 lba= 0;
	__u32 transfer_len = 0;
	__u64 cap = 0;
	//__u8 read_rsp[0x100000] = {0};
	cap = read_capacity_for_cap(fd);
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1; i++)
	{
		LOG_INFO("\n%s i %d cap %llx\n", __func__, i, cap);
		if(cap <= 0)
		{
			break;
		}
		j = 0;
		dpo = (rand() % 0x2);
		fua = (rand() % 0x2);
		opcode = (rand() % 0x3);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		// opcode = 0;
		if(0 == opcode)
		{
			opcode = READ6_CMD;
			len = READ6_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			// while(((lba + 1) * transfer_len * 4096) > cap || lba > 0x1fffff || transfer_len > 0xff);
			while(((lba + 1) * transfer_len * 4096) > cap || lba > 0x1fffff || transfer_len > 0x1f);
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
				//byte_count = 256 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else if(1 == opcode)
		{
			opcode = READ10_CMD;
			len = READ10_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			//while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffff || transfer_len > 0xffff);
			while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffff || transfer_len > 0x1f);
			// grp_num = (rand() % 0x5) +2;
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else if(2 == opcode)
		{
			opcode = READ16_CMD;
			len = READ16_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			//while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffffffffffff || transfer_len > (0xffffffff >> 12));
			while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffffffffffff || transfer_len > 0x1f);
			// grp_num = (rand() % 0x5) +2;
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else
		{
			;
		}
		randdata = 5;
		if(9 == randdata)
		{			
			lba = cap + 1;
			//transfer_len = (rand() % (cap / 4096 + cap % 4096));
			transfer_len = 0x1f;//(rand() % (0x1f));
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
			if(cap > (0xffffffffUL + 0xffffUL))
			{
				opcode = READ16_CMD;
				len = READ16_LEN;
			}
			else if(cap > (0x1fffff + 0xff))
			{
				opcode = rand() % 0x2;
				if(0 == opcode)
				{
					opcode = READ10_CMD;
					len = READ10_LEN;
				}
				else
				{
					opcode = READ16_CMD;
					len = READ16_LEN;
				}
			}
			else
			{
				;
			}
			LOG_INFO("%s rand9 lba: %llx transfer_len %x\n", __func__, lba, transfer_len);		
		}
		else if(8 == randdata)
		{
			while((lba + transfer_len) <= (cap + 1))
			{
				lba = (cap) - (rand() % 0x50);
				transfer_len = (rand() % 0x1f);
			}
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}

			if(cap > (0xffffffffUL + 0xffffUL))
			{
				opcode = READ16_CMD;
				len = READ16_LEN;
			}
			else if(cap > (0x1fffff + 0xff))
			{
				opcode = (rand() % (0x2));
				if(0 == opcode)
				{
					opcode = READ10_CMD;
					len = READ10_LEN;
				}
				else
				{
					opcode = READ16_CMD;
					len = READ16_LEN;
				}
			}
			LOG_INFO("%s rand8 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else
		{
			if(READ6_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE6_CMD, 0, 0, 0,
						lba, transfer_len, byte_count, SG4_TYPE);			
			}
			else if(READ10_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE10_CMD, 0, 0, 0,
						lba, transfer_len, byte_count, SG4_TYPE);			
			}
			else if(READ16_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE16_CMD, 0, 0, 0,
						lba, transfer_len, byte_count, SG4_TYPE);			
			}
			else
			{
				;
			}
		}


		error_status = scsi_read(fd, rd_buf, opcode, dpo, fua, grp_num,
 			lba, transfer_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/8 sense_data err randdata %d\n", __func__, randdata);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9/8 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
				LOG_INFO("\n memcmp:\n");
				error_status = memcmp(wr_buf, rd_buf, byte_count);
				if(0 != error_status)
				{
					LOG_ERROR("memcmp : %s err %d\n", __func__, error_status);
					ret = -1;
					LOG_INFO("\n write:\n");
					for(j = 0; j < (byte_count / 10); j++)
					{
						//LOG_INFO("write%x[%d]:", len, (j * 10));
						for(int i = 0; i < 10; i++)
						{
							LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
							// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
						}
						LOG_INFO("\n");
					}
					if((byte_count % 10) > 0)
					{
						//LOG_INFO("write%x[%d]:", len, (j * 10));
						for(int i = 0; i < (byte_count % 10); i++)
						{
							LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
							j++;
						}
						LOG_INFO("\n");
					}
					LOG_INFO("\n");

					LOG_INFO("\n read:\n");
					for(j = 0; j < (byte_count / 10); j++)
					{
						//LOG_INFO("read%d[%d]:", len, (j * 10));
						for(int i = 0; i < 10; i++)
						{
							LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
						}
						LOG_INFO("\n");
					}
					if((byte_count % 10) > 0)
					{
						//LOG_INFO("read%d[%d]:", len, (j * 10));
						for(int i = 0; i < (byte_count % 10); i++)
						{
							LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
							j++;
						}
						LOG_INFO("\n");
					}
					LOG_INFO("\n");
					break;
				}
			}
		}
		if(-1 != ret)
		{
			LOG_INFO("\nsuccess:\n");
			for(j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for(int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if((byte_count % 10) > 0)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for(int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
					j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_write(int fd, __u8 *write_rsp, __u8 opcode, __u8 dpo, __u8 fua, __u8 grp_num,
 			__u64 lba, __u32 transfer_len,__u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 write_cmd_len = 0;
	unsigned char write6 [WRITE6_LEN] = {
		WRITE6_CMD, 0, 0, 0, 0, 0};
	unsigned char write10 [WRITE10_LEN] = {
		WRITE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char write16 [WRITE16_LEN] = {
		WRITE16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;	
	if(WRITE6_CMD == opcode)
	{
		//put_unaligned_be32((uint32_t)lba, (write6 + 1));
		write6[1] = ((uint32_t)lba >> 16) & 0x1f;
		write6[2] = ((uint32_t)lba >> 8) & 0xff;
		write6[3] = ((uint32_t)lba) & 0xff;
		write6[4] = transfer_len;
		write_cmd_len = WRITE6_LEN;
		cmd_addr = write6;
		// for(int i = 0; i< write6_LEN; i++)
		// {
		// 	LOG_INFO("write6[%d]: %x\n", i, write6[i]);
		// }
	}
	else if(WRITE10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (write10 + 2));
		put_unaligned_be16((uint16_t)transfer_len, (write10 + 7));
		write10[1] = ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		write10[6] = (grp_num & 0x1f);
		write_cmd_len = WRITE10_LEN;
		cmd_addr = write10;
		// for(int i = 0; i< write10_LEN; i++)
		// {
		// 	LOG_INFO("write10[%d]: %x\n", i, write10[i]);
		// }
	}
	else if(WRITE16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (write16 + 2));
		put_unaligned_be32((uint32_t)transfer_len, (write16 + 10));
		write16[1] = ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		write16[14] = (grp_num & 0x1f);
		write_cmd_len = WRITE16_LEN;
		cmd_addr = write16;
		// for(int i = 0; i< write16_LEN; i++)
		// {
		// 	LOG_INFO("write16[%d]: %x\n", i, write16[i]);
		// }
	}
	else
	{
		perror("scsi prefetch cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	if (fd < 0 || byte_count < 0) {
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	for(int i = 0; i< write_cmd_len; i++)
	{
		LOG_INFO("write%d[%d]: %x\n", write_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %x dpo %d fua %d, lba %llx transfer_len %x grp_num %x, byte_count %x\n", 
			__func__, opcode, dpo, fua, lba, transfer_len, grp_num, byte_count);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, write_rsp, write_cmd_len, byte_count,
			SG_DXFER_TO_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO write data error ret %d\n", ret);
	}
	return ret;	
}

void scsi_write_test(__u8 fd, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0;
	__u32 byte_count = 0;
	__u8 opcode = 0, dpo = 0, fua = 0, grp_num = 0;
	__u8 len = 0;
 	__u64 lba= 0;
	__u32 transfer_len = 0;
	__u64 cap = 0;
	//__u8 write_rsp[0x100000] = {0};
	cap = read_capacity_for_cap(fd);
	LOG_INFO("Start : %s\n", __func__);
	for(i = 0; i < 1000; i++)
	{
		LOG_INFO("\n%s i %d cap %llx\n", __func__, i, cap);
		if(cap <= 0)
		{
			break;
		}
		j = 0;
		dpo = (rand() % 0x2);
		fua = (rand() % 0x2);
		opcode = (rand() % 0x3);
		if(1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		// opcode = 1;
		if(0 == opcode)
		{
			opcode = WRITE6_CMD;
			len = WRITE6_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			// while(((lba + 1) * transfer_len * 4096) > cap || lba > 0x1fffff || transfer_len > 0xff);
			while(((lba + 1) * transfer_len) > cap || lba > 0x1fffff || transfer_len > 0x1f);
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
				//byte_count = 256 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else if(1 == opcode)
		{
			opcode = WRITE10_CMD;
			len = WRITE10_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			//while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffff || transfer_len > 0xffff);
			while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffff || transfer_len > 0x1f);
			// grp_num = (rand() % 0x5) +2;
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else if(2 == opcode)
		{
			opcode = WRITE16_CMD;
			len = WRITE16_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
			}
			//while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffffffffffff || transfer_len > (0xffffffff >> 12));
			while(((lba + 1) * transfer_len * 4096) > cap || lba > 0xffffffffffffffff || transfer_len > 0x1f);
			// grp_num = (rand() % 0x5) +2;
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = 1 << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
		}
		else
		{
			;
		}

		if(9 == randdata)
		{			
			lba = cap + 1;
			//transfer_len = (rand() % (cap / 4096 + cap % 4096));
			transfer_len = 0x1f;//(rand() % (0x1f));
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
			if(cap > (0xffffffff + 0xffff))
			{
				opcode = WRITE16_CMD;
				len = WRITE16_LEN;
			}
			else if(cap > (0x1fffff + 0xff))
			{
				opcode = rand() % 0x2;
				if(0 == opcode)
				{
					opcode = WRITE10_CMD;
					len = WRITE10_LEN;
				}
				else
				{
					opcode = WRITE16_CMD;
					len = WRITE16_LEN;
				}
			}
			else
			{
				;
			}
			LOG_INFO("%s rand9 lba: %llx transfer_len %x\n", __func__, lba, transfer_len);		
		}
		else if(8 == randdata)
		{
			while((lba + transfer_len) <= (cap + 1))
			{
				lba = (cap) - (rand() % 0x50);
				transfer_len = (rand() % 0x1f);
			}
			if(0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}

			if(cap > (0xffffffff + 0xffff))
			{
				opcode = WRITE16_CMD;
				len = WRITE16_LEN;
			}
			else if(cap > (0x1fffff + 0xff))
			{
				opcode = (rand() % (0x2));
				if(0 == opcode)
				{
					opcode = WRITE10_CMD;
					len = WRITE10_LEN;
				}
				else
				{
					opcode = WRITE16_CMD;
					len = WRITE16_LEN;
				}
			}
			LOG_INFO("%s rand8 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else
		{
			;
		}


/*  		for (size_t i = 0; i < RW_BUFFER_SIZE; i++)
		{
			MEM8_GET(wr_buf+i) = BYTE_RAND();
			// LOG_INFO("%X",MEM8_GET(wr_buf));
		} 
 */		LOG_INFO("\nsuccess:\n");
		for(j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("write%x[%d]:", len, (j * 10));
			for(int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if((byte_count % 10) > 0)
		{
			LOG_INFO("write%x[%d]:", len, (j * 10));
			for(int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");

		error_status = scsi_write(fd, wr_buf, opcode, dpo, fua, grp_num,
 			lba, transfer_len, byte_count, SG4_TYPE);
		if(0 == err_event)
		{
			if(error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if(9 == randdata || 8 == randdata)
			{
				if((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_INFO("stop : %s randdata9/8 sense_data err randdata %d\n", __func__, randdata);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s randdata9/8 sense_data pass\n", __func__);
				}
			}
			else
			{
				if(error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if(-1 == ret)
	{
		LOG_INFO("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

/* int hpb_read_buffer(int fd, __u8 *buf, __u8 buf_id, __u32 hpb_region, 
	__u32 hpb_subregion, __u32 alloc_len,__u32 byte_count, __u8 sg_type)
{

	int ret;
	unsigned char hpb_read_buffer[HPB_READ_BUF_CMDLEN] = {HPB_READ_BUF_CMD,
		0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0) {
		print_error("scsi read cmd: wrong parameters\n");
		return -EINVAL;
	}

	hpb_read_buffer[1] = buf_id;
	put_unaligned_be16((__u32)hpb_region, hpb_read_buffer + 2);
	put_unaligned_be16((__u32)hpb_subregion, hpb_read_buffer + 4);
	put_unaligned_be24((__u32)alloc_len, hpb_read_buffer + 6);
	for(int i = 0; i< READ_BUF_CMDLEN; i++)
	{
		LOG_INFO("hpb_read_buffer[%d]: %x\n", i, hpb_read_buffer[i]);
	}
	LOG_INFO("Start : %s hpb_region %x hpb_subregion %x alloc_len %x buf_id %x byte_count %x\n", \
			__func__, hpb_region, hpb_subregion, alloc_len, buf_id, byte_count);
	ret = send_scsi_cmd_maxio(fd, hpb_read_buffer, buf,
			HPB_READ_BUF_CMDLEN, byte_count,
			SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0) {
		print_error("SG_IO READ BUFFER data error ret %d\n", ret);
	}

	return ret;
}
 */
void test_send_scsi(struct tool_options *opt)
{
	int fd;
	int oflag = O_RDWR;
	int i = 1;
	fd = open(opt->path, oflag);
	LOG_INFO("Start : %s fd %d\n", __func__, fd);

	if (fd < 0) {
		print_error("open");
	}
	// while(i < 1001)
	{
		LOG_INFO("Start :  loop %d\n", i++);
		// inquiry_test(fd, 1);
		// request_sense_test(fd, 1);
		// mode_select_test(fd, 1);
		// mode_sense_test(fd, 1);
		// unmap_test(fd, 1);
		// read_capacity_test(fd, 1);
		// format_unit_test(fd);
		// test_unit_ready_test(fd);
		// verify_test(fd, 1);
		// send_diagnostic_test(fd, 1);
		// report_lun_test(fd ,1);
		// sync_cache_test(fd, 1);
		// prefetch_test(fd, 1);

		// scsi_read_test(fd, 1);
		// scsi_write_test(fd, 1);
		// read_buffer_test(fd);
		// write_buffer_test(fd);
		hpb_read_buffer(fd, rd_buf, 1, 20, 20, 8000, 8000, SG4_TYPE);
	}
	close(fd);
}






