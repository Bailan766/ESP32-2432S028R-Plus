#include "web_file_manager.h"
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>
#include "storage/sdcard.h"
#include "storage/scanner.h"
#include "reader/novel.h"
#include "system/net_manager.h"
#include "config.h"

static WebServer* s_server = nullptr;
static bool s_isRunning = false;
static File s_uploadFile;
static char s_lastAction[64] = "就绪，等待网页连接";
static uint32_t s_uploadCount = 0;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CYD Plus 摆烂播放器 - 空中文件管理</title>
<style>
:root {
  --bg: #0d1117; --card: #161b22; --border: #30363d;
  --text: #c9d1d9; --heading: #f0f6fc; --accent: #58a6ff;
  --green: #3fb950; --red: #f85149;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 16px; line-height: 1.5; }
.container { max-width: 680px; margin: 0 auto; }
header { text-align: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 1px solid var(--border); }
h1 { color: var(--heading); font-size: 20px; font-weight: 600; margin-bottom: 6px; }
.sub { font-size: 13px; color: #8b949e; }
.card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 16px; margin-bottom: 16px; }
.drop-zone { border: 2px dashed var(--accent); border-radius: 8px; padding: 26px 16px; text-align: center; cursor: pointer; transition: 0.2s; background: rgba(88, 166, 255, 0.04); }
.drop-zone:hover { background: rgba(88, 166, 255, 0.1); border-color: #79c0ff; }
.drop-title { font-size: 15px; font-weight: 600; color: var(--heading); margin-bottom: 4px; }
.drop-tip { font-size: 12px; color: #8b949e; }
.tabs { display: flex; gap: 8px; margin-bottom: 12px; }
.tab-btn { flex: 1; padding: 8px 12px; background: #21262d; border: 1px solid var(--border); color: var(--text); border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 500; }
.tab-btn.active { background: #1f6feb; border-color: #388bfd; color: #fff; }
.file-list { list-style: none; max-height: 320px; overflow-y: auto; }
.file-item { display: flex; justify-content: space-between; align-items: center; padding: 10px 12px; border-bottom: 1px solid var(--border); font-size: 13px; }
.file-item:last-child { border-bottom: none; }
.file-name { color: var(--heading); font-weight: 500; word-break: break-all; margin-right: 8px; }
.file-meta { display: flex; align-items: center; gap: 10px; flex-shrink: 0; }
.file-size { color: #8b949e; font-size: 12px; }
.btn-del { background: rgba(248, 81, 73, 0.15); color: var(--red); border: 1px solid rgba(248, 81, 73, 0.4); padding: 3px 8px; border-radius: 4px; cursor: pointer; font-size: 12px; }
.btn-del:hover { background: var(--red); color: #fff; }
.progress-bar { height: 6px; background: #21262d; border-radius: 3px; overflow: hidden; margin-top: 10px; display: none; }
.progress-fill { height: 100%; background: var(--green); width: 0%; transition: width 0.15s; }
.status-msg { font-size: 12px; text-align: center; margin-top: 6px; color: var(--accent); min-height: 18px; }
</style>
</head>
<body>
<div class="container">
  <header>
    <h1>📻 CYD Plus 空中无线文件管理</h1>
    <p class="sub">无需拔插 TF 卡，直接通过网页拖拽上传歌曲与小说</p>
  </header>

  <div class="card">
    <div class="drop-zone" id="dropZone">
      <div class="drop-title">📂 点击或拖拽文件至此处上传</div>
      <div class="drop-tip">支持 MP3/WAV 音频（存入曲库）与 TXT 文本（存入 /novel/ 小说）</div>
      <input type="file" id="fileInput" multiple style="display:none">
    </div>
    <div class="progress-bar" id="pBar"><div class="progress-fill" id="pFill"></div></div>
    <div class="status-msg" id="statusMsg"></div>
  </div>

  <div class="card">
    <div class="tabs">
      <button class="tab-btn active" id="tabMusic" onclick="switchTab('/')">🎵 音乐曲库 (/)</button>
      <button class="tab-btn" id="tabNovel" onclick="switchTab('/novel')">📖 小说书架 (/novel)</button>
    </div>
    <ul class="file-list" id="fileList">
      <li class="file-item" style="justify-content:center; color:#8b949e;">正在加载文件列表...</li>
    </ul>
  </div>
</div>

<script>
let curDir = '/';
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const pBar = document.getElementById('pBar');
const pFill = document.getElementById('pFill');
const statusMsg = document.getElementById('statusMsg');

dropZone.onclick = () => fileInput.click();
fileInput.onchange = () => handleFiles(fileInput.files);

dropZone.ondragover = (e) => { e.preventDefault(); dropZone.style.borderColor = '#388bfd'; };
dropZone.ondragleave = () => { dropZone.style.borderColor = 'var(--accent)'; };
dropZone.ondrop = (e) => {
  e.preventDefault();
  dropZone.style.borderColor = 'var(--accent)';
  if (e.dataTransfer.files.length) handleFiles(e.dataTransfer.files);
};

function switchTab(dir) {
  curDir = dir;
  document.getElementById('tabMusic').className = 'tab-btn ' + (dir === '/' ? 'active' : '');
  document.getElementById('tabNovel').className = 'tab-btn ' + (dir === '/novel' ? 'active' : '');
  loadFiles();
}

async function loadFiles() {
  const listEl = document.getElementById('fileList');
  listEl.innerHTML = '<li class="file-item" style="justify-content:center; color:#8b949e;">加载中...</li>';
  try {
    const res = await fetch('/api/list?dir=' + encodeURIComponent(curDir));
    const files = await res.json();
    if (!files || files.length === 0) {
      listEl.innerHTML = '<li class="file-item" style="justify-content:center; color:#8b949e;">当前目录为空</li>';
      return;
    }
    listEl.innerHTML = files.map(f => `
      <li class="file-item">
        <span class="file-name">${f.name}</span>
        <div class="file-meta">
          <span class="file-size">${formatSize(f.size)}</span>
          <button class="btn-del" onclick="deleteFile('${curDir}/${f.name}')">删除</button>
        </div>
      </li>
    `).join('');
  } catch (e) {
    listEl.innerHTML = '<li class="file-item" style="justify-content:center; color:var(--red);">加载失败，请检查网络</li>';
  }
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

async function deleteFile(path) {
  if (!confirm('确定要从 TF 卡删除 ' + path + ' 吗？')) return;
  try {
    const res = await fetch('/api/delete?path=' + encodeURIComponent(path), { method: 'POST' });
    const j = await res.json();
    if (j.status === 'ok') {
      loadFiles();
    } else {
      alert('删除失败');
    }
  } catch (e) {
    alert('网络异常');
  }
}

async function handleFiles(files) {
  if (!files || !files.length) return;
  pBar.style.display = 'block';

  for (let i = 0; i < files.length; i++) {
    const file = files[i];
    statusMsg.innerText = `正在上传 (${i + 1}/${files.length}): ${file.name}...`;
    pFill.style.width = '0%';

    const formData = new FormData();
    formData.append('file', file, file.name);

    await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/upload');
      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          pFill.style.width = pct + '%';
        }
      };
      xhr.onload = () => resolve();
      xhr.onerror = () => reject();
      xhr.send(formData);
    });
  }

  statusMsg.innerText = '✅ 全部上传完成！TF 卡已更新';
  pFill.style.width = '100%';
  setTimeout(() => { pBar.style.display = 'none'; }, 2000);
  loadFiles();
}

loadFiles();
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
  if (!s_server) return;
  s_server->send(200, "text/html", INDEX_HTML);
}

static void handleFileList() {
  if (!s_server) return;
  String dir = s_server->arg("dir");
  if (dir.length() == 0) dir = "/";

  File root = SD.open(dir);
  if (!root || !root.isDirectory()) {
    s_server->send(200, "application/json", "[]");
    return;
  }

  String json = "[";
  bool first = true;
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      // 取出基名
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);

      if (!name.startsWith("._") && !name.equalsIgnoreCase("System Volume Information")) {
        if (!first) json += ",";
        json += "{\"name\":\"" + name + "\",\"size\":" + String(file.size()) + "}";
        first = false;
      }
    }
    file = root.openNextFile();
  }
  json += "]";
  s_server->send(200, "application/json", json);
}

static void handleFileUpload() {
  if (!s_server) return;
  HTTPUpload& upload = s_server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    slash = filename.lastIndexOf('\\');
    if (slash >= 0) filename = filename.substring(slash + 1);

    String destPath = "/";
    if (filename.endsWith(".txt") || filename.endsWith(".TXT")) {
      if (!SD.exists("/novel")) SD.mkdir("/novel");
      destPath = "/novel/" + filename;
    } else {
      destPath = "/" + filename;
    }

    if (SD.exists(destPath)) {
      SD.remove(destPath);
    }
    s_uploadFile = SD.open(destPath, FILE_WRITE);
    snprintf(s_lastAction, sizeof(s_lastAction), "正在接收: %s", filename.c_str());
    LOG_SD("空中传书/歌: 开始接收 %s -> %s", filename.c_str(), destPath.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (s_uploadFile) {
      s_uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (s_uploadFile) {
      s_uploadFile.close();
      s_uploadCount++;
      snprintf(s_lastAction, sizeof(s_lastAction), "成功存入: %s (%.1fMB)",
               upload.filename.c_str(), (float)upload.totalSize / (1024.0f * 1024.0f));
      LOG_SD("空中传书/歌: 接收完毕 %s, 大小: %u 字节", upload.filename.c_str(), upload.totalSize);
    }
  }
}

static void handleUploadFinish() {
  if (!s_server) return;
  s_server->send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handleFileDelete() {
  if (!s_server) return;
  String path = s_server->arg("path");
  if (path.length() == 0) {
    s_server->send(400, "application/json", "{\"status\":\"missing_path\"}");
    return;
  }
  // 修正双斜杠
  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }

  bool ok = false;
  if (SD.exists(path)) {
    ok = SD.remove(path);
    snprintf(s_lastAction, sizeof(s_lastAction), "已删除: %s", path.c_str());
  }
  s_server->send(200, "application/json", ok ? "{\"status\":\"ok\"}" : "{\"status\":\"fail\"}");
}

void web_file_manager_init() {
}

void web_file_manager_start() {
  if (s_isRunning) return;

  net_wake_radio(0xFFFFFFFF); // 传书期间保持 Wi-Fi 射频常开
  if (!s_server) {
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, handleRoot);
    s_server->on("/api/list", HTTP_GET, handleFileList);
    s_server->on("/api/upload", HTTP_POST, handleUploadFinish, handleFileUpload);
    s_server->on("/api/delete", HTTP_POST, handleFileDelete);
  }

  s_server->begin();
  s_isRunning = true;
  s_uploadCount = 0;
  snprintf(s_lastAction, sizeof(s_lastAction), "就绪，等待网页连接");
  LOG_SYS("空中文件管理器已启动: http://%s", WiFi.localIP().toString().c_str());
}

void web_file_manager_stop() {
  if (!s_isRunning) return;
  if (s_server) {
    s_server->stop();
    delete s_server;
    s_server = nullptr;
  }
  s_isRunning = false;

  // 上传结束后自动重扫曲库与小说
  if (sdcard_is_available()) {
    scanner_scan_all();
    novel_scan_all();
  }
  net_set_power_save(true);
  LOG_SYS("空中文件管理器已退出，已刷新曲库与小说索引");
}

void web_file_manager_loop() {
  if (s_isRunning && s_server) {
    s_server->handleClient();
  }
}

bool web_file_manager_is_running() {
  return s_isRunning;
}

String web_file_manager_get_ip() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "WiFi 未连接";
}

const char* web_file_manager_get_last_action() {
  return s_lastAction;
}

uint32_t web_file_manager_get_upload_count() {
  return s_uploadCount;
}
