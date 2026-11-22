#include "web_file_manager.h"
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>
#include "storage/sdcard.h"
#include "storage/scanner.h"
#include "reader/novel.h"
#include "hardware/audio.h"
#include "hardware/battery.h"
#include "hardware/display.h"
#include "player/player.h"
#include "system/net_manager.h"
#include "system/settings_manager.h"
#include "system/power_manager.h"
#include "system/weather.h"
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
<title>CYD Plus 摆烂播放器 - 桌面控制台与文件管理</title>
<style>
:root {
  --bg: #0d1117; --card: #161b22; --border: #30363d;
  --text: #c9d1d9; --heading: #f0f6fc; --accent: #58a6ff;
  --green: #3fb950; --red: #f85149; --amber: #d29922;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 14px; line-height: 1.5; }
.container { max-width: 680px; margin: 0 auto; }
header { text-align: center; margin-bottom: 16px; padding-bottom: 12px; border-bottom: 1px solid var(--border); }
h1 { color: var(--heading); font-size: 20px; font-weight: 600; margin-bottom: 4px; }
h2 { color: var(--heading); font-size: 15px; font-weight: 600; margin-bottom: 10px; }
.sub { font-size: 13px; color: #8b949e; }
.card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 14px; margin-bottom: 14px; }
.stat-row { display: flex; flex-wrap: wrap; gap: 8px; font-size: 12px; margin-bottom: 10px; }
.badge { background: #21262d; border: 1px solid var(--border); padding: 3px 8px; border-radius: 12px; color: var(--heading); }
.ctrl-row { display: flex; align-items: center; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
.ctrl-label { min-width: 72px; font-size: 13px; color: #8b949e; }
input[type=range] { flex: 1; min-width: 120px; accent-color: var(--accent); }
input[type=text], select, textarea { background: #0d1117; color: var(--heading); border: 1px solid var(--border); border-radius: 6px; padding: 6px 8px; font-size: 13px; }
textarea { width: 100%; min-height: 92px; font-family: monospace; resize: vertical; }
.btn-group { display: flex; gap: 6px; flex-wrap: wrap; flex: 1; }
.btn { padding: 6px 10px; background: #21262d; border: 1px solid var(--border); color: var(--text); border-radius: 6px; cursor: pointer; font-size: 12px; }
.btn.active { background: #1f6feb; border-color: #388bfd; color: #fff; }
.btn.primary { background: #238636; border-color: #2ea043; color: #fff; }
.drop-zone { border: 2px dashed var(--accent); border-radius: 8px; padding: 22px 14px; text-align: center; cursor: pointer; transition: 0.2s; background: rgba(88, 166, 255, 0.04); }
.drop-zone:hover { background: rgba(88, 166, 255, 0.1); border-color: #79c0ff; }
.drop-title { font-size: 15px; font-weight: 600; color: var(--heading); margin-bottom: 4px; }
.drop-tip { font-size: 12px; color: #8b949e; }
.tabs { display: flex; gap: 8px; margin-bottom: 12px; }
.tab-btn { flex: 1; padding: 8px 12px; background: #21262d; border: 1px solid var(--border); color: var(--text); border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 500; }
.tab-btn.active { background: #1f6feb; border-color: #388bfd; color: #fff; }
.file-list { list-style: none; max-height: 280px; overflow-y: auto; }
.file-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 10px; border-bottom: 1px solid var(--border); font-size: 13px; }
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
    <h1>📻 CYD Plus 桌面摆件控制台</h1>
    <p class="sub">远程遥控音量音效 · 时钟城市设置 · 自定义电台 · 无线传歌传书</p>
  </header>

  <div class="card">
    <h2>🎛 摆件状态与远程控制</h2>
    <div class="stat-row">
      <span class="badge" id="bBattery">电量: --</span>
      <span class="badge" id="bTrack">状态: --</span>
      <span class="badge" id="bSleep">定时: 关闭</span>
    </div>

    <div class="ctrl-row">
      <span class="ctrl-label">音量 <b id="vLbl">70%</b></span>
      <input type="range" id="volRange" min="0" max="100" step="5" value="70" onchange="sendCtrl({volume:this.value})">
      <span class="ctrl-label" style="min-width:56px;">亮度 <b id="bLbl">80%</b></span>
      <input type="range" id="briRange" min="10" max="100" step="5" value="80" onchange="sendCtrl({brightness:this.value})">
    </div>

    <div class="ctrl-row">
      <span class="ctrl-label">音效模式</span>
      <div class="btn-group">
        <button class="btn" id="eq0" onclick="sendCtrl({eq:0})">原声</button>
        <button class="btn" id="eq1" onclick="sendCtrl({eq:1})">温暖低音</button>
        <button class="btn" id="eq2" onclick="sendCtrl({eq:2})">清晰人声</button>
      </div>
    </div>

    <div class="ctrl-row">
      <span class="ctrl-label">睡眠定时</span>
      <div class="btn-group">
        <button class="btn" id="sl0" onclick="sendCtrl({sleep_min:0})">关闭</button>
        <button class="btn" id="sl15" onclick="sendCtrl({sleep_min:15})">15分</button>
        <button class="btn" id="sl30" onclick="sendCtrl({sleep_min:30})">30分</button>
        <button class="btn" id="sl60" onclick="sendCtrl({sleep_min:60})">60分</button>
      </div>
    </div>

    <div class="ctrl-row">
      <span class="ctrl-label">本地播放</span>
      <div class="btn-group">
        <button class="btn" onclick="sendCtrl({cmd:'prev'})">上一首</button>
        <button class="btn primary" onclick="sendCtrl({cmd:'toggle'})">播放 / 暂停</button>
        <button class="btn" onclick="sendCtrl({cmd:'next'})">下一首</button>
        <button class="btn" id="clkBtn" onclick="toggleAlwaysOn()">时钟常亮</button>
        <button class="btn" id="chimeBtn" onclick="toggleChime()">整点报时</button>
      </div>
    </div>

    <div class="ctrl-row" style="margin-bottom:0;">
      <span class="ctrl-label">天气城市</span>
      <select id="cityPreset" onchange="applyPresetCity(this.value)">
        <option value="">常用城市快捷选...</option>
        <option value="101110506|富平县">富平县 (101110506)</option>
        <option value="101110101|西安">西安 (101110101)</option>
        <option value="101010100|北京">北京 (101010100)</option>
        <option value="101020100|上海">上海 (101020100)</option>
        <option value="101280101|广州">广州 (101280101)</option>
        <option value="101280601|深圳">深圳 (101280601)</option>
        <option value="101210101|杭州">杭州 (101210101)</option>
        <option value="101270101|成都">成都 (101270101)</option>
      </select>
      <input type="text" id="cityName" placeholder="城市名" style="width:76px;">
      <input type="text" id="cityCode" placeholder="9位代码" style="width:94px;">
      <button class="btn primary" onclick="saveCity()">同步天气</button>
    </div>
  </div>

  <div class="card">
    <h2>📡 自定义网络电台 (/radio.txt)</h2>
    <p class="sub" style="margin-bottom:8px;">每行一条，格式：<b>台名|流派|http://音频流地址</b>（保存后在网络收音机自动生效）</p>
    <textarea id="radioTxt" placeholder="例如：&#10;怀旧金曲|华语|http://stream.zeno.fm/f3wvbbqmdg8uv"></textarea>
    <div style="margin-top:8px; display:flex; gap:8px; align-items:center;">
      <button class="btn primary" onclick="saveRadio()">保存电台列表</button>
      <span class="sub" id="radioMsg"></span>
    </div>
  </div>

  <div class="card">
    <div class="drop-zone" id="dropZone">
      <div class="drop-title">📂 点击或拖拽文件至此处上传</div>
      <div class="drop-tip">支持 MP3/WAV 音频（存入曲库）与 TXT 文本（自动存入 /novel/；radio.txt 存入根目录）</div>
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
let curState = { clockAlwaysOn: false, hourlyChime: false };
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

async function loadStatus() {
  try {
    const res = await fetch('/api/status');
    const st = await res.json();
    curState = st;
    document.getElementById('bBattery').innerText = `电量: ${st.batteryPct}% (${st.batteryV}V)`;
    document.getElementById('bTrack').innerText = st.playing ? `播放: ${st.track}` : `待机: ${st.track}`;
    document.getElementById('bSleep').innerText = st.sleepRemSec > 0 ? `定时: 剩${Math.ceil(st.sleepRemSec/60)}分` : '定时: 关闭';
    document.getElementById('volRange').value = st.volume;
    document.getElementById('vLbl').innerText = st.volume + '%';
    document.getElementById('briRange').value = st.brightness;
    document.getElementById('bLbl').innerText = st.brightness + '%';
    [0,1,2].forEach(i => document.getElementById('eq'+i).className = 'btn' + (st.eq === i ? ' active' : ''));
    [0,15,30,60].forEach(m => document.getElementById('sl'+m).className = 'btn' + (st.sleepMin === m ? ' active' : ''));
    document.getElementById('clkBtn').className = 'btn' + (st.clockAlwaysOn ? ' active' : '');
    document.getElementById('chimeBtn').className = 'btn' + (st.hourlyChime ? ' active' : '');
    if (!document.getElementById('cityCode').value) {
      document.getElementById('cityName').value = st.cityName || '';
      document.getElementById('cityCode').value = st.cityCode || '';
    }
  } catch (e) {}
}

async function sendCtrl(params) {
  const q = new URLSearchParams(params).toString();
  await fetch('/api/control?' + q, { method: 'POST' });
  loadStatus();
}

function toggleAlwaysOn() { sendCtrl({ clock_always_on: curState.clockAlwaysOn ? 0 : 1 }); }
function toggleChime() { sendCtrl({ hourly_chime: curState.hourlyChime ? 0 : 1 }); }
function applyPresetCity(val) {
  if (!val) return;
  const [cd, nm] = val.split('|');
  document.getElementById('cityCode').value = cd;
  document.getElementById('cityName').value = nm;
}
async function saveCity() {
  const city_code = document.getElementById('cityCode').value.trim();
  const city_name = document.getElementById('cityName').value.trim();
  if (!city_code) return alert('请输入城市代码');
  await sendCtrl({ city_code, city_name });
  alert('城市已保存并请求刷新天气');
}

async function loadRadio() {
  try {
    const res = await fetch('/api/radio');
    document.getElementById('radioTxt').value = await res.text();
  } catch (e) {}
}
async function saveRadio() {
  const txt = document.getElementById('radioTxt').value;
  const res = await fetch('/api/radio', { method: 'POST', body: txt });
  const j = await res.json();
  document.getElementById('radioMsg').innerText = j.status === 'ok' ? '已保存至 TF 卡 /radio.txt' : '保存失败(需挂载TF卡)';
}

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

loadStatus();
loadRadio();
loadFiles();
setInterval(loadStatus, 4000);
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
    if (filename.equalsIgnoreCase("radio.txt")) {
      destPath = "/radio.txt";
    } else if (filename.endsWith(".txt") || filename.endsWith(".TXT")) {
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

static String jsonEscape(const char* src) {
  if (!src) return "";
  String out;
  for (const char* p = src; *p; p++) {
    if (*p == '"' || *p == '\\') out += '\\';
    out += *p;
  }
  return out;
}

static void handleStatus() {
  if (!s_server) return;
  SystemSettings& cfg = settings_get();
  const TrackInfo* t = player_get_current_track();
  String trackName = (t && strlen(t->title) > 0) ? jsonEscape(t->title) : "无曲目";
  String cityName = jsonEscape(cfg.weatherCityName);
  String cityCode = jsonEscape(cfg.weatherCityCode);

  char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"volume\":%u,\"brightness\":%u,\"eq\":%u,\"sleepMin\":%u,"
           "\"sleepRemSec\":%u,\"clockAlwaysOn\":%s,\"hourlyChime\":%s,"
           "\"batteryPct\":%u,\"batteryV\":\"%.2f\",\"playing\":%s,"
           "\"track\":\"%s\",\"cityName\":\"%s\",\"cityCode\":\"%s\"}",
           cfg.volume, cfg.brightness, (unsigned)audio_get_eq(),
           (unsigned)power_get_sleep_timer_minutes(),
           (unsigned)power_get_sleep_timer_remaining_sec(),
           cfg.clockAlwaysOn ? "true" : "false",
           cfg.hourlyChime ? "true" : "false",
           battery_get_percent(), battery_get_voltage(),
           (player_get_state() == STATE_PLAYING) ? "true" : "false",
           trackName.c_str(), cityName.c_str(), cityCode.c_str());
  s_server->send(200, "application/json", buf);
}

static void handleControl() {
  if (!s_server) return;
  SystemSettings& cfg = settings_get();
  bool needSave = false;

  if (s_server->hasArg("volume")) {
    int v = constrain(s_server->arg("volume").toInt(), 0, 100);
    cfg.volume = (uint8_t)v;
    audio_set_volume(cfg.volume);
    needSave = true;
  }
  if (s_server->hasArg("brightness")) {
    int b = constrain(s_server->arg("brightness").toInt(), 5, 100);
    cfg.brightness = (uint8_t)b;
    display_set_brightness(cfg.brightness);
    needSave = true;
  }
  if (s_server->hasArg("eq")) {
    int eq = constrain(s_server->arg("eq").toInt(), 0, 2);
    cfg.eqMode = (uint8_t)eq;
    audio_set_eq((AudioEQMode)eq);
    needSave = true;
  }
  if (s_server->hasArg("sleep_min")) {
    int m = constrain(s_server->arg("sleep_min").toInt(), 0, 240);
    power_set_sleep_timer((uint16_t)m);
  }
  if (s_server->hasArg("clock_always_on")) {
    cfg.clockAlwaysOn = (s_server->arg("clock_always_on").toInt() != 0);
    needSave = true;
  }
  if (s_server->hasArg("hourly_chime")) {
    cfg.hourlyChime = (s_server->arg("hourly_chime").toInt() != 0);
    needSave = true;
  }
  if (s_server->hasArg("city_code")) {
    String cd = s_server->arg("city_code");
    cd.trim();
    if (cd.length() >= 5 && cd.length() < sizeof(cfg.weatherCityCode)) {
      snprintf(cfg.weatherCityCode, sizeof(cfg.weatherCityCode), "%s", cd.c_str());
      if (s_server->hasArg("city_name")) {
        String nm = s_server->arg("city_name");
        nm.trim();
        if (nm.length() > 0) {
          snprintf(cfg.weatherCityName, sizeof(cfg.weatherCityName), "%s", nm.c_str());
        }
      }
      settings_save();
      needSave = false;
      weather_fetch();
    }
  }
  if (s_server->hasArg("cmd")) {
    String cmd = s_server->arg("cmd");
    if (cmd == "prev") player_prev();
    else if (cmd == "next") player_next();
    else if (cmd == "toggle") player_toggle();
  }

  if (needSave) settings_save();
  s_server->send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handleRadioGet() {
  if (!s_server) return;
  if (!sdcard_is_available() || !SD.exists("/radio.txt")) {
    s_server->send(200, "text/plain",
                   "# 格式: 台名|流派|http://流地址\n"
                   "经典流行金曲|华语流行|http://stream.zeno.fm/f3wvbbqmdg8uv\n"
                   "Lo-Fi 治愈轻音|学习专注|http://stream.zeno.fm/4v7rz7hbg8uv\n");
    return;
  }
  File f = SD.open("/radio.txt", FILE_READ);
  if (!f) {
    s_server->send(500, "text/plain", "");
    return;
  }
  String content = f.readString();
  f.close();
  s_server->send(200, "text/plain; charset=utf-8", content);
}

static void handleRadioPost() {
  if (!s_server) return;
  if (!sdcard_is_available()) {
    s_server->send(500, "application/json", "{\"status\":\"no_sd\"}");
    return;
  }
  String body = s_server->arg("plain");
  if (SD.exists("/radio.txt")) SD.remove("/radio.txt");
  File f = SD.open("/radio.txt", FILE_WRITE);
  if (!f) {
    s_server->send(500, "application/json", "{\"status\":\"fail\"}");
    return;
  }
  f.print(body);
  f.close();
  snprintf(s_lastAction, sizeof(s_lastAction), "已更新自定义电台 /radio.txt");
  s_server->send(200, "application/json", "{\"status\":\"ok\"}");
}

void web_file_manager_init() {
}

void web_file_manager_start() {
  if (s_isRunning) return;

  net_wake_radio(0xFFFFFFFF); // 传书期间保持 Wi-Fi 射频常开
  if (!s_server) {
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, handleRoot);
    s_server->on("/api/status", HTTP_GET, handleStatus);
    s_server->on("/api/control", HTTP_POST, handleControl);
    s_server->on("/api/radio", HTTP_GET, handleRadioGet);
    s_server->on("/api/radio", HTTP_POST, handleRadioPost);
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
