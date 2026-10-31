#include "sdcard.h"

static SDCardState s_sdState = SD_STATE_UNCHECKED;
static uint32_t s_sdSizeMB = 0;

// 挂载重试频率阶梯：CYD 板载 TF 卡槽信号完整性一般，
// 大容量卡（SDXC 64GB+）在 4MHz 下更容易出现"这次能挂、下次挂不上"的临界现象。
// 100kHz 放在最后，用于唤醒被软复位打断后卡死的卡片。
static const uint32_t kMountFreqs[] = { 4000000, 1000000, 400000, 100000, 4000000 };
static const int kMountFreqCount = sizeof(kMountFreqs) / sizeof(kMountFreqs[0]);

// 总线恢复序列
// 卡片若在 ESP32 软复位时正处于多块读传输中，会持续等待/驱动数据线，
// 此时必须用【足够长的空时钟】把剩余字节全部移出，它才能回到空闲态接受 CMD0。
// 一个 512 字节块 = 4096 个时钟，这里给到 10000 个（约 100ms）确保收尾干净。
static void sdBusRecover() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  SPI.end();                 // 彻底释放外设，避免软复位后残留状态
  delay(20);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  SPI.setFrequency(100000);  // 100kHz 慢速收尾，兼容性最好
  for (int i = 0; i < 1250; i++) SPI.transfer(0xFF); // 10000 个时钟
  delay(30);
}

static void recordSuccess(uint32_t freqKHz) {
  s_sdState = SD_STATE_AVAILABLE;
  s_sdSizeMB = (uint32_t)(SD.cardSize() / (1024 * 1024));

  uint8_t cardType = SD.cardType();
  const char* typeStr = "Unknown";
  if (cardType == CARD_MMC) typeStr = "MMC";
  else if (cardType == CARD_SD) typeStr = "SDSC";
  else if (cardType == CARD_SDHC) typeStr = "SDHC";

  LOG_SD("TF 卡挂载成功! 类型: %s, 容量: %u MB, 总线: %u kHz",
         typeStr, s_sdSizeMB, (unsigned)freqKHz);
}

static bool mountOnce(uint32_t freq) {
  if (!SD.begin(SD_CS_PIN, SPI, freq)) {
    SD.end();
    return false;
  }
  recordSuccess(freq / 1000);
  return true;
}

// 兜底方案：不传 SPIClass，交给 SD 库自己调用 SPI.begin()（使用 VSPI 默认引脚 18/19/23/5）
static bool mountDefault() {
  if (!SD.begin(SD_CS_PIN)) {
    SD.end();
    return false;
  }
  recordSuccess(0);
  return true;
}

static bool mountWithRetry(const char* tag) {
  // CYD 的 TF 卡槽 3.3V 由板上 LDO 供电，上电斜率较缓；
  // 若在电源尚未稳定时发起 CMD0，卡片会整轮无响应。
  delay(600);

  for (int i = 0; i < kMountFreqCount; i++) {
    sdBusRecover();
    delay(80);

    if (mountOnce(kMountFreqs[i])) return true;

    if (i < kMountFreqCount - 1) {
      LOG_SD("%s: 第 %d/%d 次 (%u kHz) 失败，恢复总线后重试...",
             tag, i + 1, kMountFreqCount, (unsigned)(kMountFreqs[i] / 1000));
      delay(150);
    }
  }

  // 兜底：换用 SD 库自带的默认 SPI 初始化路径
  sdBusRecover();
  delay(80);
  if (mountDefault()) return true;

  s_sdState = SD_STATE_NOT_FOUND;
  s_sdSizeMB = 0;
  return false;
}

void sdcard_init() {
  LOG_SD("初始化 HSPI 总线并检测 TF 卡 (SCK=%d, MISO=%d, MOSI=%d, CS=%d)...",
         SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!mountWithRetry("首次挂载")) {
    LOG_SD("未检测到 TF 卡 (系统平滑切入 DEMO 演示与单板开发模式，不阻塞系统运行)");
    LOG_SD("排查建议: 卡片推到底听到卡嗒声 / 格式化为 FAT32 / 优先使用 <=32GB 卡");
  }
}

bool sdcard_reinit() {
  SD.end();
  delay(80);

  if (mountWithRetry("重新挂载")) return true;

  LOG_SD("重新检测: 未检测到 TF 卡");
  return false;
}

SDCardState sdcard_get_state() {
  return s_sdState;
}

bool sdcard_is_available() {
  return s_sdState == SD_STATE_AVAILABLE;
}

uint32_t sdcard_get_size_mb() {
  return s_sdSizeMB;
}

const char* sdcard_get_state_str() {
  switch (s_sdState) {
    case SD_STATE_AVAILABLE: return "正常挂载";
    case SD_STATE_NOT_FOUND: return "未检测到";
    case SD_STATE_ERROR:     return "读写异常";
    default:                 return "未初始化";
  }
}
