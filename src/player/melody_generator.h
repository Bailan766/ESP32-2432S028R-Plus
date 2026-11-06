#pragma once
#include <Arduino.h>
#include <AudioGenerator.h>
#include <AudioOutput.h>
#include "songs.h"

// ==============================================================================
// 高保真内置旋律合成器 (AudioGeneratorMelody)
// 将 include/songs.h 中的单音曲谱实时合成为 44.1kHz 16-bit PCM CD级立体声音频流，
// 支持 ADSR 包络平滑与二次/三次谐波温润音色，彻底告别粗劣蜂鸣！
// ==============================================================================
class AudioGeneratorMelody : public AudioGenerator {
public:
  AudioGeneratorMelody();
  virtual ~AudioGeneratorMelody();

  bool begin(const Note* notes, uint16_t count, AudioOutput* out, bool loop = true);
  virtual bool loop() override;
  virtual bool stop() override;
  virtual bool isRunning() override;

  uint16_t getCurrentFreq() const { return m_currentFreq; }
  uint32_t getElapsedSec() const { return (uint32_t)(m_elapsedSamples / 22050ULL); }

private:
  const Note* m_notes;
  uint16_t m_noteCount;
  uint16_t m_noteIdx;
  bool m_shouldLoop;

  uint32_t m_sampleInNote;
  uint32_t m_noteTotalSamples;
  uint64_t m_elapsedSamples;

  float m_phase;
  float m_phaseStep;
  uint16_t m_currentFreq;
};
