#pragma once
#include <Arduino.h>

// OpenAPI 3.0 description of the robot's HTTP API, served at /openapi.json so
// it can be fed to Swagger UI, Postman or a client generator.
static const char OPENAPI_JSON[] PROGMEM = R"JSON({
"openapi":"3.0.3",
"info":{"title":"MF26 Roboter-API","version":"1.0.0",
"description":"Den Roboter fahren und sein Gesicht gestalten. Alle Daten sind JSON. Es gibt keine Anmeldung - der Roboter vertraut jedem in seinem Netzwerk."},
"servers":[{"url":"/","description":"Der Roboter selbst"}],
"tags":[{"name":"drive","description":"Fahren"},{"name":"face","description":"Gesicht"},{"name":"system","description":"Status und Einstellungen"}],
"paths":{
"/api/status":{"get":{"tags":["system"],"summary":"Aktueller Zustand des Roboters","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Status"}}}}}}},
"/api/drive":{"post":{"tags":["drive"],"summary":"Fahrbefehl setzen","description":"x lenkt, y gibt Gas. Werte von -1 bis 1. Der Roboter hält von selbst an, wenn 700 ms lang kein Befehl kommt - ein Programm sollte also mindestens zweimal pro Sekunde senden.","requestBody":{"required":true,"content":{"application/json":{"schema":{"$ref":"#/components/schemas/Drive"},"example":{"x":0,"y":0.5}}}},"responses":{"200":{"description":"Angenommen","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Ok"}}}},"400":{"description":"Daten waren kein JSON-Objekt"}}}},
"/api/stop":{"post":{"tags":["drive"],"summary":"Beide Räder sofort stoppen","responses":{"200":{"description":"Gestoppt","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Ok"}}}}}}},
"/api/settings":{"put":{"tags":["system"],"summary":"Tempolimit und Radtrimmung setzen","description":"Die Trimmung sind Mikrosekunden auf den 1500-us-Neutralpuls und wird im Gerät gespeichert. Damit stellt man ein schleichendes Servo ruhig.","requestBody":{"required":true,"content":{"application/json":{"schema":{"$ref":"#/components/schemas/Settings"},"example":{"speed":60,"trimLeft":0,"trimRight":-8}}}},"responses":{"200":{"description":"Aktualisiert","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Settings"}}}}}}},
"/api/face":{
"get":{"tags":["face"],"summary":"Aktuelles Gesicht lesen","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Face"}}}}}},
"put":{"tags":["face"],"summary":"Gesicht ändern","description":"Teilweise Änderung - nur die gesendeten Felder ändern sich. Formen koennen als Name oder als Nummer angegeben werden. Wird gespeichert und überlebt einen Neustart.","requestBody":{"required":true,"content":{"application/json":{"schema":{"$ref":"#/components/schemas/Face"},"example":{"eye":"happy","mouth":"cat","brow":"raised","color":24063}}}},"responses":{"200":{"description":"Das Gesicht nach der Änderung","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Face"}}}},"400":{"description":"Unbekannter Formname","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Error"}}}}}}},
"/api/face/options":{"get":{"tags":["face"],"summary":"Alle verfügbaren Formen auflisten","description":"Die Position eines Namens in der Liste ist ebenfalls ein gültiger Wert.","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Options"}}}}}}},
"/api/face/reset":{"post":{"tags":["face"],"summary":"Standardgesicht wiederherstellen","responses":{"200":{"description":"Das Standardgesicht","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Face"}}}}}}},
"/api/blink":{"post":{"tags":["face"],"summary":"Einmal blinzeln, sofort","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Ok"}}}}}}},
"/api/identify":{"post":{"tags":["face"],"summary":"Kurz blinken, um den Roboter am Tisch zu finden","description":"3 Sekunden lang blitzt der Bildschirm statt des Gesichts zu zeigen. Wirkt nicht, waehrend der Pairing-Screen (QR-Code) angezeigt wird.","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Ok"}}}}}}},
"/api/pair":{"post":{"tags":["system"],"summary":"Zurück zum Pairing-Screen","description":"Zeigt wieder die QR-Codes zum Verbinden, so wie beim ersten Start: erst das WLAN des Roboters, dann seine Steuerseite - bis zum nächsten echten Fahrbefehl.","responses":{"200":{"description":"OK","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Ok"}}}}}}}
},
"components":{"schemas":{
"Ok":{"type":"object","properties":{"ok":{"type":"boolean"}}},
"Error":{"type":"object","properties":{"error":{"type":"string"}}},
"Drive":{"type":"object","properties":{
"x":{"type":"number","minimum":-1,"maximum":1,"description":"Lenkung. -1 ganz links, 1 ganz rechts."},
"y":{"type":"number","minimum":-1,"maximum":1,"description":"Gas. -1 volle Rückwärts, 1 volle Vorwärts."}}},
"Settings":{"type":"object","properties":{
"speed":{"type":"integer","minimum":10,"maximum":100,"description":"Höchsttempo in Prozent."},
"trimLeft":{"type":"integer","minimum":-120,"maximum":120,"description":"Neutralpunkt des linken Rades in Mikrosekunden."},
"trimRight":{"type":"integer","minimum":-120,"maximum":120,"description":"Neutralpunkt des rechten Rades in Mikrosekunden."}}},
"Status":{"type":"object","properties":{
"ip":{"type":"string"},"mode":{"type":"string","enum":["ap"]},"ssid":{"type":"string","description":"Name des eigenen WLANs."},"stations":{"type":"integer","description":"Mit dem WLAN verbundene Geräte."},
"rssi":{"type":"integer"},"clients":{"type":"integer","description":"Verbundene Websocket-Clients."},
"uptimeMs":{"type":"integer"},"freeHeap":{"type":"integer"},
"speed":{"type":"integer"},"trimLeft":{"type":"integer"},"trimRight":{"type":"integer"},
"drive":{"$ref":"#/components/schemas/Drive"}}},
"Options":{"type":"object","properties":{
"eye":{"type":"array","items":{"type":"string"}},
"mouth":{"type":"array","items":{"type":"string"}},
"brow":{"type":"array","items":{"type":"string"}},
"nose":{"type":"array","items":{"type":"string"}}}},
"Face":{"type":"object","properties":{
"eye":{"type":"string","description":"rounded, circle, square, oval, happy, sleepy"},
"mouth":{"type":"string","description":"smile, flat, open, cat, frown, grin, squiggle"},
"brow":{"type":"string","description":"none, flat, angry, sad, raised"},
"nose":{"type":"string","description":"none, dot, triangle, line"},
"color":{"type":"integer","minimum":0,"maximum":65535,"description":"Farbe der Gesichtszüge als RGB565."},
"background":{"type":"integer","minimum":0,"maximum":65535,"description":"Hintergrundfarbe als RGB565."},
"eyeWidth":{"type":"integer","minimum":16,"maximum":90},
"eyeHeight":{"type":"integer","minimum":16,"maximum":110},
"eyeGap":{"type":"integer","minimum":20,"maximum":75,"description":"Abstand von der Bildmitte zu jedem Auge."},
"eyeY":{"type":"integer","minimum":50,"maximum":150},
"mouthWidth":{"type":"integer","minimum":16,"maximum":110},
"mouthY":{"type":"integer","minimum":130,"maximum":215},
"thickness":{"type":"integer","minimum":2,"maximum":10,"description":"Strichstärke der gezeichneten Züge."},
"blinkEvery":{"type":"integer","minimum":1,"maximum":20,"description":"Sekunden zwischen dem Blinzeln."},
"lookEvery":{"type":"integer","minimum":1,"maximum":20,"description":"Sekunden zwischen dem Umsehen."},
"autoBlink":{"type":"boolean"},"autoLook":{"type":"boolean"},
"tears":{"type":"boolean","description":"Animierte Tränen, die aus beiden Augen spritzen."}}}
}}})JSON";

// A small self-contained Swagger-style browser for the spec above. No CDN, so
// it still works when the robot's network has no route to the internet.
static const char API_DOCS_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MF26 API</title>
<style>
  :root{--bg:#0d1117;--panel:#161b22;--line:#26303d;--fg:#e6edf3;--muted:#8b949e;
        --accent:#5acdff;--get:#3fb950;--post:#d29922;--put:#a371f7}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--fg);
       font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
  header{padding:22px 20px;border-bottom:1px solid var(--line)}
  h1{margin:0;font-size:21px}
  header p{margin:6px 0 0;color:var(--muted);font-size:13.5px;max-width:70ch}
  header a{color:var(--accent)}
  main{padding:18px 20px 60px;max-width:900px}
  h2{font-size:12px;text-transform:uppercase;letter-spacing:.09em;
     color:var(--muted);margin:26px 0 10px}
  .op{border:1px solid var(--line);border-radius:10px;margin-bottom:10px;
      background:var(--panel);overflow:hidden}
  .op summary{display:flex;gap:11px;align-items:center;padding:12px 14px;
              cursor:pointer;list-style:none}
  .op summary::-webkit-details-marker{display:none}
  .verb{font:600 11px/1 ui-monospace,Menlo,monospace;padding:5px 8px;border-radius:5px;
        color:#0d1117;min-width:46px;text-align:center}
  .get{background:var(--get)}.post{background:var(--post)}.put{background:var(--put)}
  .path{font-family:ui-monospace,Menlo,monospace;font-size:13.5px}
  .sum{color:var(--muted);font-size:13px;margin-left:auto;text-align:right}
  .body{padding:0 14px 14px;border-top:1px solid var(--line)}
  .desc{color:var(--muted);font-size:13.5px;margin:12px 0}
  table{border-collapse:collapse;width:100%;font-size:13px;margin:10px 0}
  th{text-align:left;color:var(--muted);font-weight:600;font-size:11px;
     text-transform:uppercase;letter-spacing:.06em;padding:6px 8px;
     border-bottom:1px solid var(--line)}
  td{padding:6px 8px;border-bottom:1px solid #1d242e;vertical-align:top}
  td code{font-family:ui-monospace,Menlo,monospace;color:var(--accent)}
  .rng{color:var(--muted);font-family:ui-monospace,Menlo,monospace;font-size:12px}
  textarea{width:100%;background:#0d1117;color:var(--fg);border:1px solid var(--line);
           border-radius:7px;padding:9px;font-family:ui-monospace,Menlo,monospace;
           font-size:12.5px;min-height:76px;resize:vertical}
  button{background:var(--accent);color:#04222e;border:0;border-radius:7px;
         padding:8px 15px;font-weight:600;cursor:pointer;font-size:13px}
  button:hover{filter:brightness(1.08)}
  pre{background:#0d1117;border:1px solid var(--line);border-radius:7px;
      padding:10px;overflow:auto;font-size:12.5px;margin:10px 0 0;max-height:280px}
  .row{display:flex;gap:10px;align-items:center;margin-top:10px}
  .status{font-family:ui-monospace,Menlo,monospace;font-size:12.5px;color:var(--muted)}
</style>
</head>
<body>
<header>
  <h1>MF26 Roboter-API</h1>
  <p id="intro"></p>
  <p>Maschinenlesbare Beschreibung: <a href="/openapi.json">/openapi.json</a> — passt in Swagger UI oder Postman.
     Die Live-Steuerung läuft außerdem über einen Websocket auf <code>/ws</code>.</p>
</header>
<main id="out">Lädt…</main>

<script>
(async () => {
  const spec = await (await fetch('/openapi.json')).json();
  document.getElementById('intro').textContent = spec.info.description;
  const out = document.getElementById('out');
  out.textContent = '';

  const schema = ref => {
    if (!ref) return null;
    const name = ref.replace('#/components/schemas/', '');
    return spec.components.schemas[name];
  };

  const groups = {};
  for (const [path, ops] of Object.entries(spec.paths)) {
    for (const [verb, op] of Object.entries(ops)) {
      const tag = (op.tags || ['other'])[0];
      (groups[tag] = groups[tag] || []).push({ path, verb, op });
    }
  }

  const TAGS = {drive:'Fahren', face:'Gesicht', system:'System', other:'Sonstiges'};

  for (const tag of Object.keys(groups)) {
    const h = document.createElement('h2');
    h.textContent = TAGS[tag] || tag;
    out.appendChild(h);

    for (const { path, verb, op } of groups[tag]) {
      const d = document.createElement('details');
      d.className = 'op';

      const s = document.createElement('summary');
      s.innerHTML = `<span class="verb ${verb}">${verb.toUpperCase()}</span>
                     <span class="path">${path}</span>
                     <span class="sum">${op.summary || ''}</span>`;
      d.appendChild(s);

      const body = document.createElement('div');
      body.className = 'body';

      if (op.description) {
        const p = document.createElement('p');
        p.className = 'desc';
        p.textContent = op.description;
        body.appendChild(p);
      }

      // request/response field table
      const rb = op.requestBody?.content?.['application/json'];
      const resp = op.responses?.['200']?.content?.['application/json'];
      const sch = schema(rb?.schema?.$ref) || schema(resp?.schema?.$ref);
      if (sch?.properties) {
        const t = document.createElement('table');
        t.innerHTML = '<tr><th>Feld</th><th>Typ</th><th>Bereich</th><th>Bedeutung</th></tr>';
        for (const [k, v] of Object.entries(sch.properties)) {
          const rng = v.minimum !== undefined ? `${v.minimum} … ${v.maximum}`
                    : (v.enum ? v.enum.join(' | ') : '');
          const tr = document.createElement('tr');
          tr.innerHTML = `<td><code>${k}</code></td><td>${v.type || 'object'}</td>
                          <td class="rng">${rng}</td><td>${v.description || ''}</td>`;
          t.appendChild(tr);
        }
        body.appendChild(t);
      }

      // try it
      let ta = null;
      if (rb) {
        ta = document.createElement('textarea');
        ta.value = JSON.stringify(rb.example || {}, null, 2);
        body.appendChild(ta);
      }
      const row = document.createElement('div');
      row.className = 'row';
      const btn = document.createElement('button');
      btn.textContent = 'Senden';
      const st = document.createElement('span');
      st.className = 'status';
      row.append(btn, st);
      body.appendChild(row);

      const pre = document.createElement('pre');
      pre.hidden = true;
      body.appendChild(pre);

      btn.onclick = async () => {
        st.textContent = 'sende…';
        try {
          const init = { method: verb.toUpperCase() };
          if (ta) {
            init.headers = { 'Content-Type': 'application/json' };
            init.body = ta.value;
          }
          const t0 = performance.now();
          const r = await fetch(path, init);
          const ms = Math.round(performance.now() - t0);
          const text = await r.text();
          st.textContent = `${r.status} ${r.statusText} · ${ms} ms`;
          try { pre.textContent = JSON.stringify(JSON.parse(text), null, 2); }
          catch { pre.textContent = text; }
          pre.hidden = false;
        } catch (e) {
          st.textContent = 'fehlgeschlagen';
          pre.textContent = String(e);
          pre.hidden = false;
        }
      };

      d.appendChild(body);
      out.appendChild(d);
    }
  }
})();
</script>
</body>
</html>
)HTML";
