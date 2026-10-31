# CYD Plus 网页一键刷机工具包 (Web Flasher)

本目录为 **ESP32-2432S028R Plus 摆烂播放器** 的免环境网页一键刷机包，基于 Web Serial API 构建。

---

## 🛠 使用方式

### 方式 1: 本地双击刷机（推荐）
在项目根目录下，直接双击运行：
👉 **`本地网页刷机.bat`** (或 `flash_web.bat`)

脚本会自动启动微型本地 Web 服务并为您打开 Chrome / Edge 浏览器，点击页面的 **“⚡ 开始连接并刷入”** 按钮即可一键完成烧录。

### 方式 2: 部署到 GitHub Pages 在线刷机
1. 将 `web_flasher` 目录下的所有文件上传至 GitHub 仓库（例如分支 `gh-pages` 或根目录）；
2. 在仓库的 **Settings -> Pages** 中开启 GitHub Pages；
3. 任何玩家直接在浏览器中打开网址（如 `https://<用户名>.github.io/<仓库名>/`），插上板子即可免配环境在线刷机！

---

## 📦 固件说明

- **`firmware_full.bin` (0x0)**: 全量固件镜像（内含 Bootloader + Partitions 分区表 + BootApp0 + v1.1.4 应用程序）。适合全新刷机或疑难故障急救。
- **`firmware.bin` (0x10000)**: 应用程序增量固件。仅更新系统程序，自动保留机器原有的 Wi-Fi 配置与触控校准参数。
