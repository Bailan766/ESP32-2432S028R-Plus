#include "melody_generator.h"
#include <math.h>

AudioGeneratorMelody::AudioGeneratorMelody() {
  m_notes = nullptr;
  m_noteCount = 0;
  m_noteIdx = 0;
  m_shouldLoop = true;
  m_sampleInNote = 0;
  m_noteTotalSamples = 0;
  m_elapsedSamples = 0;
  m_phase = 0.0f;
  m_phaseStep = 0.0f;
  m_currentFreq = 0;
  running = false;
  output = nullptr;
}

AudioGeneratorMelody::~AudioGeneratorMelody() {
  stop();
}

bool AudioGeneratorMelody::begin(const Note* notes, uint16_t count, AudioOutput* out, bool loop) {
  if (!notes || count == 0 || !out) return false;

  stop();

  m_notes = notes;
  m_noteCount = count;
  m_noteIdx = 0;
  m_shouldLoop = loop;
  m_sampleInNote = 0;
  m_elapsedSamples = 0;
  m_phase = 0.0f;
  m_phaseStep = 0.0f;
  m_currentFreq = 0;

  this->output = out;
  if (!output->SetRate(44100)) return false;
  if (!output->SetBitsPerSample(16)) return false;
  if (!output->SetChannels(2)) return false;
  if (!output->begin()) return false;

  running = true;
  return true;
}

bool AudioGeneratorMelody::stop() {
  running = false;
  m_currentFreq = 0;
  m_phase = 0.0f;
  m_sampleInNote = 0;
  m_noteIdx = 0;
  return true;
}

bool AudioGeneratorMelody::isRunning() {
  return running;
}

bool AudioGeneratorMelody::loop() {
  if (!running || !output) return false;

  int batch = 0;
  while (batch < 64) {
    if (m_noteIdx >= m_noteCount) {
      if (m_shouldLoop) {
        m_noteIdx = 0;
        m_sampleInNote = 0;
      } else {
        stop();
        return false;
      }
    }

    // 从 Flash PROGMEM 读取当前音符
    Note curNote;
    memcpy_P(&curNote, &m_notes[m_noteIdx], sizeof(Note));
    m_currentFreq = curNote.freq;

    // 44.1kHz 下当前音符所对应的总采样数
    m_noteTotalSamples = ((uint32_t)curNote.duration * 44100UL) / 1000UL;
    if (m_noteTotalSamples == 0) m_noteTotalSamples = 1;

    int16_t sampleVal = 0;
    if (curNote.freq > 0) {
      // 尾部预留 6ms 连音断奏微缝隙，防止同音连续粘连并消除爆音
      uint32_t gap = (m_noteTotalSamples > 300) ? 265 : (m_noteTotalSamples / 5);
      if (m_sampleInNote < (m_noteTotalSamples - gap)) {
        uint32_t activeLen = m_noteTotalSamples - gap;
        // 起音 6ms 平滑淡入
        uint32_t attack = (activeLen > 400) ? 265 : (activeLen / 4);
        // 释音 6ms 平滑淡出
        uint32_t release = (activeLen > 400) ? 265 : (activeLen / 4);

        float env = 1.0f;
        if (m_sampleInNote < attack) {
          env = (float)m_sampleInNote / (float)attack;
        } else if (m_sampleInNote > (activeLen - release)) {
          env = (float)(activeLen - m_sampleInNote) / (float)release;
        } else {
          // 仿电钢琴/八音盒自然声学指数微衰减
          float decayProgress = (float)(m_sampleInNote - attack) / (float)(activeLen - attack - release + 1);
          env = 1.0f - 0.25f * decayProgress;
        }

        m_phaseStep = (2.0f * (float)M_PI * (float)curNote.freq) / 44100.0f;
        // 谐波拟合音色：75% 基波正弦 + 20% 二次谐波 + 5% 三次谐波
        float wave = sinf(m_phase) + 0.25f * sinf(2.0f * m_phase) + 0.08f * sinf(3.0f * m_phase);
        wave *= 0.75f;

        sampleVal = (int16_t)(wave * env * 14000.0f);

        m_phase += m_phaseStep;
        if (m_phase >= 2.0f * (float)M_PI) m_phase -= 2.0f * (float)M_PI;
      } else {
        sampleVal = 0;
      }
    } else {
      sampleVal = 0;
      m_phase = 0.0f;
    }

    int16_t s[2] = { sampleVal, sampleVal };
    if (!output->ConsumeSample(s)) {
      // DMA 缓冲区满，让出 CPU
      return true;
    }

    m_sampleInNote++;
    m_elapsedSamples++;
    batch++;

    if (m_sampleInNote >= m_noteTotalSamples) {
      m_sampleInNote = 0;
      m_noteIdx++;
    }
  }

  return true;
}
