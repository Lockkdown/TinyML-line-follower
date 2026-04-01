// file: firmware/src/wifi_ui.h
// purpose: HTML/CSS/JS for the wifi controller web interface
// dependencies: none

#pragma once

// Gửi dữ liệu về web UI
const char WIFI_CONTROLLER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Robot Controller</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#111;color:#eee;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;gap:24px}
  h1{font-size:1.4rem;letter-spacing:.1em;color:#aaa}
  #status{padding:6px 18px;border-radius:20px;font-size:.85rem;font-weight:bold;background:#333;color:#888;transition:all .3s}
  #status.ok{background:#1a3a1a;color:#4caf50}
  #status.err{background:#3a1a1a;color:#f44336}
  .cam-wrap{width:100%;max-width:320px;aspect-ratio:4/3;background:#222;border-radius:10px;overflow:hidden;display:flex;align-items:center;justify-content:center;}
  #cam{width:100%;height:100%;object-fit:cover;display:block;}
  #cam-err{display:none;color:#666;font-size:.85rem;}
  .grid{display:grid;grid-template-columns:repeat(3,90px);gap:15px;}
  .btn{border:none;border-radius:12px;color:#fff;font-size:1.5rem;font-weight:bold;cursor:pointer;padding:15px;user-select:none;touch-action:manipulation;transition:transform .1s, filter .1s;}
  .btn:active, .btn.pressed{transform:scale(0.92);filter:brightness(0.8);}
  
  .btn-forward { grid-column: 2; background: #2196F3; }
  .btn-left { grid-column: 1; background: #4CAF50; }
  .btn-stop { grid-column: 2; background: #F44336; }
  .btn-right { grid-column: 3; background: #4CAF50; }
  .btn-backward { grid-column: 2; background: #FF9800; }
  
  .speed-ctrl { display: flex; flex-direction: column; align-items: center; gap: 10px; width: 100%; max-width: 300px; }
  .speed-lbl { font-size: 1rem; color: #ccc; }
  .speed-slider { width: 100%; }
  .speed-presets { display: flex; gap: 10px; }
  .preset-btn { padding: 8px 15px; border: none; border-radius: 8px; background: #444; color: #fff; cursor: pointer; transition: 0.2s; }
  .preset-btn:hover { background: #555; }
  .preset-btn:active { transform: scale(0.95); }

  small{font-size:0.8rem;font-weight:normal;opacity:0.8;}
  .hint{color:#777;font-size:.85rem;}
</style>
</head>
<body>
<h1>ROBOT CONTROLLER</h1>
<div id="status">Connecting...</div>
<div class="cam-wrap">
  <img id="cam" alt="">
  <span id="cam-err">Camera unavailable</span>
</div>

<div class="speed-ctrl">
  <div class="speed-lbl">Speed: <span id="speed-val">100</span></div>
  <input type="range" id="speed-slider" class="speed-slider" min="40" max="255" value="100">
  <div class="speed-presets">
    <button class="preset-btn" onclick="setSpeed(70)">🐢 Dataset (70)</button>
    <button class="preset-btn" onclick="setSpeed(180)">🚀 Test (180)</button>
  </div>
</div>

<div class="grid">
  <!-- Hàng 1 -->
  <div style="grid-column: 1"></div>
  <button class="btn btn-forward" id="btn-forward" data-action="forward">&#9650;<br><small>W</small></button>
  <div style="grid-column: 3"></div>
  
  <!-- Hàng 2 -->
  <button class="btn btn-left" id="btn-left" data-action="left">&#9664;<br><small>A</small></button>
  <button class="btn btn-stop" id="btn-stop" data-action="stop">&#9632;<br><small>SPACE</small></button>
  <button class="btn btn-right" id="btn-right" data-action="right">&#9654;<br><small>D</small></button>
  
  <!-- Hàng 3 -->
  <div style="grid-column: 1"></div>
  <button class="btn btn-backward" id="btn-backward" data-action="backward">&#9660;<br><small>S</small></button>
  <div style="grid-column: 3"></div>
</div>
<p class="hint">Hold button or key to move &bull; Release to stop</p>
<script>
const KEY_MAP = {w:'forward', a:'left', d:'right', s:'backward', ' ':'stop'};
const STOP_ON_RELEASE = new Set(['w','a','d','s']);
const statusEl = document.getElementById('status');

function setStatus(ok, text) {
  statusEl.textContent = text;
  statusEl.className = ok ? 'ok' : 'err';
}

async function sendCommand(action) {
  try {
    const res = await fetch('/' + action, {method:'POST'});
    setStatus(res.ok, res.ok ? 'Connected' : 'Error ' + res.status);
  } catch(e) {
    setStatus(false, 'Disconnected');
  }
}

const speedSlider = document.getElementById('speed-slider');
const speedVal = document.getElementById('speed-val');

function setSpeed(val) {
  speedSlider.value = val;
  speedVal.textContent = val;
  fetch('/speed', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'value=' + val
  });
}

speedSlider.addEventListener('input', (e) => {
  speedVal.textContent = e.target.value;
});

speedSlider.addEventListener('change', (e) => {
  setSpeed(e.target.value);
});

document.querySelectorAll('button[data-action]').forEach(btn => {
  const action = btn.dataset.action;
  btn.addEventListener('pointerdown', () => { btn.classList.add('pressed'); sendCommand(action); });
  btn.addEventListener('pointerup',   () => { btn.classList.remove('pressed'); if (action !== 'stop') sendCommand('stop'); });
  btn.addEventListener('pointerleave',() => { if (btn.classList.contains('pressed')) { btn.classList.remove('pressed'); sendCommand('stop'); } });
});

document.addEventListener('keydown', e => {
  const action = KEY_MAP[e.key.toLowerCase()];
  if (!action || e.repeat) return;
  const btn = document.querySelector('[data-action="' + action + '"]');
  if (btn) btn.classList.add('pressed');
  sendCommand(action);
});

document.addEventListener('keyup', e => {
  const key = e.key.toLowerCase();
  const action = KEY_MAP[key];
  if (!action) return;
  const btn = document.querySelector('[data-action="' + action + '"]');
  if (btn) btn.classList.remove('pressed');
  if (STOP_ON_RELEASE.has(key)) sendCommand('stop');
});

// Initial connectivity check
sendCommand('stop');

// Camera polling
const camEl  = document.getElementById('cam');
const camErr = document.getElementById('cam-err');
let isPolling = false;

function pollCamera() {
  if (isPolling) return;
  isPolling = true;

  const img = new Image();
  
  img.onload = () => {
    camEl.src = img.src;
    camEl.style.display = 'block';
    camErr.style.display = 'none';
    isPolling = false;
    setTimeout(pollCamera, 10); 
  };
  
  img.onerror = () => {
    camEl.style.display = 'none';
    camErr.style.display = 'block';
    isPolling = false;
    setTimeout(pollCamera, 1000); // Retry later if error occurs
  };
  
  img.src = '/snapshot?t=' + Date.now();
}

pollCamera();
</script>
</body>
</html>
)rawliteral";
