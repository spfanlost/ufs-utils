/**
 * @file test_send_cmd.h
 * @author yumeng (imyumeng@qq.com)
 * @brief
 * @version 0.1
 * @date 2019-10-08
 *
 * @copyright Copyright (c) 2019
 *
 */
#ifndef _TEST_SEND_CMD_H_
#define _TEST_SEND_CMD_H_
#include <linux/types.h>
#include "common_types.h"
#include "../options.h"
#include "../scsi_bsg_util.h"

#define CFG_UNIT_DESC_NUM (0x08)

typedef enum
{
	/* UPIU Query request vendor function */
	UPIU_QUERY_VENDOR_FUNC_0 = 0xC0,
	UPIU_QUERY_VENDOR_FUNC_1 = 0xC1,
	UPIU_QUERY_VENDOR_FUNC_MAX = 0xFF,
} QUERY_VENDOR_FUNCTION;

typedef enum
{
	QUERY_VENDOR_OPCODE_0 = 0xF0,
	QUERY_VENDOR_OPCODE_1 = 0xF1,
	QUERY_VENDOR_OPCODE_MAX = 0xFF,
} QUERY_VENDOR_OPCODE;

typedef enum
{
	ABORT_TASK_FUNC = 0x01,
	ABORT_TASK_SET_FUNC = 0x02,
	CLEAR_TASK_SET_FUNC = 0x04,
	LU_RESET_FUNC = 0x08,
	QUERY_TASK_FUCNC = 0x80,
	QUERY_TASK_SET_FUNC = 0x81,
} TASK_FUNC;

#pragma pack(push, 1)
typedef struct
{
	byte_t bLength;
	byte_t bDescriptorType;
	byte_t bDevice;
	byte_t bDeviceClass;
	byte_t bDeviceSubClass;
	byte_t bProtocol;
	byte_t bNumberLU;
	byte_t bNumberWLU;
	byte_t bBootEnable;
	byte_t bDescrAccessEn;
	byte_t bInitPowerMode;
	byte_t bHighPriorityLUN;
	byte_t bSecureRemovalType;
	byte_t bSecurityLU;
	byte_t bBackgroundOpsTermLat;
	byte_t bInitActiveICCLevel;
	word_t wSpecVersion;
	word_t wManufactureDate;
	byte_t iManufactureName;
	byte_t iProductName;
	byte_t iSerialNumber;
	byte_t iOemID;
	word_t ManufacturerID;
	byte_t bUD0BaseOffset;
	byte_t bUDConfigPLength;
	byte_t bDeviceRTTCap;
	word_t wPeriodicRTCUpdate;
	byte_t bUFSFeaturesSupport;
	byte_t bFFUTimeout;
	byte_t bQueueDepth;
	word_t wDeviceVersion;
	byte_t bNumSecureWPArea;
	dword_t dPSAMaxDataSize;
	byte_t bPSAStateTimeout;
	byte_t iProductRevisionLevel;
	byte_t Reserved1;
	dword_t Reserved2;
	dword_t Reserved3;
	dword_t Reserved4;
	dword_t Reserved5;
	dword_t Reserved6;
	word_t wHPBVersion;
	byte_t bHPBControl;
	dword_t Reserved8;
	qword_t Reserved9;
	dword_t dExtendedUFSFeaturesSupport;
	byte_t bWriteBoosterBufferPreserveUserSpaceEn;
	byte_t bWriteBoosterBufferType;
	dword_t dNumSharedWriteBoosterBufferAllocUnits;
} UFS_DEVICE_DESC;

typedef struct
{
	byte_t bLUEnable;
	byte_t bBootLunID;
	byte_t bLUWriteProtect;
	byte_t bMemoryType;
	dword_t dNumAllocUnits;
	byte_t bDataReliability;
	byte_t bLogicalBlockSize;
	byte_t bProvisioningType;
	word_t wContextCapabilities;
	byte_t reserved[3];
	word_t wLUMaxActiveHPBRegions;
	word_t wHPBPinnedRegionStartIdx;
	word_t wNumHPBPinnedRegions;
	dword_t dLUNumWriteBoosterBufferAllocUnits;
} UFS_UNIT_CONFIG_DESC;

typedef struct
{
	byte_t bLength;
	byte_t bDescriptorType;
	byte_t bConfDescContinue;
	byte_t bBootEnable;
	byte_t bDescrAccessEn;
	byte_t bInitPowerMode;
	byte_t bHighPriorityLUN;
	byte_t bSecureRemovalType;
	byte_t bInitActiveICCLevel;
	word_t wPeriodicRTCUpdate;
	byte_t bHPBControl;
	byte_t bRPMBRegionEnable;
	byte_t bRPMBRegion1Size;
	byte_t bRPMBRegion2Size;
	byte_t bRPMBRegion3Size;
	byte_t bWriteBoosterBufferPreserveUserSpaceEn;
	byte_t bWriteBoosterBufferType;
	dword_t dNumSharedWriteBoosterBufferAllocUnits;
	UFS_UNIT_CONFIG_DESC UnitConfig[CFG_UNIT_DESC_NUM];
} UFS_CONFIG_DESC;
#pragma pack(pop)

struct utp_upiu_task
{
	__be32 input_param1;
	__be32 input_param2;
	__be32 input_param3;
	__be32 reserved[2];
};

/**
 * struct utp_upiu_req - general upiu request structure
 * @header:UPIU header structure DW-0 to DW-2
 * @sc: fields structure for scsi command DW-3 to DW-7
 * @qr: fields structure for query request DW-3 to DW-7
 */
struct utp_upiu_req_ext
{
	struct utp_upiu_header header;
	union
	{
		struct utp_upiu_cmd sc;
		struct utp_upiu_query qr;
		struct utp_upiu_query tr;
		/* use utp_upiu_query to host the 4 dwords of uic command */
		struct utp_upiu_query uc;
		struct utp_upiu_task tm;
	};
};

/* request (CDB) structure of the sg_io_v4 */
struct ufs_bsg_request_ext
{
	__u32 msgcode;
	struct utp_upiu_req_ext upiu_req;
};

/* response (request sense data) structure of the sg_io_v4 */
struct ufs_bsg_reply_ext
{
	/*
	 * The completion result. Result exists in two forms:
	 * if negative, it is an -Exxx system errno value. There will
	 * be no further reply information supplied.
	 * else, it's the 4-byte scsi error result, with driver, host,
	 * msg and status fields. The per-msgcode reply structure
	 * will contain valid data.
	 */
	__u32 result;

	/* If there was reply_payload, how much was received? */
	__u32 reply_payload_rcv_len;

	struct utp_upiu_req_ext upiu_rsp;
};

extern UFS_DEVICE_DESC device_desc;

int do_vendor_query(QUERY_VENDOR_FUNCTION vendor_func, QUERY_VENDOR_OPCODE vendor_opcode);
int read_device_desc_ext(UFS_DEVICE_DESC *desc_buff);
UFS_CONFIG_DESC *read_one_config_desc(byte_t cfg_index);
void rdwr_one_desc_rand_idn(void);
void rdwr_dev_desc_rand_index(void);
void rdwr_cfg_desc_rand_index(void);
void write_one_config_desc(byte_t idx, byte_t *data_buf);
void read_all_config_descs(struct tool_options *opt);
void modify_config_descs(byte_t num);
void modify_config_descs_ext(void);
void get_currnet_pwr_mode(void);
void ufs_query_test(void);
void exe_task_on_cmd_going1(void);
void exe_task_on_cmd_going2(void);

int abort_task(byte_t lun, byte_t ttg, byte_t iid);
int do_task(byte_t task_func, byte_t lun, byte_t ttg, byte_t iid);
#endif
