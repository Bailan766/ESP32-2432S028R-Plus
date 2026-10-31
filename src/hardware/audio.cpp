#include "audio.h"
#include "amplifier.h"
#include "config.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutput.h>
#include <driver/dac.h>
#include "../demo_music.h"

// ==============================================================================
// 音频输出驱动：直接按采样率写 DAC / PWM，不使用 I2S
//
// 为什么不用 ESP8266Audio 自带的 AudioOutputI2S(INTERNAL_DAC)：
//   1) 该实现把 DAC 模式的 sample_rate 硬编码为 44100，与 8kHz 演示 WAV 不匹配，
//      实测表现为"既不出声也不推进"；
//   2) 其 i2s_set_dac_mode(BOTH_EN) 会同时占用 GPIO25/GPIO26，
//      而 GPIO25 正是 XPT2046 触摸屏软件 SPI 的时钟线，会导致播放时触控失灵。
//
// 输出通道由 config.h 的 AUDIO_OUTPUT_USE_PWM 选择：
//   0 -> 内部 DAC2 (GPIO26)，用于板载功放
//   1 -> LEDC PWM (GPIO27)，用于外接 PAM8302 等模拟功放模块
// ==============================================================================

static void audioOutInit() {
#if AUDIO_OUTPUT_USE_PWM
  ledcSetup(AUDIO_PWM_CHANNEL, AUDIO_PWM_CARRIER, AUDIO_PWM_RES);
  ledcAttachPin(AUDIO_PWM_PIN, AUDIO_PWM_CHANNEL);
  ledcWrite(AUDIO_PWM_CHANNEL, 128);   // 50% 占空比 = 中点偏置
  // 外接功放模式下，板载功放输入(GPIO26)仍悬空会持续自激嗡叫；
  // 顺带把 DAC2 也钳在中点，让板载功放保持安静。
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);
#else
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);
#endif
}

static inline void audioWriteSample(uint8_t v) {
#if AUDIO_OUTPUT_USE_PWM
  ledcWrite(AUDIO_PWM_CHANNEL, v);
#else
  dacWrite(AUDIO_DAC_PIN, v);
#endif
}

class AudioOutputDirect : public AudioOutput {
public:
  AudioOutputDirect() {
    hertz = 8000;
    bps = 16;
    channels = 1;
    gainF2P6 = 64;
  }

  bool begin() override {
    audioOutInit();
    s_nextUs = micros();
    s_budget = 0;
    return true;
  }

  bool ConsumeSample(int16_t sample[2]) override {
    // 每处理一小批就让出一次 CPU，避免生成器的 loop() 长时间不返回、
    // 导致主循环（触摸/按键/UI）被完全饿死。
    if (s_budget >= kBudget) {
      s_budget = 0;
      return false;
    }
    s_budget++;

    MakeSampleStereo16(sample);          // 单声道/8bit 统一转成 16bit 有符号
    int16_t v = Amplify(sample[LEFTCHANNEL]);

    // 按采样率节流到下一个采样点
    const uint32_t periodUs = 1000000UL / hertz;
    while ((int32_t)(micros() - s_nextUs) < 0) { /* busy wait */ }
    s_nextUs += periodUs;

    audioWriteSample((uint8_t)((v >> 8) + 128));  // 16bit -> 8bit 无符号
    return true;
  }

  bool stop() override {
    audioWriteSample(128);
    return true;
  }

private:
  static const uint8_t kBudget = 48;
  uint32_t s_nextUs = 0;
  uint8_t s_budget = 0;
};

static uint8_t s_volume = 70; // 默认 70%

static AudioFileSourceSD *s_file = nullptr;
static AudioFileSourcePROGMEM *s_fileProgmem = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorWAV *s_wav = nullptr;
static AudioOutputDirect *s_out = nullptr;

static bool s_isPlayingAudio = false;
static bool s_isWav = false;

// 待机中点偏置：功放为常开状态，若音频脚处于高阻/悬空，功放输入拾噪会自激，
// 表现为贴近喇叭可听到的低频嗡声。把输出稳定在中点可钳住输入电平。
static void audio_hold_idle_bias() {
  if (s_isPlayingAudio) return;
  audioWriteSample(128);
#if AUDIO_OUTPUT_USE_PWM
  // 板载功放常开且输入悬空，持续钳住 GPIO26 让它保持安静
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);
#endif
}

void audio_init() {
#if AUDIO_OUTPUT_USE_PWM
  LOG_AUDIO("初始化音频子系统 (LEDC PWM -> GPIO%d, %d kHz 载波, 外接功放模式)...",
            AUDIO_PWM_PIN, AUDIO_PWM_CARRIER / 1000);
#else
  LOG_AUDIO("初始化音频子系统 (内部 DAC2 -> GPIO%d / CN1 音频接口, 外接功放模式)...", AUDIO_DAC_PIN);
#endif

  amp_init();

  s_out = new AudioOutputDirect();
  s_mp3 = new AudioGeneratorMP3();
  s_wav = new AudioGeneratorWAV();
  s_file = new AudioFileSourceSD();
  s_fileProgmem = new AudioFileSourcePROGMEM(demo_wav, demo_wav_len);

  audioOutInit();
  audio_set_volume(s_volume);
  audio_hold_idle_bias();
  LOG_AUDIO("音频输出已钳位于中点偏置，待机静音");
}

void audio_set_volume(uint8_t volume_percent) {
  s_volume = constrain(volume_percent, 0, 100);
  LOG_AUDIO("音量全局设置: %d%%", s_volume);
  if (s_out) {
    s_out->SetGain(s_volume / 100.0f);
  }
}

uint8_t audio_get_volume() {
  return s_volume;
}

bool audio_play_mp3(const char* filepath) {
  audio_stop();
  if (!s_file->open(filepath)) {
    LOG_AUDIO("无法打开MP3文件: %s", filepath);
    return false;
  }

  amp_enable();
  if (!s_mp3->begin(s_file, s_out)) {
    LOG_AUDIO("MP3解码器启动失败: %s", filepath);
    s_file->close();
    amp_disable();
    s_isPlayingAudio = false;
    audio_hold_idle_bias();
    return false;
  }
  s_isPlayingAudio = true;
  s_isWav = false;
  LOG_AUDIO("开始播放MP3: %s", filepath);
  return true;
}

bool audio_play_demo_wav() {
  audio_stop();
  s_fileProgmem->close();
  s_fileProgmem->open(demo_wav, demo_wav_len);

  amp_enable();
  if (!s_wav->begin(s_fileProgmem, s_out)) {
    LOG_AUDIO("Demo WAV解码器启动失败");
    s_fileProgmem->close();
    amp_disable();
    s_isPlayingAudio = false;
    audio_hold_idle_bias();
    return false;
  }
  s_isPlayingAudio = true;
  s_isWav = true;
  LOG_AUDIO("开始播放内部闪存 Demo WAV (size=%u, pos=%u)",
            s_fileProgmem->getSize(), s_fileProgmem->getPos());
  return true;
}

void audio_pause() {
  if (s_isPlayingAudio) {
    s_out->stop();
    amp_disable();
  }
  s_isPlayingAudio = false;
  audio_hold_idle_bias();
}

void audio_resume() {
  if (!s_isPlayingAudio) {
    amp_enable();
    s_out->begin();
    s_isPlayingAudio = true;
  }
}

void audio_stop() {
  if (s_mp3 && s_mp3->isRunning()) s_mp3->stop();
  if (s_wav && s_wav->isRunning()) s_wav->stop();
  if (s_file && s_file->isOpen()) s_file->close();
  if (s_fileProgmem && s_fileProgmem->isOpen()) s_fileProgmem->close();

  amp_disable();
  s_isPlayingAudio = false;
  audio_hold_idle_bias();
}

// 硬件自检蜂鸣：走与播放相同的输出通道。
//   1) 播放中直接返回，避免抢占音频输出；
//   2) 短于 60ms 的 UI 点击反馈音直接跳过——反复开关功放反而会引入 POP 爆音。
void audio_test_beep(uint16_t freq, uint16_t duration_ms) {
  if (s_isPlayingAudio) return;
  if (duration_ms < 60) return;
  if (freq < 100 || freq > 8000) return;

  amp_enable();
  audioOutInit();
  delay(10);

  const uint32_t halfPeriodUs = 500000UL / freq;
  const unsigned long start = millis();
  while (millis() - start < duration_ms) {
    audioWriteSample(210);
    delayMicroseconds(halfPeriodUs);
    audioWriteSample(45);
    delayMicroseconds(halfPeriodUs);
  }

  amp_disable();
  audio_hold_idle_bias();
}

void audio_update() {
  if (!s_isPlayingAudio) return;

  if (s_isWav) {
    if (s_wav && s_wav->isRunning()) {
      if (!s_wav->loop()) {
        LOG_AUDIO("WAV 播放结束 (pos=%u / size=%u)",
                  s_fileProgmem->getPos(), s_fileProgmem->getSize());
        s_wav->stop();
        amp_disable();
        s_isPlayingAudio = false;
        audio_hold_idle_bias();
      }
    }
  } else {
    if (s_mp3 && s_mp3->isRunning()) {
      if (!s_mp3->loop()) {
        s_mp3->stop();
        s_file->close();
        amp_disable();
        s_isPlayingAudio = false;
        audio_hold_idle_bias();
        LOG_AUDIO("MP3 播放结束");
      }
    }
  }
}

bool audio_is_playing() {
  return s_isPlayingAudio;
}

uint32_t audio_get_mp3_position() {
  return 0;
}

uint32_t audio_get_mp3_duration() {
  return 0;
}
