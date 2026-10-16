#include "sdcard.h"

static SDCardState s_sdState = SD_STATE_UNCHECKED;
static uint32_t s_sdSizeMB = 0;

void sdcard_init() {
  LOG_SD("初始化 HSPI 总线并检测 TF 卡 (SCK=%d, MISO=%d, MOSI=%d, CS=%d)...",
         SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // 初始化 SPI 接口
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // 尝试挂载 SD 卡
  if (SD.begin(SD_CS_PIN, SPI, SD_SPI_SPEED)) {
    s_sdState = SD_STATE_AVAILABLE;
    s_sdSizeMB = (uint32_t)(SD.cardSize() / (1024 * 1024));
    uint8_t cardType = SD.cardType();
    const char* typeStr = "Unknown";
    if (cardType == CARD_MMC) typeStr = "MMC";
    else if (cardType == CARD_SD) typeStr = "SDSC";
    else if (cardType == CARD_SDHC) typeStr = "SDHC";

    LOG_SD("TF 卡挂载成功! 类型: %s, 容量: %u MB", typeStr, s_sdSizeMB);
  } else {
    s_sdState = SD_STATE_NOT_FOUND;
    s_sdSizeMB = 0;
    LOG_SD("未检测到 TF 卡 (系统平滑切入 DEMO 演示与单板开发模式，不阻塞系统运行)");
  }
}

bool sdcard_reinit() {
  SD.end();
  delay(20);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (SD.begin(SD_CS_PIN, SPI, SD_SPI_SPEED)) {
    s_sdState = SD_STATE_AVAILABLE;
    s_sdSizeMB = (uint32_t)(SD.cardSize() / (1024 * 1024));
    LOG_SD("TF 卡重新挂载成功! 容量: %u MB", s_sdSizeMB);
    return true;
  } else {
    s_sdState = SD_STATE_NOT_FOUND;
    s_sdSizeMB = 0;
    LOG_SD("重新检测: 未检测到 TF 卡");
    return false;
  }
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
