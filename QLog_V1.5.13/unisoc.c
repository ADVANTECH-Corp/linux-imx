/******************************************************************************
  @file    unisoc.c
  @brief   unisoc log tool.

  DESCRIPTION
  QLog Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2020 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/

#include "qlog.h"

//#define UNISOC_DUMP_TEST

typedef uint8_t   BYTE;
typedef uint32_t  DWORD;

/* DIAG */
#define DIAG_SWVER_F             0                   // Information regarding MS software
#define DIAG_SYSTEM_F            5
#define MSG_BOARD_ASSERT         255

#define ITC_REQ_TYPE	         209
#define ITC_REQ_SUBTYPE          100
#define ITC_REP_SUBTYPE          101

#define UET_SUBTYPE_CORE0        102                 // UE Time Diag SubType field, core0
#define TRACET_SUBTYPE           103                 // Trace Time Diag subtype filed
#define UET_SUBTYPE_CORE1        104                 // UE Time Diag SubType field, core1

#define UET_SEQ_NUM              0x0000FFFF          // UE Time Diag Sequnce number fi
#define UET_DIAG_LEN             (DIAG_HDR_LEN + 12) // UE Time Diag Length field (20)

/* ASSERT */
#define NORMAL_INFO              0x00
#define DUMP_MEM_DATA            0x01
#define DUMP_ALL_ASSERT_INFO_END 0x04
#define DUMP_AP_MEMORY           0x08

#define HDLC_HEADER 0x7e

//Export type definitions
#define DIAG_HDR_LEN     8
typedef struct
{
    unsigned int sn;			// Sequence number
    unsigned short len;			// Package length
    unsigned char type;
    unsigned char subtype;
} DIAG_HEADER;

typedef struct
{
    DIAG_HEADER header;
    uint8_t data[0];					// Package data
}DIAG_PACKAGE;

// SMP package
#define SMP_START_FLAG  0x7E7E7E7E
#define SMP_START_LEN   4
#define SMP_HDR_LEN     8
#define SMP_RESERVED    0x5A5A
typedef struct
{
    unsigned short len;
    unsigned char  lcn;
    unsigned char  packet_type;
    unsigned short reserved;    //0x5A5A
    unsigned short check_sum;
} SMP_HEADER;

typedef struct
{
    SMP_HEADER header;
    uint8_t data[0];					// Package data
} SMP_PACKAGE;

static int m_bUE_Obtained  = 0;
static int m_bVer_Obtained = 0;
static int m_bSendTCmd = 0;
static const char *s_pRequest = NULL;
static int m_bAT_result = 0;
static int unisoc_exx00u_is_blue_screen = 0;
static int unisoc_exx00u_blue_screen_fd = -1;
extern uint32_t query_panic_addr;

struct ql_region_cmd {
    const char *type;
    const char *filename;
    uint32_t base;
    uint32_t size;
    uint32_t skip;
};

struct ql_require_cmd {
    const char *type;
    const char *filename;
    uint32_t addr;
    uint32_t mask;
    uint32_t value;
};

struct ql_cmd_header {
    const char *type;
};

struct ql_cmd {
    union {
        struct ql_region_cmd region;
        struct ql_cmd_header cmd;
        struct ql_require_cmd require;
    };
};

struct ql_unisoc_exx00u_xml_data {
    struct ql_cmd ql_cmd_table[57];
    int ql_cmd_count;
};

static struct ql_unisoc_exx00u_xml_data ql_exx00u_xml_data_defult =
{
    {
        {{{"region", "bootrom", 0x00000000, 0x10000, 1}}},
        {{{"region", "sysram",  0x00800000, 0x78000, 0}}},
        {{{"region", "bbsram", 0x40080000, 0x20000, 0}}},
        {{{"region", "riscv", 0x50000000, 0xc000, 0}}},
        {{{"region", "rfreg", 0x50030000, 0x5100, 0}}},
        {{{"region", "sysreg", 0x50080000, 0x64, 0}}},
        {{{"region", "clkrst", 0x50081000, 0xA8, 0}}},
        {{{"region", "monitor", 0x50083000, 0x44, 0}}},
        {{{"region", "watchdog", 0x50084000, 0x20, 0}}},
        {{{"region", "idle", 0x50090000, 0x2a8, 0}}},
        {{{"region", "idleres", 0x50094000, 0x30, 0}}},
        {{{"region", "pwrctrl", 0x500A0000, 0xD8, 0}}},
        {{{"region", "sys_ctrl", 0x50100000, 0x250, 0}}},
        {{{"region", "mailbox", 0x50104000, 0x67C, 0}}},
        {{{"region", "timer3", 0x50105000, 0x100, 0}}},
        {{{"region", "analog", 0x50109000, 0x240, 0}}},
        {{{"region", "aon_ifc", 0x5010a004, 0x44, 0}}},
        {{{"region", "iomux", 0x5010c000, 0x178, 0}}},
        {{{"region", "pmic", 0x50308000, 0x1000, 0}}},
        {{{"region", "irq", 0x08800000, 0x200, 0}}},
        {{{"region", "irq1", 0x08800800, 0x4C, 0}}},
        {{{"region", "timer1", 0x08808000, 0x100, 0}}},
        {{{"region", "timer2", 0x08809000, 0x100, 0}}},
        {{{"region", "timer4", 0x0880E000, 0x100, 0}}},
        {{{"region", "ap_ifc", 0x0880f004, 0x94, 0}}},
        {{{"region", "sci1", 0x08814000, 0x100, 0}}},
        {{{"region", "sci2", 0x08815000, 0x100, 0}}},
        {{{"region", "f8", 0x09000000, 0x200, 0}}},
        {{{"region", "axidma", 0x090c0000, 0x400, 0}}},
        {{{"region", "riscvram", 0x10040000, 0xA000, 1}}},
        {{{"require", " ", 0x500A0034, 0x03, 0x03}}},
        {{{"region", "riscvem", 0x14110000, 0x6000, 0}}},
        {{{"require", " ", 0x500A0034, 0x03, 0x03}}},
        {{{"region", "txrx", 0x25000000, 0x20000, 0}}},
        {{{"require", " ", 0x500A0034, 0x03, 0x03}}},
        {{{"region", "rfad", 0x25100000, 0x3000, 0}}},
        {{{"region", "coeff", 0x25200000, 0x200, 0}}},
        {{{"region", "ldtc", 0x25300000, 0x24000, 0}}},
        {{{"region", "otdoa", 0x25400000, 0x60000, 0}}},
        {{{"region", "measpwr", 0x25500000, 0xA0000, 0}}},
        {{{"region", "iddet", 0x25600000, 0xc000, 0}}},
        {{{"region", "uldft", 0x25700000, 0x3400, 0}}},
        {{{"region", "pusch", 0x25800000, 0x38000, 0}}},
        {{{"region", "ulpcdci", 0x25900000, 0x200, 0}}},
        {{{"region", "dlfft", 0x25a00000, 0x16000, 0}}},
        {{{"region", "csirs", 0x25b00000, 0xd000, 0}}},
        {{{"region", "ldtc1", 0x26000000, 0x7203A0, 0}}},
        {{{"region", "corr", 0x26000800, 0x1c, 0}}},
        {{{"region", "rxcap", 0x27000000, 0x17000, 0}}},
        {{{"region", "bbapb", 0x40000000, 0x20000, 0}}},
        {{{"require", " ", 0x500A002C, 0x03, 0x03}}},
        {{{"region", "bcpubrom", 0x400A0000, 0x30000, 0}}},
        {{{"region", "zaxidma", 0x22000000, 0x400, 0}}},
        {{{"require", " ", 0x500A0024, 0x03, 0x03}}},
        {{{"region", "zitcm", 0x21400000, 0x4000, 1}}},
        {{{"region", "zdtcm", 0x21500000, 0x4000, 1}}},
        {{{"region", "psram", 0x80000000, 0x1000000, 0}}}
    },
    57
};

#define unisoc_log_max (24*1024)
static uint8_t *unisoc_log_buf = NULL;
static size_t unisoc_log_size = 0;

#include "diag_pkt.c"
static struct diag_pkt *unisoc_diag_pkt;

uint8_t rid = 0x00;

int m_bVer_Obtained_change(void)
{
    m_bVer_Obtained = 0;
    return 0;
}

static void hexdump(const uint8_t *d, size_t size) {
    unsigned i;
    unsigned n=64;

    printf("%s size=%zd\n", __func__, size);
    for (i = 0; i < size; i++) {
        if (i%n==0)
            printf("%04d:", i);
        printf(" %02x", d[i]);
        if (i%n==(n-1))
            printf("\n");
    }

    printf("\n");
}

static unsigned short frm_chk( unsigned short *src, int len,int nEndian )
{
    unsigned int sum = 0;
    unsigned short SourceValue, DestValue;
    unsigned short lowSourceValue, hiSourceValue;

    /* Get sum value of the source.*/
    while (len > 1)
    {
        SourceValue = *src++;
        if( nEndian == 1 )
		{
			// Big endian
			DestValue   = 0;
			lowSourceValue = (unsigned short)(( SourceValue & 0xFF00 ) >> 8);
			hiSourceValue = (unsigned short)(( SourceValue & 0x00FF ) << 8);
			DestValue = (unsigned short)(lowSourceValue | hiSourceValue);
		}
		else
		{
			// Little endian
			DestValue = qlog_le16(SourceValue);
		}
        sum += DestValue;
        len -= 2;
    }

    if (len == 1)
    {
        sum += *( (unsigned char *) src );
    }

    sum = (sum >> 16) + (sum & 0x0FFFF);
    sum += (sum >> 16);

    return (unsigned short)(~sum);
}

static int CheckHeader(SMP_HEADER *pHdr)
{
	if( frm_chk((unsigned short*)pHdr, SMP_HDR_LEN, 0) == 0)
		return 1;

	return 0;
}

/* get sys timestamp */
static long GetTimeStamp()
{
    struct timeval tm;
    gettimeofday(&tm, NULL);

    return (tm.tv_sec * 1000 + tm.tv_usec / 1000);
}

static void WriteDataToFile(int logfd, const void *lpBuffer, DWORD dwDataSize, uint32_t header)
{
    //printf("%s size=%d\n", __func__, dwDataSize);
    uint32_t le32 = qlog_le32(dwDataSize);

    if (header >= 4) {
        lpBuffer -= 4;
        dwDataSize += 4;
        memcpy((void *)lpBuffer, &le32, 4);
        qlog_logfile_save(logfd, lpBuffer, dwDataSize);
        return;
    }

    qlog_logfile_save(logfd, &le32, sizeof(dwDataSize));
    qlog_logfile_save(logfd, lpBuffer, dwDataSize);
}

static int unisoc_diag_fd = -1;

static int unisoc_send_cmd(uint8_t cmd) {
    uint8_t lpBuf[2] = {cmd, '\n'};

    qlog_dbg("%s cmd='%c'\n", __func__, cmd);
    return qlog_poll_write(unisoc_diag_fd, lpBuf, 2, 0);
}

static int unisco_send_at_command(const char* lpBuf_tmp)
{
    int len = strlen(lpBuf_tmp);
    int total_len = 11 + len;
    uint8_t lpBuf[256] = {0x7e, 0x54, 0xaf, 0x26, 0x6f};
    unsigned timeout = 0;

    if (m_bSendTCmd)
        return 0;

    lpBuf[5] = (total_len - 2)&0xff;
    lpBuf[6] = ((total_len - 2)>>8)&0xff;;
    lpBuf[7] = 0x68;
    lpBuf[8] = 0x00;
    memmove(lpBuf + 9, lpBuf_tmp, len);
    lpBuf[9 + len] = 0x0d;
    lpBuf[9 + len + 1] = 0x7e;
    qlog_dbg("> %s\n", lpBuf_tmp);
    s_pRequest = lpBuf_tmp;
    m_bAT_result = 0;
    qlog_poll_write(unisoc_diag_fd, lpBuf, total_len, 0);

    do {
        unsigned t = 100;
        usleep(t*1000);
        timeout += t;
    } while (s_pRequest && timeout < 3000);

    s_pRequest = NULL;
    return m_bAT_result;
}

static int unisco_process_at_command(const uint8_t *lpBuf, size_t size)
{
    size_t len = 0;
    size_t i = 0;
    static char at_buff[256];

    while (i < size) {
        if (lpBuf[i] != 0x0d && lpBuf[i] != 0x0a) {
            at_buff[len++] = lpBuf[i];
            if ((len + 1) == sizeof(at_buff))
                    break;
        }
        i++;
    }
    at_buff[len] = 0;

    qlog_dbg("< %s\n", at_buff);
    if (s_pRequest) {
        if (strstr(at_buff, "OK")) {
            m_bAT_result = 1;
            s_pRequest = NULL;
        }
        else if (strstr(at_buff, "ERROR")) {
            m_bAT_result = 0;
            s_pRequest = NULL;
        }
    }

    return 0;
}

static void unisoc_handle_diag_pkt_func(struct diag_pkt *diag_pkt) ;
static int unisoc_init_filter(int fd, const char *cfg) {
    uint8_t lpBuf[32] = {0};
    DIAG_HEADER request;
    unsigned nTryNum = 0;

//need 'at+armlog=1' to enable LOG
    m_bUE_Obtained = 0;
    m_bVer_Obtained = 0;
    m_bSendTCmd = 0;
    unisoc_diag_fd = fd;

    if (g_is_unisoc_chip == 1)
    {
        if (!unisoc_diag_pkt) {
            unisoc_diag_pkt = (struct diag_pkt *)diag_pkt_malloc( sizeof(DIAG_PACKAGE) + 2, 0x4008 + 2, 1, unisoc_handle_diag_pkt_func);
            if (!unisoc_diag_pkt)
                return -1;
        }
        unisoc_diag_pkt->pkt_len = 0;
        unisoc_diag_pkt->last_7d = 0;
    }
    unisoc_send_cmd('0');

    if (g_is_unisoc_chip == 1)
    {
        //logle -> Option ->  Device Configure -> General
        while (nTryNum++ < 10 && !unisco_send_at_command("AT+ARMLOG=1"));
        unisco_send_at_command("AT+SPLOGLEVEL=1,3,\"60FFF870000000000000000000000000\",\"EFF202E708FFFFFFEFFF70F7CCBFF1E000FF030CFDFFFFFD0000000000000081\"");
        unisco_send_at_command("AT+SPDSPOP=2");
        unisco_send_at_command("AT+SPCAPLOG=1");
        unisco_send_at_command("AT+SPCAPLEN=80");

        nTryNum = 0;
        while (qlog_exit_requested != 0)
        {
            if (!m_bSendTCmd && m_bUE_Obtained == 0 && nTryNum++ < 5) {
                // query modem UE time
                request.len         = qlog_le16(DIAG_HDR_LEN);
                request.type        = DIAG_SYSTEM_F;
                request.subtype     = 0x11;
                request.sn          = qlog_le32(0);

                lpBuf[0] = 0x7E;
                memcpy(lpBuf+1, &request, DIAG_HDR_LEN);
                lpBuf[DIAG_HDR_LEN+1] = 0x7E;

                if (qlog_poll_write(fd, lpBuf, DIAG_HDR_LEN+2, 0) <= 0) { return -1; }
            }

            if (!m_bSendTCmd && m_bVer_Obtained == 0) {
                // query modem version
                request.len         = qlog_le16(DIAG_HDR_LEN);
                request.type        = DIAG_SWVER_F;
                request.subtype     = 0x0;
                request.sn          = qlog_le32(0);

                lpBuf[0] = 0x7E;
                memcpy(lpBuf+1, &request, DIAG_HDR_LEN);
                lpBuf[DIAG_HDR_LEN+1] = 0x7E;

                if (qlog_poll_write(fd, lpBuf, DIAG_HDR_LEN+2, 0) <= 0) { return -1; }
            }

            sleep(5);

            /*
                AP DUMP
                AT+QCFG="APRSTLEVEL",0
                AT+QTEST="dump",0

                CP DUMP
                AT+QCFG="modemrstlevel",0
                AT+QTEST="dump",1
            */

    #ifdef UNISOC_DUMP_TEST
            if (!m_bSendTCmd) {
                uint8_t asser_modem[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x05, 0x04, 0x7e};
                //uint8_t asser_ap[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x05, 0x08, 0x7e};

                if (write(fd, asser_modem, sizeof(asser_modem)) == -1) { return -1; };
            }
    #endif
        }
    }
    else if (g_is_unisoc_chip == 2)
    {
        const uint8_t at_prepare[] =  {0x7e, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x62, 0x60, 0x00, 0x00,
                                        0x00, 0x00, 0x7e};
        if (qlog_poll_write(fd, at_prepare, sizeof(at_prepare), 0) <= 0) { return -1; }
        usleep(1);

        const uint8_t at_armlog[] =  {0x7e, 0x6f, 0x00, 0x73, 0x00, 0x13, 0x00, 0x68, 0x00, 0x41, 0x54,
                                        0x2b, 0x41, 0x52, 0x4d, 0x4c, 0x4f, 0x47, 0x3d, 0x31, 0x7e};
        if (qlog_poll_write(fd, at_armlog, sizeof(at_armlog), 0) <= 0) { return -1; }
        usleep(100*1000);   //after 900ms, send at_splogping

        const uint8_t at_splogping[] =  {0x7e, 0x14, 0x00, 0x00, 0x00, 0x25, 0x00, 0x68, 0x00, 0x61, 0x74, 0x2b, 0x73, 0x70,
                                        0x6c, 0x6f, 0x67, 0x70, 0x69, 0x6e, 0x67, 0x3d, 0x31, 0x39, 0x35, 0x33, 0x36, 0x2c,
                                        0x31, 0x2c, 0x30, 0x2c, 0x30, 0x2c, 0x30, 0x2c, 0x30, 0x0d, 0x7e};
        if (qlog_poll_write(fd, at_splogping, sizeof(at_splogping), 0) <= 0) { return -1; }
        usleep(1);

        //time and version Not written
    }

    return 0;
}

static int unisoc_logfile_init(int logfd, unsigned logfile_seq) {
    // write first reserved package
    BYTE lpBuf[32] = {0};
    DIAG_HEADER ph = {0};

    ph.sn      = qlog_le32(0);
    ph.type    = ITC_REQ_TYPE;
    ph.subtype = ITC_REP_SUBTYPE;
    ph.len     = qlog_le16(DIAG_HDR_LEN + 8);

    memcpy(lpBuf, &ph, DIAG_HDR_LEN);
    ((DWORD*)(lpBuf + DIAG_HDR_LEN ))[0] = qlog_le32(7);
    lpBuf[ DIAG_HDR_LEN + 4 ] = 0; //0:Little Endian,1: Big Endian
    lpBuf[ DIAG_HDR_LEN + 5 ] = 1;
    lpBuf[ DIAG_HDR_LEN + 6 ] = 1;

    WriteDataToFile(logfd, lpBuf, DIAG_HDR_LEN + 8, 0);

    // write default sys time package
    memset(&ph, 0, sizeof( DIAG_HEADER));
    ph.sn      = qlog_le32(UET_SEQ_NUM);
    ph.type    = ITC_REQ_TYPE;
    ph.subtype = UET_SUBTYPE_CORE0;
    ph.len     = qlog_le16(UET_DIAG_LEN);

    memcpy(lpBuf, &ph, DIAG_HDR_LEN);
    ((uint64_t *)(lpBuf + DIAG_HDR_LEN ))[0] = qlog_le64(GetTimeStamp());
    ((uint32_t *)(lpBuf + DIAG_HDR_LEN ))[2]  = qlog_le32(0);

    WriteDataToFile(logfd, lpBuf, UET_DIAG_LEN, 0);

    return 0;
}

static void unisoc_proc_log_buf(int logfd, const void *inBuf, size_t size) {
    const uint8_t *lpBuf = inBuf;
    size_t cur = 0;
    SMP_PACKAGE* lpPkg;
    unsigned char  agStartFlag[SMP_START_LEN] = {0x7E,0x7E,0x7E,0x7E};

    if (unisoc_log_buf == NULL) {
        unisoc_log_buf = (uint8_t *)malloc(unisoc_log_max);
    }
    if (unisoc_log_buf == NULL) {
        return;
    }
    //hexdump(buf, size);

    //printf("%s cur=%zd, add=%zd\n", __func__, unisoc_log_size, size);
    if (unisoc_log_size) {
        if ((unisoc_log_size + size) > unisoc_log_max) {
            printf("%s cur=%zd, add=%zd\n", __func__, unisoc_log_size, size);
            return;
        }
        memcpy(unisoc_log_buf + unisoc_log_size, inBuf, size);
        size += unisoc_log_size;
        inBuf = unisoc_log_buf;
    }

    while (cur < size) {
        uint16_t pkt_len;
        lpBuf = inBuf + cur;

        if ((cur + SMP_START_LEN + sizeof(SMP_HEADER)) > size) {
            break;
        }

        if (memcmp(lpBuf, agStartFlag, SMP_START_LEN)) {
            if (cur == 0) {
                //printf("%s agStartFlag %02x%02x%02x%02x\n", __func__, lpBuf[0], lpBuf[1], lpBuf[2], lpBuf[3]);
                //hexdump(lpBuf, MIN(100, size-cur));
            }
            cur++;
            if (cur == size) {
                cur--;
                break;
            }
            continue;
        }

        lpPkg = ( SMP_PACKAGE *)(lpBuf + SMP_START_LEN);

        if (CheckHeader(&lpPkg->header) == 0) {
            printf("%s CheckHeader fail\n", __func__);
            hexdump(lpBuf, MIN(100, size-cur));
            cur++;
            continue;
        }

        if (lpPkg->header.lcn != 0) {
            printf("%s header.lcn=0x%x\n", __func__, lpPkg->header.lcn);
            hexdump(lpBuf, MIN(100, size-cur));
            cur++;
            continue;
        }

        if (lpPkg->header.packet_type != 0 && lpPkg->header.packet_type != 0xf8) {
            printf("%s header.packet_type=0x%x\n", __func__, lpPkg->header.packet_type);
            hexdump(lpBuf, MIN(100, size-cur));
            cur++;
            continue;
        }

        if (qlog_le16(lpPkg->header.reserved) != 0x5A5A) {
            printf("%s header.reserved=0x%x\n", __func__, qlog_le16(lpPkg->header.reserved));
            hexdump(lpBuf, MIN(100, size-cur));
            cur++;
            continue;
        }

        pkt_len = qlog_le16(lpPkg->header.len);
        if ((cur + SMP_START_LEN + pkt_len) > size) {
            //printf("%s wait for more data, cur=%zd, size=%zd, len=%d\n", __func__, cur, size, lpPkg->header.len);
            break;
        }

        cur += (SMP_START_LEN + pkt_len);
        WriteDataToFile(logfd, lpBuf + SMP_START_LEN + sizeof(SMP_HEADER), pkt_len - sizeof(SMP_HEADER), sizeof(SMP_HEADER));
    }

    unisoc_log_size = size - cur;
    if (unisoc_log_size)
        memmove(unisoc_log_buf, lpBuf, unisoc_log_size);
    //printf("%s left=%zd\n", __func__, unisoc_log_size);
}

void* unisoc_cpdump_thread_func(void *arg)
{
    int t = 0;
    unsigned g_rx_log_count_temp = 0;

    do
    {
        qlog_dbg("g_rx_log_count:%d  g_rx_log_count_temp:%d\n",g_rx_log_count,g_rx_log_count_temp);
        g_rx_log_count_temp = g_rx_log_count;
        sleep(3);
        if (t++ > 4)
            break;
    } while (g_rx_log_count != g_rx_log_count_temp);

    unisoc_send_cmd('t');

    return (void*)0;
}

static void HandleAssertProc(DIAG_PACKAGE* lpPkg) {

    //printf("%s subtype=%d, len=%d\n", __func__, lpPkg->header.subtype, lpPkg->header.len);
    pthread_t thread_id_unisoc_cpdump;

    if (NORMAL_INFO == lpPkg->header.subtype)
    {
        if (!m_bSendTCmd) {
            g_donot_split_logfile = 1;
            qlog_dbg("pay attention: modem crash!\n");
            m_bSendTCmd = 1;
            pthread_create(&thread_id_unisoc_cpdump, NULL, unisoc_cpdump_thread_func, NULL);
            //unisoc_send_cmd('t'); //Print the all assert information.
        }
    }
    else if (DUMP_MEM_DATA == lpPkg->header.subtype) // recv mem data
    {
    }
    else if (DUMP_ALL_ASSERT_INFO_END == lpPkg->header.subtype) // recv assert end
    {
        qlog_dbg("%s DUMP_ALL_ASSERT_INFO_END\n", __func__);
        qlog_exit_requested = 1;
        m_bSendTCmd = 0;
        g_donot_split_logfile = 0;
        //unisoc_send_cmd('z'); //Reset MCU
    }
    else
   {
        qlog_dbg("%s subtype=%d\n", __func__, lpPkg->header.subtype);
   }
}

static int HandleDiagPkt(const uint8_t *lpBuf, size_t size) {
    uint16_t pkt_len;
    DIAG_PACKAGE* lpPkg;

    if (lpBuf[0] != 0x7e)
    {
        if (lpBuf[0] == 0x30)
        {
            if (lpBuf[1] == 0x0d && lpBuf[2] == 0x0a && lpBuf[3] == 0x7e && lpBuf[4] == 0x5e)
            {
                return 0;
            }
        }
    }
    else
    {
        const uint8_t at_tag1[] = {0x7e, 0x54, 0xaf, 0x26, 0x6f};
        const uint8_t at_tag2[] =  {0x7e, 0x54, 0xaf, 0x00, 0x00, 0x08, 0x00, 0xd5, 0x00, 0x7e};
        const uint8_t at_ctx[] =  {0x7e, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0x9c, 0x00};

        if (lpBuf[1] == 0x5e && lpBuf[2] == 0x40 && lpBuf[3] == 0x5e && lpBuf[4] == 0x40)
        {
            return 0;
        }

        if (lpBuf[1] == 0x00 && lpBuf[2] == 0x00 && lpBuf[3] == 0x00 && lpBuf[4] == 0x00 && lpBuf[5] == 0x00 && lpBuf[6] == 0xff && lpBuf[7] == 0x00)
        {
            return 0;
        }

        if (!memcmp(lpBuf, at_tag1, 5) || !memcmp(lpBuf, at_tag2, 5)) {
            if ((size_t)(lpBuf[5] + (lpBuf[6]<<8) + 2) == size) {
                return 0;
            }
        }

        if (!memcmp(lpBuf, at_ctx, 5) && lpBuf[7] == at_ctx[7] && lpBuf[8] == at_ctx[8]) {
            if ((size_t)(lpBuf[5] + (lpBuf[6]<<8) + 2) == size) {
                unisco_process_at_command(lpBuf + sizeof(at_ctx), size - sizeof(at_ctx) - 1);
                return 0;
            }
        }
    }

    lpPkg = (DIAG_PACKAGE *)(lpBuf + 1);

    pkt_len = qlog_le16(lpPkg->header.len);

    if (pkt_len > 0x4008) {
        qlog_dbg("larger pkt_len= %d\n", pkt_len);
    }

    if (pkt_len != ( size - 2)) {
        if (pkt_len)
            qlog_dbg("fix pkt_len= %d -> %zd\n", pkt_len, size - 2);
        pkt_len = size - 2;
        lpPkg->header.len = qlog_le16(pkt_len);
    }

    if (DIAG_SYSTEM_F == lpPkg->header.type && 0x11 == lpPkg->header.subtype)
    {
        m_bUE_Obtained = 1;
    }
    else if (DIAG_SWVER_F == lpPkg->header.type && 0x0 == lpPkg->header.subtype)
    {
        m_bVer_Obtained = 1;
    }
    else if (DIAG_SYSTEM_F == lpPkg->header.type && 0x1 == lpPkg->header.subtype)
    {
    }
    else if (MSG_BOARD_ASSERT == lpPkg->header.type)
    {
        HandleAssertProc(lpPkg);
    }
    else
    {
        qlog_dbg("%s unknow type subtype\n", __func__);
        hexdump(lpBuf, size);
    }

    return 1;
}

static int unisoc_logfile_fd = -1;
static void unisoc_handle_diag_pkt_func(struct diag_pkt *diag_pkt) {
    if (HandleDiagPkt(diag_pkt->buf, diag_pkt->pkt_len)) {
        WriteDataToFile(unisoc_logfile_fd, diag_pkt->buf+1, diag_pkt->pkt_len-2, sizeof(diag_pkt->reserved));
    }
}

static void unisoc_proc_diag_buf(int logfd, const void *inBuf, size_t size) {
#ifdef UNISOC_DUMP_TEST
    static int unisoc_raw_diag_fd = -1;
    if (unisoc_raw_diag_fd == -1) {
        unisoc_raw_diag_fd = qlog_create_file_in_logdir("../raw_diag.bin");
    }
    if (unisoc_raw_diag_fd  != -1) {
        if (write(unisoc_raw_diag_fd , inBuf, size) == -1) { };
    }
#endif

    if (unisoc_diag_pkt && size) {
        unisoc_logfile_fd = logfd;
        diag_pkt_input(unisoc_diag_pkt, inBuf, size);
    }
}

extern int g_unisoc_log_type;
static size_t unisoc_logfile_save(int logfd, const void *buf, size_t size) {
#if 0
    static size_t max_size = 4095;

    if (size > max_size) {
        max_size = size;
        qlog_dbg("max_size = %zd\n", max_size);
    }
#endif
    if (g_unisoc_log_type == 1) {
        size_t cur = 0, len = 0;
        for (cur = 0; cur < size; cur += len) {
            len = size - cur;
            if (len > (2028*2)) len = (2028*2); // 2028 is max packet len
            unisoc_proc_log_buf(logfd, buf + cur, len);
        }
    }
    else if (g_unisoc_log_type == 0)
        unisoc_proc_diag_buf(logfd, buf, size);
    return size;
}

static int unisoc_clean_filter(int fd) {
    if (unisoc_diag_pkt && unisoc_diag_pkt->errors) {
        qlog_dbg("unisoc_diag_pkt errors = %u\n", unisoc_diag_pkt->errors);
    }
    return 0;
}

static int sciu2sMessage(int usbfd, uint16_t wValue)
{
    int ret = 0;
    struct usbdevfs_ctrltransfer control;

    //control message that sciu2s.ko will send on tty is open
    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = wValue;
    control.wIndex = 0;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ret = ioctl(usbfd, USBDEVFS_CONTROL, &control);
    if (ret == -1)
        printf("errno: %d (%s)\n", errno, strerror(errno));
    return ret;
}

static int m_printf_count = 0;
static void unisoc_status_bar(long byte_recv)
{
    const char status[] = {'-', '\\', '|', '/'};
    const long size = (16*1024*1024); //too much print on uart debug will cost too much time

    if ((byte_recv/size) > m_printf_count) {
        qlog_raw_log("status: %c", status[m_printf_count%4]);
        m_printf_count++;
    }
}

int unisoc_catch_8310_dump(int ttyfd, const char *logfile_dir, int RX_URB_SIZE, const char* (*qlog_time_name)(int))
{
    int ret = -1;
    int nwrites = 0;
    int nreads = 0;
    char *pbuf_dump = NULL;
    int unisoc_logfile_dump = -1;
    char logFileName[100] = {0};
    char cure_dir_path[256] = {0};
    char dump_dir[262] = {0};

    snprintf(dump_dir, sizeof(dump_dir), "%.172s/dump_%.80s", logfile_dir, qlog_time_name(1));
    mkdir(dump_dir, 0755);
    if (!qlog_avail_space_for_dump(dump_dir, 256)) {
         qlog_dbg("no enouth disk to save dump\n");
         qlog_exit_requested = 1;
         goto error;
    }

    snprintf(logFileName, sizeof(logFileName), "apdump_%.80s.logel",qlog_time_name(1));
    snprintf(cure_dir_path,sizeof(cure_dir_path),"%.155s/%.80s",dump_dir,logFileName);
    qlog_dbg("unisoc_catch_dump : cure_dir_path:%s\n",cure_dir_path);

    pbuf_dump = (char*)malloc(RX_URB_SIZE);
    if (pbuf_dump == NULL)
    {
        ret = -1;
        goto error;
    }

    unisoc_logfile_dump = qlog_logfile_create_fullname(0, cure_dir_path, 0, 1);
    if (unisoc_logfile_dump == -1)
    {
        qlog_dbg("unisoc_catch_dump : open failed\n");
        ret = -1;
        goto error;
    }

    const uint8_t trub[] = {0x7e, 0x32, 0x00, 0x38, 0x00, 0x0c, 0x00, 0x62, 0x00, 0x33, 0x00, 0x00, 0x00, 0x7e};
    ret = qlog_poll_write(ttyfd, trub, sizeof(trub), 1000);
    if (ret < 0)
        goto error;
    usleep(1000);

    ret = qlog_poll_write(ttyfd, "t\n", 2, 1000);
    if (ret < 0)
        goto error;
    usleep(1000);

    long unisoc_send_total_size = 0;
    while(qlog_exit_requested == 0)
    {
        nreads = qlog_poll_read(ttyfd, pbuf_dump, RX_URB_SIZE, 12000);
        if (nreads == 0)
        {
            qlog_dbg("unisoc ap dump capture success!\n");
            ret = 1;
            break;
        }

        nwrites = qlog_logfile_save(unisoc_logfile_dump, pbuf_dump , nreads);
        if (nreads != nwrites)
        {
            qlog_dbg("nreads:%d  nwrites:%d\n",nreads,nwrites);
            ret = -1;
            break;
        }

        unisoc_send_total_size = unisoc_send_total_size + nwrites;
        unisoc_status_bar(unisoc_send_total_size);
    }

    error:
    if (pbuf_dump)
        free(pbuf_dump);
    if (unisoc_logfile_dump != -1)
        qlog_logfile_close(unisoc_logfile_dump);

    return ret;
}

int unisoc_catch_dump(int usbfd, int ttyfd, const char *logfile_dir, int RX_URB_SIZE, const char* (*qlog_time_name)(int))
{
    int ret = -1;
    int nwrites = 0;
    int nreads = 0;
    char *pbuf_dump = NULL;
    int unisoc_logfile_dump = -1;
    char logFileName[100] = {0};
    char cure_dir_path[256] = {0};

    snprintf(logFileName, sizeof(logFileName), "apdump_%.80s.logel",qlog_time_name(1));
    snprintf(cure_dir_path,sizeof(cure_dir_path),"%.155s/%.80s",logfile_dir,logFileName);
    qlog_dbg("unisoc_catch_dump : cure_dir_path:%s\n",cure_dir_path);

    pbuf_dump = (char*)malloc(RX_URB_SIZE);
    if (pbuf_dump == NULL)
    {
        ret = -1;
        goto error;
    }

    unisoc_logfile_dump = qlog_logfile_create_fullname(0, cure_dir_path, 0, 1);
    if (unisoc_logfile_dump == -1)
    {
        qlog_dbg("unisoc_catch_dump : open failed\n");
        ret = -1;
        goto error;
    }

    if (g_is_unisoc_chip == 1)
    {
        ret = sciu2sMessage(usbfd, 0x0101);
        if (ret != 0)
        {
            qlog_dbg("ioctl USBDEVFS_CONTROL failed, errno = %d(%s)\n", errno, strerror(errno));
            goto error;
        }

        ret = qlog_poll_write(ttyfd, "t\n", 2, 1000);
        if (ret < 0)
            goto error;
    }
    else if (g_is_unisoc_chip == 3)
    {
        ret = sciu2sMessage(usbfd, 0x0100);
        if (ret != 0)
        {
            qlog_dbg("ioctl USBDEVFS_CONTROL failed, errno = %d(%s)\n", errno, strerror(errno));
            goto error;
        }
        usleep(1000);

        ret = qlog_poll_write(ttyfd, "0\n", 2, 1000);
        if (ret < 0)
            goto error;
        usleep(1000);

        const uint8_t trub[] = {0x7e, 0x54, 0x52, 0xf6, 0x75, 0x0c, 0x00, 0x62, 0x60, 0x00, 0x00, 0x00, 0x00, 0x7e};
        ret = qlog_poll_write(ttyfd, trub, sizeof(trub), 1000);
        if (ret < 0)
            goto error;
        usleep(1000);

        ret = qlog_poll_write(ttyfd, "t\n", 2, 1000);
        if (ret < 0)
            goto error;
        usleep(1000);
    }

    long unisoc_send_total_size = 0;
    while(qlog_exit_requested == 0)
    {
        nreads = qlog_poll_read(ttyfd, pbuf_dump, RX_URB_SIZE, 12000);
        if (nreads == 0)
        {
            qlog_dbg("unisoc ap dump capture success!\n");
            ret = 1;
            break;
        }

        nwrites = qlog_logfile_save(unisoc_logfile_dump, pbuf_dump , nreads);
        if (nreads != nwrites)
        {
            qlog_dbg("nreads:%d  nwrites:%d\n",nreads,nwrites);
            ret = -1;
            break;
        }

        unisoc_send_total_size = unisoc_send_total_size + nwrites;
        unisoc_status_bar(unisoc_send_total_size);
    }

    error:
    if (pbuf_dump)
        free(pbuf_dump);
    if (unisoc_logfile_dump != -1)
        qlog_logfile_close(unisoc_logfile_dump);

    return ret;
}

qlog_ops_t unisoc_qlog_ops = {
    .init_filter = unisoc_init_filter,
    .clean_filter = unisoc_clean_filter,
    .logfile_init = unisoc_logfile_init,
    .logfile_save = unisoc_logfile_save,
};

/* returns 1 if line starts with prefix, 0 if it does not */
static int strStartsWith(const char *line, const char *prefix)
{
    for (; *line != '\0' && *prefix != '\0'; line++, prefix++)
    {
        if (*line != *prefix)
        {
            return 0;
        }
    }

    return *prefix == '\0';
}

uint8_t PacketCrc(const void* packet, int size)
{
    int i;
    uint8_t* p = (uint8_t*)packet;
    uint8_t crc = 0;
    for (i=3;i<size-1;i++)
        crc ^= p[i];

    return crc;
}

size_t unisoc_exx00u_send_cmd(int fd, uint8_t* buff, size_t size)
{
    return qlog_poll_write(fd, buff, size, 1000);
}

size_t unisoc_exx00u_recv_cmd(int fd, uint8_t* buff, size_t size)
{
    return qlog_poll_read(fd, buff, size, 3000);
}

size_t unisoc_exx00u_send_cmd_complete(int fd, uint32_t addr, uint16_t size, int type)
{
    uint8_t buf_send_query[11] = {0xad, 0x00, 0x07, 0xff, 0x00, 0xb1, 0x86, 0xc1, 0x80, 0x01, 0x88};
    uint8_t buf_send_readBlock[13] = {0xad, 0x00, 0x09, 0xfe, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x88};
    uint8_t buf_send_read32[11] = {0xad, 0x00, 0x07, 0xfe, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

    rid = ((rid == 255) ? 1 : (rid + 1));

    switch (type)
    {
        case 0:    //blue screen dump status query
        {
            buf_send_query[9] = rid;
            memmove(buf_send_query+5, &addr, 4);
            buf_send_query[10] = PacketCrc(buf_send_query, 11);
            break;
        }
        case 1:    //readBlock
        {
            buf_send_readBlock[9] = rid;
            memmove(buf_send_readBlock+5, &addr, 4);
            memmove(buf_send_readBlock+10, &size, 2);
            buf_send_readBlock[12] = PacketCrc(buf_send_readBlock, 13);
            break;
        }
        case 2:    //read32
        {
            buf_send_read32[9] = rid;
            memmove(buf_send_read32+5, &addr, 4);
            buf_send_read32[10] = PacketCrc(buf_send_read32, 11);
            break;
        }
        default:
            qlog_dbg("unisoc_exx00u_cmd_complete switch error\n");
    }

    if (type == 0)
        return unisoc_exx00u_send_cmd(fd, buf_send_query, sizeof(buf_send_query));
    else if (type == 1)
        return unisoc_exx00u_send_cmd(fd, buf_send_readBlock, sizeof(buf_send_readBlock));
    else
        return unisoc_exx00u_send_cmd(fd, buf_send_read32, sizeof(buf_send_read32));
}

size_t unisoc_exx00u_recv_cmd_deal(int fd, uint8_t *pbuf, uint32_t* pvalue, uint16_t* psize, int type)
{
    size_t rc = unisoc_exx00u_recv_cmd(fd, pbuf, QLOG_BUF_SIZE);
    switch (type)
    {
        case 0:             //readBlock
        {
            uint8_t buf_tmp[2] = {0};
            buf_tmp[0] = pbuf[2];
            buf_tmp[1] = pbuf[1];
            *psize = *((uint16_t*)buf_tmp) - 2;
            break;
        }
        case 1:             //read32
        {
            *pvalue = *((uint32_t*)(pbuf + 5));
            break;
        }
        default:
            qlog_dbg("unisoc_exx00u_recv_cmd_deal switch error\n");
    }

    return rc;
}

int unisoc_exx00u_catch_blue_screen(uint8_t* pbuf, const char *logfile_dir)
{
    int i;
    int dumpfile_fd = -1;
    size_t wc = 0;
    uint32_t rvalue = 0;
    uint16_t rsize = 0;
    char filename[380] = {0};
    uint8_t filename_tmp[4] = {0};
    char blue_screen_logfile_dir[280] = {0};

    if (pbuf[0] == 0xad && pbuf[1] == 0x00 && pbuf[2] == 0x03 && pbuf[3] == 0xff && pbuf[4] == rid && pbuf[5] == 0x01)
    {
        qlog_dbg("modem into blue screen dump ...\n");
        unisoc_exx00u_is_blue_screen = 1;
    }
    else
    {
        return 0;
    }

    for (i=0;i<ql_exx00u_xml_data_defult.ql_cmd_count;i++)
    {
        if (qlog_exit_requested)
                return -1;

        if (strStartsWith(ql_exx00u_xml_data_defult.ql_cmd_table[i].cmd.type, "region"))
        {
            if (ql_exx00u_xml_data_defult.ql_cmd_table[i].region.skip == 1)
            {
                continue;
            }
            else
            {
                if ((i+1) < ql_exx00u_xml_data_defult.ql_cmd_count && strStartsWith(ql_exx00u_xml_data_defult.ql_cmd_table[i+1].cmd.type, "require"))
                {
                    //get rvalue
                    unisoc_exx00u_send_cmd_complete(unisoc_exx00u_blue_screen_fd, ql_exx00u_xml_data_defult.ql_cmd_table[i+1].require.addr, 0x00, 2);
                    unisoc_exx00u_recv_cmd_deal(unisoc_exx00u_blue_screen_fd, pbuf, &rvalue, &rsize, 1);

                    //check whether to perform dump
                    if (!((rvalue & ql_exx00u_xml_data_defult.ql_cmd_table[i+1].require.mask) == ql_exx00u_xml_data_defult.ql_cmd_table[i+1].require.value))
                        continue;
                }

                memmove(filename_tmp, &(ql_exx00u_xml_data_defult.ql_cmd_table[i].region.base), 4);
                snprintf(blue_screen_logfile_dir, sizeof(blue_screen_logfile_dir), "%.255s/BlueScreen", logfile_dir);
                if (access(blue_screen_logfile_dir, F_OK) && errno == ENOENT)
                    mkdir(blue_screen_logfile_dir, 0755);

                snprintf(filename, sizeof(filename), "%s/%02x%02x%02x%02x.bin", blue_screen_logfile_dir, filename_tmp[3], filename_tmp[2], filename_tmp[1], filename_tmp[0]);
                dumpfile_fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0444);
                if (dumpfile_fd == -1)
                {
                    qlog_dbg("%s open error\n", __func__);
                    return -1;
                }

                qlog_dbg("save to filename:%s\n", filename);

                while (ql_exx00u_xml_data_defult.ql_cmd_table[i].region.size > 2048)
                {
                    if (qlog_exit_requested)
                        return -1;

                    wc = unisoc_exx00u_send_cmd_complete(unisoc_exx00u_blue_screen_fd, ql_exx00u_xml_data_defult.ql_cmd_table[i].region.base, 0x0800, 1);
                    if (wc != 13)
                    {
                        qlog_dbg("%s send dump cmd fail\n", __func__);
                        return -1;
                    }
                    unisoc_exx00u_recv_cmd_deal(unisoc_exx00u_blue_screen_fd, pbuf, &rvalue, &rsize, 0);

                    ql_exx00u_xml_data_defult.ql_cmd_table[i].region.size -= 2048;
                    ql_exx00u_xml_data_defult.ql_cmd_table[i].region.base += 2048;

                    wc = qlog_logfile_save(dumpfile_fd, pbuf+5, rsize);
                    if (wc != (size_t)rsize)
                    {
                        qlog_dbg("%s save dump file error\n", __func__);
                        return -1;
                    }
                }

                if (ql_exx00u_xml_data_defult.ql_cmd_table[i].region.size > 0)
                {
                    wc = unisoc_exx00u_send_cmd_complete(unisoc_exx00u_blue_screen_fd, ql_exx00u_xml_data_defult.ql_cmd_table[i].region.base, ql_exx00u_xml_data_defult.ql_cmd_table[i].region.size, 1);
                    if (wc != 13)
                    {
                        qlog_dbg("%s send dump cmd fail\n", __func__);
                        return -1;
                    }
                    unisoc_exx00u_recv_cmd_deal(unisoc_exx00u_blue_screen_fd, pbuf, &rvalue, &rsize, 0);

                    wc = qlog_logfile_save(dumpfile_fd, pbuf+5, rsize);
                    if (wc != (size_t)rsize)
                    {
                        qlog_dbg("%s save dump file error\n", __func__);
                        return -1;
                    }
                }

                if (dumpfile_fd != -1)
                {
                    close(dumpfile_fd);
                    dumpfile_fd = -1;
                }
            }
        }
    }

    return 1;
}

static int unisoc_exx00u_init_filter(int fd, const char *cfg) {

    if (!unisoc_exx00u_is_blue_screen)          //blue dump
    {
        unisoc_exx00u_blue_screen_fd = fd;
        while (qlog_exit_requested == 0 && query_panic_addr != 0)
        {
            unisoc_exx00u_send_cmd_complete(fd, query_panic_addr, 0, 0);
            sleep(3);
        }
    }
    else if (g_is_unisoc_exx00u_chip == 2)   //EC800G
    {
        unisoc_diag_fd = fd;
        unisoc_send_cmd('0');
        usleep(100);
        uint8_t unisoc_modem_capture_log[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x05, 0x25, 0x7e};
        if (write(unisoc_diag_fd, unisoc_modem_capture_log, sizeof(unisoc_modem_capture_log)) == -1) { return -1; };
        usleep(200);
        uint8_t unisoc_modem_get_time[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x05, 0x11, 0x7e};
        if (write(unisoc_diag_fd, unisoc_modem_get_time, sizeof(unisoc_modem_get_time)) == -1) { return -1; };
        usleep(100);
        uint8_t unisoc_modem_get_version[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x00, 0x00, 0x7e};
        if (write(unisoc_diag_fd, unisoc_modem_get_version, sizeof(unisoc_modem_get_version)) == -1) { return -1; };
        usleep(100);
    }

    return 0;
}

static int unisoc_exx00u_clean_filter(int fd) {
    if (g_is_unisoc_exx00u_chip == 2)  //EC800G
    {
        uint8_t unisoc_modem_stop_capture_log[] = {0x7e, 0x00, 0x00, 0x00,  0x00, 0x08, 0x00, 0x05, 0x26, 0x7e};
        if (write(fd, unisoc_modem_stop_capture_log, sizeof(unisoc_modem_stop_capture_log)) == -1) { return -1; };
        usleep(100);
    }
    return 0;
}

static int unisoc_exx00u_logfile_init(int logfd, unsigned logfile_seq) {

    return 0;
}

static size_t unisoc_exx00u_logfile_save(int logfd, const void *buf, size_t size) {
    if (size <= 0 || NULL == buf || logfd <= 0)
        return size;

    return qlog_logfile_save(logfd, buf, size);
}

qlog_ops_t unisoc_exx00u_qlog_ops = {
    .init_filter = unisoc_exx00u_init_filter,
    .clean_filter = unisoc_exx00u_clean_filter,
    .logfile_init = unisoc_exx00u_logfile_init,
    .logfile_save = unisoc_exx00u_logfile_save,
};
