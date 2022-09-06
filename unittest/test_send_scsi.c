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
//#include "../scsi_bsg_util.h"
#include "../ufs_rpmb.h"
#include "../options.h"
// #include "auto_header.h"
#include "test_send_scsi.h"

extern byte_t current_pwr_mode;

static const char *const snstext[] = {
	"No Sense",		   /* 0: There is no sense information */
	"Recovered Error", /* 1: The last command completed successfully
				 but used error correction */
	"Not Ready",	   /* 2: The addressed target is not ready */
	"Medium Error",	   /* 3: Data error detected on the medium */
	"Hardware Error",  /* 4: Controller or device failure */
	"Illegal Request", /* 5: Error in request */
	"Unit Attention",  /* 6: Removable medium was changed, or
				 the target has been reset, or ... */
	"Data Protect",	   /* 7: Access to the data is blocked */
	"Blank Check",	   /* 8: Reached unexpected written or unwritten
				 region of the medium */
	"Vendor Specific",
	"Copy Aborted",	   /* A: COPY or COMPARE was aborted */
	"Aborted Command", /* B: The target aborted the command */
	"Equal",		   /* C: A SEARCH DATA command found data equal */
	"Volume Overflow", /* D: Medium full with still data to be written */
	"Miscompare",	   /* E: Source data and data on the medium
				 do not agree */
};

sense_data_t g_sense_data = {0};
// __u8
static inline void put_unaligned_be16(__u16 val, void *p)
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
	struct sg_io_v4 io_hdr_v4 = {0};
	struct sg_io_hdr io_hdr_v3 = {0};
	__u8 sense_buffer[SENSE_BUFF_LEN] = {0};

	if ((byte_cnt && buf == NULL) || cdb == NULL)
	{
		print_error("send_scsi_cmd_maxio: wrong parameters");
		return -EINVAL;
	}

	if (sg_type == SG4_TYPE)
	{
		io_hdr_v4.guard = 'Q';
		io_hdr_v4.protocol = BSG_PROTOCOL_SCSI;
		io_hdr_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
		io_hdr_v4.response = (__u64)sense_buffer;
		io_hdr_v4.max_response_len = SENSE_BUFF_LEN;
		io_hdr_v4.request_len = cmd_len;
		if (dir == SG_DXFER_FROM_DEV)
		{
			io_hdr_v4.din_xfer_len = (__u32)byte_cnt;
			io_hdr_v4.din_xferp = (__u64)buf;
		}
		else
		{
			io_hdr_v4.dout_xfer_len = (__u32)byte_cnt;
			io_hdr_v4.dout_xferp = (__u64)buf;
		}
		io_hdr_v4.request = (__u64)cdb;
		sg_struct = &io_hdr_v4;
	}
	else
	{
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
		   ((errno == EINTR) || (errno == EAGAIN)))
		;
	g_sense_data.sense_key = 0;
	g_sense_data.asc = 0;
	g_sense_data.ascq = 0;
	if (sg_type == SG4_TYPE)
	{
		if (io_hdr_v4.info != 0)
		{
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
	else
	{
		if (io_hdr_v3.status)
		{
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
				__u8 cmplst, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char format_cmd[FORMAT_CMDLEN] = {
		FORMAT_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi format cmd: wrong parameters\n");
		return -EINVAL;
	}
	format_cmd[1] = ((0x20 & (longlist << 5)) || (0x8 & (cmplst << 3)));
	for (int i = 0; i < FORMAT_CMDLEN; i++)
	{
		LOG_INFO("format_cmd[%d]: %x\n", i, format_cmd[i]);
	}
	LOG_INFO("Start : %s longlist %d , cmplst %d\n", __func__, longlist, cmplst);
	ret = send_scsi_cmd_maxio(fd, format_cmd, format_rsp, FORMAT_CMDLEN, byte_count,
							  SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		if (((0 != longlist) || (0 != cmplst)) && (-22 == ret))
		{
			// print_error("SG_IO format data error ret %d\n", ret);
			//  ret = 1;
		}
		else
		{
			print_error("SG_IO format data error ret %d\n", ret);
		}
	}
	return ret;
}

void format_unit_test(__u8 fd, __u8 lun)
{
	__u32 i = 0;
	int error_status = 0, ret = 1;
	__u8 longlist = 0;
	__u8 cmplst = 0;
	__u8 randdata = 0;
	__u8 format_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		ret = 1;
		randdata = (rand() % 0x10);
		if (0x9 == randdata)
		{
			do
			{
				longlist = (rand() % 0x2);
				cmplst = (rand() % 0x2);
			} while ((0 == longlist) && (0 == cmplst));
			LOG_INFO("%s rand9 longlist %d cmplst %d\n", __func__, longlist, cmplst);
		}
		else
		{
			longlist = 0;
			cmplst = 0;
		}
		LOG_INFO("%s i %x randdata %d\n", __func__, i, randdata);
		error_status = format_unit(fd, format_rsp, longlist,
								   cmplst, 0, sg_type);
		if (lun > 31 && 0xd0 != lun)
		{
			if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
				((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
			{
				LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
				ret = -1;
				break;
			}
			else
			{
				LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
			}
		}
		else
		{
			if ((0 != longlist) || (0 != cmplst))
			{
				if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
				{
					LOG_ERROR("stop : %s longlist%d cmplst%d sense_data err\n", __func__, longlist, cmplst);
					ret = -1;
					break;
				}
			}
			else
			{
				if (error_status < 0)
				{
					LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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
	unsigned char mode_select_cmd[MODE_SELECT_CMDLEN] = {
		MODE_SELECT_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi mode_select cmd: wrong parameters\n");
		return -EINVAL;
	}
	mode_select_cmd[1] = ((sp & 0x1) | ((pf & 0x1) << 4));
	// put_unaligned_be16(para_list_len, (mode_select_cmd + 7));
	mode_select_cmd[7] = (__u8)(para_list_len);
	mode_select_cmd[8] = (__u8)(para_list_len >> 8);
	for (int i = 0; i < MODE_SELECT_CMDLEN; i++)
	{
		LOG_INFO("mode_select_cmd[%d]: %x\n", i, mode_select_cmd[i]);
	}
	LOG_INFO("\n%s sp %d para_list_len %x\n", __func__, sp, para_list_len);
	ret = send_scsi_cmd_maxio(fd, mode_select_cmd, mode_select_req,
							  WRITE_BUF_CMDLEN, byte_count, SG_DXFER_TO_DEV, sg_type);
	if (ret < 0)
	{
		print_error("SG_IO mode_select data error ret %d\n", ret);
	}
	return ret;
}

void mode_select_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0, j = 0;
	int error_status = 0, ret = 1;
	__u8 sp = 0, pf = 0, page_code = 0, para_list_len = 0;
	__u8 randdata = 0;
	__u32 byte_count = 0;
	//__u8 mode_select_req[100] = {0};
	__u8 *mode_select_req;
	mode_select_req = (__u8 *)wr_buf;
	LOG_INFO("\nStart : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d \n", __func__, i);
		sp = (rand() % 0x2);
		page_code = (rand() % 0x4);
		pf = 1;
		ret = 1;
		// page_code = 2;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (0 == page_code)
		{
			para_list_len = 0x14;
			byte_count = para_list_len;
			for (int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (RW_ERR_RECOVERY & 0x3f); // rw_err_recovery
			mode_select_req[9] = 0x0a;
			mode_select_req[10] = 0x80;
			mode_select_req[11] = 0x03;
			mode_select_req[16] = 0x03;
			mode_select_req[18] = 0xff;
			mode_select_req[19] = 0xff;
		}
		else if (1 == page_code)
		{
			para_list_len = 0x1c;
			byte_count = para_list_len;
			for (int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (CACHING & 0x3f); // caching
			mode_select_req[9] = 0x12;
			mode_select_req[10] = 0x4;
		}
		else if (2 == page_code)
		{
			para_list_len = 0x14;
			byte_count = para_list_len;
			for (int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			mode_select_req[8] = (CONTROL & 0x3f); // control
			mode_select_req[9] = 0x0a;
			mode_select_req[11] = 0x10;
			mode_select_req[16] = 0xff;
			mode_select_req[17] = 0xff;
		}
		else
		{
			para_list_len = 0xff;
			byte_count = para_list_len;
			for (int i = 0; i < byte_count; i++)
			{
				mode_select_req[i] = 0;
			}
			// mode_select_req[8] = (ALL_PAGE & 0x3f);//all_page
			// mode_select_req[9] = 0x26;
			mode_select_req[8] = (RW_ERR_RECOVERY & 0x3f); // rw_err_recovery
			mode_select_req[9] = 0x0a;
			mode_select_req[10] = 0x80;
			mode_select_req[11] = 0x03;
			mode_select_req[16] = 0x03;
			mode_select_req[18] = 0xff;
			mode_select_req[19] = 0xff;
			mode_select_req[20] = (CACHING & 0x3f); // caching
			mode_select_req[21] = 0x12;
			mode_select_req[22] = 0x5;
			mode_select_req[40] = (CONTROL & 0x3f); // control
			mode_select_req[41] = 0x0a;
			mode_select_req[43] = 0x10;
			mode_select_req[48] = 0xff;
			mode_select_req[49] = 0xff;
		}
		if (9 == randdata)
		{
			while ((mode_select_req[8] == (RW_ERR_RECOVERY & 0x3f)) || (mode_select_req[8] == (CACHING & 0x3f)) || (mode_select_req[8] == (CONTROL & 0x3f)))
			{
				mode_select_req[8] = (rand() % 0x20) & 0x3f;
			}
			LOG_INFO("%s rand9 opcode: %x\n", __func__, mode_select_req[8]);
		}
		else if (8 == randdata)
		{
			while ((para_list_len == 0x14) || (para_list_len == 0x1c) || (para_list_len == 0xff))
			{
				para_list_len = (rand() % 0x30);
				byte_count = para_list_len;
			}
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, para_list_len);
		}
		else if (7 == randdata)
		{
			while ((mode_select_req[9] == 0xa) || (mode_select_req[9] == 0x12))
			{
				mode_select_req[9] = (rand() % 0x20);
				LOG_INFO("%s rand7 page_length: %x\n", __func__, mode_select_req[9]);
			}
		}
		else if (6 == randdata)
		{
			pf = 0;
			LOG_INFO("%s rand7 pf: %x\n", __func__, pf);
		}
		else
		{
			;
		}
		// LOG_INFO("\nsuccess:\n");
		// for(int i = 0; i< byte_count; i++)
		// {
		// 	LOG_INFO("mode_select_req[%d]: %x\n", i, mode_select_req[i]);
		// }

		LOG_INFO("\n");

		for (j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("mode_select_req[%d]:", (j * 10));
			for (int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if ((byte_count % 10) > 0)
		{
			LOG_INFO("mode_select_req[%d]:", (j * 10));
			for (int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				// j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");
		error_status = mode_select(fd, mode_select_req, sp, pf, para_list_len,
								   byte_count, sg_type);
		if (1 == err_event)
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else
			{
				if (9 == randdata || 7 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x26 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				else if (8 == randdata)
				{
					LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
				}
				else if (6 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
					if (error_status < 0)
					{
						LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
		}
		else
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else
			{
				if (error_status < 0)
				{
					LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int mode_sense(int fd, __u8 *mode_sense_rsp, __u8 page_code, __u8 pc,
			   __u8 subpage_code, __u16 alloc_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char mode_sense_cmd[MODE_SENSE_CMDLEN] = {
		MODE_SENSE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi mode_sense cmd: wrong parameters\n");
		return -EINVAL;
	}
	mode_sense_cmd[1] = 0x8;
	mode_sense_cmd[2] = (0xc0 & (pc << 6)) | (0x3f & page_code);
	put_unaligned_be16(alloc_len, (mode_sense_cmd + 7));
	for (int i = 0; i < 10; i++)
	{
		LOG_INFO("mode_sense_cmd[%d]: %x\n", i, mode_sense_cmd[i]);
	}
	LOG_INFO("Start : %s pc %d page_code %x subpage_code %x alloc_len %x\n", __func__, pc, page_code, subpage_code, alloc_len);
	ret = send_scsi_cmd_maxio(fd, mode_sense_cmd, mode_sense_rsp, MODE_SENSE_CMDLEN, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO mode_sense data error ret %d\n", ret);
	}
	return ret;
}

void mode_sense_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0, j = 0;
	int error_status = 0, ret = 0;
	__u8 page_code = 0, pc = 0, subpage_code = 0;
	__u16 alloc_len = 0;
	__u32 byte_count = 0;
	__u8 randdata = 0;
	//__u8 mode_sense_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		page_code = (rand() % 0x4);
		pc = (rand() % 0x4);
		ret = 1;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (0 == page_code)
		{
			page_code = RW_ERR_RECOVERY;
			alloc_len = 0x14;
			byte_count = 0x14;
		}
		else if (1 == page_code)
		{
			page_code = CACHING;
			alloc_len = 0x1c;
			byte_count = 0x1c;
		}
		else if (2 == page_code)
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
		if (9 == randdata)
		{
			while ((page_code == RW_ERR_RECOVERY) || (page_code == CACHING) || (page_code == CONTROL) || (page_code == ALL_PAGE))
			{
				page_code = (rand() % 0x40);
			}
			LOG_INFO("%s rand9 page_code: %x\n", __func__, page_code);
		}
		else if (8 == randdata)
		{
			while ((alloc_len == 0x14) || (alloc_len == 0x1c) || (alloc_len == 0xff))
			{
				alloc_len = (rand() % 0x30);
				byte_count = alloc_len;
			}
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
		}
		else
		{
			;
		}
		error_status = mode_sense(fd, rd_buf, page_code, pc,
								  subpage_code, alloc_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else if (error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				else if (8 == randdata)
				{
					LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
		LOG_INFO("\nsuccess:\n");
		// for(int i = 0; i< byte_count; i++)
		// {
		// 	LOG_INFO("mode_sense_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
		// }
		for (j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("mode_sense_rsp[%d]:", (j * 10));
			for (int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if ((byte_count % 10) > 0)
		{
			LOG_INFO("mode_sense_rsp[%d]:", (j * 10));
			for (int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
				// j++;
			}
			LOG_INFO("\n");
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int prefetch(int fd, __u8 *prefetch_rsp, __u8 opcode, __u8 immed,
			 __u64 lba, __u32 prefetch_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 prefetch_cmd_len = 0;
	unsigned char prefetch_cmd10[PRE_FETCH10_CMDLEN] = {
		PRE_FETCH10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char prefetch_cmd16[PRE_FETCH16_CMDLEN] = {
		PRE_FETCH16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;
	if (PRE_FETCH10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (prefetch_cmd10 + 2));
		put_unaligned_be16((uint16_t)prefetch_len, (prefetch_cmd10 + 7));
		prefetch_cmd_len = PRE_FETCH10_CMDLEN;
		prefetch_cmd10[1] = (immed & 0x1) << 1;
		cmd_addr = prefetch_cmd10;
		for (int i = 0; i < PRE_FETCH10_CMDLEN; i++)
		{
			LOG_INFO("prefetch_cmd10[%d]: %x\n", i, prefetch_cmd10[i]);
		}
	}
	else if (PRE_FETCH16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (prefetch_cmd16 + 2));
		put_unaligned_be32((uint32_t)prefetch_len, (prefetch_cmd16 + 10));
		prefetch_cmd_len = PRE_FETCH16_CMDLEN;
		prefetch_cmd16[1] = (immed & 0x1) << 1;
		cmd_addr = prefetch_cmd16;
		for (int i = 0; i < PRE_FETCH16_CMDLEN; i++)
		{
			LOG_INFO("prefetch_cmd16[%d]: %x\n", i, prefetch_cmd16[i]);
		}
	}
	else
	{
		perror("scsi prefetch cmd: wrong parameters opcode\n");
		return -EINVAL;
	}

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	LOG_INFO("Start : %s opcode %d, immed %d, lba %llx, prefetch_len %x\n",
			 __func__, opcode, immed, lba, prefetch_len);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, prefetch_rsp, prefetch_cmd_len, byte_count,
							  SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO prefetch data error ret %d\n", ret);
	}
	return ret;
}

void prefetch_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	;
	__u8 immed = 0, opcode = 0;
	__u64 lba = 0;
	__u32 prefetch_len = 0;
	__u32 byte_count = 0;
	__u64 cap = 0;
	__u8 randdata = 0;
	__u8 prefetch_rsp[1] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, lun);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		LOG_INFO("\n%s i %d\n", __func__, i);
		immed = (rand() % 0x2);
		opcode = (rand() % 0x2) + 1;
		ret = 1;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (1 == opcode)
		{
			opcode = PRE_FETCH10_CMD;
		}
		else if (2 == opcode)
		{
			opcode = PRE_FETCH16_CMD;
		}
		else
		{
			opcode = 0xff;
		}

		do
		{
			if (PRE_FETCH10_CMD == opcode)
			{
				if (cap < 0xffffffff)
				{
					lba = rand() % cap;
				}
				else
				{
					lba = rand() % 0xffffffff;
				}
				if (cap < 0xffff)
				{
					prefetch_len = rand() % cap;
				}
				else
				{
					prefetch_len = rand() % 0xffff;
				}
			}
			else if (PRE_FETCH16_CMD == opcode)
			{
				if (cap < 0xffffffffffffffff)
				{
					lba = rand() % cap;
				}
				else
				{
					lba = rand() % 0xffffffffffffffff;
				}
				if (cap < 0xffffffff)
				{
					prefetch_len = rand() % cap;
				}
				else
				{
					prefetch_len = rand() % 0xffffffff;
				}
			}
		} while ((lba + prefetch_len) > cap);

		if (9 == randdata)
		{
			lba = cap + 1;
			prefetch_len = 0;
			LOG_INFO("%s rand9 lba: %llx\n", __func__, lba);
		}
		else if (8 == randdata)
		{
			while ((lba + prefetch_len) <= (cap + 1))
			{
				lba = cap - (rand() % 0x50);
				prefetch_len = (rand() % 0x100);
			}
			LOG_INFO("%s rand8 lba: %llx prefetch_len: %x\n", __func__, lba, prefetch_len);
		}
		else if (7 == randdata)
		{
			if (SYNCHRONIZE_CACHE10_CMD == opcode)
			{
				if (cap <= 0xffff)
				{
					lba = 0;
					prefetch_len = cap + 1;
				}
				else
				{
					lba = (__u32)cap;
					prefetch_len = rand() % 0xffff + 2;
				}
			}
			else if (SYNCHRONIZE_CACHE16_CMD == opcode)
			{
				lba = (__u32)cap;
				prefetch_len = rand() % 0xffff + 2;
			}
			else
			{
				lba = (__u32)cap;
				prefetch_len = 2;
			}
			LOG_INFO("%s rand7 lba %llx prefetch_len %x\n", __func__, lba, prefetch_len);
		}
		else if (6 == randdata)
		{
			lba = 0;
			prefetch_len = 0;
			LOG_INFO("%s rand6 lba %llx prefetch_len: %x\n", __func__, lba, prefetch_len);
		}
		else
		{
			;
		}
		error_status = prefetch(fd, prefetch_rsp, opcode, immed,
								lba, prefetch_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				LOG_INFO("stop : %s lun%x\n", __func__, lun);
				if ((9 == randdata || 8 == randdata || 7 == randdata))
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%x sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_read_buffer(int fd, __u8 *buf, __u8 mode, __u8 buf_id,
					 __u32 buf_offset, __u32 byte_count, __u8 sg_type)
{

	int ret;
	unsigned char read_buf_cmd[READ_BUF_CMDLEN] = {READ_BUFFER_CMD,
												   0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		print_error("scsi read cmd: wrong parameters\n");
		return -EINVAL;
	}

	read_buf_cmd[1] = (mode & 0x1f);
	read_buf_cmd[2] = buf_id;
	put_unaligned_be24((__u32)buf_offset, read_buf_cmd + 3);
	put_unaligned_be24((__u32)byte_count, read_buf_cmd + 6);
	for (int i = 0; i < READ_BUF_CMDLEN; i++)
	{
		LOG_INFO("read_buf_cmd[%d]: %x\n", i, read_buf_cmd[i]);
	}
	LOG_INFO("Start : %s buf_offset %x buf_id %x byte_count %x\n",
			 __func__, buf_offset, buf_id, byte_count);
	ret = send_scsi_cmd_maxio(fd, read_buf_cmd, buf,
							  READ_BUF_CMDLEN, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO READ BUFFER data error ret %d\n", ret);
	}

	return ret;
}

void read_buffer_test(__u8 fd, __u8 lun)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0;
	__u8 mode = 0xff, buf_id = 0;
	//__u8 buf_id_max = 0, buf_id_flag = 0;
	__u8 host2reg = 0xff;
	__u32 buf_offset = 0;
	__u32 byte_count = 0;
	// __u8 read_buffer_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		host2reg = 0;
		ret = 1;
		if (0xff == mode)
		{
			mode = (rand() % 0x2); // expect for vendor mode
		}
		mode = 1;
		if (0 == mode)
		{
			mode = BUFFER_EHS_MODE; // error history
			buf_id = (rand() % 0x2);
			if (0 == buf_id)
			{
				buf_offset = 0;
			}
			else if (1 == buf_id)
			{
				buf_id = (rand() % 0xe0) + 0x10;
				buf_offset = (rand() % 0xff);
			}
			byte_count = 5;
		}
		else if (1 == mode)
		{
			mode = BUFFER_DATA_MODE; // data
			buf_offset = (rand() % 0x30);
			// if(0 == buf_id_flag)
			// {
			// 	buf_id = (rand() % 0x2);
			// 	if(0 == buf_id)
			// 	{
			// 		buf_id_max = (rand() % 0x2) + 1;
			// 	}
			// }
			// if(buf_id_max > 1)
			// {
			// 	buf_id_flag = 1;
			// 	if((buf_id == (buf_id_max - 1)) || (buf_id > 1))
			// 	{
			// 		buf_id_flag = 0;
			// 		buf_id = (rand() % 0x2);
			// 	}
			// 	else
			// 	{
			// 		buf_id++;
			// 	}
			// }
			// else
			// {
			// 	buf_id_flag = 0;
			// }
			buf_id = (rand() % 0x2);
			buf_offset = 0;
			host2reg = (rand() % 0x3);
			if (0 == host2reg)
			{
				byte_count = (rand() % (4 * 1024) + 1) * 8;
			}
			else if (1 == host2reg)
			{
				byte_count = 32 * 1024;
			}
			else
			{
				byte_count = 4; //(rand() % 0x50) + 1;
			}
		}
		else if (2 == mode)
		{
			mode = BUFFER_VENDOR_MODE; // vendor specific
			LOG_ERROR("stop %s: expect for vendor mode\n", __func__);
			// buf_id = ;
			// buf_offset = ;
			// byte_count = ;
		}
		else
		{
			;
		}

		if (BUFFER_DATA_MODE == mode)
		{
			for (size_t i = 0; i < byte_count; i++)
			{
				MEM8_GET(wr_buf + i) = BYTE_RAND();
				MEM8_GET(rd_buf + i) = 0x0;
				// LOG_INFO("%X",MEM8_GET(wr_buf));
			}
			error_status = scsi_write_buffer(fd, wr_buf, mode, buf_id,
											 buf_offset, byte_count, sg_type);
		}

		error_status = scsi_read_buffer(fd, rd_buf, mode, buf_id,
										buf_offset, byte_count, sg_type);
		if (error_status < 0)
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if (0)
			{
				LOG_INFO("\nsuccess:\n");
				for (j = 0; j < (byte_count / 10); j++)
				{
					LOG_INFO("rd_buf[%d]:", (j * 10));
					for (int i = 0; i < 10; i++)
					{
						LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
					}
					LOG_INFO("\n");
				}
				if ((byte_count % 10) > 0)
				{
					LOG_INFO("rd_buf[%d]:", (j * 10));
					for (int i = 0; i < (byte_count % 10); i++)
					{
						LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
					}
					LOG_INFO("\n");
				}
			}
			else if (BUFFER_DATA_MODE == mode)
			{
				error_status = memcmp(wr_buf, rd_buf, byte_count);
				if (0 != error_status)
				{
					LOG_INFO("\n memcmp fail:\n");
					LOG_ERROR("memcmp : %s err %d\n", __func__, error_status);
					ret = -1;
					LOG_INFO("\n wr_buf:\n");
					for (j = 0; j < (byte_count / 10); j++)
					{
						// LOG_INFO("wr_buf[%d]:", (j * 10));
						for (int i = 0; i < 10; i++)
						{
							LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
							// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
						}
						LOG_INFO("\n");
					}
					if ((byte_count % 10) > 0)
					{
						// LOG_INFO("wr_buf[%d]:", (j * 10));
						for (int i = 0; i < (byte_count % 10); i++)
						{
							LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
							// j++;
						}
						LOG_INFO("j%d\n", j);
					}
					LOG_INFO("\n");

					LOG_INFO("\n rd_buf:\n");
					for (j = 0; j < (byte_count / 10); j++)
					{
						// LOG_INFO("rd_buf[%d]:", (j * 10));
						for (int i = 0; i < 10; i++)
						{
							LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
						}
						LOG_INFO("\n");
					}
					if ((byte_count % 10) > 0)
					{
						// LOG_INFO("rd_buf[%d]:", (j * 10));
						for (int i = 0; i < (byte_count % 10); i++)
						{
							LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
							// j++;
						}
						LOG_INFO("\nj%d\n", j);
					}
					LOG_INFO("\n");
					break;
				}
				else
				{
					LOG_INFO("\n memcmp pass:\n");
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int read_capacity(int fd, __u8 *read_capacity_rsp, __u8 opcode,
				  __u32 lba, __u32 alloc_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 read_capacity_cmd_len = 0;
	unsigned char read_capacity_cmd10[READ_CAPACITY10_CMDLEN] = {
		READ_CAPACITY10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char read_capacity_cmd16[READ_CAPACITY16_CMDLEN] = {
		READ_CAPACITY16_CMD,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	};
	unsigned char *cmd_addr;
	LOG_INFO("Start : %s opcode %x, lba %x, alloc_len %x\n",
			 __func__, opcode, lba, alloc_len);
	if (READ_CAPACITY10_CMD == opcode)
	{
		put_unaligned_be16((uint16_t)lba, (read_capacity_cmd10 + 2));
		read_capacity_cmd_len = READ_CAPACITY10_CMDLEN;
		cmd_addr = read_capacity_cmd10;
		// for(int i = 0; i< READ_CAPACITY10_CMDLEN; i++)
		// {
		// 	LOG_INFO("read_capacity_cmd10[%d]: %x\n", i, read_capacity_cmd10[i]);
		// }
	}
	else if (READ_CAPACITY16_CMD == opcode)
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
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi read_capacity cmd: wrong parameters\n");
		return -EINVAL;
	}
	ret = send_scsi_cmd_maxio(fd, cmd_addr, read_capacity_rsp, read_capacity_cmd_len,
							  byte_count, SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO read_capacity data error ret %d\n", ret);
	}
	return ret;
}

void read_capacity_test(__u8 fd, __u8 lun, __u8 err_event)
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
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		opcode = (rand() % 0x2);
		// lba = (rand() % 0x100);
		ret = 1;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}

		if (0 == opcode)
		{
			opcode = READ_CAPACITY10_CMD;
			byte_count = 0x8;
		}
		else if (1 == opcode)
		{
			opcode = READ_CAPACITY16_CMD;
			alloc_len = 0x20; // ufs spec 194
			byte_count = 0x20;
		}
		else
		{
			;
		}

		if (9 == randdata)
		{
			if (READ_CAPACITY16_CMD == opcode)
			{
				while ((0 == alloc_len) || (0x20 == alloc_len))
				{
					alloc_len = (rand() % 0x40);
					byte_count = alloc_len;
				}
			}
			LOG_INFO("%s rand9 page_code: %x\n", __func__, alloc_len);
		}
		else
		{
			;
		}

		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = read_capacity(fd, rd_buf, opcode,
									 lba, alloc_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata)
				{
					LOG_INFO("stop : %s randdata9 response panding check\n", __func__);
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
		if (error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for (int i = 0; i < byte_count; i++)
			{
				LOG_INFO("read_capacity_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
			}
			LOG_INFO("\n");
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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
		 */
		LOG_INFO("stop : %s result is 1 \n", __func__);
	}
}

__u64 read_capacity_for_cap(__u8 fd, __u8 lun)
{
	int error_status = 0, ret = 0;
	__u8 opcode = 0;
	__u32 alloc_len = 0;
	__u64 lba = 0;
	__u32 byte_count = 0;
	__u64 cap = 0;
	//__u8 read_capacity_rsp[100] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	LOG_INFO("Start : %s\n", __func__);
	opcode = 1;
	if (0 == opcode)
	{
		opcode = READ_CAPACITY10_CMD;
		byte_count = 0x8;
	}
	else if (1 == opcode)
	{
		opcode = READ_CAPACITY16_CMD;
		alloc_len = 0x20; // ufs spec 194
		byte_count = 0x20;
	}
	else
	{
		;
	}

	error_status = read_capacity(fd, rd_buf, opcode,
								 lba, alloc_len, byte_count, sg_type);
	if (error_status < 0)
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

	if (-1 == ret || lun > 31)
	{
		LOG_ERROR("stop : %s read cap fail\n", __func__);
		cap = 0;
	}
	else
	{
		if (READ_CAPACITY10_CMD == opcode)
		{
			cap = ((__u32)((__u8 *)rd_buf)[0] << 24) + ((__u32)((__u8 *)rd_buf)[1] << 16) + ((__u32)((__u8 *)rd_buf)[2] << 8) + ((__u8 *)rd_buf)[3];
		}
		else if (READ_CAPACITY16_CMD == opcode)
		{
			cap = ((__u64)((__u8 *)rd_buf)[0] << 56) + ((__u64)((__u8 *)rd_buf)[1] << 48) + ((__u64)((__u8 *)rd_buf)[2] << 40) + ((__u64)((__u8 *)rd_buf)[3] << 32) +
				  ((__u64)((__u8 *)rd_buf)[4] << 24) + ((__u64)((__u8 *)rd_buf)[5] << 16) + ((__u64)((__u8 *)rd_buf)[6] << 8) + ((__u8 *)rd_buf)[7];
		}
		LOG_INFO("stop : %s result is 1 cap:%llx\n", __func__, cap);
	}
	return cap;
}

int report_lun(int fd, __u8 *report_lun_rsp, __u8 select_report,
			   __u16 alloc_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char report_lun_cmd[REPORT_LUNS_CMDLEN] = {REPORT_LUNS_CMD,
														0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		print_error("scsi report_lun cmd: wrong parameters\n");
		return -EINVAL;
	}

	report_lun_cmd[2] = select_report;
	put_unaligned_be16((__u16)alloc_len, report_lun_cmd + 6);
	for (int i = 0; i < REPORT_LUNS_CMDLEN; i++)
	{
		LOG_INFO("report_lun_cmd[%d]: %x\n", i, report_lun_cmd[i]);
	}
	LOG_INFO("Start : %s select_report %d, alloc_len %d\n",
			 __func__, select_report, alloc_len);
	ret = send_scsi_cmd_maxio(fd, report_lun_cmd, report_lun_rsp,
							  REPORT_LUNS_CMDLEN, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO report_lun data error ret %d\n", ret);
	}

	return ret;
}

void report_lun_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1, randdata = 0, j = 0;
	__u32 byte_count = 0;
	__u8 select_report = 0;
	__u16 alloc_len = 0;
	//__u8 report_lun_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		ret = 1;
		select_report = (rand() % 0x3);
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}

		if (0 == select_report)
		{
			alloc_len = 0x20;
		}
		else if (1 == select_report)
		{
			alloc_len = 0x28;
		}
		else if (2 == select_report)
		{
			alloc_len = 0x40;
		}
		else
		{
			;
		}
		byte_count = alloc_len;
		randdata = 9;
		if (9 == randdata)
		{
			select_report = ((rand() % 0x5) + 0x3) & 0x3f;
			LOG_INFO("%s rand9 select_report: %x\n", __func__, select_report);
		}
		else if (8 == randdata)
		{
			if (0 == select_report)
			{
				while (0x20 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x30;
				}
			}
			else if (1 == select_report)
			{
				while (0x28 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x50;
				}
			}
			else if (2 == select_report)
			{
				while (0x40 == alloc_len || 0 == alloc_len)
				{
					alloc_len = rand() % 0x80;
				}
			}
			else
			{
				;
			}
			byte_count = alloc_len;
			LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
		}
		else
		{
			;
		}

		error_status = report_lun(fd, rd_buf, select_report,
								  alloc_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun < 0x20 && 0x81 != lun)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20 || 0x81 == lun)
			{
				if (9 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				else if (8 == randdata)
				{
					LOG_INFO("stop : %s randdata8 response panding check\n", __func__);
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
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
		for (j = 0; j < (byte_count / 10); j++)
		{
			LOG_INFO("report_lun_rsp[%d]:", (j * 10));
			for (int i = 0; i < 10; i++)
			{
				LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
			}
			LOG_INFO("\n");
		}
		if ((byte_count % 10) > 0)
		{
			LOG_INFO("report_lun_rsp[%d]:", (j * 10));
			for (int i = 0; i < (byte_count % 10); i++)
			{
				LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
				// j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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

	if (fd < 0 || byte_count < 0)
	{
		print_error("scsi request_sense cmd: wrong parameters\n");
		return -EINVAL;
	}

	request_sense_cmd[4] = alloc_len;

	for (int i = 0; i < REQUEST_SENSE_CMDLEN; i++)
	{
		LOG_INFO("request_sense_cmd[%d]: %x\n", i, request_sense_cmd[i]);
	}
	LOG_INFO("Start : %s  alloc_len %d\n",
			 __func__, alloc_len);
	ret = send_scsi_cmd_maxio(fd, request_sense_cmd, request_sense_rsp,
							  REQUEST_SENSE_CMDLEN, byte_count, SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO request_sense data error ret %d\n", ret);
	}

	return ret;
}

void request_sense_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1;
	__u32 byte_count = 0;
	__u8 alloc_len = 0;
	__u8 randdata = 0;
	__u8 request_sense_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (1 == err_event)
		{
			randdata = (rand() % 0xa);
		}
		alloc_len = 0x12;
		byte_count = alloc_len;
		ret = 1;
		if (9 == randdata)
		{
			while (0x12 == alloc_len)
			{
				alloc_len = (rand() % 0x25) + 1;
			}
			byte_count = alloc_len;
		}
		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = request_sense(fd, request_sense_rsp, alloc_len,
									 byte_count, sg_type);

		if (0 == err_event)
		{
			if (error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if (9 == randdata)
			{
				LOG_INFO("stop : %s randdata9 response panding check\n", __func__);
			}
			else
			{
				if (error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		if (error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for (int i = 0; i < byte_count; i++)
			{
				LOG_INFO("request_sense_rsp[%d]: %x\n", i, request_sense_rsp[i]);
			}
			LOG_INFO("\n");
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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

int statr_stop_unit(int fd, __u8 *ssu_rsp, __u8 immed, __u8 power_conditions, __u8 pcm, __u8 loej,
					__u8 no_flush, __u8 start, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char statr_stop_unit_cmd[START_STOP_UNIT_CMDLEN] = {
		START_STOP_UNIT_CMD, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi statr_stop_unit cmd: wrong parameters\n");
		return -EINVAL;
	}
	statr_stop_unit_cmd[1] = (immed & 0x1);
	statr_stop_unit_cmd[3] = (pcm & 0xf);
	statr_stop_unit_cmd[4] = ((power_conditions & 0xf) << 4) | ((no_flush & 0x1) << 2) | ((loej & 0x1) << 1) | (start & 0x1);
	for (int i = 0; i < START_STOP_UNIT_CMDLEN; i++)
	{
		LOG_INFO("statr_stop_unit_cmd[%d]: %x\n", i, statr_stop_unit_cmd[i]);
	}
	LOG_INFO("Start : %s immed %d, power_conditions %d, no_flush %d start %d\n",
			 __func__, immed, power_conditions, no_flush, start);
	ret = send_scsi_cmd_maxio(fd, statr_stop_unit_cmd, ssu_rsp, START_STOP_UNIT_CMDLEN, byte_count,
							  SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO statr_stop_unit_cmd data error ret %d\n", ret);
	}
	return ret;
}

void statr_stop_unit_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	;
	__u8 immed = 0, no_flush = 0, power_conditions = 0, start = 0, pcm = 0, loej = 0;
	__u8 randdata = 0;
	__u8 pre_pwr_mode = 0xff, next_pwr_mode = 0xff;
	__u32 byte_count = 0;
	__u8 ssu_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		immed = (rand() % 0x2);
		start = (rand() % 0x2);
		start = 1;
		ret = 1;
		pre_pwr_mode = 0xff;
		next_pwr_mode = 0xff;
		current_pwr_mode = 0xff;
		get_currnet_pwr_mode();
		pre_pwr_mode = current_pwr_mode;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}

		switch (pre_pwr_mode)
		{
		case 0x00:
			power_conditions = (rand() % 0x3) + 1;
			break;
		case 0x10:
			power_conditions = 1;
			break;
		case 0x11:
			power_conditions = (rand() % 0x3) + 1;
			break;
		case 0x20:
			if (immed)
			{
				power_conditions = (rand() % 0x2) + 1;
			}
			else
			{
				power_conditions = 2;
			}
			break;
		case 0x22:
			power_conditions = (rand() % 0x3) + 1;
			break;
		case 0x30:
			if (immed)
			{
				power_conditions = (rand() % 0x2);
				if (0 == power_conditions)
				{
					power_conditions = 1;
				}
				else
				{
					power_conditions = 3;
				}
			}
			else
			{
				power_conditions = 3;
			}
			break;
		case 0x33:
			power_conditions = (rand() % 0x2);
			if (0 == power_conditions)
			{
				power_conditions = 1;
			}
			else
			{
				power_conditions = 3;
			}
			break;
		// case 4:
		// 	immed = 0;
		// break;
		default:
			LOG_ERROR("current_pwr_mode err%d\n", current_pwr_mode);
			pre_pwr_mode = 0xff;
			break;
		}
		if (lun < 0x20)
		{
			power_conditions = 0;
		}
		if (0x9 == randdata)
		{
			switch (pre_pwr_mode)
			{
			// lun not ready
			case 0x10:
				do
				{
					power_conditions = (rand() % 0xf);
				} while (1 == power_conditions);
				break;
			case 0x20:
				if (immed)
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while ((1 == power_conditions) || (2 == power_conditions));
				}
				else
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while ((2 == power_conditions));
				}
				break;
			case 0x30:
				if (immed)
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while ((1 == power_conditions) || (3 == power_conditions));
				}
				else
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while (3 == power_conditions);
				}
				break;
			// invalid field in cdb
			case 0x00:
				if (lun > 32)
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while ((1 == power_conditions) || (2 == power_conditions) || (3 == power_conditions) || (4 == power_conditions));
				}
				else
				{
					do
					{
						power_conditions = (rand() % 0xf);
					} while ((0 == power_conditions));
				}
				break;
			case 0x11:
				do
				{
					power_conditions = (rand() % 0xf);
				} while ((1 == power_conditions) || (2 == power_conditions) || (3 == power_conditions) || (4 == power_conditions));
				break;
			case 0x22:
				do
				{
					power_conditions = (rand() % 0xf);
				} while ((1 == power_conditions) || (2 == power_conditions) || (3 == power_conditions) || (4 == power_conditions));
				break;
			case 0x33:
				do
				{
					power_conditions = (rand() % 0xf);
				} while ((1 == power_conditions) || (3 == power_conditions));
			// case 4:
			// break;
			default:
				LOG_ERROR("current_pwr_mode err%d\n", current_pwr_mode);
				break;
			}
			LOG_INFO("%s rand9 pre_pwr_mode %x power_conditions %d \n", __func__, pre_pwr_mode, power_conditions);
		}
		else if (0x8 == randdata)
		{
			do
			{
				pcm = (rand() % 0x2);
				loej = (rand() % 0x2);
			} while ((0 == pcm) && (0 == loej));
			LOG_INFO("%s rand8 pcm %d loej %d\n", __func__, pcm, loej);
		}
		else
		{
			LOG_INFO("%s normal pre_pwr_mode %x power_conditions %d \n", __func__, pre_pwr_mode, power_conditions);
		}
		// power_conditions = 0x1;

		error_status = statr_stop_unit(fd, ssu_rsp, immed, power_conditions, pcm, loej,
									   no_flush, start, byte_count, sg_type);
		get_currnet_pwr_mode();
		next_pwr_mode = current_pwr_mode & 0xf;

		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31 && 0xd0 != lun)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
			else if (0xd0 == lun)
			{
				if (0x4 == power_conditions)
				{
					LOG_ERROR("deepsleep power mode can't check now \n");
				}
				else if (0 == immed)
				{
					if ((power_conditions != next_pwr_mode) && 0x4 != power_conditions)
					{
						LOG_ERROR("power mode check fail power_conditions %d next_pwr_mode %d\n", power_conditions, next_pwr_mode);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("power mode check pass \n");
					}
				}
				else
				{
					LOG_INFO("ssu pass \n");
				}
			}
			else if (lun < 0x20)
			{
				if (error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("n_lun on/off pass \n");
				}
			}
			else
			{
				LOG_INFO("ssu pass \n");
			}
		}
		else
		{
			if (0xd0 == lun)
			{
				if (9 == randdata)
				{
					if (0x00 == pre_pwr_mode || 0x11 == pre_pwr_mode || 0x22 == pre_pwr_mode || 0x33 == pre_pwr_mode)
					{
						if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
						{
							LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
							ret = -1;
							break;
						}
						else
						{
							LOG_ERROR("stop : %s randdata%x sense_data pass\n", __func__, randdata);
						}
					}
					else if (0x10 == pre_pwr_mode || 0x20 == pre_pwr_mode || 0x30 == pre_pwr_mode)
					{
						if ((0x2 != g_sense_data.sense_key) || (0x1a != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
						{
							LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
							ret = -1;
							break;
						}
						else
						{
							LOG_INFO("stop : %s randdata%x sense_data pass\n", __func__, randdata);
						}
					}
					else
					{
						if (0xff == pre_pwr_mode)
						{
							LOG_ERROR("stop : %s randdata%x device pwr get err :pre_pwr_mode %x\n", __func__, randdata, pre_pwr_mode);
						}
						else
						{
							LOG_ERROR("stop : %s randdata%x device pwr no konw err :pre_pwr_mode %x next_pwr_mode %x\n", __func__, randdata, pre_pwr_mode, next_pwr_mode);
						}
						ret = -1;
						break;
					}
				}
				if (8 == randdata)
				{
					if ((0x2 != g_sense_data.sense_key) || (0x1a != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						if (0xff == pre_pwr_mode)
						{
							LOG_ERROR("stop : %s randdata%x device pwr get err :pre_pwr_mode %x\n", __func__, randdata, pre_pwr_mode);
						}
						else
						{
							LOG_ERROR("stop : %s randdata%x device pwr no konw err :pre_pwr_mode %x next_pwr_mode %x\n", __func__, randdata, pre_pwr_mode, next_pwr_mode);
						}
						ret = -1;
						break;
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						if (0xff == pre_pwr_mode)
						{
							LOG_ERROR("stop : %s randdata%x device pwr get err :pre_pwr_mode %x\n", __func__, randdata, pre_pwr_mode);
						}
						break;
					}
					else if (0xd0 == lun)
					{
						if (0x4 == power_conditions)
						{
							LOG_ERROR("deepsleep power mode can't check now \n");
						}
						else if (0 == immed)
						{
							if ((power_conditions != next_pwr_mode) && 0x4 != power_conditions)
							{
								LOG_ERROR("power mode check fail power_conditions %d next_pwr_mode %d\n", power_conditions, next_pwr_mode);
								ret = -1;
								break;
							}
							else
							{
								LOG_INFO("power mode check pass \n");
							}
						}
						else
						{
							LOG_INFO("ssu pass \n");
						}
					}
					else
					{
						LOG_INFO("ssu pass \n");
					}
				}
			}
			else if (lun < 0x20)
			{
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int send_diagnostic(int fd, __u8 *send_diagnostic_rsp, __u8 self_test_code, __u8 pf, __u8 self_test,
					__u8 devoffl, __u8 unitoffl, __u16 para_list_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char send_diagnostic_cmd[SEND_DIAGNOSTIC_CMDLEN] = {
		SEND_DIAGNOSTIC_CMD, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi send_diagnostic_cmd cmd: wrong parameters\n");
		return -EINVAL;
	}
	send_diagnostic_cmd[1] = ((self_test_code & 0x7) << 5) | ((pf & 0x1) << 4) | ((self_test & 0x1) << 2) |
							 ((devoffl & 0x1) << 1) | (unitoffl & 0x1);
	put_unaligned_be16((uint16_t)para_list_len, (send_diagnostic_cmd + 3));
	for (int i = 0; i < SEND_DIAGNOSTIC_CMDLEN; i++)
	{
		LOG_INFO("send_diagnostic_cmd[%d]: %x\n", i, send_diagnostic_cmd[i]);
	}
	LOG_INFO("Start : %s self_test_code %d, self_test %d, pf %d devoffl %d, unitoffl %d, para_list_len %d\n",
			 __func__, self_test_code, self_test, pf, devoffl, unitoffl, para_list_len);
	ret = send_scsi_cmd_maxio(fd, send_diagnostic_cmd, send_diagnostic_rsp, SEND_DIAGNOSTIC_CMDLEN, byte_count,
							  SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO send_diagnostic_cmd data error ret %d\n", ret);
	}
	return ret;
}

void send_diagnostic_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 1, randdata = 0;
	__u32 byte_count = 0;
	__u8 self_test_code = 0, self_test = 0, devoffl = 0, unitoffl = 0, pf = 0;
	__u16 para_list_len = 0;
	__u8 send_diagnostic_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		self_test = (rand() % 0x2);
		devoffl = (rand() % 0x2);
		unitoffl = (rand() % 0x2);
		// pf = (rand() % 0x2);
		pf = (rand() % 0x1);
		para_list_len = 0;
		self_test_code = 0;
		ret = 1;
		if (1 == self_test)
		{
			self_test_code = 0;
		}
		else
		{
			do
			{
				self_test_code = (rand() % 0x7);
			} while (0x3 == self_test_code || 0x4 == self_test_code);
		}
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (9 == randdata)
		{
			para_list_len = (rand() % 0xf) + 1;
			LOG_INFO("%s rand9 para_list_len: %x\n", __func__, para_list_len);
		}
		// else if(8 == randdata)
		// {
		// 	self_test = 1;
		// 	self_test_code = (rand() % 0x6) + 1;
		// 	LOG_INFO("%s rand8 para_list_len: %x\n", __func__, self_test_code);
		// }
		else
		{
			;
		}
		error_status = send_diagnostic(fd, send_diagnostic_rsp, self_test_code, pf, self_test,
									   devoffl, unitoffl, para_list_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata9 sense_data err randdata %d\n", __func__, randdata);
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
					if (error_status < 0)
					{
						LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int sync_cache(int fd, __u8 *sync_cache_rsp, __u8 opcode, __u8 immed,
			   __u64 lba, __u32 nlb, __u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 sync_cache_cmd_len = 0;
	unsigned char sync_cache_cmd10[PRE_FETCH10_CMDLEN] = {
		SYNCHRONIZE_CACHE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char sync_cache_cmd16[PRE_FETCH16_CMDLEN] = {
		SYNCHRONIZE_CACHE16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;
	if (SYNCHRONIZE_CACHE10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (sync_cache_cmd10 + 2));
		put_unaligned_be16((uint16_t)nlb, (sync_cache_cmd10 + 7));
		sync_cache_cmd_len = PRE_FETCH10_CMDLEN;
		sync_cache_cmd10[1] = (immed & 0x1) << 1;
		cmd_addr = sync_cache_cmd10;
	}
	else if (SYNCHRONIZE_CACHE16_CMD == opcode)
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

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi synchronize_cache cmd: wrong parameters\n");
		return -EINVAL;
	}
	for (int i = 0; i < sync_cache_cmd_len; i++)
	{
		LOG_INFO("sync_cache_cmd%d[%d]: %x\n", sync_cache_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %d, immed %d, lba %llx, nlb %x\n",
			 __func__, opcode, immed, lba, nlb);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, sync_cache_rsp, sync_cache_cmd_len, byte_count,
							  SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO synchronize_cache data error ret %d\n", ret);
	}
	return ret;
}

void sync_cache_test(__u8 fd, __u8 lun, __u8 err_event)
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
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, lun);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		LOG_INFO("\n%s i %d\n", __func__, i);
		immed = (rand() % 0x2);
		opcode = (rand() % 0x2) + 1;
		ret = 1;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (1 == opcode)
		{
			opcode = SYNCHRONIZE_CACHE10_CMD;
		}
		else if (2 == opcode)
		{
			opcode = SYNCHRONIZE_CACHE16_CMD;
		}
		else
		{
			opcode = 0xff;
		}

		do
		{
			if (SYNCHRONIZE_CACHE10_CMD == opcode)
			{
				if (cap < 0xffffffff)
				{
					lba = rand() % cap;
				}
				else
				{
					lba = rand() % 0xffffffff;
				}
				if (cap < 0xffff)
				{
					nlb = rand() % cap;
				}
				else
				{
					nlb = rand() % 0xffff;
				}
			}
			else if (SYNCHRONIZE_CACHE16_CMD == opcode)
			{
				if (cap < 0xffffffffffffffff)
				{
					lba = rand() % cap;
				}
				else
				{
					lba = rand() % 0xffffffffffffffff;
				}
				if (cap < 0xffffffff)
				{
					nlb = rand() % cap;
				}
				else
				{
					nlb = rand() % 0xffffffff;
				}
			}
		} while ((lba + nlb) > cap);
		// randdata = 7;
		if (9 == randdata)
		{
			lba = cap + 1;
			nlb = 0;
			LOG_INFO("%s rand9 lba: %llx\n", __func__, lba);
		}
		else if (8 == randdata)
		{
			while ((lba + nlb) <= (cap + 1))
			{
				lba = cap - (rand() % 0x50);
				nlb = (rand() % 0x100);
			}
			LOG_INFO("%s rand8 lba: %llx nlb: %x\n", __func__, lba, nlb);
		}
		else if (7 == randdata)
		{
			if (SYNCHRONIZE_CACHE10_CMD == opcode)
			{
				if (cap <= 0xffff)
				{
					lba = 0;
					nlb = cap + 1;
				}
				else
				{
					lba = (__u32)cap;
					nlb = rand() % 0xffff + 2;
				}
			}
			else if (SYNCHRONIZE_CACHE16_CMD == opcode)
			{
				lba = (__u32)cap;
				nlb = rand() % 0xffff + 2;
			}
			else
			{
				lba = (__u32)cap;
				nlb = 2;
			}
			LOG_INFO("%s rand7 lba %llx nlb %x\n", __func__, lba, nlb);
		}
		else if (6 == randdata)
		{
			lba = 0;
			nlb = 0;
			LOG_INFO("%s rand6 lba %llx nlb: %x\n", __func__, lba, nlb);
		}
		else
		{
			;
		}
		error_status = sync_cache(fd, sync_cache_rsp, opcode, immed,
								  lba, nlb, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata || 8 == randdata || 7 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%x sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int inquiry(int fd, __u8 *inquiry_rsp, __u8 evpd, __u8 page_code,
			__u16 alloc_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char inquiry_cmd[INQUIRY_CMDLEN] = {
		INQUIRY_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi inquiry cmd: wrong parameters\n");
		return -EINVAL;
	}
	inquiry_cmd[1] = evpd;
	inquiry_cmd[2] = page_code;
	inquiry_cmd[3] = (alloc_len >> 8) & 0xff;
	inquiry_cmd[4] = (alloc_len)&0xff;

	for (int i = 0; i < INQUIRY_CMDLEN; i++)
	{
		LOG_INFO("inquiry_cmd[%d]: %x\n", i, inquiry_cmd[i]);
	}
	LOG_INFO("Start : %s evpd %d , page_code %d\n", __func__, evpd, page_code);
	ret = send_scsi_cmd_maxio(fd, inquiry_cmd, inquiry_rsp, INQUIRY_CMDLEN, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO inquiry data error ret %d\n", ret);
	}
	return ret;
}

void inquiry_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0;
	__u32 byte_count = 0;
	__u8 evpd = 0, page_code = 0;
	__u8 randdata = 0;
	__u16 alloc_len = 0;
	//__u8 inquiry_rsp[100] = {0};
	LOG_INFO("Start : %s err_event%d\n", __func__, err_event);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		evpd = (rand() % 0x2);
		ret = 1;
		// lba = (rand() % 0x100);
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (0 == evpd)
		{
			page_code = 0;
			alloc_len = 0x24;
			byte_count = 0x24;
		}
		else if (1 == evpd)
		{
			page_code = (rand() % 0x2);
			if (0 == page_code)
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
		// randdata = 9;
		if (9 == randdata)
		{
			page_code = (rand() % 0x20) + 1;
			LOG_INFO("%s rand9 page_code: %x\n", __func__, page_code);
		}
		else if (8 == randdata)
		{
			while ((alloc_len == 0x24) || (alloc_len == 0x6) || (alloc_len == 0x16))
			{
				alloc_len = (rand() % 0x30) + 1;
				byte_count = alloc_len;
				LOG_INFO("%s rand8 alloc_len: %x\n", __func__, alloc_len);
			}
		}
		else if (7 == randdata)
		{
			// device hardware reset to establish UAC
		}
		else
		{
			;
		}
		error_status = inquiry(fd, rd_buf, evpd, page_code,
							   alloc_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			if (9 == randdata)
			{
				if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				if (error_status < 0)
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		if (error_status >= 0)
		{
			LOG_INFO("\nsuccess:\n");
			for (int i = 0; i < byte_count; i++)
			{
				LOG_INFO("inquiry_rsp[%d]: %x\n", i, ((__u8 *)rd_buf)[i]);
			}
			LOG_INFO("\n");
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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
	unsigned char test_unit_ready_cmd[TEST_UNIT_READY_CMDLEN] = {
		TEST_UNIT_READY_CMD, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi test_unit_ready cmd: wrong parameters\n");
		return -EINVAL;
	}
	LOG_INFO("Start : %s \n", __func__);
	for (int i = 0; i < TEST_UNIT_READY_CMDLEN; i++)
	{
		LOG_INFO("test_unit_ready_cmd[%d]: %x\n", i, test_unit_ready_cmd[i]);
	}
	ret = send_scsi_cmd_maxio(fd, test_unit_ready_cmd, test_unit_ready_rsp,
							  TEST_UNIT_READY_CMDLEN, byte_count, SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO test_unit_ready data error ret %d\n", ret);
	}
	return ret;
}

void test_unit_ready_test(__u8 fd, __u8 lun)
{
	__u32 i = 0;
	int error_status = 0;
	__u32 byte_count = 0;
	__u8 test_unit_ready_rsp[1] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		error_status = test_unit_ready(fd, test_unit_ready_rsp,
									   byte_count, sg_type);
		if (error_status < 0)
		{
			LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
			break;
		}
	}
	if (error_status < 0)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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
	unsigned char unmap_cmd[UNMAP_CMDLEN] = {
		UNMAP_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi unmap cmd: wrong parameters\n");
		return -EINVAL;
	}
	unmap_cmd[1] = anchor & 0x1;
	put_unaligned_be16((uint16_t)para_list_len, unmap_cmd + 7);

	for (int i = 0; i < UNMAP_CMDLEN; i++)
	{
		LOG_INFO("unmap_cmd[%d]: %x\n", i, unmap_cmd[i]);
	}

	LOG_INFO("Start : %s para_list_len %x byte_count%x\n", __func__, para_list_len, byte_count);
	ret = send_scsi_cmd_maxio(fd, unmap_cmd, unmap_req,
							  UNMAP_CMDLEN, byte_count,
							  SG_DXFER_TO_DEV, sg_type);
	if (ret < 0)
	{
		print_error("SG_IO unmap data error ret %d\n", ret);
	}
	return ret;
}

void unmap_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0;
	__u32 byte_count = 0;
	__u64 cap = 0;
	__u16 para_list_len = 0;
	__u8 anchor = 0;
	__u8 unmap_list = 0, randdata = 0;
	// __u8 unmap_req[100] = {0};
	__u64 unmap_lba[10] = {0};
	__u32 unmap_nlb[10] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	LOG_INFO("Start : %s\n", __func__);
	cap = read_capacity_for_cap(fd, lun);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		ret = 1;
		unmap_list = (rand() % 10) + 1;
		para_list_len = 8 + 16 * unmap_list;
		anchor = 0;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		// randdata = 7;
		if (9 == randdata)
		{
			anchor = 1;
			LOG_INFO("%s rand9 anchor: %x\n", __func__, anchor);
		}
		// else if(8 == randdata)
		// {
		// 	para_list_len = 0;
		// 	LOG_INFO("%s rand8 para_list_len: 0\n", __func__);
		// }
		else if (7 == randdata)
		{
			para_list_len = 8 + ((rand() % 0x8) + 12) * unmap_list;
			LOG_INFO("%s rand7 para_list_len: %x\n", __func__, para_list_len);
		}
		else
		{
			para_list_len = 8 + 16 * unmap_list;
			LOG_INFO("%s rand%d\n", __func__, randdata);
		}
		// para_list_len = 16;
		if (para_list_len > 0)
		{
			byte_count = para_list_len;
		}
		else
		{
			byte_count = 8;
		}
		for (size_t j = 0; j < byte_count; j++)
		{
			MEM8_GET(wr_buf + j) = 0;
		}
		if (para_list_len > 7)
		{
			put_unaligned_be16((uint16_t)(para_list_len - 2), (__u8 *)wr_buf);
			put_unaligned_be16((uint16_t)(para_list_len - 8), (__u8 *)wr_buf + 2);
		}
		else if (para_list_len < 7 && para_list_len > 1)
		{
			put_unaligned_be16((uint16_t)(para_list_len - 2), (__u8 *)wr_buf);
		}
		else
		{
			;
		}
		for (j = 0; j < unmap_list; j++)
		{
			do
			{
				unmap_lba[j] = rand() % cap; // rand()% 0xf + 1;
				unmap_nlb[j] = rand() % cap; // rand()% 0xf + 1;
			} while ((unmap_lba[j] + unmap_nlb[j]) > cap);

			put_unaligned_be64((__u64)unmap_lba[j], (__u8 *)wr_buf + 8 + 16 * j);
			put_unaligned_be32((__u32)unmap_nlb[j], (__u8 *)wr_buf + 16 + 16 * j);
			if (5 == randdata)
			{
				put_unaligned_be64((cap), (__u8 *)wr_buf + 8 + 16 * j);
				put_unaligned_be32(((__u32)unmap_nlb[j] + 1), (__u8 *)wr_buf + 16 + 16 * j);
			}
			else if (4 == randdata)
			{
				put_unaligned_be64((cap + 1), (__u8 *)wr_buf + 8 + 16 * j);
			}
			else
			{
				;
			}
		}
		// for(int i = 0; i < byte_count; i++)
		// {
		// 	LOG_INFO("unmap_cmd_para[%d]: %x\n", i, ((__u8 *)wr_buf)[i]);
		// }
		for (j = 0; j < (byte_count / 8); j++)
		{
			LOG_INFO("unmap_cmd_para[%d]:", (j * 8));
			for (int i = 0; i < 8; i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 8 + i));
			}
			LOG_INFO("\n");
		}
		if ((byte_count % 8) > 0)
		{
			LOG_INFO("unmap_cmd_para[%d]:", (j * 8));
			for (int i = 0; i < (byte_count % 8); i++)
			{
				LOG_INFO("%x ", *((__u8 *)wr_buf + j * 8 + i));
				// j++;
			}
			LOG_INFO("\n");
		}
		LOG_INFO("\n");

		error_status = unmap(fd, wr_buf, anchor, para_list_len, byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				else if (5 == randdata || 4 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
				else if (7 == randdata)
				{
					if (para_list_len < 8)
					{
						if ((0x5 != g_sense_data.sense_key) || (0x26 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
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
						*/
						/* if((((para_list_len - 8) % 16) % 4) > 0)
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
						else */
						{
							if (0x0 != g_sense_data.sense_key && 0x0 != g_sense_data.asc && 0x0 != g_sense_data.ascq)
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
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
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
	unsigned char verify_cmd[VERIFY_CMDLEN] = {
		VERIFY_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi verify cmd: wrong parameters\n");
		return -EINVAL;
	}
	put_unaligned_be32((uint32_t)lba, verify_cmd + 2);
	put_unaligned_be16((uint16_t)verify_len, verify_cmd + 7);
	for (int i = 0; i < VERIFY_CMDLEN; i++)
	{
		LOG_INFO("verify_cmd[%d]: %x\n", i, verify_cmd[i]);
	}
	LOG_INFO("Start : %s lba %x, verify_len %x\n", __func__, lba, verify_len);
	ret = send_scsi_cmd_maxio(fd, verify_cmd, verify_rsp,
							  VERIFY_CMDLEN, byte_count, SG_DXFER_NONE, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO verify data error ret %d\n", ret);
	}
	return ret;
}

void verify_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, randdata = 0;
	__u32 byte_count = 0;
	__u32 lba = 0;
	__u16 verify_len = 0;
	__u8 verify_rsp[100] = {0};
	__u64 cap = 0;
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, lun);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		ret = 1;
		LOG_INFO("\n%s i %d\n", __func__, i);
		do
		{
			if (cap < 0xffffffff)
			{
				lba = rand() % cap;
			}
			else
			{
				lba = rand() % 0xffffffff;
			}
			if (cap < 0xffff)
			{
				verify_len = rand() % cap;
			}
			else
			{
				verify_len = rand() % 0xffff;
			}
		} while ((lba + verify_len) > cap);

		if (1 == err_event)
		{
			randdata = rand() % 0xa;
		}
		// randdata = 7;
		if (9 == randdata)
		{
			lba = (__u32)cap + rand() % 0x1f + 1;
			verify_len = 0;
			LOG_INFO("%s rand9 lba %x verify_len %x\n", __func__, lba, verify_len);
		}
		else if (8 == randdata)
		{
			lba = (__u32)cap;
			verify_len = 2;

			LOG_INFO("%s rand8 lba %x verify_len %x\n", __func__, lba, verify_len);
		}
		else if (7 == randdata)
		{
			if (cap <= 0xffff)
			{
				lba = 0;
				verify_len = cap + 1;
			}
			else
			{
				lba = (__u32)cap;
				verify_len = rand() % 0xffff + 2;
			}
			LOG_INFO("%s rand7 lba %x verify_len %x\n", __func__, lba, verify_len);
		}
		else if (6 == randdata)
		{
			lba = 0;
			verify_len = 0;

			LOG_INFO("%s rand6 lba %x verify_len %x\n", __func__, lba, verify_len);
		}
		else
		{
			;
		}
		error_status = verify(fd, verify_rsp, lba, verify_len,
							  byte_count, sg_type);
		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20)
			{
				if (9 == randdata || 8 == randdata || 7 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%x sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int hpb_read_buffer(int fd, __u8 *hpb_read_buffer_rsp, __u8 buf_id, __u16 hpb_region,
					__u16 hpb_subregion, __u32 alloc_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char hpb_read_buf_cmd[HPB_READ_BUF_CMDLEN] = {HPB_READ_BUF_CMD,
														   0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		print_error("scsi hpb_read_buffer cmd: wrong parameters\n");
		return -EINVAL;
	}

	hpb_read_buf_cmd[1] = buf_id;
	put_unaligned_be16((__u32)hpb_region, hpb_read_buf_cmd + 2);
	put_unaligned_be16((__u32)hpb_subregion, hpb_read_buf_cmd + 4);
	put_unaligned_be24((__u32)alloc_len, hpb_read_buf_cmd + 6);
	for (int i = 0; i < HPB_READ_BUF_CMDLEN; i++)
	{
		LOG_INFO("hpb_read_buf_cmd[%d]: %x\n", i, hpb_read_buf_cmd[i]);
	}
	LOG_INFO("Start : %s\n buf_id %d, hpb_region %d, hpb_subregion %d, alloc_len %d\n",
			 __func__, buf_id, hpb_region, hpb_subregion, alloc_len);
	ret = send_scsi_cmd_maxio(fd, hpb_read_buf_cmd, hpb_read_buffer_rsp,
							  HPB_READ_BUF_CMDLEN, byte_count, SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO hpb_read_buffer data error ret %d\n", ret);
	}
	return ret;
}

#if 1
void hpb_read_buffer_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0;
	__u8 buf_id = 0;
	__u16 hpb_region = 0, hpb_subregion = 0;
	__u32 alloc_len = 0;
	// __u32 buf_offset = 0;
	__u32 byte_count = 0;
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		ret = 1;
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		if (9 == randdata)
		{
			;
		}
		error_status = hpb_read_buffer(fd, rd_buf, buf_id, hpb_region,
									   hpb_subregion, alloc_len, byte_count, sg_type);

		if (error_status < 0)
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
		else
		{
			LOG_INFO("\nsuccess:\n");
			for (j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("hpb_read_buffer[%d]:", (j * 10));
				for (int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if ((byte_count % 10) > 0)
			{
				LOG_INFO("rd_buf[%d]:", (j * 10));
				for (int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
					// j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}
#endif

int hpb_write_buffer(int fd, __u8 *hpb_write_buffer_rsp, __u8 buf_id, __u16 hpb_region,
					 __u32 lba, __u8 hpb_read_id, __u32 para_list_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char hpb_write_buf_cmd[HPB_WRITE_BUF_CMDLEN] = {HPB_WRITE_BUF_CMD,
															 buf_id, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		print_error("scsi hpb_read_buffer cmd: wrong parameters\n");
		return -EINVAL;
	}
	if (0x01 == buf_id)
	{
		put_unaligned_be16((uint16_t)hpb_region, (hpb_write_buf_cmd + 2));
	}
	else if (0x02 == buf_id)
	{
		put_unaligned_be64((uint32_t)lba, (hpb_write_buf_cmd + 2));
		put_unaligned_be32((uint16_t)para_list_len, (hpb_write_buf_cmd + 7));
		hpb_write_buf_cmd[6] = hpb_read_id;
	}
	else if (0x03 == buf_id)
	{
		// none
	}
	else
	{
		perror("scsi hpb_write_buffer cmd: wrong parameters opcode\n");
		return -EINVAL;
	}
	for (int i = 0; i < HPB_WRITE_BUF_CMDLEN; i++)
	{
		LOG_INFO("hpb_write_buf_cmd[%d]: %x\n", i, hpb_write_buf_cmd[i]);
	}

	LOG_INFO("Start : %s\n buf_id %d, hpb_region %d, lba %d, hpb_read_id %d, para_list_len %d\n",
			 __func__, buf_id, hpb_region, lba, hpb_read_id, para_list_len);
	ret = send_scsi_cmd_maxio(fd, hpb_write_buf_cmd, hpb_write_buffer_rsp,
							  HPB_WRITE_BUF_CMDLEN, byte_count, SG_DXFER_TO_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO hpb_write_buffer data error ret %d\n", ret);
	}
	return ret;
}

int hpb_read(int fd, __u8 *hpb_read_rsp, __u8 dpo, __u8 fua, __u8 hpb_rd_id,
			 __u32 lba, __u64 hpb_entry, __u8 transfer_len, __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char hpb_read_cmd[HPB_READ_CMDLEN] = {
		HPB_READ_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	hpb_read_cmd[1] = ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
	put_unaligned_be32((uint64_t)lba, (hpb_read_cmd + 2));
	put_unaligned_be64((uint32_t)hpb_entry, (hpb_read_cmd + 6));

	hpb_read_cmd[14] = transfer_len;
	hpb_read_cmd[15] = hpb_rd_id;
	if (fd < 0 || byte_count < 0)
	{
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	for (int i = 0; i < HPB_READ_CMDLEN; i++)
	{
		LOG_INFO("hpb_read[%d]: %x\n", i, hpb_read_cmd[i]);
	}
	LOG_INFO("Start : %s dpo %d fua %d lba %x transfer_len %x hpb_rd_id %x hpb_entry %llx byte_count %x\n",
			 __func__, dpo, fua, lba, transfer_len, hpb_rd_id, hpb_entry, byte_count);
	ret = send_scsi_cmd_maxio(fd, hpb_read_cmd, hpb_read_rsp, HPB_READ_CMDLEN, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO read data error ret %d\n", ret);
	}
	return ret;
}

void hpb_read_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0;
	__u32 byte_count = 0;
	__u8 transfer_len = 0, dpo = 0, fua = 0, hpb_rd_id = 0;
	__u64 hpb_entry = 0;
	__u32 lba = 0;
	__u64 cap = 0;
	//__u8 read_rsp[0x100000] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, lun);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		LOG_INFO("\n%s i %d cap %llx\n", __func__, i, cap);
		j = 0;
		ret = 1;
		dpo = (rand() % 0x2);
		fua = (rand() % 0x2);
		hpb_rd_id = (rand() % 0xf);
		hpb_entry = (rand() % 0xf);
		if (1 == err_event)
		{
			randdata = (rand() % 0xf);
		}
		// opcode = 0;
		do
		{
			lba = (rand() % cap);
			transfer_len = (rand() % 0x1f);
		} while ((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > 0x1f);
		if (0 == transfer_len)
		{
			transfer_len = 1;
			byte_count = 1 << 12;
		}
		else
		{
			byte_count = transfer_len << 12;
		}
		// randdata = 5;
		if (9 == randdata)
		{
			lba = cap + 1;
			// transfer_len = (rand() % (cap / 4096 + cap % 4096));
			transfer_len = 0x1f; //(rand() % (0x1f));
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
			LOG_INFO("%s rand9 lba: %x transfer_len %x\n", __func__, lba, transfer_len);
		}
		else if (8 == randdata)
		{
			while ((lba + transfer_len) <= (cap + 1))
			{
				lba = (cap) - (rand() % 0x50);
				transfer_len = (rand() % 0x1f);
			}
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}

			LOG_INFO("%s rand8 lba: %x transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (7 == randdata)
		{
			lba = 0;
			transfer_len = cap + 2;
			byte_count = transfer_len << 12;
			LOG_INFO("%s rand7 lba: %x transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else
		{
			error_status = scsi_write(fd, wr_buf, WRITE16_CMD, 0, 0, 0,
									  lba, transfer_len, 0, byte_count, sg_type);
		}

		error_status = hpb_read(fd, rd_buf, dpo, fua, hpb_rd_id,
								lba, hpb_entry, transfer_len, byte_count, sg_type);

		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31 && lun != 0xb0)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20 || lun == 0xb0)
			{
				if (9 == randdata || 8 == randdata || 7 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
					error_status = memcmp(wr_buf, rd_buf, byte_count);
					if (0 != error_status)
					{
						LOG_INFO("\n memcmp fail:\n");
						LOG_ERROR("memcmp : %s err %d\n", __func__, error_status);
						ret = -1;
						LOG_INFO("\n write16:\n");
						for (j = 0; j < (byte_count / 10); j++)
						{
							// LOG_INFO("write%x[%d]:", len, (j * 10));
							for (int i = 0; i < 10; i++)
							{
								LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
								// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
							}
							LOG_INFO("\n");
						}
						if ((byte_count % 10) > 0)
						{
							// LOG_INFO("write%x[%d]:", len, (j * 10));
							for (int i = 0; i < (byte_count % 10); i++)
							{
								LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
								// j++;
							}
							LOG_INFO("j%d\n", j);
						}
						LOG_INFO("\n");

						LOG_INFO("\n read:\n");
						for (j = 0; j < (byte_count / 10); j++)
						{
							// LOG_INFO("read%d[%d]:", len, (j * 10));
							for (int i = 0; i < 10; i++)
							{
								LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
							}
							LOG_INFO("\n");
						}
						if ((byte_count % 10) > 0)
						{
							// LOG_INFO("read%d[%d]:", len, (j * 10));
							for (int i = 0; i < (byte_count % 10); i++)
							{
								LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
								// j++;
							}
							LOG_INFO("j%d\n", j);
						}
						LOG_INFO("\n");
						break;
					}
					else
					{
						LOG_INFO("\n memcmp pass:\n");
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
// if(-1 != ret)
#ifdef RD_WR_PRINTF
		{
			LOG_INFO("\nsuccess:\n");
			for (j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for (int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if ((byte_count % 10) > 0)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for (int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
					// j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
#endif
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_write_buffer(int fd, __u8 *buf, __u8 mode, __u8 buf_id, __u32 buf_offset,
					  __u32 byte_count, __u8 sg_type)
{
	int ret;
	unsigned char write_buf_cmd[WRITE_BUF_CMDLEN] = {
		WRITE_BUFFER_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi write cmd: wrong parameters\n");
		return -EINVAL;
	}

	write_buf_cmd[1] = mode;
	write_buf_cmd[2] = buf_id;
	put_unaligned_be24((uint32_t)buf_offset, write_buf_cmd + 3);
	put_unaligned_be24(byte_count, write_buf_cmd + 6);

	for (int i = 0; i < WRITE_BUF_CMDLEN; i++)
	{
		LOG_INFO("write_buf_cmd[%d]: %x\n", i, write_buf_cmd[i]);
	}

	LOG_INFO("Start : %s mode %d , buf_id %d\n", __func__, mode, buf_id);
	ret = send_scsi_cmd_maxio(fd, write_buf_cmd, buf,
							  WRITE_BUF_CMDLEN, byte_count,
							  SG_DXFER_TO_DEV, sg_type);
	if (ret < 0)
	{
		print_error("SG_IO WRITE BUFFER data error ret %d\n", ret);
	}
	return ret;
}

void write_buffer_test(__u8 fd, __u8 lun)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0;
	__u8 mode = 0xff, buf_id = 0;
	__u32 buf_offset = 0;
	__u32 byte_count = 0;
	// __u8 write_buffer_rsp[100] = {0};
	LOG_INFO("Start : %s\n", __func__);
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d\n", __func__, i);
		j = 0;
		ret = 1;
		if (0xff == mode)
		{
			mode = (rand() % 0x2); // expect for vendor mode
		}
		mode = 1;
		if (0 == mode)
		{
			mode = BUFFER_FFU_MODE; // download microcode mode
			byte_count = (rand() % 0x50) + 1;
			buf_offset = (rand() % 0x5);
			buf_id = 0;
		}
		else if (1 == mode)
		{
			mode = BUFFER_DATA_MODE; // data
			buf_offset = (rand() % 0x5);
			buf_id = (rand() % 0x2);
			byte_count = 4; //(rand() % 0x50) + 1;
							// byte_count = 0xef000;
		}
		else if (2 == mode)
		{
			mode = BUFFER_VENDOR_MODE; // vendor specific
			LOG_ERROR("stop %s: expect for vendor mode\n", __func__);
			// buf_id = ;
			// buf_offset = ;
			// byte_count = ;
		}
		else
		{
			;
		}
		if (1)
		{
			LOG_INFO("\n");
			for (j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("wr_buf[%d]:", (j * 10));
				for (int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if ((byte_count % 10) > 0)
			{
				LOG_INFO("wr_buf[%d]:", (j * 10));
				for (int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
					// j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}

		error_status = scsi_write_buffer(fd, wr_buf, mode, buf_id,
										 buf_offset, byte_count, sg_type);
		if (error_status < 0 && fd > 0)
		{
			if (lun > 31)
			{
				if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
					((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
				{
					LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
			else
			{
				LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
				ret = -1;
				break;
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_read(int fd, __u8 *read_rsp, __u8 opcode, __u8 dpo, __u8 fua, __u8 grp_num,
			  __u64 lba, __u32 transfer_len, __u8 rd_protect, __u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 read_cmd_len = 0;
	unsigned char read6[READ6_LEN] = {
		READ6_CMD, 0, 0, 0, 0, 0};
	unsigned char read10[READ10_LEN] = {
		READ10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char read16[READ16_LEN] = {
		READ16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;
	if (READ6_CMD == opcode)
	{
		// put_unaligned_be32((uint32_t)lba, (read6 + 1));
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
	else if (READ10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (read10 + 2));
		put_unaligned_be16((uint16_t)transfer_len, (read10 + 7));
		read10[1] = ((rd_protect & 0x7) << 5) | ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		read10[6] = (grp_num & 0x1f);
		read_cmd_len = READ10_LEN;
		cmd_addr = read10;
		// for(int i = 0; i< READ10_LEN; i++)
		// {
		// 	LOG_INFO("read10[%d]: %x\n", i, read10[i]);
		// }
	}
	else if (READ16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (read16 + 2));
		put_unaligned_be32((uint32_t)transfer_len, (read16 + 10));
		read16[1] = ((rd_protect & 0x7) << 5) | ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
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

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	for (int i = 0; i < read_cmd_len; i++)
	{
		LOG_INFO("read%d[%d]: %x\n", read_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %x dpo %d fua %d, lba %llx transfer_len %x grp_num %x, byte_count %x\n",
			 __func__, opcode, dpo, fua, lba, transfer_len, grp_num, byte_count);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, read_rsp, read_cmd_len, byte_count,
							  SG_DXFER_FROM_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO read data error ret %d\n", ret);
	}
	return ret;
}

void scsi_read_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0;
	__u32 byte_count = 0;
	__u8 opcode = 0, dpo = 0, fua = 0, grp_num = 0, rd_protect = 0;
	__u8 len = 0;
	__u64 lba = 0;
	__u32 transfer_len = 0;
	__u64 cap = 0;
	//__u8 read_rsp[0x100000] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, 0);
	LOG_INFO("lun %x\n", lun);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		if (cap <= 0)
		{
			cap = 6;
		}
		LOG_INFO("\n%s i %d cap %llx\n", __func__, i, cap);
		j = 0;
		ret = 1;
		dpo = (rand() % 0x2);
		fua = (rand() % 0x2);
		opcode = (rand() % 0x3);
		rd_protect = 0;
		if (1 == err_event)
		{
			randdata = (rand() % 0xc);
		}
		opcode = 0;
		if (0 == opcode)
		{
			opcode = READ6_CMD;
			len = READ6_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % cap);
				// transfer_len = 20;
			}
			// while(((lba + 1) * transfer_len) > cap || lba > 0x1fffff || transfer_len > 0xff);
			while ((lba + transfer_len) > cap || lba > 0x1fffff || transfer_len > cap);
			if (0 == transfer_len)
			{
				transfer_len = 1;
				// byte_count = 256 << 12;
			}
			grp_num = 0;
			byte_count = transfer_len << 12;
		}
		else if (1 == opcode)
		{
			opcode = READ10_CMD;
			len = READ10_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % cap);
				// transfer_len = 20;
			}
			// while((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > 0xffff);
			while ((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > cap);
			grp_num = (rand() % 0xa);
			if (0 == transfer_len)
			{
				transfer_len = 1;
			}
			byte_count = transfer_len << 12;
		}
		else if (2 == opcode)
		{
			opcode = READ16_CMD;
			len = READ16_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % cap);
				// transfer_len = 20;
			}
			// while((lba + transfer_len) > cap || lba > 0xffffffffffffffff || transfer_len > (0xffffffff >> 12));
			while ((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > cap);
			grp_num = (rand() % 0xa);
			if (0 == transfer_len)
			{
				transfer_len = 1;
			}
			byte_count = transfer_len << 12;
		}
		else
		{
			;
		}
		// randdata = 9;
		if (9 == randdata)
		{
			lba = cap + 1;
			// transfer_len = (rand() % (cap / 4096 + cap % 4096));
			transfer_len = cap; //(rand() % (0x1f));
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
#if 1
			if (cap > (0xffffffffUL + 0xffffUL))
			{
				opcode = READ16_CMD;
				len = READ16_LEN;
			}
			else if (cap > (0x1fffff + 0xff))
#else
			if (cap > (0x1fffff + 0xff))
#endif
			{
				opcode = rand() % 0x2;
				if (0 == opcode)
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
		else if (8 == randdata)
		{
			while ((lba + transfer_len) <= (cap + 1))
			{
				lba = (cap) - (rand() % 0x5);
				transfer_len = (rand() % cap);
			}
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}

#if 1
			if (cap > (0xffffffffUL + 0xffffUL))
			{
				opcode = READ16_CMD;
				len = READ16_LEN;
			}
			else if (cap > (0x1fffff + 0xff))
#else
			if (cap > (0x1fffff + 0xff))
#endif
			{
				opcode = (rand() % (0x2));
				if (0 == opcode)
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
			LOG_INFO("%s rand8 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (7 == randdata)
		{
			lba = 0;
			transfer_len = cap + 2;
			byte_count = transfer_len << 12;

			if ((cap + 2) > (0xffff))
			{
				opcode = READ16_CMD;
				len = READ16_LEN;
			}
			else if ((cap + 2) > (0xff))
			{
				opcode = (rand() % (0x2));
				if (0 == opcode)
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
			LOG_INFO("%s rand7 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (100 == randdata)
		{
			lba = (rand() % cap) + 0x100000000;
			transfer_len = (rand() % cap) + 1;
			byte_count = transfer_len << 12;
			opcode = READ16_CMD;
			len = READ16_LEN;
			LOG_INFO("%s rand5 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (4 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			grp_num = (rand() % 0xf) + 0x10;
			LOG_INFO("%s rand4 grp_num: %x \n", __func__, grp_num);
		}
		else if (3 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			rd_protect = (rand() % 0x7) + 1;
			LOG_INFO("%s rand3 rd_protect: %x \n", __func__, rd_protect);
		}
		else if (10 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			grp_num = 0xf;
			LOG_INFO("%s rand10 grp_num: %x \n", __func__, grp_num);
		}
		else if (11 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			for (size_t i = 0; i < (byte_count); i++)
			{
				MEM32_GET((__u32 *)wr_buf + i) = 0xabcd1234;
			}
			grp_num = 0xe;
			LOG_INFO("%s rand11 grp_num: %x \n", __func__, grp_num);
		}
		else if (12 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			grp_num = 0xd;
			LOG_INFO("%s rand12 grp_num: %x \n", __func__, grp_num);
			if (READ10_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE10_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else if (READ16_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE16_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else
			{
				;
			}
		}
		else if (13 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			grp_num = 0xc;
			LOG_INFO("%s rand10 grp_num: %x \n", __func__, grp_num);
			if (READ10_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE10_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else if (READ16_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE16_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else
			{
				;
			}
		}
		// else if(6 == randdata)
		// {
		// 	;
		// }
		else
		{
			// if(6 == randdata)
			{
				for (size_t i = 0; i < (byte_count); i++)
				{
					MEM32_GET((__u8 *)wr_buf + i) = BYTE_RAND();
				}
			}
			if (READ6_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE6_CMD, 0, 0, 0,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else if (READ10_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE10_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else if (READ16_CMD == opcode)
			{
				error_status = scsi_write(fd, wr_buf, WRITE16_CMD, dpo, fua, grp_num,
										  lba, transfer_len, 0, byte_count, sg_type);
			}
			else
			{
				;
			}
		}

		for (size_t i = 0; i < (byte_count); i++)
		{
			MEM32_GET((__u8 *)rd_buf + i) = 0;
		}
		error_status = scsi_read(fd, rd_buf, opcode, dpo, fua, grp_num,
								 lba, transfer_len, rd_protect, byte_count, sg_type);

		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31 && lun != 0xb0)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 31 || lun == 0xb0)
			{
				if (9 == randdata || 8 == randdata || 7 == randdata) // || 5 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (4 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if ((0x5 != g_sense_data.sense_key) || (0x0 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (3 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (10 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (11 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					error_status = memcmp(wr_buf, rd_buf, byte_count);
					if (0 != error_status)
					{
						LOG_ERROR("stop : %s randdata%x memcmp err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d memcmp pass\n", __func__, randdata);
					}
				}
				else if (((12 == randdata) || (13 == randdata)) && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if ((0xe != g_sense_data.sense_key) || (0x0 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_ERROR("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
					error_status = memcmp(wr_buf, rd_buf, byte_count);
					if (0 != error_status)
					{
						LOG_INFO("\n memcmp fail:\n");
						LOG_ERROR("memcmp : %s err %d\n", __func__, error_status);
						ret = -1;
						LOG_INFO("\n write%d:\n", len);
						for (j = 0; j < (byte_count / 10); j++)
						{
							// LOG_INFO("write%x[%d]:", len, (j * 10));
							for (int i = 0; i < 10; i++)
							{
								LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
								// LOG_INFO("%X",MEM8_GET(wr_buf + j * 10 + i));
							}
							LOG_INFO("\n");
						}
						if ((byte_count % 10) > 0)
						{
							// LOG_INFO("write%x[%d]:", len, (j * 10));
							for (int i = 0; i < (byte_count % 10); i++)
							{
								LOG_INFO("%x ", *((__u8 *)wr_buf + j * 10 + i));
								// j++;
							}
							LOG_INFO("\nj%d\n", j);
						}
						LOG_INFO("\n");

						LOG_INFO("\n read:\n");
						for (j = 0; j < (byte_count / 10); j++)
						{
							// LOG_INFO("read%d[%d]:", len, (j * 10));
							for (int i = 0; i < 10; i++)
							{
								LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
							}
							LOG_INFO("\n");
						}
						if ((byte_count % 10) > 0)
						{
							// LOG_INFO("read%d[%d]:", len, (j * 10));
							for (int i = 0; i < (byte_count % 10); i++)
							{
								LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
								// j++;
							}
							LOG_INFO("\nj%d\n", j);
						}
						LOG_INFO("\n");
						break;
					}
					else
					{
						LOG_INFO("\n memcmp pass:\n");
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
// if(-1 != ret)
#ifdef RD_WR_PRINTF
		{
			LOG_INFO("\nsuccess:\n");
			for (j = 0; j < (byte_count / 10); j++)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for (int i = 0; i < 10; i++)
				{
					LOG_INFO("%x ", ((__u8 *)rd_buf)[(j * 10 + i)]);
				}
				LOG_INFO("\n");
			}
			if ((byte_count % 10) > 0)
			{
				LOG_INFO("read%d[%d]:", len, (j * 10));
				for (int i = 0; i < (byte_count % 10); i++)
				{
					LOG_INFO("%x ", *((__u8 *)rd_buf + j * 10 + i));
					// j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
#endif
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

int scsi_write(int fd, __u8 *write_rsp, __u8 opcode, __u8 dpo, __u8 fua, __u8 grp_num,
			   __u64 lba, __u32 transfer_len, __u8 wr_protect, __u32 byte_count, __u8 sg_type)
{
	int ret;
	__u8 write_cmd_len = 0;
	unsigned char write6[WRITE6_LEN] = {
		WRITE6_CMD, 0, 0, 0, 0, 0};
	unsigned char write10[WRITE10_LEN] = {
		WRITE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char write16[WRITE16_LEN] = {
		WRITE16_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char *cmd_addr;
	if (WRITE6_CMD == opcode)
	{
		// put_unaligned_be32((uint32_t)lba, (write6 + 1));
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
	else if (WRITE10_CMD == opcode)
	{
		put_unaligned_be32((uint32_t)lba, (write10 + 2));
		put_unaligned_be16((uint16_t)transfer_len, (write10 + 7));
		write10[1] = ((wr_protect & 0x7) << 5) | ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
		write10[6] = (grp_num & 0x1f);
		write_cmd_len = WRITE10_LEN;
		cmd_addr = write10;
		// for(int i = 0; i< write10_LEN; i++)
		// {
		// 	LOG_INFO("write10[%d]: %x\n", i, write10[i]);
		// }
	}
	else if (WRITE16_CMD == opcode)
	{
		put_unaligned_be64((uint64_t)lba, (write16 + 2));
		put_unaligned_be32((uint32_t)transfer_len, (write16 + 10));
		write16[1] = ((wr_protect & 0x7) << 5) | ((dpo & 0x1) << 4) | ((fua & 0x1) << 3);
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

	if (fd < 0 || byte_count < 0)
	{
		perror("scsi prefetch cmd: wrong parameters\n");
		return -EINVAL;
	}
	for (int i = 0; i < write_cmd_len; i++)
	{
		LOG_INFO("write%d[%d]: %x\n", write_cmd_len, i, ((__u8 *)cmd_addr)[i]);
	}
	LOG_INFO("Start : %s opcode %x dpo %d fua %d, lba %llx transfer_len %x wr_protect %x grp_num %x, byte_count %x\n",
			 __func__, opcode, dpo, fua, lba, transfer_len, wr_protect, grp_num, byte_count);
	ret = send_scsi_cmd_maxio(fd, cmd_addr, write_rsp, write_cmd_len, byte_count,
							  SG_DXFER_TO_DEV, sg_type);

	if (ret < 0)
	{
		print_error("SG_IO write data error ret %d\n", ret);
	}
	return ret;
}

void scsi_write_test(__u8 fd, __u8 lun, __u8 err_event)
{
	__u32 i = 0;
	int error_status = 0, ret = 0, j = 0, randdata = 0, wr_protect = 0;
	__u32 byte_count = 0;
	__u8 opcode = 0, dpo = 0, fua = 0, grp_num = 0;
	__u8 len = 0;
	__u64 lba = 0;
	__u32 transfer_len = 0;
	__u64 cap = 0;
	//__u8 write_rsp[0x100000] = {0};
	__u8 sg_type = 0;
	if (lun > 31)
	{
		sg_type = SG3_TYPE;
	}
	else
	{
		sg_type = SG4_TYPE;
	}
	cap = read_capacity_for_cap(fd, 0);
	LOG_INFO("Start : %s\n", __func__);
	for (i = 0; i < TEST_CASE_NUM; i++)
	{
		LOG_INFO("\n%s i %d cap %llx\n", __func__, i, cap);
		if (cap <= 0)
		{
			cap = 6;
		}
		len = len;
		len = 0;
		j = 0;
		ret = 1;
		dpo = (rand() % 0x2);
		fua = (rand() % 0x2);
		wr_protect = 0;
		opcode = (rand() % 0x3);
		if (1 == err_event)
		{
			randdata = (rand() % 0x10);
		}
		//  opcode = 0;
		if (0 == opcode)
		{
			opcode = WRITE6_CMD;
			len = WRITE6_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
				// transfer_len = 20;
			}
			// while((lba + transfer_len) > cap || lba > 0x1fffff || transfer_len > 0xff);
			while ((lba + transfer_len) > cap || lba > 0x1fffff || transfer_len > 0x2f);
			if (0 == transfer_len)
			{
				transfer_len = 1;
				// byte_count = 256 << 12;
			}
			byte_count = transfer_len << 12;
		}
		else if (1 == opcode)
		{
			opcode = WRITE10_CMD;
			len = WRITE10_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
				// transfer_len = 20;
			}
			// while((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > 0xffff);
			while ((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > 0x2f);
			grp_num = (rand() % 0xa);
			if (0 == transfer_len)
			{
				transfer_len = 1;
				// byte_count = 1 << 12;
			}
			byte_count = transfer_len << 12;
		}
		else if (2 == opcode)
		{
			opcode = WRITE16_CMD;
			len = WRITE16_LEN;
			do
			{
				lba = (rand() % cap);
				transfer_len = (rand() % 0x1f);
				// transfer_len = 20;
			}
			// while((lba + transfer_len) > cap || lba > 0xffffffffffffffff || transfer_len > (0xffffffff >> 12));
			while ((lba + transfer_len) > cap || lba > 0xffffffff || transfer_len > 0x2f);
			grp_num = (rand() % 0xa);
			if (0 == transfer_len)
			{
				transfer_len = 1;
			}
			byte_count = transfer_len << 12;
		}
		else
		{
			;
		}
		// randdata = 6;
		if (9 == randdata)
		{
			lba = cap + 1;
			// transfer_len = (rand() % (cap / 4096 + cap % 4096));
			transfer_len = 0x1f; //(rand() % (0x1f));
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}
			if (cap > (0xffffffff + 0xffff))
			{
				opcode = WRITE16_CMD;
				len = WRITE16_LEN;
			}
			else if (cap > (0x1fffff + 0xff))
			{
				opcode = rand() % 0x2;
				if (0 == opcode)
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
		else if (8 == randdata)
		{
			while ((lba + transfer_len) <= (cap + 1))
			{
				lba = (cap) - (rand() % 0x5);
				transfer_len = (rand() % 0x1f);
			}
			if (0 == transfer_len)
			{
				transfer_len = 1;
				byte_count = transfer_len << 12;
			}
			else
			{
				byte_count = transfer_len << 12;
			}

			if (cap > (0xffffffff + 0xffff))
			{
				opcode = WRITE16_CMD;
				len = WRITE16_LEN;
			}
			else if (cap > (0x1fffff + 0xff))
			{
				opcode = (rand() % (0x2));
				if (0 == opcode)
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
		else if (7 == randdata)
		{
			lba = 0;
			transfer_len = cap + 2;
			byte_count = transfer_len << 12;

			if ((cap + 2) > (0xffff))
			{
				opcode = WRITE16_CMD;
				len = WRITE16_LEN;
			}
			else if ((cap + 2) > (0xff))
			{
				opcode = (rand() % (0x2));
				if (0 == opcode)
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
			LOG_INFO("%s rand7 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (100 == randdata)
		{
			lba = (rand() % cap) + 0x100000000;
			transfer_len = (rand() % cap) + 1;
			byte_count = transfer_len << 12;
			opcode = READ16_CMD;
			len = READ16_LEN;
			LOG_INFO("%s rand5 lba: %llx transfer_len: %x\n", __func__, lba, transfer_len);
		}
		else if (4 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			grp_num = (rand() % 0xf) + 0x10;
			LOG_INFO("%s rand4 grp_num: %x \n", __func__, grp_num);
		}
		else if (3 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
		{
			wr_protect = (rand() % 0x7) + 1;
			LOG_INFO("%s rand3 rd_protect: %x \n", __func__, wr_protect);
		}
		else if (6 == randdata)
		{
			;
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
		 */
// #ifdef RD_WR_PRINTF
#if 0
		{
			LOG_INFO("\nsuccess:\n");
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
					//j++;
				}
				LOG_INFO("\n");
			}
			LOG_INFO("\n");
		}
#else
		(void)j;
#endif

		error_status = scsi_write(fd, wr_buf, opcode, dpo, fua, grp_num,
								  lba, transfer_len, wr_protect, byte_count, sg_type);

		if (0 == err_event)
		{
			if (error_status < 0)
			{
				if (lun > 31)
				{
					if (((0x5 != g_sense_data.sense_key) || (0x20 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)) &&
						((0x6 != g_sense_data.sense_key) || (0x29 != g_sense_data.asc) || (0x0 != g_sense_data.ascq)))
					{
						LOG_ERROR("stop : %s lun%d sense_data err\n", __func__, lun);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
					}
				}
				else
				{
					LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
					ret = -1;
					break;
				}
			}
		}
		else
		{
			if (lun < 0x20 || lun == 0xb0)
			{
				if (9 == randdata || 8 == randdata || 7 == randdata)
				{
					if ((0x5 != g_sense_data.sense_key) || (0x21 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_INFO("stop : %s randdata%x sense_data err \n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%x sense_data pass\n", __func__, randdata);
					}
				}
				else if (4 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if ((0x5 != g_sense_data.sense_key) || (0x0 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (3 == randdata && (opcode == READ16_CMD || opcode == READ10_CMD))
				{
					if ((0x5 != g_sense_data.sense_key) || (0x24 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else if (100 == randdata)
				{
					if ((0x7 != g_sense_data.sense_key) || (0x0 != g_sense_data.asc) || (0x0 != g_sense_data.ascq))
					{
						LOG_ERROR("stop : %s randdata%x wp sense_data err\n", __func__, randdata);
						ret = -1;
						break;
					}
					else
					{
						LOG_INFO("stop : %s randdata%d sense_data pass\n", __func__, randdata);
					}
				}
				else
				{
					if (error_status < 0)
					{
						LOG_INFO("stop : %s error_status %d\n", __func__, error_status);
						ret = -1;
						break;
					}
				}
			}
			else
			{
				if (0x5 != g_sense_data.sense_key)
				{
					LOG_INFO("stop : %s lun%d sense_data err\n", __func__, lun);
					ret = -1;
					break;
				}
				else
				{
					LOG_INFO("stop : %s lun%d sense_data pass\n", __func__, lun);
				}
			}
		}
	}
	if (-1 == ret)
	{
		LOG_ERROR("stop : %s result is -1\n", __func__);
	}
	else
	{
		LOG_INFO("stop : %s result is 1\n", __func__);
	}
}

__u8 lun_rand(struct tool_options *opt, __u8 max_lun)
{
	__u8 lun_num = 0;

	memset(opt, 0, sizeof(struct tool_options));
	lun_num = rand() % max_lun;
	if (8 == lun_num)
	{
		// lun_num = 9;
	}
	lun_num = 8;
	switch (lun_num)
	{
	case 0:
		strcpy(opt->path, "/dev/bsg/0:0:0:0");
		break;
	case 1:
		strcpy(opt->path, "/dev/bsg/0:0:0:1");
		break;
	case 2:
		strcpy(opt->path, "/dev/bsg/0:0:0:2");
		break;
	case 3:
		strcpy(opt->path, "/dev/bsg/0:0:0:3");
		break;
	case 4:
		strcpy(opt->path, "/dev/bsg/0:0:0:4");
		break;
	case 5:
		strcpy(opt->path, "/dev/bsg/0:0:0:5");
		break;
	case 6:
		strcpy(opt->path, "/dev/bsg/0:0:0:6");
		break;
	case 7:
		strcpy(opt->path, "/dev/bsg/0:0:0:7");
		break;
	case 8:
		lun_num = 0xb0;
		strcpy(opt->path, "/dev/sg2"); // boot
		break;
	case 9:
		lun_num = 0xc4;
		strcpy(opt->path, "/dev/sg1"); // rpmb
		break;
	case 10:
		lun_num = 0xd0;
		strcpy(opt->path, "/dev/sg0"); // device
		break;
	default:
		print_error("lun err: %x", lun_num);
		break;
	}
	LOG_INFO("lun_num %x\n", lun_num);
	return lun_num;
}
void cmd_exe(__u8 cmd, __u8 lun, __u8 fd)
{
	switch (cmd)
	{
	case 0:
		inquiry_test(fd, lun, 1);
		break;
	case 1:
		request_sense_test(fd, lun, 1);
		break;
	case 2:
		mode_select_test(fd, lun, 1);
		break;
	case 3:
		mode_sense_test(fd, lun, 1);
		break;
	case 4:
		unmap_test(fd, lun, 1);
		break;
	case 5:
		read_capacity_test(fd, lun, 1);
		break;
	case 6:
		format_unit_test(fd, lun);
		break;
	case 7:
		test_unit_ready_test(fd, lun);
		break;
	case 8:
		verify_test(fd, lun, 1);
		break;
	case 9:
		send_diagnostic_test(fd, lun, 1);
		break;
	case 10:
		report_lun_test(fd, lun, 1);
		break;
	case 11:
		sync_cache_test(fd, lun, 1);
		break;
	case 12:
		prefetch_test(fd, lun, 1);
		break;
	case 13:
		scsi_read_test(fd, lun, 1);
		break;
	case 14:
		scsi_write_test(fd, lun, 1);
		break;
	case 15:
		hpb_read_test(fd, lun, 0);
		break;
	case 16:
		read_buffer_test(fd, lun);
		break;
	case 17:
		write_buffer_test(fd, lun);
		break;
	default:
		// if (test_case < 256)
		LOG_ERROR("Error case number! please try again:\n");
		break;
	}
}
void test_send_scsi(struct tool_options *opt)
{
	int fd, test_case, lun;
	int oflag = O_RDWR;
	int i = 1, cmd = 0;
	// while(i < 1001)
	{
		LOG_INFO("Start :  loop %d\n", i++);
#if 1
		LOG_COLOR(SKBLU_LOG, "scsi_cmd_test>");
		fflush(stdout);
		scanf("%d", &test_case);
		printf("%d\n", test_case);
		switch (test_case)
		{
		case 0:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			inquiry_test(fd, 0, 1);
			break;
		case 1:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			request_sense_test(fd, 0, 1);
			break;
		case 2:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			mode_select_test(fd, 0, 1);
			break;
		case 3:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			mode_sense_test(fd, 0, 1);
			break;
		case 4:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			unmap_test(fd, 0, 1);
			break;
		case 5:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			read_capacity_test(fd, 0, 1);
			break;
		case 6:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			format_unit_test(fd, 0);
			break;
		case 7:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			test_unit_ready_test(fd, 0);
			break;
		case 8:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			verify_test(fd, 0, 1);
			break;
		case 9:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			send_diagnostic_test(fd, 0, 1);
			break;
		case 10:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			report_lun_test(fd, 0, 1);
			break;
		case 11:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			sync_cache_test(fd, 0, 1);
			break;
		case 12:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			prefetch_test(fd, 0, 0);
			break;
		case 13:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:6");
			// strcpy(opt->path,"/dev/sg1");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			scsi_read_test(fd, 6, 1);
			break;
		case 14:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/sg0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			scsi_write_test(fd, 0xb0, 1);
			break;
		case 15:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			read_buffer_test(fd, 0);
			break;
		case 16:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			write_buffer_test(fd, 0);
			break;
		case 17:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/sg0");
			// strcpy(opt->path,"/dev/bsg/0:0:0:49488");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			statr_stop_unit_test(fd, 0, 0);
			break;
		case 18:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			hpb_read_buffer(fd, rd_buf, 1, 0, 0, 8000, 8000, SG4_TYPE);
			break;
		case 19:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			hpb_write_buffer(fd, wr_buf, 1, 0, 1, 0, 4, 0, SG4_TYPE);
			break;
		case 20:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			hpb_read_test(fd, 0, 1);
			break;
		case 100:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:1");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			while (i)
			{
				LOG_INFO("Start :  loop %d\n", i++);
				inquiry_test(fd, 0, 1);
				request_sense_test(fd, 0, 1);
				mode_select_test(fd, 0, 1);
				mode_sense_test(fd, 0, 1);
				unmap_test(fd, 0, 1);
				read_capacity_test(fd, 0, 1);
				format_unit_test(fd, 0);
				test_unit_ready_test(fd, 0);
				verify_test(fd, 0, 1);
				send_diagnostic_test(fd, 0, 1);
				report_lun_test(fd, 0, 1);
				sync_cache_test(fd, 0, 1);
				prefetch_test(fd, 0, 1);

				scsi_read_test(fd, 0, 1);
				scsi_write_test(fd, 0, 1);
				hpb_read_test(fd, 0, 0);
				read_buffer_test(fd, 0);
				write_buffer_test(fd, 0);
			}
			// hpb_read_buffer(fd, rd_buf, 1, 20, 20, 8000, 8000, SG4_TYPE);
			break;
		case 101:
			// while(1)
			{
				lun = lun_rand(opt, 11);
				fd = open(opt->path, oflag);
				LOG_INFO("Start : %s fd %d\n", __func__, fd);
				cmd = (rand() % 18);
				if (fd < 0)
				{
					print_error("open");
				}
				cmd = 13;
				cmd_exe(cmd, lun, fd);
				close(fd);
			}
			break;
		case 21:
			// while(1)
			{
				lun = lun_rand(opt, 8);
				fd = open(opt->path, oflag);
				LOG_INFO("Start : %s fd %d\n", __func__, fd);

				if (fd < 0)
				{
					print_error("open");
				}
				scsi_read_test(fd, lun, 1);
				scsi_write_test(fd, lun, 1);
				close(fd);
			}
			break;
		case 22:
			memset(opt, 0, sizeof(struct tool_options));
			strcpy(opt->path, "/dev/bsg/0:0:0:0");
			fd = open(opt->path, oflag);
			LOG_INFO("Start : %s fd %d\n", __func__, fd);

			if (fd < 0)
			{
				print_error("open");
			}
			while (1)
			{
				read_buffer_test(fd, 0);
			}
			break;
		case 23:
			while (1)
			{
				lun = lun_rand(opt, 8);
				fd = open(opt->path, oflag);
				LOG_INFO("Start : %s fd %d\n", __func__, fd);

				if (fd < 0)
				{
					print_error("open");
				}
				scsi_write_test(fd, lun, 1);
				close(fd);
			}
			break;
		default:
			// if (test_case < 256)
			LOG_ERROR("Error case number! please try again:\n");
			break;
		}
		if (21 != test_case)
		{
			close(fd);
		}
#endif
	}
}
