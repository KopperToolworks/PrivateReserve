#include "net.h"

#include "app.h"
#include "docs_qr.h"
#include "door.h"
#include "store.h"

#include "config.h"

#include <WebServer.h>
#include <WiFi.h>
#include <cstdarg>
#include <cmath>
#include <cstring>

namespace {
WebServer server(80);
char json[12288];

void jsonEsc(char* dst, size_t n, const char* s) {
  size_t o = 0;
  if (!s) {
    dst[0] = 0;
    return;
  }
  while (*s && o + 2 < n) {
    const unsigned char c = static_cast<unsigned char>(*s++);
    if (c < 0x20) {
      continue;
    }
    if (c == '"' || c == '\\') {
      dst[o++] = '\\';
    }
    dst[o++] = static_cast<char>(c);
  }
  dst[o] = 0;
}

void append(char*& p, const char* end, const char* s) {
  while (*s && p < end - 1) {
    *p++ = *s++;
  }
  *p = 0;
}

void appendf(char*& p, const char* end, const char* fmt, ...) {
  if (p >= end - 1) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(p, static_cast<size_t>(end - p), fmt, ap);
  va_end(ap);
  if (n < 0) {
    return;
  }
  if (n >= end - p) {
    p = const_cast<char*>(end - 1);
  } else {
    p += n;
  }
}

float jsonNum(float v) {
  return std::isfinite(v) ? v : 0.f;
}

void buildState() {
  const View& v = app.view();
  char* p = json;
  const char* end = json + sizeof(json);
  json[0] = 0;
  char m[40], n[40], b[48], su[80], r[40], r2[40], ig[56];
  jsonEsc(m, sizeof(m), v.hdr_mode);
  jsonEsc(n, sizeof(n), v.hdr_net);
  jsonEsc(b, sizeof(b), v.hero_big);
  jsonEsc(su, sizeof(su), v.hero_sub);
  jsonEsc(r, sizeof(r), v.hero_right);
  jsonEsc(r2, sizeof(r2), v.hero_right2);
  jsonEsc(ig, sizeof(ig), app.lastIgnore());
  appendf(p, end,
          "{\"screen\":%u,\"mode\":\"%s\",\"net\":\"%s\",\"ap\":%s,"
          "\"hero\":%u,\"big\":\"%s\",\"sub\":\"%s\",\"cls\":\"%c\","
          "\"right\":\"%s\",\"right2\":\"%s\",\"amps\":%.3f,\"trip\":%.3f,"
          "\"pos\":%d,\"counts\":%ld,\"known\":%s,\"moving\":%s,"
          "\"openMark\":%s,\"closeMark\":%s,\"showTravel\":%s,"
          "\"showAmps\":%s,\"showPin\":%s,\"graph\":%u,\"bin\":%u,"
          "\"spark\":%u,\"entry\":%s,\"range\":%s,\"showProg\":%s,"
          "\"displayOn\":%s,\"ignore\":\"%s\",\"faultKnob\":%s,"
          "\"tileCols\":%u",
          static_cast<unsigned>(v.screen), m, n,
          v.net_ap ? "true" : "false", static_cast<unsigned>(v.hero),
          b, su, v.hero_cls ? v.hero_cls : ' ',
          r, r2, jsonNum(v.amps), jsonNum(v.trip), v.pos_pct,
          static_cast<long>(v.counts), v.pos_known ? "true" : "false",
          v.moving ? "true" : "false", v.open_mark ? "true" : "false",
          v.close_mark ? "true" : "false", v.show_travel ? "true" : "false",
          v.show_amps ? "true" : "false", v.show_pin ? "true" : "false",
          v.graph, v.live_bin, v.spark, v.show_entry ? "true" : "false",
          v.show_range ? "true" : "false", v.show_prog ? "true" : "false",
          app.displayOn() ? "true" : "false", ig,
          v.fault_knob ? "true" : "false",
          v.tile_cols ? v.tile_cols : 4u);

  append(p, end, ",\"tiles\":[");
  for (uint8_t i = 0; i < v.ntiles; ++i) {
    if (i) {
      append(p, end, ",");
    }
    char k[32], val[24], g[32];
    jsonEsc(k, sizeof(k), v.tiles[i].k);
    jsonEsc(val, sizeof(val), v.tiles[i].v);
    jsonEsc(g, sizeof(g), v.tiles[i].g);
    appendf(p, end, "{\"k\":\"%s\",\"v\":\"%s\",\"g\":\"%s\",\"pip\":%u,\"sel\":%s}",
            k, val, g, v.tiles[i].pip, v.tiles[i].sel ? "true" : "false");
  }
  append(p, end, "],\"rows\":[");
  for (uint8_t i = 0; i < v.nrows; ++i) {
    if (i) {
      append(p, end, ",");
    }
    char k[48], val[48];
    jsonEsc(k, sizeof(k), v.rows[i].k);
    jsonEsc(val, sizeof(val), v.rows[i].v);
    appendf(p, end, "{\"k\":\"%s\",\"v\":\"%s\",\"dim\":%u}", k, val,
            v.rows[i].dim);
  }
  append(p, end, "],\"items\":[");
  for (uint8_t i = 0; i < v.nitems; ++i) {
    if (i) {
      append(p, end, ",");
    }
    char k[56], val[36];
    jsonEsc(k, sizeof(k), v.items[i].label);
    jsonEsc(val, sizeof(val), v.items[i].value);
    appendf(p, end, "{\"l\":\"%s\",\"v\":\"%s\",\"pip\":%u,\"sel\":%s}", k, val,
            v.items[i].pip, v.items[i].sel ? "true" : "false");
  }
  append(p, end, "],\"ftr\":[");
  for (uint8_t i = 0; i < v.nftr; ++i) {
    if (i) {
      append(p, end, ",");
    }
    char f[36];
    jsonEsc(f, sizeof(f), v.ftr[i]);
    appendf(p, end, "\"%s\"", f);
  }
  char pinp[88], pinl[48];
  jsonEsc(pinp, sizeof(pinp), v.pin_parts);
  jsonEsc(pinl, sizeof(pinl), v.pin_lv);
  appendf(p, end, "],\"pin\":\"%s\",\"pinlv\":\"%s\"", pinp, pinl);
  char ev[24], eu[40], en[64], pl[20], pr[20];
  jsonEsc(ev, sizeof(ev), v.entry_val);
  jsonEsc(eu, sizeof(eu), v.entry_unit);
  jsonEsc(en, sizeof(en), v.entry_note);
  jsonEsc(pl, sizeof(pl), v.prog_l);
  jsonEsc(pr, sizeof(pr), v.prog_r);
  appendf(p, end,
          ",\"entryVal\":\"%s\",\"entryUnit\":\"%s\",\"entryNote\":\"%s\","
          "\"rval\":%ld,\"rmin\":%ld,\"rmax\":%ld,\"rdef\":%ld",
          ev, eu, en,
          static_cast<long>(v.range_val), static_cast<long>(v.range_min),
          static_cast<long>(v.range_max), static_cast<long>(v.range_def));
  appendf(p, end, ",\"prog\":[%u,%u,%u,%u],\"progl\":\"%s\",\"progr\":\"%s\"",
          v.prog[0], v.prog[1], v.prog[2], v.prog[3], pl, pr);

  appendf(p, end, ",\"histN\":%d,\"hall\":[", kHistLen);
  for (uint8_t i = 0; i < app.histCount(); ++i) {
    if (i) {
      append(p, end, ",");
    }
    appendf(p, end, "%u", app.hallHist()[app.histChronoIndex(i)]);
  }
  append(p, end, "],\"amp\":[");
  for (uint8_t i = 0; i < app.histCount(); ++i) {
    if (i) {
      append(p, end, ",");
    }
    appendf(p, end, "%u", app.ampHist()[app.histChronoIndex(i)]);
  }
  append(p, end, "]");

  if (v.graph) {
    const LoadTable* t = store.armedTable();
    const LoadBin* bins =
        t ? (v.graph == 1 ? t->close : t->open) : nullptr;
    append(p, end, ",\"mean\":[");
    for (int b = 0; b < kBinCount; ++b) {
      if (b) {
        append(p, end, ",");
      }
      appendf(p, end, "%.3f",
              bins && bins[b].valid ? jsonNum(bins[b].mean) : 0);
    }
    append(p, end, "],\"trip\":[");
    for (int b = 0; b < kBinCount; ++b) {
      if (b) {
        append(p, end, ",");
      }
      appendf(p, end, "%.3f",
              bins && bins[b].valid ? jsonNum(bins[b].trip) : 0);
    }
    append(p, end, "],\"ok\":[");
    for (int b = 0; b < kBinCount; ++b) {
      if (b) {
        append(p, end, ",");
      }
      append(p, end, bins && bins[b].valid ? "1" : "0");
    }
    append(p, end, "]");
  }

  if (v.show_qr) {
    char url[80];
    jsonEsc(url, sizeof(url), kDocsUrl);
    appendf(p, end, ",\"qr\":1,\"qrN\":%d,\"docsUrl\":\"%s\",\"qrBits\":\"",
            kDocsQrSize, url);
    const int nbytes = static_cast<int>(sizeof(kDocsQrBits));
    for (int i = 0; i < nbytes; ++i) {
      appendf(p, end, "%02X", pgm_read_byte(&kDocsQrBits[i]));
    }
    append(p, end, "\"");
  } else {
    append(p, end, ",\"qr\":0");
  }
  const bool wifi_form = v.screen == Screen::SetNetwork ||
                         v.screen == Screen::SetWifiCleared;
  appendf(p, end, ",\"wifiForm\":%s}", wifi_form ? "true" : "false");
}

constexpr char kCss[] PROGMEM = R"CSS(
:root{--bg:#0b0d10;--panel:#14181d;--line:#242a31;--text:#e6e9ec;--dim:#8a939c;--faint:#5f6a75;--ok:#4caf6d;--move:#e0a33a;--fault:#e04b43;--idle:#5b6570;--accent:#5aa9e6}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;overflow:hidden;background:#1b1f24;color:var(--text);font-family:system-ui,sans-serif;-webkit-text-size-adjust:100%}
#viewport{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;overflow:hidden;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
#shell{flex:0 0 auto;width:max-content;transform-origin:center center}
nav{display:flex;gap:8px;margin-bottom:10px;font-size:.8rem;flex-wrap:wrap}
nav a{color:var(--accent);text-decoration:none;cursor:pointer}
nav a.on{color:var(--text);font-weight:600}
.chrome{display:flex;gap:6px;margin-bottom:12px;flex-wrap:wrap}
.chrome button{background:#242a31;color:var(--text);border:1px solid #333b44;border-radius:5px;padding:6px 10px}
.stage{display:flex;flex-direction:row;align-items:flex-start;gap:12px;width:max-content}
.keys{display:grid;grid-template-columns:1fr 1fr;grid-template-rows:1fr 2fr 1fr;gap:7px;width:112px;flex:0 0 112px;height:184px}
.keys button{background:#242a31;color:var(--text);border:1px solid #333b44;border-radius:8px;display:flex;align-items:center;justify-content:center;cursor:pointer;padding:0;min-height:0;overflow:hidden;-webkit-tap-highlight-color:transparent}
.keys button:active{background:#333b44}
.keys svg{width:48%;height:48%;display:block;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.keys svg .head{fill:currentColor;stroke:none}
#t-{grid-column:1;grid-row:1}#t+{grid-column:2;grid-row:1}
#pr{grid-column:1/3;grid-row:2}#ho{grid-column:1/3;grid-row:3}
#pr svg{width:40%;height:40%}#ho svg{width:52%;height:56%}
.bezel{background:#000;border-radius:9px;padding:7px;width:max-content}
.frame{width:320px;height:170px;overflow:hidden}
.screen{width:320px;height:170px;background:var(--bg);color:var(--text);padding:6px;display:flex;flex-direction:column;gap:4px;font-family:"DejaVu Sans Condensed",system-ui,sans-serif;overflow:hidden}
.screen.off{background:#000;padding:0}
.hdr{height:15px;display:flex;justify-content:space-between;font-size:10px;font-weight:700;letter-spacing:.08em}
.hdr em{font-style:normal;color:var(--dim);font-weight:400}
.net{font-size:9px;color:var(--dim)}
.dot{width:5px;height:5px;border-radius:50%;background:var(--ok);display:inline-block;margin-right:4px}
.dot.ap{background:var(--move)}
.hero{height:40px;display:flex;justify-content:space-between;align-items:center}
.big{font-size:22px;line-height:22px;font-weight:700}
.sub2{font-size:9px;color:var(--dim);margin-top:2px}
.banner{border-radius:4px;padding:4px 7px}
.banner .t{font-size:14px;font-weight:700}
.banner .d{font-size:9px;margin-top:2px}
.banner.bad{background:#2a1211;border:1px solid var(--fault)}
.banner.warn{background:#2a2211;border:1px solid var(--move)}
.banner.ok{background:#0f2418;border:1px solid var(--ok)}
.banner.bad .t{color:#ff8a82}.banner.warn .t{color:#f0c070}.banner.ok .t{color:#79d79a}
.travel{height:32px}
.trow{display:flex;align-items:center;gap:6px}
.tend{font-size:9px;color:var(--dim);width:38px}
.track{position:relative;flex:1;height:14px;background:var(--panel);border:1px solid var(--line);border-radius:3px;overflow:hidden}
.fill{position:absolute;top:0;bottom:0;left:0;background:#243447}
.mark{position:absolute;top:0;bottom:0;width:1px;background:#4a5563}
.mark.on{background:var(--ok)}
.knob{position:absolute;top:-1px;bottom:-1px;width:3px;background:var(--text)}
.knob.move{background:var(--move)}.knob.fault{background:var(--fault)}
.tiles{display:grid;gap:5px}
.tiles.c4{grid-template-columns:repeat(4,1fr);height:42px}
.tiles.c2{grid-template-columns:repeat(2,1fr);height:42px}
.tile{background:var(--panel);border:1px solid var(--line);border-radius:4px;padding:3px 5px}
.tile.sel{border-color:var(--accent);box-shadow:inset 2px 0 0 var(--accent)}
.tile .k{font-size:8px;color:var(--dim);text-transform:uppercase}
.tile .v{font-size:12px;font-weight:700}
.pip{width:7px;height:7px;border-radius:50%;background:var(--idle);display:inline-block;margin-right:4px}
.pip.on{background:var(--ok)}.pip.warn{background:var(--move)}.pip.bad{background:var(--fault)}
.rows{display:flex;flex-direction:column;gap:2px;flex:1}
.row{display:flex;font-size:9px;gap:6px}
.rk{color:var(--dim)}.rv{margin-left:auto;font-weight:700}
.row.dim .rv{color:var(--faint);font-weight:400}
.prog{display:flex;gap:2px;height:8px;background:#1e242b;padding:1px;border-radius:2px}
.prog i{flex:1;display:block;height:6px;background:#2b333c}
.prog i.ok{background:var(--ok)}.prog i.mv{background:var(--move)}
.gcap{display:flex;justify-content:space-between;font-size:8px;color:var(--faint)}
.hero-r{text-align:right}
.list{display:flex;flex-direction:column;gap:2px;flex:1}
.li{display:flex;align-items:center;gap:5px;font-size:13px;padding:2px 4px;line-height:16px}
.li.sel{background:#1d2733;box-shadow:inset 2px 0 0 var(--accent)}
.lv{margin-left:auto;font-size:10px;color:var(--faint)}
.pinbar{height:13px;display:flex;align-items:center;background:#101418;border:1px solid var(--line);border-radius:3px;padding:0 5px;font-size:8px;color:var(--dim)}
.pinbar .lv{margin-left:auto;color:var(--text);font-weight:700}
.ftr{height:12px;display:flex;gap:10px;font-size:8px;color:var(--faint);text-transform:uppercase}
.ftr b{color:var(--dim)}
.qrrow{display:flex;gap:10px;align-items:flex-start;flex:1;min-height:0}
.qrrow svg{flex:0 0 111px;width:111px;height:111px;background:#fff}
.qrtext{padding-top:6px;min-width:0}
.qrtext .host{font-size:9px;color:var(--dim)}
.qrtext .repo{font-size:11px;font-weight:700;margin:2px 0 8px}
.qrtext a{color:var(--accent);font-size:9px;text-decoration:none;word-break:break-all}
.qrtext .note{font-size:8px;color:var(--faint);margin-top:8px}
.env{height:4px;background:#1e242b;border-radius:2px;position:relative}
.env .ef{position:absolute;inset:0 auto 0 0}
.trip{position:absolute;top:-2px;bottom:-2px;width:1px;background:var(--fault)}
.c-ok{color:var(--ok)}.c-move{color:var(--move)}.c-fault{color:var(--fault)}
.entry{display:flex;gap:6px;align-items:center}
.field{border:1px solid var(--accent);padding:2px 8px;font-size:20px;font-weight:700}
.rng{height:6px;background:#1e242b;border-radius:3px;position:relative}
.rng .ef{position:absolute;top:0;bottom:0;left:0;background:var(--accent)}
.warnline{font-size:9px;color:#cbb083;background:#2a2211;border:1px solid var(--move);border-radius:4px;padding:4px 6px}
.weblink{margin-top:2px}
.weblink a{color:var(--accent);font-size:9px;text-decoration:none}
)CSS";

constexpr char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>Private Reserve</title><link rel="stylesheet" href="/screen.css"></head>
<body>
<div id="viewport"><div id="shell">
<nav>
<a data-n="status">Main</a>
<a data-n="cal">Calibration</a>
<a data-n="diag">Diagnostics</a>
<a data-n="set">Settings</a>
<a data-n="docs">Docs</a>
</nav>
<div class="stage">
<div class="bezel"><div class="frame"><div class="screen" id="s"></div></div></div>
<div class="keys" role="group" aria-label="Knob">
<button id="t-" title="Turn counterclockwise" aria-label="Turn counterclockwise">
<svg viewBox="0 0 24 24"><g transform="scale(-1 1) translate(-24 0)">
<path d="M16.46 6.29A7.25 7.25 0 1 1 7.54 6.29" style="stroke-width:2.6;stroke-linecap:butt"/>
<path class="head" d="M3.1 6.29l6.5-3.45v6.9z"/>
</g></svg>
</button>
<button id="t+" title="Turn clockwise" aria-label="Turn clockwise">
<svg viewBox="0 0 24 24"><g>
<path d="M16.46 6.29A7.25 7.25 0 1 1 7.54 6.29" style="stroke-width:2.6;stroke-linecap:butt"/>
<path class="head" d="M3.1 6.29l6.5-3.45v6.9z"/>
</g></svg>
</button>
<button id="pr" title="Press" aria-label="Press">
<svg viewBox="0 0 24 24"><path d="M18 4v10H8"/><path d="M11 10.5L7 14l4 3.5"/></svg>
</button>
<button id="ho" title="Hold back" aria-label="Hold back">
<svg viewBox="0 0 24 24"><path d="M20 12H7"/><path d="M12 6L6 12l6 6"/></svg>
</button>
</div>
</div></div></div>
<script>
const S=document.getElementById('s');
function pip(p){return p===1?'on':p===2?'warn':p===3?'bad':''}
function cmd(c,n){fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'c='+c+(n!=null?'&n='+n:'')}).then(()=>tick())}
document.getElementById('t-').onclick=()=>cmd('turn',-1);
document.getElementById('t+').onclick=()=>cmd('turn',1);
document.getElementById('pr').onclick=()=>cmd('press');
document.getElementById('ho').onclick=()=>cmd('hold');
document.querySelectorAll('nav a').forEach(a=>a.onclick=()=>{
  const m={status:0,diag:1,cal:2,set:3,docs:4};
  cmd('nav', m[a.dataset.n]);
});
function spark(arr,color,win){
  if(!arr||!arr.length)return '';
  const n=arr.length, N=win||40, c=color||'#5aa9e6';
  let mn=Math.min(...arr),mx=Math.max(...arr); if(mx<=mn)mx=mn+1;
  const xAt=i=>((N-n+i)/(N-1)*110);
  const yAt=v=>(24-(v-mn)/(mx-mn)*22);
  const pts=arr.map((v,i)=> xAt(i).toFixed(1)+','+yAt(v).toFixed(1)).join(' ');
  const lx=xAt(n-1).toFixed(1), ly=yAt(arr[n-1]).toFixed(1);
  const line=n>1?`<polyline points="${pts}" fill="none" stroke="${c}" stroke-width="1.5"/>`:'';
  return `<svg width="112" height="26">${line}<circle cx="${lx}" cy="${ly}" r="2" fill="${c}"/></svg>`;
}
function graph(st){
  if(!st.mean)return '';
  const w=308,h=62,gx0=16,gx1=w-2,gy0=4,gy1=h-10,amin=.7,amax=2.6,n=st.mean.length;
  const X=b=>gx0+b/(n-1)*(gx1-gx0);
  const Y=a=>{const t=Math.max(0,Math.min(1,(a-amin)/(amax-amin)));return gy1-t*(gy1-gy0)};
  let mean='',trip='';
  st.mean.forEach((m,i)=>{if(st.ok[i]){mean+=`${X(i).toFixed(1)},${Y(m).toFixed(1)} `;trip+=`${X(i).toFixed(1)},${Y(st.trip[i]).toFixed(1)} `;}});
  return `<div><svg width="${w}" height="${h}"><line x1="${gx0}" y1="${Y(2.5)}" x2="${gx1}" y2="${Y(2.5)}" stroke="#e04b43"/><polyline points="${mean}" fill="none" stroke="#5aa9e6" stroke-width="1.5"/><polyline points="${trip}" fill="none" stroke="#e04b43" stroke-width="1"/><line x1="${X(st.bin)}" y1="${gy0}" x2="${X(st.bin)}" y2="${gy1}" stroke="#e0a33a"/></svg><div class="gcap"><span>OPEN</span><span>CLOSED</span></div></div>`;
}
function navOn(st){
  const n=st.mode.startsWith('CAL')?'cal':st.mode.startsWith('DIAG')?'diag':st.mode.startsWith('SET')?'set':st.mode.startsWith('DOC')?'docs':'status';
  document.querySelectorAll('nav a').forEach(a=>a.classList.toggle('on',a.dataset.n===n));
}
function qrSvg(hex,n){
  if(!hex||!n)return '';
  let d='',bit=0;
  const bytes=[];
  for(let i=0;i<hex.length;i+=2) bytes.push(parseInt(hex.substr(i,2),16));
  for(let y=0;y<n;y++){
    for(let x=0;x<n;x++){
      const on=(bytes[bit>>3]>>(7-(bit&7)))&1; bit++;
      if(on) d+=`M${x} ${y}h1v1h-1z`;
    }
  }
  return `<svg viewBox="0 0 ${n} ${n}" width="111" height="111" shape-rendering="crispEdges" aria-hidden="true"><rect width="${n}" height="${n}" fill="#fff"/><path fill="#000" d="${d}"/></svg>`;
}
function render(st){
  navOn(st);
  S.classList.toggle('off',!st.displayOn);
  if(!st.displayOn){S.innerHTML='';return;}
  const cls=st.cls==='k'?'c-ok':st.cls==='m'?'c-move':st.cls==='f'?'c-fault':'';
  let hero='';
  if(st.hero===2) hero=`<div class="banner bad"><div class="t">${st.big}</div><div class="d">${st.sub}</div></div>`;
  else if(st.hero===3) hero=`<div class="banner warn"><div class="t">${st.big}</div><div class="d">${st.sub}</div></div>`;
  else if(st.hero===4) hero=`<div class="banner ok"><div class="t">${st.big}</div><div class="d">${st.sub}</div></div>`;
  else if(st.hero===1){
    const amps=st.showAmps?`<div><div class="big">${st.amps.toFixed(2)} <span style="font-size:10px;color:var(--dim)">A</span></div>
      <div class="env"><div class="ef" style="width:${Math.min(100,st.amps/2.5*100)}%;background:${st.amps>st.trip?'var(--fault)':'var(--ok)'}"></div>
      ${st.trip?`<div class="trip" style="left:${st.trip/2.5*100}%"></div>`:''}</div></div>`:'';
    const side=st.spark?spark(st.spark===1?st.hall:st.amp,null,st.histN):(st.right||st.right2?`<div class="hero-r">${st.right?`<div class="rv">${st.right}</div>`:''}${st.right2?`<div class="sub2">${st.right2}</div>`:''}</div>`:'');
    hero=`<div class="hero"><div><div class="big ${cls}">${st.big}</div><div class="sub2">${st.sub}</div></div>
      <div>${side}${amps}</div></div>`;
  }
  let travel='';
  if(st.showTravel){
    const kc=st.moving?'move':st.faultKnob?'fault':'';
    travel=`<div class="travel"><div class="trow"><div class="tend">OPEN</div>
      <div class="track">${st.known?`<div class="fill" style="width:${st.pos}%"></div>`:''}
      <div class="mark ${st.openMark?'on':''}" style="left:3%"></div><div class="mark ${st.closeMark?'on':''}" style="left:95%"></div>
      ${st.known?`<div class="knob ${kc}" style="left:calc(${st.pos}% - 1px)"></div>`:''}</div>
      <div class="tend" style="text-align:right">CLOSED</div></div>
      <div class="sub2">${st.known?st.pos+'% - '+st.counts+' cnt':'unknown'}</div></div>`;
  }
  let tiles='';
  if(st.tiles&&st.tiles.length){
    tiles=`<div class="tiles c${st.tileCols|| (st.tiles.length===2?2:4)}">`+st.tiles.map(t=>`<div class="tile${t.sel?' sel':''}"><div class="k">${t.k}</div>
      <div class="v"><span class="pip ${pip(t.pip)}"></span>${t.v}</div><div class="k">${t.g}</div></div>`).join('')+`</div>`;
  }
  let items='';
  if(st.items&&st.items.length){
    items=`<div class="list">`+st.items.map(i=>`<div class="li ${i.sel?'sel':''}"><span class="pip ${pip(i.pip)}"></span><span>${i.l}</span><span class="lv">${i.v||''}</span></div>`).join('')+`</div>`;
  }
  let rows='';
  if(st.rows&&st.rows.length){
    rows=`<div class="rows">`+st.rows.map(r=>`<div class="row ${r.dim?'dim':''}"><span class="rk">${r.k}</span><span class="rv">${r.v}</span></div>`).join('')+`</div>`;
  }
  let entry='';
  if(st.entry){
    entry=`<div class="entry"><div class="field">${st.entryVal}</div><div><div>${st.entryUnit}</div><div class="sub2">${st.entryNote}</div></div></div>`;
    if(st.range){
      const pct=(st.rval-st.rmin)/Math.max(1,st.rmax-st.rmin)*100;
      entry+=`<div class="rng"><div class="ef" style="width:${pct}%"></div></div>`;
    }
  }
  let prog='';
  if(st.showProg&&st.prog){
    prog=`<div class="prog">${[0,1,2,3].map(i=>`<i class="${st.prog[i]===1?'ok':st.prog[i]===2?'mv':''}"></i>`).join('')}</div>
      <div class="trow"><span class="sub2">${st.progl||''}</span><span class="sub2" style="margin-left:auto">${st.progr||''}</span></div>`;
  }
  const pin=st.showPin?`<div class="pinbar"><b>${st.pin}</b><span class="lv">${st.pinlv}</span></div>`:'';
  const ftr=st.ftr&&st.ftr.length?`<div class="ftr">${st.ftr.map(f=>`<span>${f}</span>`).join('')}</div>`:'';
  const g=st.graph?graph(st):'';
  const wifi=st.wifiForm?`<div class="weblink"><a href="/wifi">Set SSID and password</a></div>`:'';
  const qr=st.qr?`<div class="qrrow">${qrSvg(st.qrBits,st.qrN)}<div class="qrtext">
    <div class="host">github.com/</div>
    <div class="repo">KopperToolworks/PrivateReserve</div>
    <a href="${st.docsUrl||'#'}" target="_blank" rel="noopener">Open GitHub page</a>
    <div class="note">source and docs</div>
  </div></div>`:'';
  S.innerHTML=`<div class="hdr"><div>${st.mode}</div><div class="net"><span class="dot ${st.ap?'ap':''}"></span>${st.net}</div></div>
    ${hero}${entry}${prog}${g}${travel}${tiles}${items}${rows}${wifi}${qr}${pin}${ftr}`;
}
async function tick(){
  try{
    const r=await fetch('/api/state');
    render(await r.json());
  }catch(e){}
}
setInterval(tick,250);tick();
function fit(){
  const vp=document.getElementById('viewport'),sh=document.getElementById('shell');
  const vv=window.visualViewport;
  if(vv){vp.style.top=vv.offsetTop+'px';vp.style.left=vv.offsetLeft+'px';vp.style.width=vv.width+'px';vp.style.height=vv.height+'px';vp.style.right='auto';vp.style.bottom='auto'}
  sh.style.transform='none';
  const pad=8,vw=vp.clientWidth-pad*2,vh=vp.clientHeight-pad*2;
  const s=Math.min(vw/sh.offsetWidth,vh/sh.offsetHeight);
  sh.style.transform='scale('+s+')';
}
fit();
addEventListener('resize',fit);
if(window.visualViewport){visualViewport.addEventListener('resize',fit);visualViewport.addEventListener('scroll',fit)}
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", kPage); }
void handleCss() { server.send_P(200, "text/css", kCss); }
void handleState() {
  buildState();
  server.send(200, "application/json", json);
}

void copyWifi(char* live, char* pend, size_t n, const String& v) {
  strncpy(live, v.c_str(), n - 1);
  live[n - 1] = 0;
  memcpy(pend, live, n);
}

constexpr char kWifiPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Private Reserve — Wi-Fi</title>
<style>
:root{--bg:#0b0d10;--panel:#14181d;--line:#242a31;--text:#e6e9ec;--dim:#8a939c;--accent:#5aa9e6;--move:#e0a33a;--fault:#e04b43}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px 18px 40px;max-width:28rem}
h1{font-size:1.15rem;margin:0 0 .4rem}h2{font-size:.85rem;color:var(--dim);margin:1.2rem 0 .5rem;font-weight:600}
p,label{font-size:.85rem;color:var(--dim)}a{color:var(--accent);text-decoration:none}
.note{background:#2a2211;border:1px solid var(--move);border-radius:6px;padding:.55rem .7rem;color:#cbb083;margin:0 0 1rem}
label{display:block;margin:.65rem 0 .2rem}input,select,button{width:100%;padding:.55rem .6rem;border-radius:6px;border:1px solid var(--line);background:var(--panel);color:var(--text);font-size:1rem}
.row{display:flex;gap:.4rem}.row select{flex:1}.row button{width:auto;flex:0 0 auto}
button{background:#242a31;cursor:pointer}button.pri{background:#1d3a52;border-color:var(--accent);margin-top:1rem}
#msg{min-height:1.2rem;margin-top:.7rem;font-size:.85rem}#msg.err{color:var(--fault)}#msg.ok{color:#4caf6d}
</style></head><body>
<p><a href="/">Back to controller</a></p>
<h1>Set SSID and password</h1>
<p class="note">No login. Anyone on this network can change these values. The LCD cannot type them. Motor must be idle. Join takes effect after reboot.</p>
<form id="f">
<h2>Station (site Wi-Fi)</h2>
<label for="scan">Nearby networks</label>
<div class="row"><select id="scan"><option value="">Scan to fill this list</option></select>
<button type="button" id="scanbtn">Scan</button></div>
<label for="ssid">Station SSID</label>
<input id="ssid" name="ssid" maxlength="32" autocomplete="off" placeholder="empty = SoftAP only">
<label for="pw">Station password</label>
<input id="pw" name="pw" type="password" maxlength="64" autocomplete="new-password" placeholder="leave blank to keep">
<h2>Fallback SoftAP</h2>
<label for="apssid">AP name</label>
<input id="apssid" name="apssid" maxlength="32" autocomplete="off">
<label for="appw">AP password</label>
<input id="appw" name="appw" type="password" maxlength="64" autocomplete="new-password" placeholder="leave blank to keep; 8+ chars for WPA2">
<button class="pri" type="submit">Save</button>
<p id="msg"></p>
<p>After save: Settings → Service → Reboot controller, or use PRESS reboot on the Wi-Fi-cleared screen.</p>
</form>
<script>
const $=id=>document.getElementById(id);
const msg=(t,ok)=>{const m=$('msg');m.textContent=t;m.className=ok?'ok':'err';};
async function load(){
  const r=await fetch('/api/wifi');
  const j=await r.json();
  $('ssid').value=j.ssid||'';
  $('apssid').value=j.apssid||'';
  if(!j.idle) msg('Motor busy. Stop motion before saving.',false);
}
$('scan').onchange=()=>{if($('scan').value)$('ssid').value=$('scan').value};
$('scanbtn').onclick=async()=>{
  $('scanbtn').disabled=true;$('scanbtn').textContent='Scanning…';
  msg('',true);
  const t0=Date.now();
  try{
    for(;;){
      const r=await fetch('/api/scan');
      if(!r.ok) throw 0;
      const j=await r.json();
      if(j.st==='run'){
        if(Date.now()-t0>20000) throw 0;
        await new Promise(x=>setTimeout(x,400));
        continue;
      }
      if(j.st!=='ok') throw 0;
      const nets=j.n||[];
      const sel=$('scan');
      sel.innerHTML='';
      const z=document.createElement('option');z.value='';z.textContent=nets.length?'Select a network':'No networks found';
      sel.appendChild(z);
      nets.forEach(n=>{const o=document.createElement('option');o.value=n.s;o.textContent=n.s+'  ('+n.r+' dBm)';sel.appendChild(o);});
      break;
    }
  }catch(e){msg('Scan failed.',false)}
  $('scanbtn').disabled=false;$('scanbtn').textContent='Scan';
};
$('f').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams();
  body.set('ssid',$('ssid').value);
  if($('pw').value) body.set('pw',$('pw').value);
  body.set('apssid',$('apssid').value);
  if($('appw').value) body.set('appw',$('appw').value);
  const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  if(r.status===409){msg('Motor busy. Stop motion before saving.',false);return;}
  if(!r.ok){msg('Save failed.',false);return;}
  msg('Saved. Reboot to join. Password fields were not shown again.',true);
  $('pw').value='';$('appw').value='';
};
load();
</script></body></html>
)HTML";

void handleWifiPage() { server.send_P(200, "text/html", kWifiPage); }

void handleWifiGet() {
  char* p = json;
  const char* end = json + sizeof(json);
  json[0] = 0;
  char ssid[40], ap[40];
  jsonEsc(ssid, sizeof(ssid), store.settings.wifi_sta_ssid);
  jsonEsc(ap, sizeof(ap), store.settings.wifi_ap_ssid);
  appendf(p, end, "{\"ssid\":\"%s\",\"apssid\":\"%s\",\"idle\":%s}", ssid, ap,
          door.idle() ? "true" : "false");
  server.send(200, "application/json", json);
}

void handleWifiPost() {
  if (!door.idle()) {
    server.send(409, "text/plain", "motor busy");
    return;
  }
  if (server.hasArg("ssid")) {
    const String v = server.arg("ssid");
    if (v.length() >= 32) {
      server.send(400, "text/plain", "ssid too long");
      return;
    }
    copyWifi(store.settings.wifi_sta_ssid, store.pending.wifi_sta_ssid,
             sizeof(store.settings.wifi_sta_ssid), v);
  }
  if (server.hasArg("pw")) {
    const String v = server.arg("pw");
    if (v.length() >= 64) {
      server.send(400, "text/plain", "password too long");
      return;
    }
    if (v.length()) {
      copyWifi(store.settings.wifi_sta_password, store.pending.wifi_sta_password,
               sizeof(store.settings.wifi_sta_password), v);
    }
  }
  if (server.hasArg("apssid")) {
    const String v = server.arg("apssid");
    if (v.length() < 1 || v.length() >= 32) {
      server.send(400, "text/plain", "ap ssid");
      return;
    }
    copyWifi(store.settings.wifi_ap_ssid, store.pending.wifi_ap_ssid,
             sizeof(store.settings.wifi_ap_ssid), v);
  }
  if (server.hasArg("appw")) {
    const String v = server.arg("appw");
    if (v.length() >= 64) {
      server.send(400, "text/plain", "ap password too long");
      return;
    }
    if (v.length()) {
      copyWifi(store.settings.wifi_ap_password, store.pending.wifi_ap_password,
               sizeof(store.settings.wifi_ap_password), v);
    }
  }
  store.persistSettings();
  app.compose();
  server.send(200, "text/plain", "ok");
}

void handleScan() {
  const int16_t st = WiFi.scanComplete();
  if (st == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"st\":\"run\"}");
    return;
  }
  if (st >= 0) {
    char* p = json;
    const char* end = json + sizeof(json);
    json[0] = 0;
    append(p, end, "{\"st\":\"ok\",\"n\":[");
    const int lim = st < 16 ? st : 16;
    for (int i = 0; i < lim; ++i) {
      if (i) {
        append(p, end, ",");
      }
      char ssid[40];
      jsonEsc(ssid, sizeof(ssid), WiFi.SSID(i).c_str());
      appendf(p, end, "{\"s\":\"%s\",\"r\":%d}", ssid, WiFi.RSSI(i));
    }
    append(p, end, "]}");
    WiFi.scanDelete();
    server.send(200, "application/json", json);
    return;
  }
  const int16_t r = WiFi.scanNetworks(true, false);
  if (r == WIFI_SCAN_FAILED) {
    server.send(200, "application/json", "{\"st\":\"fail\"}");
    return;
  }
  server.send(200, "application/json", "{\"st\":\"run\"}");
}

void handleCmd() {
  const String c = server.arg("c");
  const int n = server.arg("n").toInt();
  const String v = server.arg("v");
  if (c == "turn") {
    app.handle(Cmd::Turn, n == 0 ? 1 : n);
  } else if (c == "press") {
    app.handle(Cmd::Press, 0);
  } else if (c == "hold") {
    app.handle(Cmd::Hold, 0);
  } else if (c == "jog") {
    app.handle(Cmd::JogBeat, n);
  } else if (c == "nav") {
    door.cancel();
    if (!app.displayOn()) {
      app.setDisplayOn(true);
    }
    if (n == 1) {
      app.jump(Screen::DiagMenu);
    } else if (n == 2) {
      app.jump(Screen::CalMenu);
    } else if (n == 3) {
      app.jump(door.running() ? Screen::SetBlocked : Screen::SetMenu);
    } else if (n == 4) {
      app.jump(Screen::Docs);
    } else {
      app.jump(Screen::Status);
    }
  } else if (c == "ssid" && v.length() < 32 && door.idle()) {
    copyWifi(store.settings.wifi_sta_ssid, store.pending.wifi_sta_ssid,
             sizeof(store.settings.wifi_sta_ssid), v);
    store.persistSettings();
  } else if (c == "pw" && v.length() < 64 && door.idle()) {
    copyWifi(store.settings.wifi_sta_password, store.pending.wifi_sta_password,
             sizeof(store.settings.wifi_sta_password), v);
    store.persistSettings();
  } else if (c == "apssid" && v.length() < 32 && door.idle()) {
    copyWifi(store.settings.wifi_ap_ssid, store.pending.wifi_ap_ssid,
             sizeof(store.settings.wifi_ap_ssid), v);
    store.persistSettings();
  } else if (c == "appw" && v.length() < 64 && door.idle()) {
    copyWifi(store.settings.wifi_ap_password, store.pending.wifi_ap_password,
             sizeof(store.settings.wifi_ap_password), v);
    store.persistSettings();
  }
  app.compose();
  server.send(200, "text/plain", "ok");
}
}  // namespace

void webBegin() {
  server.on("/", handleRoot);
  server.on("/screen.css", handleCss);
  server.on("/wifi", handleWifiPage);
  server.on("/api/state", handleState);
  server.on("/api/wifi", HTTP_GET, handleWifiGet);
  server.on("/api/wifi", HTTP_POST, handleWifiPost);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/cmd", HTTP_GET, handleCmd);
  server.begin();
}

void webLoop() { server.handleClient(); }
