#pragma once
#include <Arduino.h>

// Bedienseite: fahren mit Joystick oder Pfeiltasten, Gesicht gestalten.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>MF26 Roboter</title>
<style>
  :root{--bg:#0d1117;--panel:#161b22;--line:#26303d;--fg:#e6edf3;--muted:#8b949e;--accent:#5acdff}
  *{box-sizing:border-box}
  [hidden]{display:none!important}
  html,body{min-height:100%}
  body{margin:0;background:var(--bg);color:var(--fg);
       font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
       display:flex;flex-direction:column;align-items:center;gap:14px;
       padding:16px 14px 40px;-webkit-user-select:none;user-select:none}
  header{display:flex;align-items:center;gap:10px;font-weight:600;letter-spacing:.3px}
  #dot{width:9px;height:9px;border-radius:50%;background:#d0454c;transition:background .2s}
  #dot.on{background:#3fb950;box-shadow:0 0 10px #3fb95088}
  #status{color:var(--muted);font-weight:400;font-size:13px}
  nav{display:flex;gap:6px;background:var(--panel);border:1px solid var(--line);
      border-radius:11px;padding:4px;align-items:center}
  nav button{background:none;border:0;color:var(--muted);padding:7px 18px;border-radius:8px;
             font:inherit;font-size:13.5px;cursor:pointer}
  nav button.on{background:var(--accent);color:#04222e;font-weight:600}
  nav a{color:var(--muted);text-decoration:none;padding:7px 14px;font-size:13.5px}
  nav a:hover{color:var(--fg)}
  .tab{display:none;flex-direction:column;align-items:center;gap:14px;width:100%}
  .tab.on{display:flex}
  canvas{display:block}
  #pad{touch-action:none}  /* nur hier Wischen abfangen, sonst scrollt die Seite nicht */
  .panel{background:var(--panel);border:1px solid var(--line);border-radius:14px;
         padding:14px 16px;width:min(340px,100%)}
  .panel h3{margin:0 0 10px;font-size:11px;text-transform:uppercase;
            letter-spacing:.09em;color:var(--muted);font-weight:600}
  .row{display:flex;align-items:center;gap:12px;margin:8px 0}
  .row label{flex:0 0 86px;color:var(--muted);font-size:13px}
  .row output{flex:0 0 42px;text-align:right;font-variant-numeric:tabular-nums;
              color:var(--accent);font-size:13px}
  input[type=range]{flex:1;accent-color:var(--accent);background:transparent}
  input[type=color]{width:42px;height:28px;border:1px solid var(--line);border-radius:7px;
                    background:none;padding:2px;cursor:pointer}
  input[type=checkbox]{accent-color:var(--accent);width:17px;height:17px}
  select{flex:1;background:#0d1117;color:var(--fg);border:1px solid var(--line);
         border-radius:8px;padding:8px 10px;font:inherit;font-size:13.5px;cursor:pointer}
  select:focus{outline:2px solid var(--accent);outline-offset:1px}
  .hint{color:var(--muted);font-size:12px;text-align:center;max-width:340px}
  kbd{background:#21262d;border:1px solid var(--line);border-bottom-width:2px;border-radius:5px;
      padding:1px 5px;font-size:11px;font-family:inherit;color:var(--fg)}
  .actions{display:flex;gap:8px;margin-top:10px}
  .actions button{flex:1;background:#0d1117;border:1px solid var(--line);color:var(--fg);
                  border-radius:8px;padding:9px;font:inherit;font-size:13px;cursor:pointer}
  .actions button:hover{border-color:var(--accent)}
  details summary{color:var(--muted);font-size:13px;cursor:pointer;outline:none}
  .nets{display:flex;flex-direction:column;gap:4px;margin:4px 0 2px;
        max-height:210px;overflow-y:auto}
  .nets button{display:flex;align-items:center;gap:9px;width:100%;text-align:left;
               background:#0d1117;border:1px solid var(--line);color:var(--fg);
               border-radius:8px;padding:9px 11px;font:inherit;font-size:13.5px;
               cursor:pointer}
  .nets button.on{border-color:var(--accent);color:var(--accent)}
  .nets .bars{flex:0 0 auto;font-size:11px;color:var(--muted);
              font-variant-numeric:tabular-nums}
  .nets .lock{margin-left:auto;color:var(--muted);font-size:11px}
  input[type=text],input[type=password]{flex:1;min-width:0;background:#0d1117;
    color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:9px 10px;
    font:inherit;font-size:13.5px}
  .state{font-size:13px;color:var(--muted);margin:0 0 10px}
  .state b{color:var(--fg);font-weight:600}
  .state.ok b{color:#3fb950}
</style>
</head>
<body>

<header><span id="dot"></span>MF26 <span id="status">verbinde…</span></header>

<nav>
  <button id="tab-drive" class="on">Fahren</button>
  <button id="tab-face">Gesicht</button>
  <button id="tab-wifi">WLAN</button>
  <a href="/docs">API</a>
</nav>

<!-- ------------------------------------------------------------ fahren -- -->
<section class="tab on" id="pane-drive">
  <canvas id="pad" width="280" height="280"></canvas>
  <div class="panel">
    <div class="row">
      <label for="speed">Tempo</label>
      <input id="speed" type="range" min="10" max="100" value="60">
      <output id="speedv">60%</output>
    </div>
    <details>
      <summary>Trimmung — gegen schleichende Räder</summary>
      <div class="row">
        <label for="tl">Links</label>
        <input id="tl" type="range" min="-120" max="120" value="0">
        <output id="tlv">0</output>
      </div>
      <div class="row">
        <label for="tr">Rechts</label>
        <input id="tr" type="range" min="-120" max="120" value="0">
        <output id="trv">0</output>
      </div>
    </details>
  </div>
  <p class="hint">Stick ziehen oder mit <kbd>↑</kbd><kbd>↓</kbd><kbd>←</kbd><kbd>→</kbd> /
     <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> steuern. <kbd>Leertaste</kbd> stoppt.</p>
</section>

<!-- ----------------------------------------------------------- gesicht -- -->
<section class="tab" id="pane-face">
  <canvas id="preview" width="240" height="240" style="border-radius:50%;background:#000"></canvas>

  <div class="panel">
    <h3>Augen</h3>
    <div class="row"><label for="eye">Form</label><select id="eye"></select></div>
    <div class="row"><label>Breite</label><input id="eyeWidth" type="range" min="16" max="90"><output></output></div>
    <div class="row"><label>Höhe</label><input id="eyeHeight" type="range" min="16" max="110"><output></output></div>
    <div class="row"><label>Abstand</label><input id="eyeGap" type="range" min="20" max="75"><output></output></div>
    <div class="row"><label>Position ↕</label><input id="eyeY" type="range" min="50" max="150"><output></output></div>
  </div>

  <div class="panel">
    <h3>Mund</h3>
    <div class="row"><label for="mouth">Form</label><select id="mouth"></select></div>
    <div class="row"><label>Breite</label><input id="mouthWidth" type="range" min="16" max="110"><output></output></div>
    <div class="row"><label>Position ↕</label><input id="mouthY" type="range" min="130" max="215"><output></output></div>
  </div>

  <div class="panel">
    <h3>Augenbrauen &amp; Nase</h3>
    <div class="row"><label for="brow">Brauen</label><select id="brow"></select></div>
    <div class="row"><label for="nose">Nase</label><select id="nose"></select></div>
  </div>

  <div class="panel">
    <h3>Aussehen</h3>
    <div class="row">
      <label for="color">Farbe</label>
      <input id="color" type="color">
      <label for="background" style="flex:0 0 auto">Hintergrund</label>
      <input id="background" type="color">
    </div>
    <div class="row"><label>Strichstärke</label><input id="thickness" type="range" min="2" max="10"><output></output></div>
  </div>

  <div class="panel">
    <h3>Verhalten</h3>
    <div class="row">
      <label for="autoBlink">Blinzeln</label>
      <input id="autoBlink" type="checkbox">
      <input id="blinkEvery" type="range" min="1" max="20"><output></output>
    </div>
    <div class="row">
      <label for="autoLook">Umsehen</label>
      <input id="autoLook" type="checkbox">
      <input id="lookEvery" type="range" min="1" max="20"><output></output>
    </div>
    <p class="hint" style="text-align:left;margin:4px 0 0">Schieberegler: Sekunden zwischen Blinzeln bzw. Umsehen.</p>
    <div class="actions">
      <button id="blinkNow">Jetzt blinzeln</button>
      <button id="reset">Zurücksetzen</button>
    </div>
  </div>
  <p class="hint">Änderungen wirken sofort und werden auf dem Roboter gespeichert.</p>
</section>

<section class="tab" id="pane-wifi">
  <div class="panel">
    <h3>Eigenes WLAN</h3>
    <p class="state" id="wifiState">wird geladen…</p>
    <div class="actions" style="margin-top:0">
      <button id="scan">Netzwerke suchen</button>
    </div>
    <div class="nets" id="nets"></div>
    <div class="row">
      <label for="wifiSsid">Name</label>
      <input id="wifiSsid" type="text" autocomplete="off" autocapitalize="none" spellcheck="false">
    </div>
    <div class="row">
      <label for="wifiPass">Passwort</label>
      <input id="wifiPass" type="password" autocomplete="off">
    </div>
    <div class="actions">
      <button id="wifiSave">Verbinden</button>
      <button id="wifiForget">Vergessen</button>
    </div>
  </div>
  <p class="hint">Der Roboter behält sein eigenes WLAN, auch wenn er zusätzlich
     in deinem Netz ist. Du kannst ihn also nie aussperren. Beim Verbinden
     wechselt er den Funkkanal — das Handy fliegt dabei kurz aus dem
     Roboter-WLAN und verbindet sich gleich wieder.</p>
</section>

<script>
(() => {
  // /gesicht zeigt nur den Editor - ohne Reiter, ohne Fahren. Dieselbe Seite,
  // damit es den Editor nicht zweimal zu pflegen gibt.
  const faceOnly = location.pathname.replace(/\/+$/, '') === '/gesicht';

  // Die Geräte-API spricht Englisch; hier stehen die deutschen Beschriftungen.
  const LABELS = {
    eye:   {rounded:'Abgerundet', circle:'Kreis', square:'Quadrat',
            oval:'Oval', happy:'Fröhlich', sleepy:'Müde'},
    mouth: {smile:'Lächeln', flat:'Strich', open:'Offen', cat:'Katze',
            frown:'Traurig', grin:'Grinsen', squiggle:'Welle'},
    brow:  {none:'Keine', flat:'Gerade', angry:'Wütend', sad:'Traurig',
            raised:'Hochgezogen'},
    nose:  {none:'Keine', dot:'Punkt', triangle:'Dreieck', line:'Strich'}
  };

  // ------------------------------------------------------------ reiter ---
  const panes = { drive: 'pane-drive', face: 'pane-face', wifi: 'pane-wifi' };
  for (const key of Object.keys(panes)) {
    document.getElementById('tab-' + key).onclick = () => {
      for (const k of Object.keys(panes)) {
        document.getElementById('tab-' + k).classList.toggle('on', k === key);
        document.getElementById(panes[k]).classList.toggle('on', k === key);
      }
    };
  }

  // ---------------------------------------------------------- joystick ---
  const pad = document.getElementById('pad'), ctx = pad.getContext('2d');
  const dot = document.getElementById('dot'), statusEl = document.getElementById('status');
  const R = 122, KNOB = 42, CX = pad.width / 2, CY = pad.height / 2, MAX = R - KNOB;
  let stick = {x:0,y:0}, keys = {x:0,y:0}, out = {x:0,y:0}, dragging = false;

  function draw() {
    const kx = CX + out.x * MAX, ky = CY - out.y * MAX;
    ctx.clearRect(0, 0, pad.width, pad.height);
    ctx.strokeStyle = '#26303d'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(CX, CY, R, 0, 7); ctx.stroke();
    ctx.strokeStyle = '#1b2430';
    ctx.beginPath();
    ctx.moveTo(CX - R + 14, CY); ctx.lineTo(CX + R - 14, CY);
    ctx.moveTo(CX, CY - R + 14); ctx.lineTo(CX, CY + R - 14); ctx.stroke();
    const g = ctx.createRadialGradient(kx - 12, ky - 14, 6, kx, ky, KNOB);
    g.addColorStop(0, '#9fe2ff'); g.addColorStop(1, '#2f8fc4');
    ctx.fillStyle = g; ctx.beginPath(); ctx.arc(kx, ky, KNOB, 0, 7); ctx.fill();
    ctx.strokeStyle = '#5acdff'; ctx.lineWidth = 2; ctx.stroke();
  }

  function setFromPointer(ev) {
    const r = pad.getBoundingClientRect();
    let dx = (ev.clientX - r.left) * (pad.width / r.width) - CX;
    let dy = (ev.clientY - r.top) * (pad.height / r.height) - CY;
    const d = Math.hypot(dx, dy);
    if (d > MAX) { dx *= MAX / d; dy *= MAX / d; }
    stick.x = dx / MAX; stick.y = -dy / MAX;
  }
  pad.addEventListener('pointerdown', e => { dragging = true; pad.setPointerCapture(e.pointerId); setFromPointer(e); mix(); });
  pad.addEventListener('pointermove', e => { if (dragging) { setFromPointer(e); mix(); } });
  const release = () => { dragging = false; stick.x = stick.y = 0; mix(); };
  pad.addEventListener('pointerup', release);
  pad.addEventListener('pointercancel', release);

  const held = new Set();
  const KEYMAP = { ArrowUp:'f', KeyW:'f', ArrowDown:'b', KeyS:'b',
                   ArrowLeft:'l', KeyA:'l', ArrowRight:'r', KeyD:'r' };
  addEventListener('keydown', e => {
    if (e.target.matches('input,select,textarea')) return;
    if (e.code === 'Space') { held.clear(); keys = {x:0,y:0}; mix(); e.preventDefault(); return; }
    const k = KEYMAP[e.code];
    if (!k || e.repeat) return;
    held.add(k); applyKeys(); e.preventDefault();
  });
  addEventListener('keyup', e => {
    const k = KEYMAP[e.code];
    if (!k) return;
    held.delete(k); applyKeys(); e.preventDefault();
  });
  addEventListener('blur', () => { held.clear(); applyKeys(); });

  function applyKeys() {
    keys.y = (held.has('f') ? 1 : 0) - (held.has('b') ? 1 : 0);
    keys.x = (held.has('r') ? 1 : 0) - (held.has('l') ? 1 : 0);
    mix();
  }
  function mix() {
    out.x = Math.abs(stick.x) > Math.abs(keys.x) ? stick.x : keys.x;
    out.y = Math.abs(stick.y) > Math.abs(keys.y) ? stick.y : keys.y;
    draw();
  }

  // -------------------------------------------------------- websocket ---
  let ws = null, ready = false;
  function connect() {
    ws = new WebSocket(`ws://${location.host}/ws`);
    ws.onopen = () => {
      ready = true; dot.classList.add('on'); statusEl.textContent = 'verbunden';
      sendTrim(); sendDrive(true);
    };
    ws.onclose = () => {
      ready = false; dot.classList.remove('on'); statusEl.textContent = 'neuer Versuch…';
      setTimeout(connect, 1000);
    };
    ws.onerror = () => ws.close();
  }
  let last = {x:999,y:999,t:0};
  function sendDrive(force) {
    if (!ready) return;
    const x = Math.round(out.x * 1000), y = Math.round(out.y * 1000), now = Date.now();
    if (!force && x === last.x && y === last.y && now - last.t < 250) return;
    ws.send(`D,${x},${y}`);
    last = {x, y, t: now};
  }
  if (!faceOnly) setInterval(sendDrive, 50);

  const speed = document.getElementById('speed'), speedv = document.getElementById('speedv');
  speed.oninput = () => { speedv.textContent = speed.value + '%'; if (ready) ws.send(`S,${speed.value}`); };
  const tl = document.getElementById('tl'), tr = document.getElementById('tr');
  const tlv = document.getElementById('tlv'), trv = document.getElementById('trv');
  function sendTrim() {
    tlv.textContent = tl.value; trv.textContent = tr.value;
    if (ready) { ws.send(`T,${tl.value},${tr.value}`); ws.send(`S,${speed.value}`); }
  }
  tl.oninput = tr.oninput = sendTrim;

  // ---------------------------------------------------- gesichtseditor ---
  const pv = document.getElementById('preview'), px = pv.getContext('2d');
  let cfg = null, options = null;

  const to565 = hex => {
    const r = parseInt(hex.slice(1,3),16), g = parseInt(hex.slice(3,5),16), b = parseInt(hex.slice(5,7),16);
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
  };
  const toHex = v => {
    const r = ((v>>11)&31)*255/31, g = ((v>>5)&63)*255/63, b = (v&31)*255/31;
    return '#' + [r,g,b].map(n => Math.round(n).toString(16).padStart(2,'0')).join('');
  };

  // Spiegelt das Zeichnen aus src/face.cpp, damit die Vorschau dem Display gleicht.
  function drawFace() {
    if (!cfg) return;
    const col = toHex(cfg.color), bg = toHex(cfg.background);
    px.fillStyle = bg; px.fillRect(0,0,240,240);
    px.fillStyle = col; px.strokeStyle = col;
    px.lineWidth = cfg.thickness; px.lineCap = 'round'; px.lineJoin = 'round';

    const roundRect = (x,y,w,h,r) => { px.beginPath(); px.roundRect(x,y,w,h,r); px.fill(); };

    const eyeAt = (cx, cy) => {
      const w = cfg.eyeWidth, h = cfg.eyeHeight;
      switch (cfg.eye) {
        case 'circle': px.beginPath(); px.ellipse(cx,cy,w/2,Math.min(w,h)/2,0,0,7); px.fill(); break;
        case 'square': px.fillRect(cx-w/2, cy-h/2, w, h); break;
        case 'oval': px.beginPath(); px.ellipse(cx,cy,w/2,h/2,0,0,7); px.fill(); break;
        case 'happy': {
          px.beginPath();
          for (let i=0;i<=14;i++){ const t=i/14;
            const x=cx-w/2+t*w, y=cy+(h*0.45)*(4*(t-0.5)**2-0.5);
            i?px.lineTo(x,y):px.moveTo(x,y); }
          px.stroke(); break; }
        case 'sleepy': { const sh = Math.max(6,h/3);
          roundRect(cx-w/2, cy+h/2-sh, w, sh, Math.min(sh/2,w/2)); break; }
        default: roundRect(cx-w/2, cy-h/2, w, h, Math.min(Math.min(w,h)/3, h/2));
      }
    };

    const browAt = (cx, cy, right) => {
      if (cfg.brow === 'none') return;
      const half = cfg.eyeWidth/2, gap = cfg.eyeHeight/2 + 12, tilt = 9;
      const inner = right ? cx-half : cx+half, outer = right ? cx+half : cx-half;
      px.beginPath();
      if (cfg.brow === 'angry') { px.moveTo(inner, cy-gap+tilt); px.lineTo(outer, cy-gap-tilt); }
      else if (cfg.brow === 'sad') { px.moveTo(inner, cy-gap-tilt); px.lineTo(outer, cy-gap+tilt); }
      else if (cfg.brow === 'raised') {
        for (let i=0;i<9;i++){ const t=i/8;
          const x=cx-half+t*2*half, y=cy-gap-4-8*Math.sin(t*Math.PI);
          i?px.lineTo(x,y):px.moveTo(x,y); }
      } else { px.moveTo(cx-half, cy-gap); px.lineTo(cx+half, cy-gap); }
      px.stroke();
    };

    const smile = 0.35;
    const mouth = () => {
      const half = cfg.mouthWidth/2, mx = 120, my = cfg.mouthY;
      px.beginPath();
      switch (cfg.mouth) {
        case 'flat': px.moveTo(mx-half,my); px.lineTo(mx+half,my); px.stroke(); break;
        case 'open': px.ellipse(mx,my,half,half*(0.5+0.5*smile),0,0,7); px.fill(); break;
        case 'cat':
          for (let s=0;s<2;s++){ px.beginPath();
            const x0 = s ? mx : mx-half;
            for (let i=0;i<=24;i++){ const t=i/24;
              const x=x0+t*half, y=my-half*0.35*Math.sin(t*Math.PI);
              i?px.lineTo(x,y):px.moveTo(x,y); }
            px.stroke(); }
          break;
        case 'frown': {
          const d = 8+12*(1-smile);
          for (let i=0;i<=24;i++){ const t=i/24;
            const x=mx-half+t*2*half, y=my-d*(1-4*(t-0.5)**2);
            i?px.lineTo(x,y):px.moveTo(x,y); }
          px.stroke(); break; }
        case 'grin': {
          const h = 10+16*smile;
          px.roundRect(mx-half, my-h/2, half*2, h, h/2); px.fill();
          px.strokeStyle = bg; px.lineWidth = 2;
          px.beginPath(); px.moveTo(mx-half+3,my); px.lineTo(mx+half-3,my); px.stroke();
          px.strokeStyle = col; px.lineWidth = cfg.thickness; break; }
        case 'squiggle':
          for (let i=0;i<=24;i++){ const t=i/24;
            const x=mx-half+t*2*half, y=my+7*Math.sin(t*3*Math.PI);
            i?px.lineTo(x,y):px.moveTo(x,y); }
          px.stroke(); break;
        default: {
          const d = 8+16*smile;
          for (let i=0;i<=24;i++){ const t=i/24;
            const x=mx-half+t*2*half, y=my+d*(1-4*(t-0.5)**2);
            i?px.lineTo(x,y):px.moveTo(x,y); }
          px.stroke(); }
      }
    };

    const nose = () => {
      if (cfg.nose === 'none') return;
      const nx = 120, ny = (cfg.eyeY + cfg.mouthY)/2, s = 6 + cfg.thickness;
      px.beginPath();
      if (cfg.nose === 'triangle') {
        px.moveTo(nx, ny+s/2); px.lineTo(nx-s/2, ny-s/2); px.lineTo(nx+s/2, ny-s/2);
        px.closePath(); px.fill();
      } else if (cfg.nose === 'line') {
        px.moveTo(nx, ny-s/2); px.lineTo(nx, ny+s/2); px.stroke();
      } else { px.arc(nx, ny, s/2, 0, 7); px.fill(); }
    };

    eyeAt(120 - cfg.eyeGap, cfg.eyeY);
    eyeAt(120 + cfg.eyeGap, cfg.eyeY);
    browAt(120 - cfg.eyeGap, cfg.eyeY, false);
    browAt(120 + cfg.eyeGap, cfg.eyeY, true);
    nose();
    mouth();
  }

  // Schreibvorgänge bündeln, damit ein Schieberegler den Roboter nicht flutet.
  let pending = null, timer = null;
  function push(patch) {
    Object.assign(cfg, patch);
    drawFace();
    pending = Object.assign(pending || {}, patch);
    clearTimeout(timer);
    timer = setTimeout(async () => {
      const body = pending; pending = null;
      try {
        const r = await fetch('/api/face', {
          method: 'PUT', headers: {'Content-Type':'application/json'},
          body: JSON.stringify(body)
        });
        if (r.ok) cfg = await r.json();
      } catch {}
      drawFace();
    }, 120);
  }

  const SLIDERS = ['eyeWidth','eyeHeight','eyeGap','eyeY','mouthWidth','mouthY',
                   'thickness','blinkEvery','lookEvery'];

  function bindUI() {
    for (const kind of ['eye','mouth','brow','nose']) {
      const sel = document.getElementById(kind);
      sel.textContent = '';
      // A string here would iterate letter by letter and fill the dropdown with
      // single characters, so insist on the list the API promises.
      if (!Array.isArray(options[kind])) {
        statusEl.textContent = 'API-Fehler: /api/face/options';
        return;
      }
      for (const name of options[kind]) {
        const o = document.createElement('option');
        o.value = name;
        o.textContent = (LABELS[kind] && LABELS[kind][name]) || name;
        sel.appendChild(o);
      }
      sel.onchange = () => push({[kind]: sel.value});
    }
    for (const id of SLIDERS) {
      const el = document.getElementById(id);
      el.oninput = () => {
        if (el.nextElementSibling) el.nextElementSibling.value = el.value;
        push({[id]: +el.value});
      };
    }
    for (const id of ['color','background']) {
      document.getElementById(id).oninput = e => push({[id]: to565(e.target.value)});
    }
    for (const id of ['autoBlink','autoLook']) {
      document.getElementById(id).onchange = e => push({[id]: e.target.checked});
    }
    document.getElementById('blinkNow').onclick = () => fetch('/api/blink', {method:'POST'});
    document.getElementById('reset').onclick = async () => {
      const r = await fetch('/api/face/reset', {method:'POST'});
      cfg = await r.json(); fillUI(); drawFace();
    };
  }

  function fillUI() {
    for (const kind of ['eye','mouth','brow','nose']) {
      document.getElementById(kind).value = cfg[kind];
    }
    for (const id of SLIDERS) {
      const el = document.getElementById(id);
      el.value = cfg[id];
      if (el.nextElementSibling) el.nextElementSibling.value = cfg[id];
    }
    document.getElementById('color').value = toHex(cfg.color);
    document.getElementById('background').value = toHex(cfg.background);
    document.getElementById('autoBlink').checked = cfg.autoBlink;
    document.getElementById('autoLook').checked = cfg.autoLook;
  }

  (async () => {
    try {
      options = await (await fetch('/api/face/options')).json();
      cfg = await (await fetch('/api/face')).json();
      bindUI(); fillUI(); drawFace();
      const st = await (await fetch('/api/status')).json();
      speed.value = st.speed; speedv.textContent = st.speed + '%';
      tl.value = st.trimLeft; tr.value = st.trimRight;
      tlv.textContent = st.trimLeft; trv.textContent = st.trimRight;
    } catch {}
  })();

  // ------------------------------------------------------------- wlan ---
  const netsBox = document.getElementById('nets');
  const ssidIn = document.getElementById('wifiSsid');
  const passIn = document.getElementById('wifiPass');
  const stateEl = document.getElementById('wifiState');
  let scanTimer = null;

  function showWifi(w) {
    stateEl.classList.toggle('ok', !!w.connected);
    if (w.connected) {
      stateEl.innerHTML = `Verbunden mit <b>${esc(w.ssid)}</b> — erreichbar unter <b>${esc(w.ip)}</b>`;
    } else if (w.connecting) {
      stateEl.innerHTML = `Verbinde mit <b>${esc(w.ssid)}</b>…`;
    } else if (w.saved) {
      stateEl.innerHTML = `<b>${esc(w.ssid)}</b> gespeichert, aber nicht verbunden`;
    } else {
      stateEl.innerHTML = `Nur eigenes WLAN <b>${esc(w.ap.ssid)}</b>`;
    }
    if (!ssidIn.value && w.ssid) ssidIn.value = w.ssid;
  }
  const esc = t => String(t ?? '').replace(/[&<>"]/g, c =>
    ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

  async function loadWifi() {
    try { showWifi(await (await fetch('/api/wifi')).json()); } catch {}
  }

  function bars(rssi) {
    const n = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
    return '▂▄▆█'.slice(0, n).padEnd(4, '·');
  }

  async function pollScan() {
    let r;
    try { r = await (await fetch('/api/wifi/scan')).json(); } catch { return; }
    if (r.scanning) { scanTimer = setTimeout(pollScan, 1200); return; }
    scanTimer = null;
    document.getElementById('scan').textContent = 'Netzwerke suchen';
    netsBox.textContent = '';
    if (!Array.isArray(r.networks) || !r.networks.length) {
      const p = document.createElement('p');
      p.className = 'hint';
      p.textContent = 'Nichts gefunden — noch einmal suchen.';
      netsBox.appendChild(p);
      return;
    }
    r.networks.sort((a, b) => b.rssi - a.rssi);
    for (const n of r.networks) {
      const b = document.createElement('button');
      b.innerHTML = `<span class="bars">${bars(n.rssi)}</span>` +
                    `<span>${esc(n.ssid)}</span>` +
                    `<span class="lock">${n.open ? 'offen' : '🔒'}</span>`;
      b.onclick = () => {
        ssidIn.value = n.ssid;
        passIn.value = '';
        (n.open ? document.getElementById('wifiSave') : passIn).focus();
        [...netsBox.children].forEach(c => c.classList.toggle('on', c === b));
      };
      netsBox.appendChild(b);
    }
  }

  document.getElementById('scan').onclick = () => {
    if (scanTimer) return;
    document.getElementById('scan').textContent = 'suche…';
    netsBox.textContent = '';
    pollScan();
  };

  document.getElementById('wifiSave').onclick = async () => {
    if (!ssidIn.value) { ssidIn.focus(); return; }
    try {
      const r = await fetch('/api/wifi', {
        method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ssid: ssidIn.value, password: passIn.value})
      });
      showWifi(await r.json());
      // der Verbindungsaufbau läuft im Hintergrund weiter
      for (const ms of [1500, 3000, 5000, 8000, 12000]) setTimeout(loadWifi, ms);
    } catch {}
  };

  document.getElementById('wifiForget').onclick = async () => {
    try { showWifi(await (await fetch('/api/wifi/forget', {method:'POST'})).json()); } catch {}
    passIn.value = '';
  };

  loadWifi();

  if (faceOnly) {
    document.querySelector('nav').hidden = true;
    document.getElementById('pane-drive').classList.remove('on');
    document.getElementById('pane-face').classList.add('on');
    statusEl.textContent = 'Gesicht';
  } else {
    draw();
    connect();
  }
})();
</script>
</body>
</html>
)HTML";
