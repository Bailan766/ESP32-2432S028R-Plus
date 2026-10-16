#include "scanner.h"
#include "sdcard.h"
#include "database.h"

static bool s_isDemoMode = true;

static void scanDirectory(File dir, int depth) {
  if (depth > 3) return; // 限制递归深度最多 3 层，保护堆栈

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    String name = entry.name();
    // 忽略隐藏文件与系统临时文件 (以 . 开头)
    if (name.startsWith(".") || name.startsWith("/.")) {
      entry.close();
      continue;
    }

    if (entry.isDirectory()) {
      scanDirectory(entry, depth + 1);
    } else {
      String lowerName = name;
      lowerName.toLowerCase();
      if (lowerName.endsWith(".mp3")) {
        // 截取文件名作为曲目名
        int slashIdx = name.lastIndexOf('/');
        String baseName = (slashIdx >= 0) ? name.substring(slashIdx + 1) : name;
        int dotIdx = baseName.lastIndexOf('.');
        String title = (dotIdx >= 0) ? baseName.substring(0, dotIdx) : baseName;

        db_add_track(title.c_str(), "TF Card", entry.path(), 180, false);
        LOG_SD("发现 MP3 音频: %s", entry.path());
      }
    }
    entry.close();
  }
}

void scanner_init() {
  scanner_scan_all();
}

void scanner_scan_all() {
  db_clear();

  if (sdcard_is_available()) {
    s_isDemoMode = false;
    LOG_SD("开始扫描 TF 卡 /Music 目录...");
    File musicDir = SD.open("/Music");
    if (musicDir && musicDir.isDirectory()) {
      scanDirectory(musicDir, 0);
      musicDir.close();
    } else {
      LOG_SD("/Music 目录不存在，尝试根目录扫描...");
      File rootDir = SD.open("/");
      if (rootDir) {
        scanDirectory(rootDir, 0);
        rootDir.close();
      }
    }

    if (db_get_track_count() == 0) {
      LOG_SD("TF 卡中未找到 .mp3 文件，自动注入演示曲目以便交互");
      s_isDemoMode = true;
    }
  } else {
    s_isDemoMode = true;
  }

  // 如果处于 DEMO 模式，注入标准测试歌曲
  if (s_isDemoMode) {
    LOG_SD("注入 DEMO 演示曲目清单 (无卡开发模式)...");
    db_add_track("超级马里奥", "Super Mario", "ROM://mario", 95, true);
    db_add_track("俄罗斯方块", "Tetris Theme", "ROM://tetris", 132, true);
    db_add_track("菊次郎的夏天", "久石让", "ROM://summer", 184, true);
    db_add_track("卡农", "帕赫贝尔", "ROM://canon", 210, true);
    db_add_track("抬棺电音神曲", "Astronomia", "ROM://astronomia", 160, true);
  }

  LOG_SD("曲库就绪，共计 %d 首曲目 (DEMO_MODE: %s)",
         db_get_track_count(), s_isDemoMode ? "YES" : "NO");
}

bool scanner_is_demo_mode() {
  return s_isDemoMode;
}
