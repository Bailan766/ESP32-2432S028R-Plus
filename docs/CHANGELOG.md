# Bailan MP3 Player 版本变更日志 (CHANGELOG.md)

所有重大变更、架构更新与阶段成果均在此记录。

---

## [v0.1.0] - 2026-10-06 (阶段一交付: 硬件全通与底层架构奠基)

### 🚀 新增功能
- **全硬件驱动与模块化分层**:
  - `src/hardware/`: 完成 LCD (ST7789)、Touch (XPT2046 独立软 SPI)、Audio (GPIO26)、Amp (GPIO4)、Battery (GPIO34 ADC)、RGB (LEDC PWM)、Buttons (BOOT GPIO0) 封装。
  - `src/storage/`: 增加 `SDCard`、`Scanner`、`Database` 模块，全面支持非阻塞 `DEMO_MODE`，未插入 TF 卡时绝不死锁。
  - `src/player/`: 实现显式播放器有限状态机（`STOPPED`, `LOADING`, `PLAYING`, `PAUSED`, `SEEKING`, `FINISHED`, `ERROR`），支持顺序播放、单曲循环、全部循环、随机播放。
  - `src/system/`: 引入基于 NVS 的持久化配置管理 (`SettingsManager`)、自动息屏节能 (`PowerManager`)、竞赛级运行遥测 (`Diagnostics`)。
  - `src/ui/`: 构建 320x240 大指触电容感触控界面（启动屏、主菜单、播放器、曲库列表、系统设置、硬件自检专页）。
- **音频防爆音时序**:
  - 实现 `GPIO4` 功放使能延迟启动与提前关断时序，彻底解决板载功放休止符底噪与上电 POP 音。
- **竞赛级数据接口**:
  - 串口提供统一标签化调试输出（`[BOOT]`, `[DISPLAY]`, `[TOUCH]`, `[AUDIO]`, `[AMP]`, `[BATTERY]`, `[SD]`, `[PLAYER]`, `[UI]`, `[SYS]`, `[ERROR]`）。
  - 支持实时输出 Free Heap、历史最低水位、运行时间、供电电压与后台任务负载。

### 🛠 优化与修复
- 修复 ST7789 驱动色彩反色问题（强制 `tft.invertDisplay(false)`）。
- 消除 `TFT_WIDTH`/`TFT_HEIGHT` 宏定义重定义冲突与头文件重名问题。
- 优化 XPT2046 电阻式触摸屏 3 次中值滤波与压力门限防抖。

---

## [v0.2.0] - 计划中 (阶段二目标)
- 引入硬件 MP3 逐帧流式解码引擎（基于 Helix MP3 / ESP32 音频解码）。
- 结合 TF 卡实现实际 `.mp3` 文件播放与 ID3 标签（歌名、歌手、专辑）解析。
- 引入 PCM 音量增益衰减算法。

## [v1.0.0] - 最终交付目标
- 完整便携式 MP3 实体机固件发布。
- 预留 TXT 小说/电子书阅读引擎接口 (`ReaderManager`)。
- 3D 打印外壳设计文件与答辩展演报告完整就绪。
