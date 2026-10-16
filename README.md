# Bailan MP3 Player (摆烂 MP3 播放器)

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32--2432S028R--Plus-blue.svg)](docs/GPIO.md)
[![Status](https://img.shields.io/badge/Version-v0.1.0-green.svg)](docs/CHANGELOG.md)

基于 **ESP32-2432S028R Plus** 一体化电阻触屏开发板（CYD Plus）打造的开源、模块化、高可靠嵌入式便携式 MP3 播放器，兼具科技创新与智能硬件竞赛展示设计标准。

---

## 📌 核心特性

- **全硬件驱动打通**: 屏幕 (ST7789 320x240 横屏)、电阻触摸 (XPT2046 独立软 SPI)、音频输出 (GPIO26)、功放使能 (GPIO4)、RGB 灯 (PWM)、电池检测 (GPIO34 ADC)、物理按键 (BOOT)。
- **硬件级静音与消底噪**: 通过 `AMP_ENABLE (GPIO4)` 严格配合音频信号建立延时，彻底根除板载功放（NS4168）的休止符白噪声、底噪及开关爆音（Anti-POP）。
- **零 TF 卡优雅开机 (DEMO MODE)**: 在未插卡情况下不卡顿、不死循环，自动切换至内置 5 首芯片旋律与内置测试小说《摆烂修仙传》，UI、触摸与动效全功能正常运转。
- **全功能 TXT 小说阅读器**:
  - **7,445 字全量点阵中文字库**: 内置 GB2312 常用汉字、全角标点，支持 UTF-8 与 GBK/GB2312 编码自动识别，杜绝乱码。
  - **大文件流式分页**: 毫秒级按需加载，支持 10MB+ 大长篇小说，64 级翻页栈回退，NVS 自动书签记忆。
  - **4 款护眼配色**: 暗黑水墨、羊皮纸卷、豆沙护眼、深夜星河一键换肤。
  - **摸鱼神器 BGM 伴读**: 阅读时可边看书边听卡内音乐，支持悬浮面板一键快进、跳转章节与背光调光。
- **模块化工程解耦**: 代码按 `hardware/`、`storage/`、`player/`、`reader/`、`system/`、`ui/` 分层，逻辑清晰，易于二次开发。
- **竞赛级遥测诊断 (SystemDiagnostics)**: 统一串口格式输出（波特率 115200），实时监控 Free Heap、历史最低水位、运行时间、供电电压及播放器核心状态机。

---

## 📂 项目结构全景

```text
BailanMP3/
├── platformio.ini              # PlatformIO 项目与编译配置文件
├── README.md                   # 本文件 (项目指南与使用说明)
│
├── include/
│   ├── config.h                # 集中式硬件引脚分配与全局常量定义
│   ├── songs.h                 # 内置 8-bit 复古旋律乐谱数据
│   └── User_Setup.h            # TFT_eSPI 驱动层引脚配置
│
├── src/
│   ├── main.cpp                # 系统主启动入口与非阻塞事件循环
│   │
│   ├── hardware/               # 硬件抽象与外设驱动层
│   │   ├── display.h / .cpp    # ST7789 LCD 初始化与背光 PWM 调光
│   │   ├── touch.h / .cpp      # XPT2046 独立软件 SPI 读取与中值滤波
│   │   ├── audio.h / .cpp      # GPIO26 音频 PWM/Tone 输出与测试音控制
│   │   ├── amplifier.h / .cpp  # GPIO4 功放使能、静音与防爆音时序
│   │   ├── battery.h / .cpp    # GPIO34 ADC 电压采样与平滑滤波
│   │   ├── rgb.h / .cpp        # RGB LED 状态律动与呼吸灯效
│   │   └── buttons.h / .cpp    # BOOT 物理按键单击/长按消抖处理
│   │
│   ├── storage/                # 存储与文件管理层
│   │   ├── sdcard.h / .cpp     # HSPI TF 卡挂载与状态探测 (非阻塞)
│   │   ├── scanner.h / .cpp    # 曲库目录扫描与 DEMO 模式后备
│   │   └── database.h / .cpp   # 内存曲目清单与 LRU 最近播放管理
│   │
│   ├── player/                 # 播放控制核心层
│   │   ├── playback_state.h/.cpp # 7态显式有限状态机 (FSM)
│   │   ├── playlist.h / .cpp   # 播放序列控制 (顺序/单曲/循环/随机)
│   │   └── player.h / .cpp     # 播放引擎、时间戳统计与旋律步进器
│   │
│   ├── reader/                 # 小说阅读与排版引擎
│   │   ├── novel.h / .cpp      # TXT 流式解析、GBK/UTF-8 识别、NVS 书签记忆
│   │   └── demo_novel.h        # 内置离线演示小说数据
│   │
│   ├── system/                 # 系统级支持服务
│   │   ├── settings_manager.h/.cpp # NVS 持久化配置 (防高频擦写保护)
│   │   ├── power_manager.h/.cpp    # 自动息屏休眠与轻触唤醒调度
│   │   └── diagnostics.h / .cpp    # 竞赛级实时系统健康遥测输出
│   │
│   └── ui/                     # 用户交互与大指触界面
│       ├── ui.h / .cpp         # 页面路由分发与公用顶栏绘制
│       ├── home.h / .cpp       # 6 大功能卡片主菜单 (音乐/小说/曲库/历史/设置/校准)
│       ├── font_cn.h / .cpp    # 7445 全量汉字点阵字库与 GB2312 映射表
│       ├── novel_list.h / .cpp # 小说书架分页浏览界面
│       ├── novel_reader_ui.h/.cpp # 沉浸式阅读界面与悬浮控制面板
│       ├── player_ui.h / .cpp  # 播放器主界面 (16段动态跳动频谱、进度条)
│       ├── music_list.h / .cpp # 曲库分页浏览器 (每页 3 首大条目)
│       ├── settings.h / .cpp   # 系统设置界面 (音量、亮度、息屏、自检入口)
│       ├── touch_cal_ui.h/.cpp # 5 点精准触屏校准界面
│       └── hardware_test_ui.h/.cpp # 专用硬件自检交互页面
│
└── docs/                       # 竞赛级工程文档与开发记录
    ├── GPIO.md                 # 硬件引脚分配、外设分析与电路避坑说明
    ├── ARCHITECTURE.md         # 软件分层架构、状态机设计与内存预算
    ├── TEST_PLAN.md            # 硬件功能逐项测试与验收步骤
    ├── CHANGELOG.md            # 版本演进记录
    └── 硬件自检说明.md         # 硬件自检与外设测试说明
```

---

## 🛠 编译与烧录指南

### 步骤 1: 环境准备
1. 推荐在 VS Code 中安装 **PlatformIO IDE** 插件；
2. 如果使用命令行，确认电脑已安装 PlatformIO Core (`pio`)。

### 步骤 2: 打开项目
在 VS Code 中点击 `File -> Open Folder`，打开本工程根目录：
`c:\Users\Administrator\Desktop\ESP32-2432S028R-Plus`

### 步骤 3: 编译项目
在 VS Code 底部状态栏点击 `√` (Build) 按钮，或在终端执行：
```bash
pio run
```
> **实测编译占用**:
> - RAM: 27,148 字节 (8.3%)，剩余 >290KB 可用堆内存。
> - Flash: 413,933 字节 (31.6%)。

### 步骤 4: 烧录固件
> **⚠️ 烧录前关键注意事项**:
> 若开发板外接了 GPS 模块（GT-U7），**必须拔掉接在 GPIO1 的连线**，因 GPIO1 与板载 CH340 芯片的 USB TX 共用，插上会导致串口烧录握手失败。

通过 USB 数据线将 ESP32-2432S028R Plus 连入电脑，确认设备管理器中的端口（当前识别为 `COM9`）：
```bash
pio run --target upload --upload-port COM9
```

### 步骤 5: 开启串口调试监视器
```bash
pio device monitor -b 115200
```
开机将观察到统一标签化的日志输出：
```text
[BOOT] ==================================================
[BOOT]   Bailan MP3 Player v0.1.0 启动中...
[BOOT]   硬件目标: ESP32-2432S028R-Plus
[BOOT] ==================================================
[SYS] 正在从 NVS 读取持久化配置...
[DISPLAY] 初始化 ST7789 LCD (320x240, 横屏模式)...
[TOUCH] 初始化 XPT2046 独立软件 SPI 触摸驱动...
[SD] 未检测到 TF 卡 (系统平滑切入 DEMO 演示与单板开发模式，不阻塞系统运行)
[SYS] Uptime: 10s | Heap: 201400B (Min:195000B) | Batt: 4.18V | TF: 未检测到 | Player: STOPPED
```

---

## 🧪 硬件测试步骤 (无 TF 卡直接测)

进入屏幕操作：
1. **主界面**: 点击 `Music`，立即进入播放器界面，屏幕动态渲染 16 频段音频频谱，板载扬声器播放内置芯片旋律，RGB 灯伴随旋律律动；
2. **触控测试**: 点击屏幕下方 `||` 暂停、`>` 播放、`|<` 上一首、`>|` 下一首、`ALL/ONE/RND` 循环模式切换；
3. **实体按键测试**: 短按板载 `BOOT` 键可播放/暂停，长按 `BOOT` 键可切歌；
4. **硬件自检专用页面**:
   - 返回主页，点击 `Settings` -> `硬件自检 Test`；
   - 观察 `LCD [OK]`、`TF Card NOT FOUND`、`电池实时电压/ADC`；
   - 点击 `[测音频 1s]`：扬声器播放 1 秒纯净蜂鸣测试音后自动关断；
   - 点击 `[切换色彩]`：RGB 灯在 红 -> 绿 -> 蓝 -> 白 -> 灭 循环轮播；
   - 按下板载 `BOOT` 键：屏幕即时显示 `[PRESSED 按下]`；
   - 点击屏幕任意角落：下方实时显示当前点击像素 `XY:(xxx, yyy)` 与压力读数。

---

## 📚 详细设计文档参考

- [docs/GPIO.md](docs/GPIO.md): 详细引脚定义、电路总线分配与防冲突规范。
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): 软件系统分层、状态机迁移与内存预算。
- [docs/TEST_PLAN.md](docs/TEST_PLAN.md): 验收测试用例与故障排查清单。
- [docs/CHANGELOG.md](docs/CHANGELOG.md): 版本变更历史与后续阶段路线图。
