#pragma once

const char WIFI_CONTROLLER_HTML[] = R"rawliteral(
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
  .grid{display:grid;grid-template-columns:repeat(3,90px);grid-template-rows:repeat(3,90px);gap:10px}
  button{width:90px;height:90px;border:none;border-radius:14px;font-size:1.05rem;font-weight:bold;cursor:pointer;background:#222;color:#ddd;border:2px solid #333;transition:background .1s,transform .1s;user-select:none;-webkit-user-select:none}
  button:active,button.pressed{background:#2a5a8a;border-color:#4a9aff;transform:scale(.93)}
  #btn-forward{grid-column:2;grid-row:1}
  #btn-left   {grid-column:1;grid-row:2}
  #btn-stop   {grid-column:2;grid-row:2;background:#2a1a1a;border-color:#833}
  #btn-stop:active,#btn-stop.pressed{background:#8a2a2a;border-color:#ff4a4a}
  #btn-right  {grid-column:3;grid-row:2}
  .hint{font-size:.75rem;color:#555;text-align:center}
</style>
</head>
<body>
<h1>ROBOT CONTROLLER</h1>
<div id="status">Connecting...</div>
<div class="grid">
  <button id="btn-forward" data-action="forward">&#9650;<br><small>W</small></button>
  <button id="btn-left"    data-action="left">&#9664;<br><small>A</small></button>
  <button id="btn-stop"    data-action="stop">&#9632;<br><small>S</small></button>
  <button id="btn-right"   data-action="right">&#9654;<br><small>D</small></button>
</div>
<p class="hint">Hold button or key to move &bull; Release to stop</p>
<script>
const KEY_MAP = {w:'forward', a:'left', d:'right', s:'stop'};
const STOP_ON_RELEASE = new Set(['w','a','d']);
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
</script>
</body>
</html>
)rawliteral";
