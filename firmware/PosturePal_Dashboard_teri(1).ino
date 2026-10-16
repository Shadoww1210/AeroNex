
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <math.h>

// LED pins
const int GREEN_LED  = D5;
const int YELLOW_LED = D6;
const int RED_LED    = D7;

// Digital IR obstacle sensor (OUT pin)
// Most common modules output LOW when a hand/object is detected.
const int IR_PIN = D0;
const bool IR_ACTIVE_LOW = true;

// MPU6050
const int MPU_ADDR = 0x68;

// Wi-Fi dashboard
const char* AP_NAME = "PosturePal";
const char* AP_PASS = "Posture123";

ESP8266WebServer server(80);

float basePitch = 0, baseRoll = 0;
float smoothPitch = 0, smoothRoll = 0;
float dp = 0, dr = 0;

bool calibrated = false;
bool timerRunning = false;
bool reminderAcknowledged = false;
bool sensorOK = false;

unsigned long badStart = 0;
unsigned long lastIRTime = 0;

bool previousIRDetected = false;

const float TILT_THRESHOLD = 15.0;
const unsigned long BAD_TIME = 15000;
const unsigned long IR_DEBOUNCE = 350;

String statusText = "CALIBRATION REQUIRED";

void setLEDs(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green);
  digitalWrite(YELLOW_LED, yellow);
  digitalWrite(RED_LED, red);
}

bool readAngles(float &p, float &r) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 6) != 6) return false;

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  p = atan2((float)ax,
       sqrt((float)ay * ay + (float)az * az))
       * 180.0 / PI;

  r = atan2((float)ay,
       sqrt((float)ax * ax + (float)az * az))
       * 180.0 / PI;

  return true;
}

void calibratePosture() {
  Serial.println("Hold your comfortable normal posture...");
  statusText = "CALIBRATING";
  setLEDs(false, true, false);

  float sumP = 0, sumR = 0;
  int count = 0;

  for (int i = 0; i < 100; i++) {
    float p, r;

    if (readAngles(p, r)) {
      sumP += p;
      sumR += r;
      count++;
    }

    server.handleClient();
    delay(20);
  }

  if (count < 80) {
    calibrated = false;
    sensorOK = false;
    statusText = "SENSOR ERROR";
    setLEDs(false, false, true);
    Serial.println("Calibration failed. Check MPU6050.");
    return;
  }

  basePitch = sumP / count;
  baseRoll = sumR / count;
  smoothPitch = basePitch;
  smoothRoll = baseRoll;

  dp = 0;
  dr = 0;
  calibrated = true;
  timerRunning = false;
  reminderAcknowledged = false;
  statusText = "GOOD";

  setLEDs(true, false, false);
  Serial.println("Calibration complete!");
}

void resetCalibration() {
  calibrated = false;
  timerRunning = false;
  reminderAcknowledged = false;
  statusText = "CALIBRATION REQUIRED";
  setLEDs(false, false, false);

  Serial.println("Calibration cleared.");
}

void handleRoot() {
  server.send(200, "text/html", R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#f4f7f8">
<title>PosturePal | Aeronox</title>
<style>
:root{color-scheme:light;--ink:#132a35;--muted:#71838c;--line:#e3eaeb;--paper:#fff;--canvas:#f4f7f8;--teal:#087f78;--teal2:#18aa9d;--mint:#e1f5ef;--amber:#a76308;--amber-bg:#fff3dc;--red:#b3454d;--red-bg:#fdebed;--blue:#466db4;--blue-bg:#eaf0fc;--shadow:0 12px 36px #25434c0b}
*{box-sizing:border-box}body{margin:0;background:var(--canvas);font-family:Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--ink);min-height:100vh}button{font:inherit}.shell{width:min(1180px,100% - 40px);margin:0 auto;padding:28px 0 36px}.institution{display:flex;align-items:center;gap:14px;padding:0 0 23px;border-bottom:1px solid var(--line)}.seal{width:42px;height:42px;flex:none;border-radius:13px;background:#e5f2ef;color:var(--teal);display:grid;place-items:center;font-weight:900;font-size:13px;letter-spacing:-.5px}.institution-name{font-size:13px;line-height:1.45;font-weight:750;letter-spacing:.05px}.institution-sub{font-size:11px;color:var(--muted);margin-top:3px}.byline{margin-left:auto;color:var(--muted);font-size:11px;letter-spacing:.5px;text-align:right}.byline strong{display:block;color:var(--teal);font-size:12px;letter-spacing:1.4px;margin-top:4px}.topbar{display:flex;align-items:center;gap:13px;padding:23px 0}.brand-icon{width:46px;height:46px;border-radius:15px;background:var(--teal);color:#fff;display:grid;place-items:center;box-shadow:0 7px 17px #087f7827}.brand-icon svg{width:27px;height:27px}.brand h1{margin:0;font-size:25px;line-height:1.1;letter-spacing:-.8px}.brand p{margin:5px 0 0;color:var(--muted);font-size:12px}.top-right{margin-left:auto;display:flex;align-items:center;gap:15px}.ap-address{color:var(--muted);font-size:11px;text-align:right}.ap-address strong{display:block;color:var(--ink);font-size:12px;margin-top:4px}.connection{display:inline-flex;align-items:center;gap:7px;border:1px solid var(--line);background:#fff;border-radius:999px;padding:8px 11px;font-size:11px;font-weight:750;white-space:nowrap}.conn-dot{width:8px;height:8px;border-radius:50%;background:#d59b36}.connection.online{color:#17745e;background:#edf8f3;border-color:#d8eee5}.connection.online .conn-dot{background:#26a879;box-shadow:0 0 0 3px #26a8791b}.connection.offline{color:#9e3f45;background:#fff1f1;border-color:#f2d8da}.connection.offline .conn-dot{background:#d15c63}.grid{display:grid;grid-template-columns:minmax(0,1.55fr) minmax(290px,.85fr);gap:16px}.card{background:var(--paper);border:1px solid var(--line);border-radius:18px;box-shadow:var(--shadow)}.hero{position:relative;overflow:hidden;padding:23px 25px 24px;background:linear-gradient(115deg,#102e38,#174d50);color:#f5fffd;border:0;min-height:190px}.hero:after{content:"";position:absolute;width:230px;height:230px;border:1px solid #ffffff12;border-radius:50%;right:-83px;top:-94px;box-shadow:0 0 0 24px #ffffff08,0 0 0 48px #ffffff05}.hero-top{position:relative;z-index:1;display:flex;align-items:center;gap:8px;color:#a9d4cc;font-size:10px;letter-spacing:1.5px;font-weight:850}.status-mark{width:7px;height:7px;border-radius:50%;background:#78d9b4;box-shadow:0 0 12px #78d9b4}.hero-main{position:relative;z-index:1;display:flex;align-items:center;gap:18px;margin-top:21px}.state-icon{width:47px;height:47px;border-radius:15px;background:#ffffff17;border:1px solid #ffffff20;display:grid;place-items:center;font-size:21px;flex:none}.state-copy{min-width:0}.state-copy h2{font-size:clamp(25px,4vw,34px);line-height:1;margin:0;letter-spacing:-.8px}.state-copy p{font-size:13px;color:#c2ded9;margin:9px 0 0}.timer{margin-left:auto;min-width:106px;padding:10px 13px;border-radius:13px;border:1px solid #ffffff30;background:#ffffff12;text-align:center}.timer[hidden]{display:none}.timer-label{display:block;color:#c2ded9;font-size:9px;letter-spacing:1px;font-weight:800}.timer-value{display:block;font-size:24px;line-height:1.2;font-weight:850;font-variant-numeric:tabular-nums}.hero.warning{background:linear-gradient(115deg,#604719,#95621b)}.hero.warning .status-mark{background:#ffcf72}.hero.warning .state-copy p{color:#ffebc2}.hero.warning .timer{background:#281b08aa;border-color:#ffdfa44d}.hero.reminder{background:linear-gradient(115deg,#702f39,#a74450)}.hero.reminder .status-mark{background:#ffb6b3}.hero.acknowledged{background:linear-gradient(115deg,#264a79,#4169a4)}.hero.neutral{background:linear-gradient(115deg,#354a54,#526970)}.hero.good{background:linear-gradient(115deg,#102e38,#174d50)}.section-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:0 0 15px}.section-head h3{margin:0;font-size:14px;letter-spacing:-.15px}.section-kicker{color:#93a2a9;font-size:9px;font-weight:850;letter-spacing:1.35px}.readings{padding:20px 21px}.reading-list{display:grid;gap:13px}.reading{padding:14px 15px 12px;background:#f8faf9;border:1px solid #edf1f0;border-radius:14px}.reading-head{display:flex;align-items:baseline;gap:8px}.reading-name{font-size:12px;color:var(--muted);font-weight:650}.reading-value{margin-left:auto;font-size:25px;line-height:1;font-weight:820;letter-spacing:-.8px;font-variant-numeric:tabular-nums}.reading-value small{font-size:13px;color:var(--teal);font-weight:700;margin-left:2px}.track{height:7px;background:#e9efed;border-radius:99px;position:relative;margin:13px 2px 7px}.track:before,.track:after{content:"";position:absolute;top:-3px;width:1px;height:13px;background:#d49a39}.track:before{left:25%}.track:after{right:25%}.range-fill{position:absolute;top:0;height:100%;border-radius:99px;background:linear-gradient(90deg,#59b89a,#0a877c);min-width:4px}.range-fill.alert{background:linear-gradient(90deg,#d2a04e,#bd5a4f)}.ticks{display:flex;justify-content:space-between;color:#9aa8ad;font-size:9px}.threshold-note{margin:13px 0 0;color:var(--muted);font-size:10px;line-height:1.45}.right-column{display:grid;grid-template-rows:auto 1fr;gap:16px}.actions{padding:20px}.calibration-state{display:flex;align-items:center;gap:8px;background:#f4f8f7;border:1px solid #e8efed;border-radius:11px;padding:10px 11px;color:var(--muted);font-size:11px;margin-bottom:14px}.calibration-state .tiny-dot{width:7px;height:7px;border-radius:50%;background:#d59b36;flex:none}.calibration-state.ready .tiny-dot{background:#28a477}.action-buttons{display:grid;gap:9px}.btn{display:flex;align-items:center;justify-content:center;gap:8px;border:0;border-radius:11px;padding:12px 13px;min-height:43px;font-size:12px;font-weight:780;cursor:pointer;transition:transform .13s,filter .13s}.btn:hover{filter:brightness(.97)}.btn:active{transform:scale(.99)}.btn:disabled{opacity:.6;cursor:wait}.btn-primary{background:var(--teal);color:#fff;box-shadow:0 5px 12px #087f7824}.btn-secondary{background:#f2f6f5;color:#29434a;border:1px solid #e2eae8}.btn-icon{font-size:14px}.sound-line{display:flex;align-items:center;gap:10px;border-top:1px solid var(--line);padding-top:14px;margin-top:14px}.sound-copy{min-width:0}.sound-copy strong{display:block;font-size:11px}.sound-copy span{display:block;color:var(--muted);font-size:10px;margin-top:3px}.sound-btn{margin-left:auto;flex:none;border:1px solid #dbe8e4;background:#eff8f4;color:#21755f;border-radius:9px;padding:8px 10px;font-size:10px;font-weight:800;cursor:pointer}.sound-btn.enabled{background:#e5f5ed;color:#177453}.feedback{min-height:14px;margin:9px 0 0;color:var(--muted);font-size:10px;line-height:1.45}.trend{padding:20px 21px;min-height:290px;display:flex;flex-direction:column}.trend-meta{display:flex;align-items:center;gap:11px;color:var(--muted);font-size:10px}.legend{display:flex;gap:12px;align-items:center}.legend span{display:flex;align-items:center;gap:5px}.legend-i{width:7px;height:7px;border-radius:50%;background:#0c9184}.legend-i.roll{background:#557fc2}.canvas-wrap{position:relative;flex:1;min-height:184px;margin-top:8px}.canvas-wrap canvas{width:100%;height:100%;position:absolute;inset:0}.chart-foot{display:flex;justify-content:space-between;color:#9aa8ad;font-size:9px;margin:4px 1px 0}.team{border-top:1px solid var(--line);margin-top:21px;padding-top:15px;display:flex;align-items:center;gap:12px;color:var(--muted);font-size:10px}.team-label{color:var(--teal);font-weight:850;font-size:9px;letter-spacing:1.2px}.team-members{color:#51666d}.team-members strong{color:#2d454d}.footer{margin-left:auto;color:#99a7ac;text-align:right}
@media(max-width:800px){.grid{grid-template-columns:1fr}.right-column{grid-template-columns:1fr 1fr;grid-template-rows:auto}.trend{min-height:290px}.actions{align-self:stretch}.shell{width:min(100% - 30px,660px)}}
@media(max-width:540px){.shell{width:min(100% - 22px,500px);padding-top:17px}.institution{gap:10px;padding-bottom:16px}.seal{width:36px;height:36px;border-radius:11px}.institution-name{font-size:11px}.institution-sub{font-size:9px}.byline{font-size:9px}.byline strong{font-size:10px;letter-spacing:1px}.topbar{gap:10px;padding:17px 0}.brand-icon{width:41px;height:41px;border-radius:13px}.brand h1{font-size:22px}.brand p{font-size:10px}.top-right{gap:7px;flex-direction:column;align-items:flex-end}.ap-address{display:none}.connection{padding:7px 9px;font-size:10px}.grid{gap:12px}.hero{padding:19px 17px 20px;min-height:170px}.hero-main{gap:12px;margin-top:19px}.state-icon{width:41px;height:41px;border-radius:13px}.state-copy h2{font-size:25px}.state-copy p{font-size:11px;line-height:1.4;max-width:190px}.timer{min-width:82px;padding:8px 9px}.timer-value{font-size:22px}.readings,.actions,.trend{padding:17px}.right-column{grid-template-columns:1fr}.trend{min-height:270px}.team{align-items:flex-start;flex-wrap:wrap;gap:7px 10px}.footer{margin-left:0;width:100%;text-align:left}.section-head h3{font-size:13px}}
.camera-card{margin-top:14px;padding:18px 20px}.camera-heading{display:flex;align-items:center;gap:10px}.camera-heading-copy{min-width:0}.camera-heading-copy h3{margin:0;font-size:14px}.camera-heading-copy p{margin:4px 0 0;color:var(--muted);font-size:10px}.camera-stage{position:relative;overflow:hidden;aspect-ratio:16/9;min-height:180px;max-height:370px;margin-top:13px;border-radius:13px;background:#102027;display:grid;place-items:center}.camera-stage video{width:100%;height:100%;position:absolute;inset:0;object-fit:cover;transform:scaleX(-1)}.camera-stage video[hidden]{display:none}.camera-placeholder{position:relative;z-index:1;max-width:320px;padding:20px;color:#dbe7e5;text-align:center;font-size:11px;line-height:1.55}.camera-placeholder[hidden]{display:none}.camera-placeholder-icon{width:42px;height:42px;border:1px solid #ffffff30;border-radius:13px;display:grid;place-items:center;margin:0 auto 11px;font-size:20px;color:#9fd8cb}.camera-badge{position:absolute;z-index:2;top:10px;left:10px;display:flex;align-items:center;gap:6px;padding:6px 8px;border-radius:999px;background:#071519b8;color:#e8f4f1;font-size:9px;font-weight:800;letter-spacing:.6px}.camera-badge i{width:6px;height:6px;border-radius:50%;background:#9aa8a7}.camera-badge.active i{background:#f16060;box-shadow:0 0 0 3px #f1606033}.camera-controls{display:flex;align-items:center;gap:12px;margin-top:12px}.camera-controls .btn{flex:none;min-width:132px}.camera-message{color:var(--muted);font-size:10px;line-height:1.45;margin:0}.camera-tilt{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:13px}.camera-mini{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:9px 11px;border:1px solid #e8efed;border-radius:11px;background:#f8faf9}.camera-mini span{color:var(--muted);font-size:10px}.camera-mini strong{font-size:16px;letter-spacing:-.3px;font-variant-numeric:tabular-nums}.camera-mini small{font-size:10px;color:var(--teal);margin-left:2px}.camera-local-note{color:#8a9a9f;font-size:9px;line-height:1.4;margin:9px 1px 0}.camera-stage.live:after{content:"";position:absolute;inset:0;pointer-events:none;box-shadow:inset 0 0 0 1px #ffffff1a;border-radius:13px}
@media(max-width:540px){.camera-card{padding:16px}.camera-stage{min-height:165px}.camera-controls{align-items:flex-start;flex-direction:column;gap:8px}.camera-controls .btn{width:100%}}
</style>
</head>
<body>
<main class="shell">
  <header class="institution">
    <div class="seal" aria-hidden="true">MMUT</div>
    <div><div class="institution-name">Madan Mohan Malaviya University of Technology</div><div class="institution-sub">Gorakhpur · Student Innovation Project</div></div>
    <div class="byline">PROJECT BY<strong>AERONOX</strong></div>
  </header>
  <section class="topbar">
    <div class="brand-icon" aria-hidden="true"><svg viewBox="0 0 32 32" fill="none"><circle cx="16" cy="6" r="3" fill="currentColor"/><path d="M16 11v8m0-5-7 4m7-4 7 4m-7 1-5 9m5-9 5 9" stroke="currentColor" stroke-width="2.3" stroke-linecap="round" stroke-linejoin="round"/></svg></div>
    <div class="brand"><h1>PosturePal</h1><p>Posture feedback, right when you need it.</p></div>
    <div class="top-right"><div class="ap-address">LOCAL DASHBOARD<strong>192.168.4.1</strong></div><div id="connection" class="connection"><span class="conn-dot"></span><span id="connectionText">Connecting</span></div></div>
  </section>

  <div class="grid">
    <div class="left-column">
      <section id="hero" class="card hero neutral" aria-live="polite">
        <div class="hero-top"><span class="status-mark"></span><span>LIVE POSTURE STATUS</span></div>
        <div class="hero-main"><div class="state-icon" id="stateIcon" aria-hidden="true">⌁</div><div class="state-copy"><h2 id="statusText">Connecting…</h2><p id="detail">Waiting for the posture sensor</p></div><div id="timer" class="timer" hidden><span class="timer-label">ADJUST IN</span><span class="timer-value" id="countdown">15s</span></div></div>
      </section>

      <section class="card camera-card">
        <div class="section-head"><div class="camera-heading"><div class="camera-heading-copy"><h3>Camera posture view</h3><p>See your sitting position alongside live sensor tilt</p></div></div></div>
        <div id="cameraStage" class="camera-stage"><video id="cameraVideo" autoplay playsinline muted hidden></video><div id="cameraPlaceholder" class="camera-placeholder"><div class="camera-placeholder-icon">◉</div><strong id="cameraPlaceholderTitle">Camera preview is off</strong><br><span id="cameraPlaceholderText">Start the laptop companion to preview your camera here.</span></div><span id="cameraBadge" class="camera-badge"><i></i><span id="cameraBadgeText">CAMERA OFF</span></span></div>
        <div class="camera-controls"><button id="cameraButton" class="btn btn-secondary" type="button" onclick="toggleCamera()"><span class="btn-icon">◉</span> Start camera</button><p id="cameraMessage" class="camera-message">Camera permission is requested only after you start it.</p></div>
        <div class="camera-tilt"><div class="camera-mini"><span>Pitch</span><strong><span id="miniPitch">—</span><small>°</small></strong></div><div class="camera-mini"><span>Roll</span><strong><span id="miniRoll">—</span><small>°</small></strong></div></div>
        <p class="camera-local-note">The preview stays on this laptop. Camera video and audio are not sent to the ESP8266; this page does not request microphone access.</p>
      </section>

      <section class="card readings" style="margin-top:14px">
        <div class="section-head"><h3>Live tilt readings</h3><span class="section-kicker">CHANGE FROM CALIBRATED POSTURE</span></div>
        <div class="reading-list">
          <div class="reading"><div class="reading-head"><span class="reading-name">Pitch · forward / back</span><span class="reading-value"><span id="pitch">—</span><small>°</small></span></div><div class="track"><div id="pitchBar" class="range-fill" style="left:50%;width:0"></div></div><div class="ticks"><span>−30°</span><span>−15°</span><span>0°</span><span>+15°</span><span>+30°</span></div></div>
          <div class="reading"><div class="reading-head"><span class="reading-name">Roll · side to side</span><span class="reading-value"><span id="roll">—</span><small>°</small></span></div><div class="track"><div id="rollBar" class="range-fill" style="left:50%;width:0"></div></div><div class="ticks"><span>−30°</span><span>−15°</span><span>0°</span><span>+15°</span><span>+30°</span></div></div>
        </div>
        <p class="threshold-note">Warning begins when either angle changes by more than <strong>15°</strong> from your calibration.</p>
      </section>

      <section class="card trend" style="margin-top:14px">
        <div class="section-head"><h3>Recent posture trend</h3><div class="trend-meta"><div class="legend"><span><i class="legend-i"></i>Pitch</span><span><i class="legend-i roll"></i>Roll</span></div></div></div>
        <div class="canvas-wrap"><canvas id="trendChart" aria-label="Recent pitch and roll readings with the plus and minus 15 degree warning threshold"></canvas></div>
        <div class="chart-foot"><span>Previous 2 minutes</span><span>Updates while this page is open</span><span>Now</span></div>
      </section>
    </div>

    <aside class="right-column">
      <section class="card actions">
        <div class="section-head"><h3>Device controls</h3><span class="section-kicker">POSTUREPAL</span></div>
        <div id="calibrationState" class="calibration-state"><span class="tiny-dot"></span><span id="calibrationText">Calibration needed</span></div>
        <div class="action-buttons"><button id="calibrateButton" class="btn btn-primary" type="button" onclick="calibrate()"><span class="btn-icon">◎</span> Calibrate normal posture</button><button id="resetButton" class="btn btn-secondary" type="button" onclick="resetCal()"><span class="btn-icon">↺</span> Reset calibration</button></div>
        <div class="sound-line"><div class="sound-copy"><strong>Browser sound alerts</strong><span>One beep for warning · three for reminder</span></div><button id="soundButton" class="sound-btn" type="button" onclick="enableSound()">Enable sound</button></div>
        <p id="message" class="feedback" aria-live="polite">Enable sound from this page to hear alerts.</p>
      </section>
      <section class="card actions">
        <div class="section-head"><h3>How to connect</h3><span class="section-kicker">QUICK START</span></div>
        <div style="font-size:11px;line-height:1.7;color:var(--muted)"><div><strong style="color:var(--ink)">1.</strong> Join the <strong style="color:var(--ink)">PosturePal</strong> Wi‑Fi network.</div><div><strong style="color:var(--ink)">2.</strong> Open <strong style="color:var(--ink)">http://192.168.4.1</strong> in your browser.</div><div><strong style="color:var(--ink)">3.</strong> Calibrate while sitting in your comfortable posture.</div><div style="margin-top:11px;padding:9px 10px;border-radius:10px;background:#f5f8f7;color:#52666b">When REMINDER appears, wave above the IR sensor to acknowledge it. Monitoring continues.</div></div>
      </section>
    </aside>
  </div>

  <footer class="team"><span class="team-label">AERONOX</span><span class="team-members"><strong>Utkarsh Tiwari</strong> · Team Leader</span><span class="team-members"><strong>Shivansh Tripathi</strong> · Member</span><span class="team-members"><strong>Zaid</strong> · Member</span><span class="footer">Madan Mohan Malaviya University of Technology · Gorakhpur</span></footer>
</main>
<script>
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
  if (currentStatus !== 'WARNING' || countdownDeadline === null) {
    timer.hidden = true;
    return;
  }
  const seconds = Math.ceil(Math.max(0, countdownDeadline - performance.now()) / 1000);
  document.getElementById('countdown').textContent = seconds + 's';
  timer.hidden = false;
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
  hero.className = 'card hero ' + tone;
  document.getElementById('stateIcon').textContent = icon;
  document.getElementById('statusText').textContent = status || 'UNKNOWN';
  document.getElementById('detail').textContent = detail;
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
    ctx.strokeStyle = Math.abs(v) === 15 ? '#e2bd7d' : '#eaf0ef';
    ctx.setLineDash(Math.abs(v) === 15 ? [4,4] : []); ctx.stroke(); ctx.setLineDash([]);
    ctx.fillStyle = Math.abs(v) === 15 ? '#a87931' : '#94a3a7';
    ctx.fillText((v>0?'+':'')+v, pad.l-7, y);
  });
  if (!history.length) {
    ctx.textAlign='center';ctx.fillStyle='#94a3a7';ctx.fillText('Readings will appear here as PosturePal is used',pad.l+plotW/2,pad.t+plotH/2);
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
  drawLine('pitch','#0c9184'); drawLine('roll','#557fc2');
  const last=history[history.length-1];
  [['pitch','#0c9184'],['roll','#557fc2']].forEach(([key,color])=>{
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
          document.getElementById('hero').className = 'card hero neutral';
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
</script>
</body>
</html>
)HTML");
}
void handleData() {
  String json = "{";
  json += "\"status\":\"" + statusText + "\",";
  json += "\"pitch\":" + String(dp, 2) + ",";
  json += "\"roll\":" + String(dr, 2) + ",";
  unsigned long remainingMs = 0;
  if (timerRunning) {
    unsigned long elapsed = millis() - badStart;
    if (elapsed < BAD_TIME) remainingMs = BAD_TIME - elapsed;
  }
  json += "\"remainingMs\":" + String(remainingMs);
  json += "}";

  server.send(200, "application/json", json);
}

void handleCalibrate() {
  calibratePosture();
  server.send(200, "text/plain",
              calibrated ? "Calibration complete" :
                           "Calibration failed");
}

void handleReset() {
  resetCalibration();
  server.send(200, "text/plain", "Calibration reset");
}

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(IR_PIN, INPUT);

  setLEDs(false, false, false);

  Wire.begin(D2, D1);
  delay(100);

  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) {
    sensorOK = false;
    statusText = "SENSOR ERROR";
    setLEDs(false, false, true);
    Serial.println("MPU6050 not detected at 0x68.");
  } else {
    sensorOK = true;
    Serial.println("MPU6050 detected.");
  }

  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(AP_NAME, AP_PASS);

  Serial.println();
  Serial.print("Wi-Fi started: ");
  Serial.println(apStarted ? "YES" : "NO");
  Serial.print("Dashboard IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/reset", HTTP_POST, handleReset);
  server.begin();

  Serial.println("PosturePal dashboard ready.");
  Serial.println("IR sensor acknowledges REMINDER when a hand is detected.");
  Serial.println("Wave a hand over the IR sensor to acknowledge REMINDER.");
  Serial.println("Type c to calibrate or r to reset.");
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'c' || cmd == 'C') calibratePosture();
    if (cmd == 'r' || cmd == 'R') resetCalibration();
  }

  // IR modules commonly go LOW when a hand/object is detected.
  // Change IR_ACTIVE_LOW to false if your module outputs HIGH on detection.
  bool irDetected = IR_ACTIVE_LOW
      ? (digitalRead(IR_PIN) == LOW)
      : (digitalRead(IR_PIN) == HIGH);

  // Trigger only on a new detection edge, so one continuous wave is not
  // repeatedly processed. Acknowledge only while REMINDER is active.
  if (irDetected && !previousIRDetected &&
      millis() - lastIRTime > IR_DEBOUNCE) {
    lastIRTime = millis();

    if (statusText == "REMINDER") {
      reminderAcknowledged = true;
      statusText = "ACKNOWLEDGED";
      setLEDs(false, true, false);
      Serial.println("Reminder acknowledged by IR hand wave.");
    }
  }
  previousIRDetected = irDetected;

  float p, r;
  if (!readAngles(p, r)) {
    sensorOK = false;
    statusText = "SENSOR ERROR";
    setLEDs(false, false, true);
    delay(100);
    return;
  }

  sensorOK = true;

  if (!calibrated) {
    statusText = "CALIBRATION REQUIRED";
    setLEDs(false, false, false);
    delay(100);
    return;
  }

  smoothPitch = 0.8 * smoothPitch + 0.2 * p;
  smoothRoll  = 0.8 * smoothRoll + 0.2 * r;

  dp = smoothPitch - basePitch;
  dr = smoothRoll - baseRoll;

  bool badTilt = (fabs(dp) > TILT_THRESHOLD ||
                  fabs(dr) > TILT_THRESHOLD);

  if (!badTilt) {
    timerRunning = false;
    reminderAcknowledged = false;
    statusText = "GOOD";
    setLEDs(true, false, false);
  } else {
    if (!timerRunning) {
      timerRunning = true;
      badStart = millis();
      reminderAcknowledged = false;
    }

    if (millis() - badStart < BAD_TIME) {
      statusText = "WARNING";
      setLEDs(false, true, false);
    } else if (reminderAcknowledged) {
      statusText = "ACKNOWLEDGED";
      setLEDs(false, true, false);
    } else {
      statusText = "REMINDER";
      setLEDs(false, false, true);
    }
  }

  Serial.print("dPitch: ");
  Serial.print(dp, 1);
  Serial.print(" | dRoll: ");
  Serial.print(dr, 1);
  Serial.print(" | ");
  Serial.println(statusText);

  server.handleClient();
  delay(100);
}
