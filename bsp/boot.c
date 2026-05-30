#include <stdint.h>
/* i.MX RT1062 boot structures: FlexSPI config block (FCB), boot data, and the
   image vector table (IVT) the boot ROM reads to find the application. */

extern uint32_t _flash_image_len;
extern void Reset_Handler(void);

typedef struct __attribute__((packed)) {
  uint32_t tag;
  uint32_t version;
  uint32_t reserved01;
  uint8_t  readSampleClkSrc;
  uint8_t  csHoldTime;
  uint8_t  csSetupTime;
  uint8_t  columnAddressWidth;
  uint8_t  deviceModeCfgEnable;
  uint8_t  reserved02;
  uint16_t waitTimeCfgCommands;
  uint32_t deviceModeSeq;
  uint32_t deviceModeArg;
  uint8_t  configCmdEnable;
  uint8_t  reserved03[3];
  uint32_t configCmdSeqs[3];
  uint32_t reserved04;
  uint32_t cfgCmdArgs[3];
  uint32_t reserved05;
  uint32_t controllerMiscOption;
  uint8_t  deviceType;
  uint8_t  sflashPadType;
  uint8_t  serialClkFreq;
  uint8_t  lutCustomSeqenable;
  uint32_t reserved06[2];
  uint32_t sflashA1Size;
  uint32_t sflashA2Size;
  uint32_t sflashB1Size;
  uint32_t sflashB2Size;
  uint32_t csPadSettingOverride;
  uint32_t scklPadSettingOverride;
  uint32_t dataPadSettingOverride;
  uint32_t dqsPadSettingOverride;
  uint32_t timeoutInMs;
  uint32_t commandInterval;
  uint32_t dataValidTime;
  uint16_t busyOffset;
  uint16_t busyBitPolarity;
  uint32_t lookupTable[64];
  uint32_t lutCustomSeq[12];
  uint32_t reserved07[4];
  uint32_t pageSize;
  uint32_t sectorSize;
  uint8_t  ipCmdSerialClkFreq;
  uint8_t  reserved08[55];
} flexspi_nor_config_t;

_Static_assert(sizeof(flexspi_nor_config_t) == 512, "fcb has to be 512 bytes");

#define INSTR(opc, n_pads, opr) ((opc << 10) | (n_pads << 8) | (opr))
#define SEQ(instr0, instr1)     (instr0 | (instr1 << 16))
#define CMD_SDR   0x01
#define RADDR_SDR 0x02
#define MODE8_SDR 0x07
#define READ_SDR  0x09
#define DUMMY_SDR 0x0C
#define STOP      0x00
#define PAD01     0x00
#define PAD04     0x02

__attribute__((section(".flash_config"), used))
const flexspi_nor_config_t fcb = {
  .tag = 0x42464346,
  .version = 0x56010000,
  .readSampleClkSrc = 1,
  .csHoldTime = 0x03,
  .csSetupTime = 0x03,
  .deviceType = 1,
  .sflashPadType = 4,
  .serialClkFreq = 6,
  .sflashA1Size = 0x00800000,
  .lookupTable = {
    SEQ(INSTR(CMD_SDR, PAD01, 0xEB), INSTR(RADDR_SDR, PAD04, 24)),
    SEQ(INSTR(MODE8_SDR, PAD04, 0xFF), INSTR(DUMMY_SDR, PAD04, 4)),
    SEQ(INSTR(READ_SDR, PAD04, 0x04), INSTR(STOP, 0, 0)),
    0,
  },
  .pageSize = 256,
  .sectorSize = 4096,
  .ipCmdSerialClkFreq = 1,
};

__attribute__((section(".boot_data"), used))
const uint32_t boot_data[3] = {
  0x60000000,
  (uint32_t)&_flash_image_len,
  0,
};

__attribute__((section(".ivt"), used))
const uint32_t image_vector_table[8] = {
  0x402000D1,
  (uint32_t)&Reset_Handler,
  0,
  0,
  (uint32_t)boot_data,
  0x60001000,
  0,
  0,
};
