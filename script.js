
const THRESHOLD_DEG = 15;
const HISTORY_LIMIT = 120;
const DATA_POLL_INTERVAL_MS = 400;
const DATA_REQUEST_TIMEOUT_MS = 6500;
const DEVICE_OFFLINE_AFTER_MS = 4000;
let audioContext = null;
let soundEnabled = false;
let currentStatus = null;
let pendingAlertStatus = null;
let countdownDeadline = null;
let history = [];
let lastHistoryAt = 0;
let updatePromise = null;
let lastSuccessAt = null;
let deviceOffline = false;
const dashboardStartedAt = performance.now();
let lastReading = null;
let cameraStream = null;
const localCompanion = location.hostname === '127.0.0.1' || location.hostname === 'localhost';

function playBeepSequence(count) {
  if (!soundEnabled || !audioContext || audioContext.state !== 'running') return false;
  const now = audioContext.currentTime;
  for (let i = 0; i < count; i++) {
    const start = now + i * 0.24;
    const oscillator = audioContext.createOscillator();
    const gain = audioContext.createGain();
    oscillator.type = 'sine';
    oscillator.frequency.setValueAtTime(880, start);
    gain.gain.setValueAtTime(0.0001, start);
    gain.gain.exponentialRampToValueAtTime(0.72, start + 0.012);
    gain.gain.exponentialRampToValueAtTime(0.0001, start + 0.18);
    oscillator.connect(gain);
    gain.connect(audioContext.destination);
    oscillator.start(start);
    oscillator.stop(start + 0.19);
  }
  return true;
}

function playAlertForStatus(status) {
  if (status !== 'WARNING' && status !== 'REMINDER') return;
  if (playBeepSequence(status === 'WARNING' ? 1 : 3)) pendingAlertStatus = null;
  else pendingAlertStatus = status;
}

async function enableSound() {
  const button = document.getElementById('soundButton');
  const message = document.getElementById('message');
  const AudioContextClass = window.AudioContext || window.webkitAudioContext;
  if (!AudioContextClass) {
    message.textContent = 'This browser does not support audio alerts.';
    return;
  }
  try {
    if (!audioContext) audioContext = new AudioContextClass();
    await audioContext.resume();
    soundEnabled = audioContext.state === 'running';
    if (!soundEnabled) {
      message.textContent = 'Sound is blocked by the browser. Check this tab’s sound settings and try again.';
      return;
    }
    button.textContent = 'Sound enabled';
    button.classList.add('enabled');
    button.setAttribute('aria-pressed', 'true');
    message.textContent = 'Sound alerts are enabled for this browser tab.';
    if (pendingAlertStatus === currentStatus) playAlertForStatus(pendingAlertStatus);
    else pendingAlertStatus = null;
  } catch (e) {
    message.textContent = 'Could not enable sound. Check this tab’s sound settings and try again.';
  }
}

function noteStatusChange(status) {
  if (status === currentStatus) return;
  currentStatus = status;
  if (status === 'WARNING' || status === 'REMINDER') {
    pendingAlertStatus = status;
    playAlertForStatus(status);
    if (!soundEnabled) document.getElementById('message').textContent = 'Enable sound to hear browser alerts.';
  } else if (status === 'GOOD' || status === 'ACKNOWLEDGED') {
    pendingAlertStatus = null;
  }
}

function renderCountdown() {
  const timer = document.getElementById('timer');
  const activeWarning = currentStatus === 'WARNING' && countdownDeadline !== null;
  const expired = currentStatus === 'REMINDER';
  const seconds = activeWarning
    ? Math.ceil(Math.max(0, countdownDeadline - performance.now()) / 1000)
    : expired ? 0 : 15;
  timer.classList.toggle('is-warning', activeWarning);
  timer.classList.toggle('is-expired', expired);
  document.getElementById('countdown').textContent = String(seconds).padStart(2, '0') + 's';
  document.getElementById('timerContext').textContent = activeWarning
    ? 'TIME TO ADJUST'
    : expired ? 'TIME EXPIRED' : 'READY WHEN NEEDED';
}

function setCountdown(remainingMs) {
  const value = Number(remainingMs);
  countdownDeadline = performance.now() + (Number.isFinite(value) ? Math.max(0, value) : 15000);
  renderCountdown();
}

function renderAngle(valueId, barId, value) {
  const number = Number(value);
  const formatted = Number.isFinite(number) ? (number > 0 ? '+' : '') + number.toFixed(1) : '—';
  document.getElementById(valueId).textContent = formatted;
  document.getElementById(valueId === 'pitch' ? 'miniPitch' : 'miniRoll').textContent = formatted;
  const bar = document.getElementById(barId);
  if (!Number.isFinite(number)) {
    bar.style.left = '50%'; bar.style.width = '0'; bar.classList.remove('alert'); return;
  }
  const clamped = Math.max(-30, Math.min(30, number));
  const zero = 50;
  const position = ((clamped + 30) / 60) * 100;
  bar.style.left = Math.min(zero, position) + '%';
  bar.style.width = Math.max(1, Math.abs(position - zero)) + '%';
  bar.classList.toggle('alert', Math.abs(number) > THRESHOLD_DEG);
}

function configureCamera() {
  const button = document.getElementById('cameraButton');
  if (!localCompanion) {
    button.disabled = true;
    button.textContent = 'Laptop companion required';
    document.getElementById('cameraPlaceholderTitle').textContent = 'Camera needs the laptop companion';
    document.getElementById('cameraPlaceholderText').textContent = 'The ESP Wi-Fi page is HTTP, so this browser blocks camera access here. Open the companion on your laptop at 127.0.0.1:8787.';
    document.getElementById('cameraMessage').textContent = 'Join PosturePal Wi-Fi on your laptop, then start the included companion.';
    return;
  }
  if (!window.isSecureContext || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    button.disabled = true;
    document.getElementById('cameraPlaceholderTitle').textContent = 'Camera is unavailable in this browser';
    document.getElementById('cameraPlaceholderText').textContent = 'Open the companion using http://127.0.0.1:8787 in a supported browser.';
    document.getElementById('cameraMessage').textContent = 'This browser does not expose camera access on the local page.';
    return;
  }
  button.disabled = false;
  document.getElementById('cameraMessage').textContent = 'Click Start camera and allow access in your browser when prompted.';
}

async function toggleCamera() {
  if (cameraStream) {
    stopCamera();
    return;
  }
  if (!localCompanion || !window.isSecureContext || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    configureCamera();
    return;
  }
  const button = document.getElementById('cameraButton');
  const message = document.getElementById('cameraMessage');
  button.disabled = true;
  message.textContent = 'Waiting for camera permission…';
  try {
    cameraStream = await navigator.mediaDevices.getUserMedia({video:{facingMode:{ideal:'user'}},audio:false});
    const video = document.getElementById('cameraVideo');
    video.srcObject = cameraStream;
    await video.play();
    video.hidden = false;
    document.getElementById('cameraPlaceholder').hidden = true;
    document.getElementById('cameraStage').classList.add('live');
    document.getElementById('cameraBadge').classList.add('active');
    document.getElementById('cameraBadgeText').textContent = 'CAMERA ON';
    button.innerHTML = '<span class="btn-icon">■</span> Stop camera';
    message.textContent = 'Camera is on in this tab. Video stays on your laptop.';
  } catch (error) {
    if (cameraStream) stopCamera();
    const text = error && error.name === 'NotAllowedError'
      ? 'Allow camera access for this local page in your browser settings, then try again.'
      : error && error.name === 'NotFoundError'
        ? 'No camera was found on this laptop.'
        : 'Could not start the camera. Check browser permission and that another app is not using it.';
    document.getElementById('cameraPlaceholderTitle').textContent = 'Camera preview is off';
    document.getElementById('cameraPlaceholderText').textContent = text;
    message.textContent = text;
  } finally {
    button.disabled = false;
  }
}

function stopCamera() {
  if (cameraStream) cameraStream.getTracks().forEach(track => track.stop());
  cameraStream = null;
  const video = document.getElementById('cameraVideo');
  video.pause();
  video.srcObject = null;
  video.hidden = true;
  document.getElementById('cameraPlaceholder').hidden = false;
  document.getElementById('cameraStage').classList.remove('live');
  document.getElementById('cameraBadge').classList.remove('active');
  document.getElementById('cameraBadgeText').textContent = 'CAMERA OFF';
  document.getElementById('cameraButton').innerHTML = '<span class="btn-icon">◉</span> Start camera';
  document.getElementById('cameraMessage').textContent = 'Camera is off. Start it whenever you are ready.';
}

function setConnection(state) {
  const pill = document.getElementById('connection');
  pill.classList.toggle('online', state === 'online');
  pill.classList.toggle('offline', state === 'offline');
  pill.classList.toggle('retrying', state === 'retrying');
  document.getElementById('connectionText').textContent = ({
    online: 'Device online',
    retrying: 'Retrying connection',
    offline: 'Device offline',
    connecting: 'Connecting to device'
  })[state] || 'Connecting to device';
}

function renderStatus(status) {
  const hero = document.getElementById('hero');
  const details = {
    'GOOD': ['good', '✓', 'Comfortable posture is within your calibrated range'],
    'WARNING': ['warning', '↗', 'Adjust your posture before the reminder'],
    'REMINDER': ['reminder', '!', 'Wave above the IR sensor to acknowledge'],
    'ACKNOWLEDGED': ['acknowledged', '✓', 'Reminder acknowledged · monitoring continues'],
    'CALIBRATING': ['neutral', '◎', 'Hold your comfortable normal posture'],
    'SENSOR ERROR': ['neutral', '!', 'Check the MPU6050 connection'],
    'CALIBRATION REQUIRED': ['neutral', '◎', 'Calibrate while sitting comfortably']
  };
  const [tone, icon, detail] = details[status] || ['neutral', '⌁', 'Waiting for posture readings'];
  hero.className = 'panel status-panel hero ' + tone;
  document.getElementById('stateIcon').textContent = icon;
  document.getElementById('statusText').textContent = status || 'UNKNOWN';
  document.getElementById('detail').textContent = detail;
  document.getElementById('stageGood').classList.toggle('current', status === 'GOOD');
  document.getElementById('stageWarning').classList.toggle('current', status === 'WARNING');
  document.getElementById('stageReminder').classList.toggle('current', status === 'REMINDER' || status === 'ACKNOWLEDGED');
  const ready = status !== 'CALIBRATION REQUIRED' && status !== 'SENSOR ERROR' && status !== 'CALIBRATING';
  document.getElementById('calibrationState').classList.toggle('ready', ready);
  document.getElementById('calibrationText').textContent = ready ? 'Calibration active' : (status === 'CALIBRATING' ? 'Calibration in progress' : 'Calibration needed');
  if (status !== 'WARNING') { countdownDeadline = null; renderCountdown(); }
}

function pushHistory(pitch, roll) {
  if (!Number.isFinite(Number(pitch)) || !Number.isFinite(Number(roll))) return;
  const time = Date.now();
  if (time - lastHistoryAt < 1000) return;
  lastHistoryAt = time;
  history.push({time, pitch: Number(pitch), roll: Number(roll)});
  if (history.length > HISTORY_LIMIT) history = history.slice(-HISTORY_LIMIT);
  drawChart();
}

function drawChart() {
  const canvas = document.getElementById('trendChart');
  const rect = canvas.getBoundingClientRect();
  if (!rect.width || !rect.height) return;
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.round(rect.width * ratio);
  canvas.height = Math.round(rect.height * ratio);
  const ctx = canvas.getContext('2d');
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  const w = rect.width, h = rect.height;
  const pad = {l:30,r:8,t:12,b:14};
  const plotW = w-pad.l-pad.r, plotH = h-pad.t-pad.b;
  const rangeEnd = Date.now();
  const rangeStart = rangeEnd - 120000;
  ctx.clearRect(0,0,w,h);
  ctx.font = '9px system-ui, sans-serif';
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  [-30,-15,0,15,30].forEach(v => {
    const y = pad.t + ((30-v)/60)*plotH;
    ctx.beginPath(); ctx.moveTo(pad.l,y); ctx.lineTo(w-pad.r,y);
    ctx.strokeStyle = Math.abs(v) === 15 ? '#735e3e' : '#1c293b';
    ctx.setLineDash(Math.abs(v) === 15 ? [4,4] : []); ctx.stroke(); ctx.setLineDash([]);
    ctx.fillStyle = Math.abs(v) === 15 ? '#c89c55' : '#71839a';
    ctx.fillText((v>0?'+':'')+v, pad.l-7, y);
  });
  if (!history.length) {
    ctx.textAlign='center';ctx.fillStyle='#71839a';ctx.fillText('Readings will appear here as PosturePal is used',pad.l+plotW/2,pad.t+plotH/2);
    return;
  }
  const drawLine = (key,color) => {
    if (history.length < 2) return;
    ctx.beginPath();
    history.forEach((point,i) => {
      const x = pad.l + Math.max(0, Math.min(1, (point.time-rangeStart)/120000))*plotW;
      const y = pad.t + ((30-Math.max(-30,Math.min(30,point[key])))/60)*plotH;
      if (!i) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    });
    ctx.strokeStyle=color;ctx.lineWidth=2;ctx.lineJoin='round';ctx.lineCap='round';ctx.stroke();
  };
  drawLine('pitch','#53d9f4'); drawLine('roll','#f7c766');
  const last=history[history.length-1];
  [['pitch','#53d9f4'],['roll','#f7c766']].forEach(([key,color])=>{
    const x=pad.l+Math.max(0,Math.min(1,(last.time-rangeStart)/120000))*plotW;
    const y=pad.t+((30-Math.max(-30,Math.min(30,last[key])))/60)*plotH;
    ctx.beginPath();ctx.arc(x,y,3,0,Math.PI*2);ctx.fillStyle=color;ctx.fill();
  });
}

async function updateData() {
  if (updatePromise) return updatePromise;
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), DATA_REQUEST_TIMEOUT_MS);
  updatePromise = (async () => {
    try {
      const response = await fetch('/data', {cache:'no-store', signal:controller.signal});
      if (!response.ok) throw new Error('Device response unavailable');
      const d = await response.json();
      const wasOffline = deviceOffline;
      lastSuccessAt = performance.now();
      deviceOffline = false;
      setConnection('online');
      if (wasOffline) document.getElementById('message').textContent = 'Connection restored. Live sensor readings are up to date.';
      noteStatusChange(d.status);
      renderStatus(d.status);
      renderAngle('pitch','pitchBar',d.pitch);
      renderAngle('roll','rollBar',d.roll);
      pushHistory(d.pitch,d.roll);
      lastReading = d;
      if (d.status === 'WARNING') setCountdown(d.remainingMs);
    } catch (e) {
      const lastResponse = lastSuccessAt === null ? dashboardStartedAt : lastSuccessAt;
      const staleFor = performance.now() - lastResponse;
      if (staleFor >= DEVICE_OFFLINE_AFTER_MS) {
        setConnection('offline');
        if (!deviceOffline) {
          deviceOffline = true;
          document.getElementById('statusText').textContent = 'Device offline';
          document.getElementById('detail').textContent = 'No sensor update for a few seconds. Reconnecting automatically…';
          document.getElementById('hero').className = 'panel status-panel hero neutral';
          document.getElementById('stateIcon').textContent = '…';
          document.getElementById('message').textContent = 'Connection interrupted. Reconnect to the PosturePal network; this page will retry automatically.';
        }
      } else {
        // A dropped or slow poll is transient. Keep the last valid readings on screen.
        setConnection('retrying');
      }
    } finally {
      clearTimeout(timeoutId);
    }
  })();
  try { return await updatePromise; }
  finally { updatePromise = null; }
}

async function calibrate() {
  const button = document.getElementById('calibrateButton');
  button.disabled = true;
  document.getElementById('message').textContent = 'Hold your comfortable posture while the sensor calibrates…';
  try {
    const response = await fetch('/calibrate',{method:'POST',cache:'no-store'});
    const result = (await response.text()).trim();
    if (!response.ok || result.toLowerCase().includes('failed')) throw new Error('Calibration failed');
    await updateData();
    document.getElementById('message').textContent = 'Calibration complete. Posture monitoring is active.';
  } catch (e) { document.getElementById('message').textContent = 'Calibration request failed. Check the device connection.'; }
  finally { button.disabled = false; }
}

async function resetCal() {
  const button = document.getElementById('resetButton');
  button.disabled = true;
  try {
    const response = await fetch('/reset',{method:'POST',cache:'no-store'});
    if (!response.ok) throw new Error('Reset request failed');
    await updateData();
    document.getElementById('message').textContent = 'Calibration reset. Calibrate again before monitoring.';
  } catch (e) { document.getElementById('message').textContent = 'Reset failed. Check the device connection.'; }
  finally { button.disabled = false; }
}

configureCamera();
setConnection('connecting');
updateData();
setInterval(updateData,DATA_POLL_INTERVAL_MS);
setInterval(renderCountdown,200);
window.addEventListener('resize',drawChart);
window.addEventListener('beforeunload',stopCamera);
