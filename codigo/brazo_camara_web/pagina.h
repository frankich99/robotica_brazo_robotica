/*
 * ============================================================================
 *  pagina.h  -  Interfaz web del brazo robótico (HTML + CSS + JS embebido)
 * ============================================================================
 *  Se sirve en  http://192.168.4.1/
 *  Incluye: video en vivo, control de 6 servos, centro y parada de seguridad.
 * ============================================================================
 */

#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"html(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b1220">
<title>Brazo robótico ESP32-CAM</title>
<style>
  :root{
    color-scheme: dark;
    --bg:#0b1220; --card:#16233a; --card2:#1e2f4a;
    --line:#2f4568; --txt:#e8f1fb; --muted:#9fb3cc;
    --ok:#3ddc97; --warn:#ffb020; --err:#ff6b6b; --accent:#4da3ff;
  }
  *{box-sizing:border-box}
  body{
    margin:0 auto; padding:18px; max-width:980px;
    font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
    background:var(--bg); color:var(--txt);
  }
  h1{font-size:1.45rem;margin:0 0 4px}
  .sub{color:var(--muted);margin:0 0 16px;font-size:.92rem}
  #status{
    padding:12px 14px;border-radius:10px;background:var(--card2);
    border-left:4px solid var(--muted);margin-bottom:16px;font-size:.92rem;
  }
  #status.ok{border-left-color:var(--ok);color:var(--ok)}
  #status.warn{border-left-color:var(--warn);color:var(--warn)}
  #status.err{border-left-color:var(--err);color:var(--err)}
  .card{
    background:var(--card);border:1px solid var(--line);
    border-radius:14px;padding:16px;margin-bottom:14px;
  }
  .top{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
  h2{font-size:1.05rem;margin:0}
  button{
    padding:10px 16px;border:0;border-radius:9px;cursor:pointer;
    font-size:.95rem;font-weight:600;background:var(--accent);color:#04121f;
    transition:filter .15s;
  }
  button:hover:not(:disabled){filter:brightness(1.12)}
  button:disabled{opacity:.4;cursor:not-allowed}
  button.stop{background:var(--err);color:#2a0a0a}
  button.ghost{background:transparent;border:1px solid var(--line);color:var(--txt)}
  .actions{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0}
  #video{
    display:block;width:100%;max-width:720px;aspect-ratio:4/3;
    object-fit:contain;background:#050a12;border-radius:10px;margin:14px auto 4px;
  }
  .placeholder{
    display:grid;place-items:center;text-align:center;
    width:100%;max-width:720px;aspect-ratio:4/3;margin:14px auto 4px;
    background:#050a12;border-radius:10px;color:var(--muted);padding:24px;
  }
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px}
  .servo{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px}
  .servo .top{margin-bottom:6px}
  .angle{font-size:1.35rem;font-weight:700;color:var(--ok);font-variant-numeric:tabular-nums}
  input[type=range]{
    width:100%;margin:16px 0 6px;accent-color:var(--ok);
    touch-action:pan-y;
  }
  .limits{display:flex;justify-content:space-between;font-size:.8rem;color:var(--muted)}
  .row{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-top:10px}
  .pos{font-size:.82rem;color:var(--muted)}
  .hint{font-size:.85rem;color:var(--muted);line-height:1.5}
  .badge{font-size:.75rem;padding:3px 9px;border-radius:999px;background:var(--card2);color:var(--muted)}
  .badge.on{background:rgba(61,220,151,.18);color:var(--ok)}
  [hidden]{display:none!important}
</style>
</head>
<body>

<h1>🤖 Brazo robótico</h1>
<p class="sub">ESP32-CAM · 6 servos · PCA9685 · Red local sin internet</p>

<div id="status">Conectando con el brazo...</div>

<section class="card">
  <div class="top">
    <h2>📷 Cámara en vivo</h2>
    <div>
      <button id="btnPhoto" class="ghost" disabled>Tomar foto</button>
      <button id="btnVideo" disabled>Iniciar video</button>
    </div>
  </div>
  <img id="video" alt="Video en directo" hidden>
  <div id="placeholder" class="placeholder">Pulsa "Iniciar video" para ver la cámara</div>
  <p id="camInfo" class="hint"></p>
</section>

<div class="actions">
  <button id="btnHome" disabled>Centrar servos activos</button>
  <button id="btnStop" class="stop" disabled>Detener todos</button>
</div>

<div id="servos" class="grid"></div>

<p class="hint" style="margin-top:16px">
  <strong>Seguridad:</strong> activa cada servo por separado y sujeta el brazo.
  Al activar, el servo va a su posición de centro. Si se pierde la conexión,
  los pulsos se desactivan a los 10 s. Desactivar pulsos no corta la
  alimentación: usa un interruptor físico para la fuente de los servos.
</p>

<script>
const $ = id => document.getElementById(id);
const cards   = $('servos');
const statusEl= $('status');
const video   = $('video');
const ph      = $('placeholder');
const btnVideo= $('btnVideo');
const btnPhoto= $('btnPhoto');
const btnHome = $('btnHome');
const btnStop = $('btnStop');
const camInfo = $('camInfo');

let latest = null;
let streamOn = false;
const desired = Array(6).fill(null);
const busy    = Array(6).fill(false);
const timers  = Array(6).fill(null);

async function api(path, data) {
  const res = await fetch(path, {
    method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: new URLSearchParams(data || {}),
    cache: 'no-store'
  });
  let json = {};
  try { json = await res.json(); } catch (e) {}
  if (!res.ok) throw new Error(json.error || ('HTTP ' + res.status));
  return json;
}

function setStatus(text, cls) {
  statusEl.textContent = text;
  statusEl.className = cls || '';
}

function stopVideo() {
  video.removeAttribute('src');
  video.hidden = true;
  ph.hidden = false;
  streamOn = false;
  btnVideo.textContent = 'Iniciar video';
}

btnVideo.addEventListener('click', () => {
  if (streamOn) { stopVideo(); return; }
  video.src = 'http://' + location.hostname + ':81/stream';
  video.hidden = false;
  ph.hidden = true;
  streamOn = true;
  btnVideo.textContent = 'Detener video';
});

video.addEventListener('error', () => {
  stopVideo();
  camInfo.textContent = 'No se recibe video. Revisa alimentación y pulsa Iniciar video.';
});

btnPhoto.addEventListener('click', () => {
  window.open('http://' + location.hostname + ':81/capture', '_blank');
});

function buildCards(state) {
  state.servos.forEach((s, i) => {
    const el = document.createElement('section');
    el.className = 'servo';
    el.innerHTML =
      '<div class="top"><h2></h2><span class="badge">inactivo</span></div>' +
      '<div class="row"><span class="pos">Ángulo</span><span class="angle">--</span></div>' +
      '<input type="range" step="1">' +
      '<div class="limits"><span></span><span></span></div>' +
      '<div class="row"><span class="pos"></span><button class="toggle">Activar</button></div>';

    el.querySelector('h2').textContent = (i + 1) + '. ' + s.name;

    const slider = el.querySelector('input');
    slider.min = s.min;
    slider.max = s.max;
    slider.value = s.target;
    slider.setAttribute('aria-label', 'Ángulo de ' + s.name);

    el.querySelector('.limits span:first-child').textContent = s.min + '°';
    el.querySelector('.limits span:last-child').textContent = s.max + '°';

    slider.addEventListener('input', () => {
      el.querySelector('.angle').textContent = slider.value + '°';
      desired[i] = slider.value;
      clearTimeout(timers[i]);
      timers[i] = setTimeout(() => sendAngle(i), 90);
    });
    slider.addEventListener('change', () => {
      clearTimeout(timers[i]);
      desired[i] = slider.value;
      sendAngle(i);
    });

    el.querySelector('.toggle').addEventListener('click', async () => {
      try {
        const on = latest.servos[i].enabled ? 0 : 1;
        render(await api('/api/enable', {i, on}));
      } catch (e) { setStatus('Error: ' + e.message, 'err'); }
    });

    cards.appendChild(el);
  });
}

function render(state) {
  latest = state;

  if (!state.ready) {
    setStatus('PCA9685 no detectado. Revisa SDA/SCL, GND y alimentación.', 'err');
  } else if (state.camera) {
    setStatus('Sistema listo · PCA9685 y cámara OK', 'ok');
  } else {
    setStatus('Servos OK · cámara no detectada', 'warn');
  }

  const anyOn = state.servos.some(s => s.enabled);
  btnHome.disabled = !state.ready || !anyOn;
  btnStop.disabled = !state.ready || !anyOn;

  btnVideo.disabled = !state.camera;
  btnPhoto.disabled = !state.camera;
  camInfo.textContent = state.camera
    ? (state.psram ? 'PSRAM detectada · VGA 640×480' : 'Sin PSRAM · QVGA 320×240')
    : 'Cámara no disponible.';

  if (!cards.children.length) buildCards(state);

  state.servos.forEach((s, i) => {
    const el = cards.children[i];
    const slider = el.querySelector('input');
    const badge = el.querySelector('.badge');

    slider.disabled = !state.ready || !s.enabled;
    if (document.activeElement !== slider && desired[i] === null) {
      slider.value = s.target;
      el.querySelector('.angle').textContent = s.target + '°';
    }
    el.querySelector('.pos:last-of-type').textContent = 'Enviado: ' + s.angle + '°';
    badge.textContent = s.enabled ? 'activo' : 'inactivo';
    badge.className = 'badge' + (s.enabled ? ' on' : '');
    const btn = el.querySelector('.toggle');
    btn.textContent = s.enabled ? 'Desactivar' : 'Activar';
    btn.disabled = !state.ready;
  });
}

async function sendAngle(i) {
  if (busy[i]) return;
  busy[i] = true;
  try {
    while (desired[i] !== null) {
      const angle = desired[i];
      desired[i] = null;
      render(await api('/api/servo', {i, angle}));
    }
  } catch (e) {
    desired[i] = null;
    setStatus('Error: ' + e.message, 'err');
  } finally { busy[i] = false; }
}

btnHome.addEventListener('click', async () => {
  try { render(await api('/api/home')); }
  catch (e) { setStatus('Error: ' + e.message, 'err'); }
});

btnStop.addEventListener('click', async () => {
  try { render(await api('/api/stop')); }
  catch (e) { setStatus('Error: ' + e.message, 'err'); }
});

async function refresh() {
  try {
    const res = await fetch('/api/status', {cache:'no-store'});
    if (!res.ok) throw new Error('HTTP ' + res.status);
    render(await res.json());
  } catch (e) {
    setStatus('Sin conexión con el brazo: ' + e.message, 'err');
  }
}

refresh();
setInterval(refresh, 1500);
setInterval(async () => {
  if (!latest || !latest.servos.some(s => s.enabled)) return;
  try { render(await api('/api/heartbeat')); } catch (e) {}
}, 2000);
</script>
</body>
</html>
)html";
