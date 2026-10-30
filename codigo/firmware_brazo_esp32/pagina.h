#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"html(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <link rel="icon" href="data:,">
  <title>Brazo ESP32-CAM · Cámara y servos</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; background: #101a24; color: #edf5fc; }
    * { box-sizing: border-box; }
    body { max-width: 860px; margin: auto; padding: 16px; }
    h1 { font-size: 1.5rem; margin-bottom: 4px; }
    p { color: #bdcdd9; margin: 8px 0 18px; }
    #status { padding: 12px; border-radius: 10px; background: #29435b; margin: 16px 0; }
    #status.error { background: #743039; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(270px,1fr)); gap: 12px; }
    .card { background: #1d2d3b; border: 1px solid #405365; border-radius: 12px; padding: 16px; }
    .top { display: flex; justify-content: space-between; align-items: center; gap: 12px; }
    h2 { font-size: 1.1rem; margin: 0; }
    .value { font-size: 1.4rem; font-weight: bold; color: #8ce5c2; }
    input[type=range] { width: 100%; margin: 26px 0 8px; accent-color: #70dec0; touch-action: pan-y; }
    .limits { display: flex; justify-content: space-between; color: #bdcdd9; font-size: .85rem; }
    button { padding: 12px 18px; border: 0; border-radius: 9px; font-size: 1rem; font-weight: bold; background: #79dfc0; color: #102322; cursor: pointer; }
    button:disabled { opacity: .45; cursor: not-allowed; }
    .actions { display: flex; flex-wrap: wrap; gap: 10px; margin: 20px 0; }
    .stop { background: #fc8888; color: #331116; }
    .hint { font-size: .9rem; }
    .camera { background: #1d2d3b; border: 1px solid #405365; border-radius: 12px; padding: 16px; margin-bottom: 18px; }
    .camera img { display: block; width: 100%; max-width: 640px; aspect-ratio: 4/3; object-fit: contain; background: #070c11; border-radius: 8px; margin: 12px auto; }
    .camera .placeholder { display: grid; place-items: center; text-align: center; width: 100%; max-width: 640px; aspect-ratio: 4/3; background: #070c11; color: #bdcdd9; border-radius: 8px; padding: 20px; margin: 12px auto; }
    .camera .top { flex-wrap: wrap; }
    .camera button { padding: 9px 14px; }
    [hidden] { display: none !important; }
  </style>
</head>
<body>
  <h1>Control del brazo robótico</h1>
  <p>ESP32-CAM + PCA9685 · Video y control local por Wi-Fi · 6 servos</p>
  <div id="status" role="status">Conectando...</div>
  <section class="camera">
    <div class="top"><h2>Cámara en vivo</h2><button id="toggle-video" disabled>Iniciar video</button></div>
    <img id="video" alt="Video en directo de la cámara del brazo" hidden>
    <div id="camera-placeholder" class="placeholder">Aquí aparecerá el video de la ESP32-CAM al grabar el programa.</div>
    <p id="camera-status" role="status">Comprobando cámara y PSRAM...</p>
  </section>
  <div class="actions">
    <button id="home" disabled>Centro (servos activos)</button>
    <button id="stop" class="stop" disabled>Desactivar todos los servos</button>
  </div>
  <div id="servos" class="grid"></div>
  <p class="hint">Activa cada servo por separado. Sujeta el brazo: al activar va al centro configurado (90°). Si se pierde la conexión, los pulsos se desactivan tras 10 s. Desactivar pulsos NO corta la alimentación ni frena el brazo: instala un interruptor físico para la fuente de los servos.</p>
  <script>
    const cards = document.getElementById('servos');
    const indicator = document.getElementById('status');
    const home = document.getElementById('home');
    const stop = document.getElementById('stop');
    const video = document.getElementById('video');
    const cameraPlaceholder = document.getElementById('camera-placeholder');
    const cameraStatus = document.getElementById('camera-status');
    const toggleVideo = document.getElementById('toggle-video');
    let latest = null;
    let liveVideo = true;
    let streamStarted = false;
    const desired = Array(6).fill(null);
    const sending = Array(6).fill(false);
    const timers = Array(6).fill(null);

    async function request(path, data) {
      const response = await fetch(path, {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams(data),
        cache: 'no-store'
      });
      const result = await response.json();
      if (!response.ok) throw Error(result.error || 'Error HTTP ' + response.status);
      return result;
    }

    function error(message) {
      indicator.textContent = message;
      indicator.classList.add('error');
    }

    function updateVideo() {
      const available = latest && latest.camera;
      toggleVideo.disabled = !available;
      toggleVideo.textContent = streamStarted ? 'Pausar video' : 'Iniciar video';
      if (!available) {
        if (streamStarted) { video.removeAttribute('src'); streamStarted = false; }
        video.hidden = true;
        cameraPlaceholder.hidden = false;
        cameraStatus.textContent = latest && latest.preview
          ? 'Vista previa local: no hay imagen real. Requiere grabar la ESP32-CAM.'
          : 'Cámara no detectada. Revisa cable flex, PSRAM y alimentación; reinicia la placa.';
        return;
      }
      if (liveVideo && !streamStarted) {
        video.src = `http://${location.hostname}:81/stream`;
        video.hidden = false;
        cameraPlaceholder.hidden = true;
        streamStarted = true;
      }
      if (!liveVideo) { video.hidden = true; cameraPlaceholder.hidden = false; cameraStatus.textContent = 'Video pausado.'; }
      else cameraStatus.textContent = latest.psram
        ? 'PSRAM detectada · 640 × 480 (VGA)'
        : 'Sin PSRAM · 320 × 240 (QVGA)';
      toggleVideo.textContent = streamStarted ? 'Pausar video' : 'Iniciar video';
    }
    video.addEventListener('error', () => {
      cameraStatus.textContent = 'No se recibe video. Comprueba conexión y alimentación; pulsa Iniciar video para reintentar.';
      liveVideo = false;
      streamStarted = false;
      video.removeAttribute('src');
      video.hidden = true;
      cameraPlaceholder.hidden = false;
      toggleVideo.textContent = 'Iniciar video';
    });
    toggleVideo.addEventListener('click', () => {
      liveVideo = !streamStarted;
      if (!liveVideo) { video.removeAttribute('src'); streamStarted = false; }
      updateVideo();
    });

    function render(state) {
      latest = state;
      indicator.classList.toggle('error', !state.ready);
      indicator.textContent = state.preview
        ? 'VISTA PREVIA: controles simulados; ningún servo físico responde'
        : state.ready
        ? 'PCA9685 conectado · Control disponible'
        : 'PCA9685 no detectado. Revisa cableado y reinicia la placa.';
      updateVideo();
      home.disabled = !state.ready || !state.servos.some(s => s.enabled);
      stop.disabled = !state.ready || !state.servos.some(s => s.enabled);
      if (!cards.children.length) {
        state.servos.forEach((s, i) => {
          const card = document.createElement('section');
          card.className = 'card';
          card.innerHTML = `<div class="top"><h2></h2><span class="value"></span></div>
            <input type="range" aria-label="Ángulo de servo" step="1">
            <div class="limits"><span></span><span></span></div>
            <p class="position"></p><button class="toggle"></button>`;
          card.querySelector('h2').textContent = `${i + 1}. ${s.name}`;
          const slider = card.querySelector('input');
          slider.setAttribute('aria-label', `Ángulo de ${s.name}`);
          slider.min = s.min;
          slider.max = s.max;
          card.querySelector('.limits span:first-child').textContent = `${s.min}°`;
          card.querySelector('.limits span:last-child').textContent = `${s.max}°`;
          slider.addEventListener('input', () => {
            card.querySelector('.value').textContent = `${slider.value}°`;
            desired[i] = slider.value;
            clearTimeout(timers[i]);
            timers[i] = setTimeout(() => sendAngle(i), 100);
          });
          slider.addEventListener('change', () => {
            clearTimeout(timers[i]);
            desired[i] = slider.value;
            sendAngle(i);
          });
          card.querySelector('.toggle').addEventListener('click', async () => {
            try {
              render(await request('/api/enable', {i, on: latest.servos[i].enabled ? 0 : 1}));
            } catch (e) { error(e.message); }
          });
          cards.append(card);
        });
      }
      state.servos.forEach((s, i) => {
        const card = cards.children[i];
        const slider = card.querySelector('input');
        slider.disabled = !state.ready || !s.enabled;
        if (document.activeElement !== slider && desired[i] === null) {
          slider.value = s.target;
          card.querySelector('.value').textContent = `${s.target}°`;
        }
        card.querySelector('.position').textContent = `Posición enviada: ${s.angle}°`;
        card.querySelector('.toggle').textContent = s.enabled ? 'Desactivar' : 'Activar';
        card.querySelector('.toggle').disabled = !state.ready;
      });
    }

    async function sendAngle(i) {
      if (sending[i]) return;
      sending[i] = true;
      try {
        while (desired[i] !== null) {
          const angle = desired[i];
          desired[i] = null;
          render(await request('/api/servo', {i, angle}));
        }
      } catch (e) {
        desired[i] = null;
        error(e.message);
      } finally { sending[i] = false; }
    }

    home.addEventListener('click', async () => {
      try { render(await request('/api/home', {})); }
      catch (e) { error(e.message); }
    });
    stop.addEventListener('click', async () => {
      try { render(await request('/api/stop', {})); }
      catch (e) { error(e.message); }
    });
    async function refresh() {
      try {
        const response = await fetch('/api/status', {cache: 'no-store'});
        if (!response.ok) throw Error('HTTP ' + response.status);
        render(await response.json());
      } catch (e) { error('Sin conexión con ESP32-CAM: ' + e.message); }
    }
    refresh();
    setInterval(refresh, 1500);
    setInterval(async () => {
      if (!latest || !latest.servos.some(s => s.enabled)) return;
      try { render(await request('/api/heartbeat', {})); }
      catch (e) { error('Sin conexión: ' + e.message); }
    }, 2000);
  </script>
</body>
</html>
)html";
