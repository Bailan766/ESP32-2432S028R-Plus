#include "scanner.h"
#include "sdcard.h"
#include "database.h"

static bool s_isDemoMode = true;

// 已收集到的 MP3 路径（去重，最多 MAX_TRACKS 首）
static String s_paths[MAX_TRACKS];
static int s_pathCount = 0;

static bool pathAlreadyCollected(const String& p) {
  for (int i = 0; i < s_pathCount; i++) {
    if (s_paths[i] == p) return true;
  }
  return false;
}

static void collectDir(File dir, int depth) {
  if (depth > 3) return; // 限制递归深度最多 3 层，保护堆栈

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;   // 目录读完，或因读卡异常提前返回

    String name = entry.name();
    // 忽略隐藏文件与系统临时文件 (以 . 开头)
    if (name.startsWith(".") || name.startsWith("/.")) {
      entry.close();
      continue;
    }

    if (entry.isDirectory()) {
      collectDir(entry, depth + 1);
    } else {
      String lowerName = name;
      lowerName.toLowerCase();
      if (lowerName.endsWith(".mp3")) {
        if (s_pathCount < MAX_TRACKS) {
          String p = entry.path();
          if (!pathAlreadyCollected(p)) {
            s_paths[s_pathCount++] = p;
          }
        }
      }
    }
    entry.close();
  }
}

static void collectFrom(const char* path) {
  File dir = SD.open(path);
  if (!dir) return;
  collectDir(dir, 0);
  dir.close();
}

void scanner_init() {
  scanner_scan_all();
}

void scanner_scan_all() {
  db_clear();
  s_pathCount = 0;

  if (sdcard_is_available()) {
    s_isDemoMode = false;

    // 优先扫描 /Music 目录，不存在则退回根目录
    File probe = SD.open("/Music");
    bool hasMusicDir = (probe && probe.isDirectory());
    if (probe) probe.close();

    const char* scanPath = hasMusicDir ? "/Music" : "/";
    if (hasMusicDir) {
      LOG_SD("开始扫描 TF 卡 /Music 目录 (最多 %d 首)...", MAX_TRACKS);
    } else {
      LOG_SD("/Music 目录不存在，扫描根目录 (最多 %d 首)...", MAX_TRACKS);
    }

    // 大容量卡在 CYD 卡槽上可能出现"目录读到一半就中断"的情况。
    // 多轮收集：每轮把新发现的曲目补进列表，直到不再有新增为止。
    for (int attempt = 0; attempt < 3; attempt++) {
      int before = s_pathCount;
      collectFrom(scanPath);
      LOG_SD("第 %d 轮扫描: 新增 %d 首，累计 %d 首",
             attempt + 1, s_pathCount - before, s_pathCount);

      if (s_pathCount >= MAX_TRACKS) break;
      if (attempt > 0 && s_pathCount == before) break; // 本轮无进展，不再重试
    }

    for (int i = 0; i < s_pathCount; i++) {
      String name = s_paths[i];
      int slashIdx = name.lastIndexOf('/');
      String baseName = (slashIdx >= 0) ? name.substring(slashIdx + 1) : name;
      int dotIdx = baseName.lastIndexOf('.');
      String title = (dotIdx >= 0) ? baseName.substring(0, dotIdx) : baseName;

      db_add_track(title.c_str(), "TF Card", name.c_str(), 180, false);
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
