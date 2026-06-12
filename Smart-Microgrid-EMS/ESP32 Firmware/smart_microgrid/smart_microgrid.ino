#include <WiFi.h>
#include <DHT.h>

/* WiFi AP */
const char* ssid     = "ESP32_Microgrid";
const char* password = "12345678";

/* Pins */
#define PIN_VOLTAGE 35
#define PIN_CURRENT 32
#define PIN_LDR     34
#define PIN_DHT     4
#define PIN_BATTERY 33

#define RELAY_LED   26
#define RELAY_FAN   27

#define DHT_TYPE DHT11

/* Calibration */
#define V_SCALE     2.0f
#define ACS_ZERO    1.65f
#define ACS_SENS    0.185f

/* Solar switching threshold */
#define SOLAR_HIGH 2.5
#define SOLAR_LOW  2.2

#define KNOWN_VOLTAGE   7.2    // what your multimeter shows
#define RAW_ESP_READING 3.27

#define SCALE_FACTOR 6.8
/* Panel max power */
#define PANEL_MAX_POWER 3.0

DHT dht(PIN_DHT, DHT_TYPE);
WiFiServer server(80);

/* Sensor values */
float solarVoltage = 0;
float solarCurrent = 0;
float solarPower   = 0;
float solarEfficiency = 0;

float batteryVoltage = 0;
int batterySOC = 0;

float temperature  = 0;
float humidity     = 0;

int ldrValue = 0;

bool ledOnSolar = false;
bool cloudPredicted = false;

String powerMode = "CLOUDY";

/* timers */
unsigned long lastSensor = 0;
unsigned long lastDHT    = 0;


/* ---------------- VOLTAGE ---------------- */

float readVoltage()
{
  long sum = 0;

  for(int i=0;i<64;i++)
  {
    sum += analogRead(PIN_VOLTAGE);
    delayMicroseconds(80);
  }

  float v = (sum/64.0) * (3.3/4095.0);

  return v * V_SCALE;
}


/* ---------------- CURRENT ---------------- */

float readCurrent()
{
  long sum = 0;

  for(int i=0;i<64;i++)
  {
    sum += analogRead(PIN_CURRENT);
    delayMicroseconds(80);
  }

  float v = (sum/64.0) * (3.3/4095.0);

  float c = (v - ACS_ZERO) / ACS_SENS;

  if(c < 0.02) c = 0;

  return c;
}


/* ---------------- LDR ---------------- */

int readLDR()
{
  long sum = 0;

  for(int i=0;i<32;i++)
  {
    sum += analogRead(PIN_LDR);
    delayMicroseconds(80);
  }

  int val = sum/32;

  int percent = map(val,4095,0,0,100);

  return percent;
}


/* ---------------- BATTERY ---------------- */

float readBatteryVoltage()
{
  long sum = 0;
  for(int i = 0; i < 64; i++) {
    sum += analogRead(PIN_BATTERY);
    delayMicroseconds(80);
  }
  float v = (sum / 64.0) * (3.3 / 4095.0);
  return v * SCALE_FACTOR;
}

  int getBatterySOC(float voltage)
{
  if(voltage >= 9.0)       return 100;
  else if(voltage >= 8.7)  return 95;
  else if(voltage >= 8.5)  return 90;
  else if(voltage >= 8.3)  return 85;
  else if(voltage >= 8.0)  return 80;
  else if(voltage >= 7.8)  return 75;
  else if(voltage >= 7.6)  return 70;
  else if(voltage >= 7.4)  return 65;
  else if(voltage >= 7.2)  return 60;
  else if(voltage >= 7.0)  return 55;
  else if(voltage >= 6.8)  return 50;
  else if(voltage >= 6.5)  return 45;
  else if(voltage >= 6.2)  return 40;
  else if(voltage >= 6.0)  return 35;
  else if(voltage >= 5.7)  return 30;
  else if(voltage >= 5.4)  return 25;
  else if(voltage >= 5.0)  return 20;
  else if(voltage >= 4.5)  return 15;
  else if(voltage >= 4.0)  return 10;
  else if(voltage >= 3.0)  return 7;
   else if(voltage >= 1.9)  return 4;
  else if(voltage >= 1.5)  return 2;
  else                     return 0;
}

/* ---------------- ENERGY MANAGEMENT ---------------- */

void runEMS()
{
  digitalWrite(RELAY_FAN, LOW);

  if(solarVoltage >= SOLAR_HIGH)
  {
    powerMode  = "SUNNY";
    ledOnSolar = true;
    digitalWrite(RELAY_LED, LOW);
  }
  else if(solarVoltage < SOLAR_LOW)
  {
    powerMode  = "CLOUDY";
    ledOnSolar = false;
    digitalWrite(RELAY_LED, HIGH);
  }
}


/* ---------------- HTML PAGE ---------------- */

String buildPage()
{
  String page = "";

  page += "<!DOCTYPE html><html><head>";
  page += "<meta http-equiv='refresh' content='3'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Smart Microgrid</title>";

  /* ---------- inject live sensor values as JS variables ---------- */
  page += "<script>";
  page += "var LIVE_SOLAR_V="    + String(solarVoltage,  2) + ";";
  page += "var LIVE_SOLAR_A="    + String(solarCurrent,  3) + ";";
  page += "var LIVE_SOLAR_W="    + String(solarPower,    2) + ";";
  page += "var LIVE_SOLAR_EFF="  + String(solarEfficiency,1) + ";";
  page += "var LIVE_BAT_V="      + String(batteryVoltage,2) + ";";
  page += "var LIVE_BAT_SOC="    + String(batterySOC)       + ";";
  page += "var LIVE_TEMP="       + String(temperature,   1) + ";";
  page += "var LIVE_HUM="        + String(humidity,      1) + ";";
  page += "var LIVE_LDR="        + String(ldrValue)         + ";";
  page += "var LIVE_MODE='"      + powerMode                + "';";
  page += "var LIVE_PRED='"      + String(cloudPredicted ? "Clouds Predicted" : "Stable") + "';";
  page += "</script>";

  /* ---------- CSS (exact, unchanged) ---------- */
  page += "<style>";
  page += "*{box-sizing:border-box;margin:0;padding:0}";
  page += "body{font-family:var(--font-sans);background:#1a1a1a;color:#e0e0e0}";
  page += ".dash{padding:16px;max-width:900px;margin:0 auto;background:#1a1a1a}";
  page += ".topbar{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}";
  page += ".topbar-title{font-size:15px;font-weight:500;color:var(--color-text-primary)}";
  page += ".topbar-sub{font-size:12px;color:var(--color-text-secondary);margin-top:2px}";
  page += ".badge{display:inline-flex;align-items:center;gap:5px;font-size:11px;padding:3px 10px;border-radius:20px;font-weight:500}";
  page += ".badge-green{background:#EAF3DE;color:#3B6D11}";
  page += ".badge-amber{background:#FAEEDA;color:#854F0B}";
  page += ".badge-red{background:#FCEBEB;color:#A32D2D}";
  page += ".dot{width:7px;height:7px;border-radius:50%;display:inline-block}";
  page += ".dot-green{background:#639922}";
  page += ".dot-amber{background:#BA7517}";
  page += ".dot-red{background:#E24B4A}";
  page += ".metric-row{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px;margin-bottom:12px}";
  page += ".mcard{background:var(--color-background-secondary);border-radius:var(--border-radius-md);padding:12px 14px;cursor:pointer;border:1.5px solid transparent;transition:border-color 0.15s}";
  page += ".mcard:hover{border-color:var(--color-border-secondary)}";
  page += ".mcard.active{border-color:#378ADD;background:var(--color-background-primary)}";
  page += ".mcard-label{font-size:11px;color:var(--color-text-secondary);margin-bottom:6px;letter-spacing:0.3px}";
  page += ".mcard-val{font-size:22px;font-weight:500;color:var(--color-text-primary);line-height:1}";
  page += ".mcard-unit{font-size:12px;color:var(--color-text-secondary);margin-left:2px}";
  page += ".mcard-sub{font-size:11px;color:var(--color-text-secondary);margin-top:5px}";
  page += ".mcard-trend{font-size:11px;margin-top:4px}";
  page += ".trend-up{color:#3B6D11}.trend-dn{color:#A32D2D}.trend-st{color:#BA7517}";
  page += ".detail-panel{background:var(--color-background-primary);border:0.5px solid var(--color-border-tertiary);border-radius:var(--border-radius-lg);padding:16px;margin-bottom:12px;min-height:260px}";
  page += ".detail-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}";
  page += ".detail-title{font-size:14px;font-weight:500;color:var(--color-text-primary)}";
  page += ".detail-close{font-size:12px;color:var(--color-text-secondary);cursor:pointer;padding:2px 8px;border:0.5px solid var(--color-border-tertiary);border-radius:var(--border-radius-md)}";
  page += ".detail-close:hover{background:var(--color-background-secondary)}";
  page += ".stat-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin-bottom:14px}";
  page += ".stat-box{background:var(--color-background-secondary);border-radius:var(--border-radius-md);padding:10px 12px}";
  page += ".stat-box-label{font-size:11px;color:var(--color-text-secondary);margin-bottom:4px}";
  page += ".stat-box-val{font-size:18px;font-weight:500;color:var(--color-text-primary)}";
  page += ".stat-box-note{font-size:11px;color:var(--color-text-secondary);margin-top:3px}";
  page += ".bar-row{display:flex;align-items:center;gap:10px;margin-bottom:8px}";
  page += ".bar-label{font-size:12px;color:var(--color-text-secondary);width:100px;flex-shrink:0}";
  page += ".bar-track{flex:1;height:8px;background:var(--color-background-secondary);border-radius:4px;overflow:hidden}";
  page += ".bar-fill{height:100%;border-radius:4px;transition:width 0.5s}";
  page += ".bar-val{font-size:12px;color:var(--color-text-primary);width:48px;text-align:right;flex-shrink:0}";
  page += ".bottom-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}";
  page += ".panel-card{background:var(--color-background-primary);border:0.5px solid var(--color-border-tertiary);border-radius:var(--border-radius-lg);padding:14px}";
  page += ".panel-card-title{font-size:12px;color:var(--color-text-secondary);margin-bottom:12px;font-weight:500}";
  page += ".load-item{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:0.5px solid var(--color-border-tertiary)}";
  page += ".load-item:last-child{border-bottom:none}";
  page += ".load-name{font-size:13px;color:var(--color-text-primary)}";
  page += ".load-src{font-size:11px;color:var(--color-text-secondary);margin-top:2px}";
  page += ".load-right{text-align:right}";
  page += ".load-status{font-size:12px;font-weight:500}";
  page += ".status-on{color:#3B6D11}.status-off{color:#A32D2D}";
  page += ".timeline{position:relative;padding-left:18px}";
  page += ".tl-item{position:relative;padding:0 0 12px 12px;border-left:1.5px solid var(--color-border-tertiary);font-size:12px;color:var(--color-text-secondary)}";
  page += ".tl-item:last-child{border-left-color:transparent;padding-bottom:0}";
  page += ".tl-dot{position:absolute;left:-5px;top:3px;width:8px;height:8px;border-radius:50%;border:1.5px solid var(--color-background-primary)}";
  page += ".tl-dot-g{background:#639922}.tl-dot-a{background:#BA7517}.tl-dot-r{background:#E24B4A}";
  page += ".tl-time{color:var(--color-text-secondary);margin-right:6px}";
  page += ".tl-msg{color:var(--color-text-primary)}";
  page += ".mode-bar{display:flex;gap:8px;margin-bottom:12px}";
  page += ".mode-btn{flex:1;padding:8px;border-radius:var(--border-radius-md);border:0.5px solid var(--color-border-tertiary);background:var(--color-background-secondary);cursor:pointer;font-size:12px;color:var(--color-text-secondary);text-align:center;transition:all 0.15s}";
  page += ".mode-btn.on{background:#EAF3DE;border-color:#3B6D11;color:#27500A;font-weight:500}";
  page += ".mode-btn.partly{background:#FAEEDA;border-color:#BA7517;color:#633806;font-weight:500}";
  page += ".mode-btn.off{background:#FCEBEB;border-color:#A32D2D;color:#791F1F;font-weight:500}";
  page += ".predict-table{width:100%;border-collapse:collapse;font-size:12px}";
  page += ".predict-table th{text-align:left;font-weight:500;color:var(--color-text-secondary);padding:6px 0;border-bottom:0.5px solid var(--color-border-tertiary)}";
  page += ".predict-table td{padding:7px 0;border-bottom:0.5px solid var(--color-border-tertiary);color:var(--color-text-primary)}";
  page += ".predict-table tr:last-child td{border-bottom:none}";
  page += ".chart-wrap{position:relative;width:100%;height:160px}";
  page += ".sendprompt-btn{display:inline-flex;align-items:center;gap:5px;font-size:12px;padding:5px 12px;border-radius:var(--border-radius-md);border:0.5px solid var(--color-border-secondary);background:transparent;cursor:pointer;color:var(--color-text-secondary);margin-top:10px}";
  page += ".sendprompt-btn:hover{background:var(--color-background-secondary)}";
  page += ".sendprompt-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}";
  page += ".soc-arc-wrap{display:flex;align-items:center;gap:20px;margin-bottom:14px}";
  page += ".soc-details{flex:1}";
  page += ".soc-row{display:flex;justify-content:space-between;font-size:12px;padding:5px 0;border-bottom:0.5px solid var(--color-border-tertiary)}";
  page += ".soc-row:last-child{border-bottom:none}";
  page += ".soc-key{color:var(--color-text-secondary)}";
  page += ".soc-val{color:var(--color-text-primary);font-weight:500}";
  page += "</style></head><body>";

  /* ---------- HTML body (exact, unchanged) ---------- */
  page += "<div class='dash'>";
  page += "<div class='topbar'>";
  page += "  <div>";
  page += "    <div class='topbar-title'>SmartGrid EMS dashboard</div>";
  page += "    <div class='topbar-sub'>Tap any card to expand details</div>";
  page += "  </div>";
  page += "  <div id='modeBadge' class='badge badge-green'><span class='dot dot-green'></span>Sunny mode</div>";
  page += "</div>";

  page += "<div class='mode-bar'>";
  page += "  <div class='mode-btn on' id='mb0' onclick='setMode(0)'>Sunny</div>";
  page += "  <div class='mode-btn' id='mb1' onclick='setMode(1)'>Cloudy</div>";
  page += "  <div class='mode-btn' id='mb2' onclick='setMode(2)'>Rainy</div>";
  page += "</div>";

  page += "<div class='metric-row'>";
  page += "  <div class='mcard' id='card-solar' onclick='openDetail(\"solar\")'>";
  page += "    <div class='mcard-label'>Solar power</div>";
  page += "    <div class='mcard-val' id='mv-solar'>1.24<span class='mcard-unit'>W</span></div>";
  page += "    <div class='mcard-sub' id='ms-solar'>5.2 V &nbsp;|&nbsp; 0.24 A</div>";
  page += "    <div class='mcard-trend trend-up' id='mt-solar'>+12% vs last hour</div>";
  page += "  </div>";
  page += "  <div class='mcard' id='card-battery' onclick='openDetail(\"battery\")'>";
  page += "    <div class='mcard-label'>Battery SOC</div>";
  page += "    <div class='mcard-val' id='mv-battery'>72<span class='mcard-unit'>%</span></div>";
  page += "    <div class='mcard-sub' id='ms-battery'>3.85 V &nbsp;|&nbsp; 5.3 hrs left</div>";
  page += "    <div class='mcard-trend trend-st' id='mt-battery'>Discharging slowly</div>";
  page += "  </div>";
  page += "  <div class='mcard' id='card-env' onclick='openDetail(\"env\")'>";
  page += "    <div class='mcard-label'>Environment</div>";
  page += "    <div class='mcard-val' id='mv-env'>31<span class='mcard-unit'>&#176;C</span></div>";
  page += "    <div class='mcard-sub' id='ms-env'>Humidity 62%</div>";
  page += "    <div class='mcard-trend trend-dn' id='mt-env'>Panel eff. drops 0.4%/&#176;C above 25&#176;C</div>";
  page += "  </div>";
  page += "  <div class='mcard' id='card-forecast' onclick='openDetail(\"forecast\")'>";
  page += "    <div class='mcard-label'>Daily forecast</div>";
  page += "    <div class='mcard-val' id='mv-forecast'>4.8<span class='mcard-unit'>Wh</span></div>";
  page += "    <div class='mcard-sub' id='ms-forecast'>Expected today</div>";
  page += "    <div class='mcard-trend trend-up' id='mt-forecast'>Peak 12:00-14:00</div>";
  page += "  </div>";
  page += "</div>";

  page += "<div id='detail-panel' class='detail-panel' style='display:none'></div>";

  page += "<div class='bottom-row'>";
  page += "  <div class='panel-card'>";
  page += "    <div class='panel-card-title'>Load management</div>";
  page += "    <div id='loadList'></div>";
  page += "  </div>";
  page += "  <div class='panel-card'>";
  page += "    <div class='panel-card-title'>Event log</div>";
  page += "    <div class='timeline' id='eventLog'></div>";
  page += "  </div>";
  page += "</div>";

  page += "<div class='panel-card' style='margin-bottom:12px'>";
  page += "  <div class='panel-card-title'>Power generation - last 20 readings</div>";
  page += "  <div class='chart-wrap'><canvas id='mainChart'></canvas></div>";
  page += "</div>";

  page += "</div>"; /* end .dash */

  /* ---------- JS (exact, unchanged — live values injected via LIVE_* vars) ---------- */
  page += "<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.js'></script>";
  page += "<script>";

  page += "let currentMode = 0;";
  page += "let currentDetail = null;";
  page += "let tick = 0;";

  /* Override mode 0 data with live ESP32 sensor values */
  page += "const modes = [";
  page += "  {label:'Sunny mode', cls:'badge-green', dot:'dot-green',";
  page += "   solar:{v:LIVE_SOLAR_V,a:LIVE_SOLAR_A,w:LIVE_SOLAR_W,eff:LIVE_SOLAR_EFF}, battery:{v:LIVE_BAT_V,soc:LIVE_BAT_SOC,hrs:5.3,state:'Discharging slowly'},";
  page += "   env:{t:LIVE_TEMP,h:LIVE_HUM}, loads:[{n:'LED load',src:'Solar',on:true},{n:'DC fan',src:'Battery',on:true}]},";
  page += "  {label:'Cloudy mode', cls:'badge-amber', dot:'dot-amber',";
  page += "   solar:{v:3.1,a:0.08,w:0.25,eff:52}, battery:{v:3.72,soc:45,hrs:3.1,state:'Discharging'},";
  page += "   env:{t:28,h:74}, loads:[{n:'LED load',src:'Solar',on:true},{n:'DC fan',src:'--',on:false}]},";
  page += "  {label:'Rainy mode', cls:'badge-red', dot:'dot-red',";
  page += "   solar:{v:0.8,a:0.01,w:0.01,eff:13}, battery:{v:3.52,soc:18,hrs:1.2,state:'Discharging fast'},";
  page += "   env:{t:25,h:88}, loads:[{n:'LED load',src:'Battery',on:true},{n:'DC fan',src:'--',on:false}]}";
  page += "];";

  page += "const historyData = {";
  page += "  0: [1.1,1.2,1.18,1.22,1.24,1.3,1.25,1.28,1.24,1.26,1.23,1.25,1.28,1.3,1.24,1.22,1.26,1.29,1.25,LIVE_SOLAR_W],";
  page += "  1: [0.4,0.35,0.3,0.28,0.25,0.22,0.26,0.24,0.25,0.23,0.27,0.25,0.24,0.26,0.25,0.23,0.24,0.25,0.26,0.25],";
  page += "  2: [0.05,0.03,0.02,0.01,0.01,0.02,0.01,0.01,0.02,0.01,0.01,0.02,0.01,0.01,0.02,0.01,0.01,0.02,0.01,0.01]";
  page += "};";

  page += "const events = [";
  page += "  {t:'10:42',m:'Cloud detected - fan load shed',c:'tl-dot-a'},";
  page += "  {t:'10:38',m:'System back to sunny mode',c:'tl-dot-g'},";
  page += "  {t:'10:31',m:'Battery low warning at 25%',c:'tl-dot-r'},";
  page += "  {t:'10:15',m:'Solar generation started',c:'tl-dot-g'},";
  page += "  {t:'09:58',m:'System boot - EMS active',c:'tl-dot-g'}";
  page += "];";

  page += "function setMode(m) {";
  page += "  currentMode = m;";
  page += "  [0,1,2].forEach(i => { document.getElementById('mb'+i).className = 'mode-btn'; });";
  page += "  document.getElementById('mb'+m).className = 'mode-btn '+(m===0?'on':m===1?'partly':'off');";
  page += "  updateAll();";
  page += "  if(currentDetail) openDetail(currentDetail);";
  page += "}";

  page += "function updateAll() {";
  page += "  const md = modes[currentMode];";
  page += "  const b = document.getElementById('modeBadge');";
  page += "  b.className = 'badge '+md.cls;";
  page += "  b.innerHTML = '<span class=\"dot '+md.dot+'\"></span>'+md.label;";
  page += "  document.getElementById('mv-solar').innerHTML = md.solar.w.toFixed(2)+'<span class=\"mcard-unit\">W</span>';";
  page += "  document.getElementById('ms-solar').textContent = md.solar.v.toFixed(1)+' V  |  '+md.solar.a.toFixed(2)+' A';";
  page += "  document.getElementById('mt-solar').textContent = 'Panel efficiency: '+md.solar.eff+'%';";
  page += "  document.getElementById('mt-solar').className = 'mcard-trend '+(md.solar.eff>70?'trend-up':md.solar.eff>40?'trend-st':'trend-dn');";
  page += "  document.getElementById('mv-battery').innerHTML = md.battery.soc+'<span class=\"mcard-unit\">%</span>';";
  page += "  document.getElementById('ms-battery').textContent = md.battery.v.toFixed(2)+' V  |  '+md.battery.hrs+' hrs left';";
  page += "  document.getElementById('mt-battery').textContent = md.battery.state;";
  page += "  document.getElementById('mt-battery').className = 'mcard-trend '+(md.battery.soc>50?'trend-up':md.battery.soc>25?'trend-st':'trend-dn');";
  page += "  document.getElementById('mv-env').innerHTML = md.env.t+'<span class=\"mcard-unit\">&#176;C</span>';";
  page += "  document.getElementById('ms-env').textContent = 'Humidity '+md.env.h+'%';";
  page += "  const wh = (md.solar.w * (md.solar.eff/100) * 6).toFixed(1);";
  page += "  document.getElementById('mv-forecast').innerHTML = wh+'<span class=\"mcard-unit\">Wh</span>';";
  page += "  renderLoads();";
  page += "  updateChart();";
  page += "}";

  page += "function renderLoads() {";
  page += "  const md = modes[currentMode];";
  page += "  const el = document.getElementById('loadList');";
  page += "  el.innerHTML = md.loads.map(l => `<div class='load-item'><div><div class='load-name'>${l.n}</div><div class='load-src'>Source: ${l.src}</div></div><div class='load-right'><div class='load-status ${l.on?'status-on':'status-off'}'>${l.on?'ON':'OFF'}</div></div></div>`).join('');";
  page += "}";

  page += "function renderEventLog() {";
  page += "  document.getElementById('eventLog').innerHTML = events.map(e => `<div class='tl-item'><div class='tl-dot ${e.c}'></div><span class='tl-time'>${e.t}</span><span class='tl-msg'>${e.m}</span></div>`).join('');";
  page += "}";

  page += "let mainChart;";
  page += "function updateChart() {";
  page += "  const colors = ['#378ADD','#BA7517','#E24B4A'];";
  page += "  const col = colors[currentMode];";
  page += "  if(mainChart) {";
  page += "    mainChart.data.datasets[0].data = historyData[currentMode];";
  page += "    mainChart.data.datasets[0].borderColor = col;";
  page += "    mainChart.data.datasets[0].backgroundColor = col+'18';";
  page += "    mainChart.update();";
  page += "  } else {";
  page += "    mainChart = new Chart(document.getElementById('mainChart'), {";
  page += "      type:'line',";
  page += "      data:{";
  page += "        labels: Array.from({length:20},(_,i)=>'-'+(20-i)*3+'s'),";
  page += "        datasets:[{data: historyData[0], borderColor:'#378ADD', backgroundColor:'#378ADD18', borderWidth:1.5, pointRadius:0, tension:0.4, fill:true}]";
  page += "      },";
  page += "      options:{responsive:true,maintainAspectRatio:false,animation:{duration:400},plugins:{legend:{display:false}},";
  page += "        scales:{x:{ticks:{color:'#888780',font:{size:10}},grid:{color:'#D3D1C722'},autoSkip:true,maxTicksLimit:6},";
  page += "                y:{ticks:{color:'#888780',font:{size:10},callback:v=>v.toFixed(2)+'W'},grid:{color:'#D3D1C722'},min:0}}}";
  page += "    });";
  page += "  }";
  page += "}";

  page += "function openDetail(type) {";
  page += "  currentDetail = type;";
  page += "  const md = modes[currentMode];";
  page += "  const p = document.getElementById('detail-panel');";
  page += "  p.style.display = 'block';";
  page += "  document.querySelectorAll('.mcard').forEach(c=>c.classList.remove('active'));";
  page += "  const card = document.getElementById('card-'+type);";
  page += "  if(card) card.classList.add('active');";
  page += "  let html = `<div class='detail-header'><div class='detail-title'>${detailTitle(type)}</div><div class='detail-close' onclick='closeDetail()'>Close</div></div>`;";

  page += "  if(type==='solar') {";
  page += "    const maxV = 6, pct = (md.solar.v/maxV*100).toFixed(0);";
  page += "    html += `<div class='stat-grid'><div class='stat-box'><div class='stat-box-label'>Voltage</div><div class='stat-box-val'>${md.solar.v.toFixed(1)} V</div><div class='stat-box-note'>Max 6 V</div></div><div class='stat-box'><div class='stat-box-label'>Current</div><div class='stat-box-val'>${md.solar.a.toFixed(3)} A</div><div class='stat-box-note'>ACS712 reading</div></div><div class='stat-box'><div class='stat-box-label'>Power</div><div class='stat-box-val'>${md.solar.w.toFixed(2)} W</div><div class='stat-box-note'>V x I</div></div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Panel voltage</div><div class='bar-track'><div class='bar-fill' style='width:${pct}%;background:#378ADD'></div></div><div class='bar-val'>${md.solar.v.toFixed(1)} V</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Efficiency</div><div class='bar-track'><div class='bar-fill' style='width:${md.solar.eff}%;background:${md.solar.eff>70?'#639922':md.solar.eff>40?'#BA7517':'#E24B4A'}'></div></div><div class='bar-val'>${md.solar.eff}%</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>LDR light level</div><div class='bar-track'><div class='bar-fill' style='width:${currentMode===0?88:currentMode===1?44:12}%;background:#378ADD'></div></div><div class='bar-val'>${currentMode===0?'HIGH':currentMode===1?'MED':'LOW'}</div></div>`;";
  page += "    html += `<div class='sendprompt-row'><button class='sendprompt-btn' onclick='sendPrompt(\"How can I improve solar efficiency in my prototype?\")'>Improve efficiency</button><button class='sendprompt-btn' onclick='sendPrompt(\"What happens if I add a second solar panel in parallel?\")'>Add second panel</button></div>`;";
  page += "  }";

  page += "  else if(type==='battery') {";
  page += "    const soc = md.battery.soc;";
  page += "    const socColor = soc>50?'#639922':soc>25?'#BA7517':'#E24B4A';";
  page += "    const radius=52, circ=2*Math.PI*radius;";
  page += "    const dash = (soc/100)*circ, gap = circ-dash;";
  page += "    html += `<div class='soc-arc-wrap'><svg width='130' height='130' viewBox='0 0 130 130'><circle cx='65' cy='65' r='${radius}' fill='none' stroke='#D3D1C7' stroke-width='10'/><circle cx='65' cy='65' r='${radius}' fill='none' stroke='${socColor}' stroke-width='10' stroke-dasharray='${dash.toFixed(1)} ${gap.toFixed(1)}' stroke-dashoffset='${(circ/4).toFixed(1)}' stroke-linecap='round'/><text x='65' y='60' text-anchor='middle' font-size='22' font-weight='500' fill='${socColor}'>${soc}</text><text x='65' y='76' text-anchor='middle' font-size='12' fill='#888780'>% SOC</text></svg><div class='soc-details'><div class='soc-row'><span class='soc-key'>Voltage</span><span class='soc-val'>${md.battery.v.toFixed(2)} V</span></div><div class='soc-row'><span class='soc-key'>Est. time left</span><span class='soc-val'>${md.battery.hrs} hrs</span></div><div class='soc-row'><span class='soc-key'>State</span><span class='soc-val'>${md.battery.state}</span></div><div class='soc-row'><span class='soc-key'>Capacity</span><span class='soc-val'>~2 Ah / 7.4 Wh</span></div><div class='soc-row'><span class='soc-key'>Load shed at</span><span class='soc-val'>20% SOC</span></div><div class='soc-row'><span class='soc-key'>Critical cutoff</span><span class='soc-val'>10% SOC</span></div></div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>LED load</div><div class='bar-track'><div class='bar-fill' style='width:4%;background:#378ADD'></div></div><div class='bar-val'>~0.3 W</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Fan load</div><div class='bar-track'><div class='bar-fill' style='width:54%;background:#BA7517'></div></div><div class='bar-val'>~4 W</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Total draw</div><div class='bar-track'><div class='bar-fill' style='width:58%;background:#E24B4A'></div></div><div class='bar-val'>~4.3 W</div></div>`;";
  page += "    html += `<div class='sendprompt-row'><button class='sendprompt-btn' onclick='sendPrompt(\"How do I calculate battery time remaining accurately in my ESP32 code?\")'>Calculate time left</button><button class='sendprompt-btn' onclick='sendPrompt(\"What is the Li-ion battery voltage to SOC curve I should use?\")'>SOC curve</button></div>`;";
  page += "  }";

  page += "  else if(type==='env') {";
  page += "    const t = md.env.t;";
  page += "    const effLoss = t > 25 ? ((t-25)*0.4).toFixed(1) : 0;";
  page += "    html += `<div class='stat-grid'><div class='stat-box'><div class='stat-box-label'>Temperature</div><div class='stat-box-val'>${t}&#176;C</div><div class='stat-box-note'>${t>30?'Hot - eff. reduced':'Normal'}</div></div><div class='stat-box'><div class='stat-box-label'>Humidity</div><div class='stat-box-val'>${md.env.h}%</div><div class='stat-box-note'>${md.env.h>80?'Very humid':'Normal'}</div></div><div class='stat-box'><div class='stat-box-label'>Eff. loss</div><div class='stat-box-val'>${effLoss}%</div><div class='stat-box-note'>Due to heat</div></div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Temperature</div><div class='bar-track'><div class='bar-fill' style='width:${(t/50*100).toFixed(0)}%;background:${t>35?'#E24B4A':t>28?'#BA7517':'#639922'}'></div></div><div class='bar-val'>${t}&#176;C</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Humidity</div><div class='bar-track'><div class='bar-fill' style='width:${md.env.h}%;background:#378ADD'></div></div><div class='bar-val'>${md.env.h}%</div></div>`;";
  page += "    html += `<div class='bar-row'><div class='bar-label'>Panel eff. now</div><div class='bar-track'><div class='bar-fill' style='width:${md.solar.eff}%;background:#639922'></div></div><div class='bar-val'>${md.solar.eff}%</div></div>`;";
  page += "    html += `<p style='font-size:12px;color:var(--color-text-secondary);margin-top:12px;line-height:1.6'>Solar panel efficiency drops approximately 0.4% for every 1&#176;C above 25&#176;C. At ${t}&#176;C you lose ~${effLoss}% efficiency. Real microgrids use this reading to derate the expected generation forecast.</p>`;";
  page += "    html += `<div class='sendprompt-row'><button class='sendprompt-btn' onclick='sendPrompt(\"How does temperature affect my 6V solar panel output?\")'>Temperature effect</button></div>`;";
  page += "  }";

  page += "  else if(type==='forecast') {";
  page += "    const hourlyBase = currentMode===0?[0,0,0.1,0.3,0.6,0.9,1.1,1.25,1.28,1.3,1.25,1.2,1.1,0.9,0.7,0.5,0.3,0.1,0,0,0,0,0,0]:currentMode===1?[0,0,0.05,0.1,0.2,0.3,0.35,0.28,0.25,0.3,0.28,0.25,0.22,0.2,0.15,0.1,0.05,0,0,0,0,0,0,0]:[0,0,0.02,0.03,0.04,0.05,0.05,0.04,0.03,0.04,0.04,0.03,0.03,0.02,0.02,0.01,0.01,0,0,0,0,0,0,0];";
  page += "    const totalWh = hourlyBase.reduce((a,b)=>a+b,0).toFixed(2);";
  page += "    const peakH = hourlyBase.indexOf(Math.max(...hourlyBase));";
  page += "    html += `<div class='stat-grid'><div class='stat-box'><div class='stat-box-label'>Expected today</div><div class='stat-box-val'>${totalWh} Wh</div><div class='stat-box-note'>Full day estimate</div></div><div class='stat-box'><div class='stat-box-label'>Peak hour</div><div class='stat-box-val'>${peakH}:00</div><div class='stat-box-note'>Best generation</div></div><div class='stat-box'><div class='stat-box-label'>Peak power</div><div class='stat-box-val'>${Math.max(...hourlyBase).toFixed(2)} W</div><div class='stat-box-note'>At peak sun</div></div></div>`;";
  page += "    html += `<div style='position:relative;width:100%;height:140px;margin-bottom:10px'><canvas id='forecastChart'></canvas></div>`;";
  page += "    html += `<table class='predict-table'><thead><tr><th>Time window</th><th>Expected power</th><th>Recommended action</th></tr></thead><tbody><tr><td>06:00-09:00</td><td>0-0.6 W</td><td>Conserve battery</td></tr><tr><td>09:00-13:00</td><td>0.6-1.3 W</td><td>Run all loads on solar</td></tr><tr><td>13:00-17:00</td><td>0.5-1.1 W</td><td>Charge battery surplus</td></tr><tr><td>17:00-19:00</td><td>0-0.3 W</td><td>Prepare for battery mode</td></tr><tr><td>19:00-06:00</td><td>0 W</td><td>Battery-only - shed fan</td></tr></tbody></table>`;";
  page += "    html += `<div class='sendprompt-row'><button class='sendprompt-btn' onclick='sendPrompt(\"How can I implement a daily solar forecast algorithm in my ESP32 firmware?\")'>Implement forecast</button><button class='sendprompt-btn' onclick='sendPrompt(\"How do I schedule load switching based on time of day in ESP32?\")'>Time-based switching</button></div>`;";
  page += "  }";

  page += "  p.innerHTML = html;";

  page += "  if(type==='forecast') {";
  page += "    const hourlyBase = currentMode===0?[0,0,0.1,0.3,0.6,0.9,1.1,1.25,1.28,1.3,1.25,1.2,1.1,0.9,0.7,0.5,0.3,0.1,0,0,0,0,0,0]:currentMode===1?[0,0,0.05,0.1,0.2,0.3,0.35,0.28,0.25,0.3,0.28,0.22,0.22,0.2,0.15,0.1,0.05,0,0,0,0,0,0,0]:[0,0,0.02,0.03,0.04,0.05,0.05,0.04,0.03,0.04,0.04,0.03,0.03,0.02,0.02,0.01,0.01,0,0,0,0,0,0,0];";
  page += "    setTimeout(()=>{";
  page += "      const fc = document.getElementById('forecastChart');";
  page += "      if(fc) new Chart(fc,{type:'bar',data:{labels:Array.from({length:24},(_,i)=>i+':00'),datasets:[{data:hourlyBase,backgroundColor:hourlyBase.map(v=>v>1?'#378ADD':v>0.5?'#3B6D11':'#D3D1C7'),borderRadius:3,borderSkipped:false}]},options:{responsive:true,maintainAspectRatio:false,animation:{duration:400},plugins:{legend:{display:false}},scales:{x:{ticks:{color:'#888780',font:{size:9},maxTicksLimit:8},grid:{display:false}},y:{ticks:{color:'#888780',font:{size:10},callback:v=>v.toFixed(1)+'W'},grid:{color:'#D3D1C722'},min:0}}}});";
  page += "    },100);";
  page += "  }";
  page += "}";

  page += "function detailTitle(t) {";
  page += "  return {solar:'Solar generation details',battery:'Battery - state of charge & runtime',env:'Environment & panel efficiency',forecast:'Daily generation forecast'}[t];";
  page += "}";

  page += "function closeDetail() {";
  page += "  document.getElementById('detail-panel').style.display='none';";
  page += "  document.querySelectorAll('.mcard').forEach(c=>c.classList.remove('active'));";
  page += "  currentDetail=null;";
  page += "}";

  /* Set initial mode based on live ESP32 mode */
  page += "renderEventLog();";
  page += "if(LIVE_MODE==='SUNNY') setMode(0);";
  page += "else if(LIVE_MODE==='CLOUDY') setMode(1);";
  page += "else setMode(2);";
  page += "updateAll();";

  page += "</script></body></html>";

  return page;
}


/* ---------------- SETUP ---------------- */

void setup()
{

Serial.begin(115200);

pinMode(RELAY_LED, OUTPUT);
pinMode(RELAY_FAN, OUTPUT);

digitalWrite(RELAY_LED, HIGH);
digitalWrite(RELAY_FAN, LOW);

analogReadResolution(12);
analogSetAttenuation(ADC_11db);

dht.begin();

WiFi.softAP(ssid,password);

Serial.println("Smart Microgrid Started");

Serial.print("Dashboard : http://");
Serial.println(WiFi.softAPIP());

server.begin();

}


/* ---------------- LOOP ---------------- */

void loop()
{

unsigned long now = millis();

/* -------- SENSOR UPDATE -------- */

if(now - lastSensor > 500)
{

lastSensor = now;

solarVoltage = readVoltage();

solarCurrent = readCurrent();

solarPower = solarVoltage * solarCurrent;

/* Solar efficiency */

solarEfficiency = (solarPower / PANEL_MAX_POWER) * 100;

if(solarEfficiency > 100)
solarEfficiency = 100;

/* Battery measurement */



batteryVoltage = readBatteryVoltage();
batterySOC = getBatterySOC(batteryVoltage);

/* Cloud prediction */

if(solarVoltage < SOLAR_HIGH && solarVoltage > SOLAR_LOW)
cloudPredicted = true;
else
cloudPredicted = false;

ldrValue = readLDR();

runEMS();

}


/* -------- DHT UPDATE -------- */

if(now - lastDHT > 2000)
{

lastDHT = now;

float t = dht.readTemperature();
float h = dht.readHumidity();

if(!isnan(t)) temperature = t;
if(!isnan(h)) humidity = h;

}


/* -------- SERIAL OUTPUT -------- */

static unsigned long lastPrint = 0;

if(now - lastPrint > 2000)
{

lastPrint = now;

Serial.printf(
"V:%.2fV I:%.2fA P:%.2fW Eff:%.1f%% Bat:%.2fV SOC:%d%% LDR:%d%% T:%.1fC H:%.1f%% Mode:%s Prediction:%s\n",
solarVoltage,
solarCurrent,
solarPower,
solarEfficiency,
batteryVoltage,
batterySOC,
ldrValue,
temperature,
humidity,
powerMode.c_str(),
cloudPredicted ? "Clouds Predicted" : "Stable"
);

}


/* -------- WEB SERVER -------- */

WiFiClient client = server.available();

if(client)
{

while(client.connected() && !client.available()) delay(1);

client.readStringUntil('\r');

String page = buildPage();

client.println("HTTP/1.1 200 OK");
client.println("Content-type:text/html");
client.println("Connection: close");
client.println();

client.print(page);

client.stop();

}

}