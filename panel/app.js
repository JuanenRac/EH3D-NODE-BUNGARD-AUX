/* EH3D-NODE-BUNGARD panel - the pages. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
   text.js (the eight languages) is joined in front of this file when the panel is packed, so LANGS and L exist here.
   No framework and no inline style: the panel is served with a strict content-security policy. The panel moves nothing:
   it shows what the head reads, measures the table on request and edits the settings of the node. */
"use strict";

// ---- small tools ---------------------------------------------------------------------------------------------------------------------

const $app = document.getElementById("app");
const REQUEST_MARK = "eh3d";   // the value of X-Requested-With that the node asks for on every request that changes something
let lang = 0;
// A text in the panel's language. A key or a language without its text is a mistake of the panel, not something to hide: it is said in the console
// (tests/check_panel_text.py makes sure it cannot happen) and the key is shown.
const t = (key, ...args) => {
  const row = L[key];
  if (!row || typeof row[lang] !== "string") { console.error("missing text: " + key); return key; }
  let text = row[lang];
  args.forEach((a, i) => { text = text.split("{" + i + "}").join(a); });
  return text;
};

function el(tag, attrs, ...kids) {
  const node = document.createElement(tag);
  for (const [name, value] of Object.entries(attrs || {})) {
    if (value === undefined || value === null || value === false) continue;
    if (name === "class") node.className = value;
    else if (name.startsWith("on")) node.addEventListener(name.slice(2), value);
    else if (name in node && name !== "list" && name !== "role") node[name] = value;
    else node.setAttribute(name, value === true ? "" : value);
  }
  const add = kid => {
    if (Array.isArray(kid)) kid.forEach(add);
    else if (kid !== null && kid !== undefined && kid !== false) node.append(kid instanceof Node ? kid : document.createTextNode(String(kid)));
  };
  kids.forEach(add);
  return node;
}

const S = {
  session: null, page: "overview", message: null, restartNeeded: false, rebooting: false, busy: false,
  cfg: null, saved: "",                       // the network settings (GET/PUT config) and what was last saved
  head: null, headSaved: "",                  // the settings of the head (GET/PUT head) and what was last saved; the passwords and the token are write-only
  measure: null,                              // the last measure of the table asked for from this page
  problems: { cfg: {}, head: {} }, touched: new Set(), server: {},   // what is drawn next to the fields; the fields left once; what the node refused
  status: null,                               // the node's own status (flash slots, network)
  ms: null,                                   // what the head reads, polled
  github: { busy: false, checked: false, available: false, latest: "", error: "", installing: false, state: "", pct: 0 },
  users: [], scan: { busy: false, list: null, error: "" }, log: { next: 0, text: "" }, channelAuto: 1,
};
const roleName = role => (role === "admin" ? t("roleAdmin") : t("roleViewer"));
const isAdmin = () => S.session && S.session.role === "admin";
const hasEthernet = () => !!(S.session && S.session.ethernet);
const isWired = () => hasEthernet() && S.cfg && S.cfg.uplink === "ethernet";

async function api(method, path, body) {
  let response;
  try {
    response = await fetch("api/v1/" + path, {
      method, credentials: "same-origin",
      headers: Object.assign({ "X-Requested-With": REQUEST_MARK }, body !== undefined ? { "Content-Type": "application/json" } : {}),
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
  } catch (error) { return { ok: false, status: 0, data: { error: "network" } }; }
  let data = {};
  try { data = await response.json(); } catch (error) { /* an empty answer */ }
  if (response.status === 401 && S.session && S.session.authenticated) { S.session.authenticated = false; start(); }
  return { ok: response.ok, status: response.status, data };
}

const errorText = code => (L["e_" + code] ? t("e_" + code) : t("e_other", String(code || "?")));
const problemText = code => (L["p_" + code] ? t("p_" + code) : String(code));

function pickLanguage(preferred) {
  let code = "";
  try { code = localStorage.getItem("eh3d_lang") || ""; } catch (error) { /* storage may be blocked */ }
  if (!code) code = preferred || (navigator.language || "en").slice(0, 2);
  const index = LANGS.findIndex(l => l[0] === code);
  lang = index >= 0 ? index : 0;
  document.documentElement.lang = LANGS[lang][0];
}

// The theme: the one the browser prefers until the person chooses the other, which is then remembered on this device.
function currentTheme() {
  let saved = "";
  try { saved = localStorage.getItem("eh3d_theme") || ""; } catch (error) { /* storage may be blocked */ }
  if (saved === "light" || saved === "dark") return saved;
  return window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}
function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  document.querySelector("meta[name=color-scheme]").setAttribute("content", theme);
}
const themeName = () => (currentTheme() === "dark" ? t("themeToLight") : t("themeToDark"));   // the button names the theme it switches to
function toggleTheme() {
  const next = currentTheme() === "dark" ? "light" : "dark";
  try { localStorage.setItem("eh3d_theme", next); } catch (error) { /* the choice lasts until the page is closed */ }
  applyTheme(next);
  render();
}
applyTheme(currentTheme());

// ---- the working copies of the settings ------------------------------------------------------------------------------------------------

const target = store => (store === "head" ? S.head : S.cfg);
const walk = (store, path, create) => {
  const parts = path.split(".");
  let obj = target(store);
  for (let i = 0; i < parts.length - 1; ++i) {
    const key = /^\d+$/.test(parts[i]) ? Number(parts[i]) : parts[i];
    if (obj[key] === undefined && create) obj[key] = {};
    obj = obj[key];
  }
  const last = parts[parts.length - 1];
  return [obj, /^\d+$/.test(last) ? Number(last) : last];
};
const getValue = (store, path) => { const [o, k] = walk(store, path); return o ? o[k] : undefined; };
function setValue(store, path, value) {
  const [o, k] = walk(store, path, true);
  o[k] = value;
  delete S.problems[store][path];
  if (store === "head") { delete S.server[path]; revalidateHead(); }
  paintProblems();
  refreshBar();
}

// What the node is sent. The head's settings go as they are read (the passwords and the token only when something was typed or an erase was asked for); the network's go as the parts the panel edits, so nothing else is overwritten.
function headBody() {
  const body = Object.assign({}, S.head);
  Object.keys(body).forEach(key => { if (/_set$/.test(key)) delete body[key]; });
  SECRETS.forEach(key => { if (!body[key]) delete body[key]; });
  return body;
}
function networkBody() {
  const c = S.cfg, ap = c.ap, sta = c.sta, body = { node: { hostname: c.node.hostname }, uplink: c.uplink, ip: Object.assign({}, c.ip),
    ap: { enabled: ap.enabled, ssid: ap.ssid, security: ap.security, channel: ap.channel, hidden: ap.hidden }, sta: { enabled: sta.enabled, ssid: sta.ssid } };
  if (typeof ap.password === "string") body.ap.password = ap.password;
  if (typeof sta.password === "string") body.sta.password = sta.password;
  return body;
}
const isDirty = () => (S.page === "head" && S.head ? JSON.stringify(headBody()) !== S.headSaved : (S.page === "network" || S.page === "wifi") && S.cfg ? JSON.stringify(networkBody()) !== S.saved : false);

// ---- the head's settings: the node says what it refuses ---------------------------------------------------------------------------------

const SECRETS = ["token", "stream_password"];
// Shows what the node refused, next to the field, until that field changes (the ranges are checked by the node, which knows them).
function revalidateHead() {
  S.problems.head = Object.assign({}, S.server);
}

// Writes the problems next to their fields in place, so a field that is being left does not lose the click that left it.
function paintProblems() {
  document.querySelectorAll("[data-store]").forEach(input => {
    const store = input.dataset.store, path = input.dataset.path, code = S.problems[store][path];
    const out = document.getElementById(input.id + "-err");
    if (out) out.textContent = code ? problemText(code) : "";
    if (code) input.setAttribute("aria-invalid", "true"); else input.removeAttribute("aria-invalid");
    input.classList.toggle("bad", !!code);
  });
}

// ---- fields ---------------------------------------------------------------------------------------------------------------------------

const fieldId = (store, path) => "f-" + store + "-" + path.replace(/[^A-Za-z0-9]/g, "-");

// One labelled field of a form. `store` is "cfg" (the network) or "head"; `path` points into it. A problem is written next to the field and tied to it for screen readers.
function field(labelKey, store, path, options = {}) {
  const current = getValue(store, path), problem = S.problems[store][path];
  const disabled = !isAdmin() || options.disabled, id = fieldId(store, path), errId = id + "-err", hintId = id + "-hint";
  const common = { id, disabled, "data-store": store, "data-path": path, "aria-describedby": (options.hint ? hintId + " " : "") + errId, "aria-invalid": problem ? "true" : undefined };
  const factor = options.factor || 1;
  const tail = [options.hint ? el("span", { class: "hint", id: hintId }, options.hint) : null, el("span", { class: "err", id: errId }, problem ? problemText(problem) : "")];
  let input;
  if (options.type === "checkbox") {
    input = el("input", Object.assign({ type: "checkbox", checked: !!current, onchange: e => { setValue(store, path, e.target.checked); if (options.rerender) render(); } }, common));
    return el("div", { class: "field-box" }, el("label", { class: "check", for: id }, input, el("span", {}, t(labelKey))), tail);
  }
  if (options.type === "select") {
    input = el("select", Object.assign({ onchange: e => { const v = e.target.value; setValue(store, path, options.number ? Number(v) : v); if (options.rerender) render(); } }, common),
      options.options.map(([value, text]) => el("option", { value: String(value), selected: String(current) === String(value) }, text)));
  } else if (options.type === "number") {
    const shown = typeof current === "number" && Number.isFinite(current) ? current / factor : "";
    input = el("input", Object.assign({ type: "number", inputMode: "numeric", min: options.min, max: options.max, step: options.step || 1, value: shown, placeholder: options.placeholder || "",
      oninput: e => setValue(store, path, e.target.value === "" ? NaN : Math.round(Number(e.target.value) * factor)),
      onblur: () => { if (store === "head") { S.touched.add(path); paintProblems(); } } }, common));
  } else if (options.type === "password") {
    const stored = getValue(store, path + "_set");
    input = el("input", Object.assign({ type: "password", value: "", autocomplete: "new-password", placeholder: stored ? "••••••••" : "", oninput: e => setValue(store, path, e.target.value) }, common));
  } else {
    input = el("input", Object.assign({ type: "text", value: current === undefined ? "" : current, maxLength: options.max, placeholder: options.placeholder || "", autocomplete: "off",
      oninput: e => setValue(store, path, e.target.value) }, common));
  }
  return el("div", { class: "field" }, el("label", { for: id }, t(labelKey)), input, tail);
}

// ---- the save bar ---------------------------------------------------------------------------------------------------------------------

let barNode = null;
function refreshBar() {
  if (!barNode) return;
  const dirty = isDirty();
  const message = S.message || (dirty ? { kind: "warn", text: t("unsaved") } : S.restartNeeded ? { kind: "warn", text: t("savedRestart") } : null);
  barNode.replaceChildren(...[
    el("span", { class: "msg " + (message ? message.kind : ""), role: "status" }, message ? message.text : ""),
    S.restartNeeded && !dirty ? el("button", { class: "b danger", type: "button", onclick: () => confirm(t("confirmAsk")) && reboot() }, t("restart")) : null,
    dirty ? el("button", { class: "b", type: "button", onclick: discard }, t("discard")) : null,
    dirty ? el("button", { class: "b primary", type: "button", disabled: S.busy, onclick: () => save(false) }, t("save")) : null,
    dirty ? el("button", { class: "b danger", type: "button", disabled: S.busy, onclick: () => save(true) }, t("saveRestart")) : null].filter(Boolean));
  barNode.hidden = !(message || dirty || S.restartNeeded);
}

// The panel's language also lives in the node's settings (ui.language): choosing it here saves it there at once, with no restart. Only an administrator can change settings.
async function persistLanguage() {
  if (!S.cfg || !S.session || !S.session.authenticated || !isAdmin()) return;
  const code = LANGS[lang][0];
  if (S.cfg.ui && S.cfg.ui.language === code) return;
  const r = await api("PUT", "config", { ui: { language: code } });
  if (r.ok && S.cfg.ui) S.cfg.ui.language = code;
}

async function discard() {
  S.problems = { cfg: {}, head: {} }; S.server = {}; S.touched = new Set(); S.message = null;
  if (S.page === "head") await loadHead();
  else if (S.page === "network" || S.page === "wifi") await loadConfig();
  render();
}

async function save(restartAfter) {
  S.busy = true; S.message = { kind: "warn", text: t("working") }; refreshBar();
  const ok = S.page === "head" ? await saveHead() : await saveNetwork();
  S.busy = false;
  if (ok && restartAfter && S.restartNeeded) { reboot(); return; }
  render();
}

async function saveHead() {
  const r = await api("PUT", "head", headBody());
  if (r.ok) {
    S.problems.head = {}; S.server = {}; S.touched = new Set();
    await loadHead();
    S.restartNeeded = S.restartNeeded || !!r.data.restart_required;
    S.message = { kind: "ok", text: r.data.restart_required ? t("savedRestart") : t("saved") };
    return true;
  }
  if (r.status === 400 && Array.isArray(r.data.problems)) {
    S.server = {};
    r.data.problems.forEach(p => { S.server[p.path.replace(/^head\./, "")] = p.code; });
    revalidateHead();
    S.message = { kind: "bad", text: t("e_invalid") };
  } else S.message = { kind: "bad", text: errorText(r.data.error) };
  return false;
}

async function saveNetwork() {
  const r = await api("PUT", "config", networkBody());
  if (r.ok) {
    S.problems.cfg = {};
    await loadConfig();
    S.restartNeeded = S.restartNeeded || !!r.data.restart_required;
    S.message = { kind: "ok", text: r.data.restart_required ? t("savedRestart") : t("saved") };
    return true;
  }
  if ((r.status === 422 || r.status === 400) && r.data.problems) {
    S.problems.cfg = {};
    r.data.problems.forEach(p => { S.problems.cfg[p.path] = p.code; });
    S.message = { kind: "bad", text: t("e_invalid") };
  } else S.message = { kind: "bad", text: errorText(r.data.error) };
  return false;
}

async function loadConfig() {
  const r = await api("GET", "config");
  if (!r.ok) return;
  S.cfg = r.data.config; S.channelAuto = r.data.channel_auto;
  S.saved = JSON.stringify(networkBody());
}
async function loadHead() {
  const r = await api("GET", "head");
  if (!r.ok) return;
  S.head = r.data.head;
  S.headSaved = JSON.stringify(headBody());
}

function waitForNode(first) {
  const wait = async () => {
    const r = await api("GET", "session");
    if (r.ok) location.reload(); else setTimeout(wait, 2000);
  };
  setTimeout(wait, first || 4000);
}
async function reboot() {
  await api("POST", "reboot", {});
  S.rebooting = true; render();
  waitForNode(4000);
}

// ---- pages: shared pieces -------------------------------------------------------------------------------------------------------------

const PAGES = [
  { id: "overview", icon: "◎", label: "navOverview", group: "grpNode" },
  { id: "status", icon: "◉", label: "navStatus", group: "grpNode" },
  { id: "head", icon: "⚙", label: "navHead", group: "grpNode" },
  { id: "network", icon: "⇄", label: "navNetwork", group: "grpNetwork" },
  { id: "wifi", icon: "◌", label: "navWifi", group: "grpNetwork" },
  { id: "users", icon: "☺", label: "navUsers", group: "grpSystem" },
  { id: "update", icon: "⇪", label: "navUpdate", group: "grpSystem" },
  { id: "log", icon: "≣", label: "navLog", group: "grpSystem" },
  { id: "help", icon: "?", label: "navHelp", group: "grpSystem" },
  { id: "about", icon: "ⓘ", label: "navAbout", group: "grpSystem" },
  { id: "system", icon: "▣", label: "navSystem", group: "grpSystem" },
];
const REPO_NAME = "EH3D-NODE-BUNGARD-AUX";

const card = (title, ...kids) => el("section", { class: "card" }, title ? el("h2", {}, title) : null, ...kids);
const kv = rows => el("dl", { class: "kv" }, rows.filter(Boolean).flatMap(([k, v]) => [el("dt", {}, k), el("dd", {}, v)]));
const note = (text, kind) => el("p", { class: "note " + (kind || ""), role: kind === "bad" ? "alert" : undefined }, text);
const yesNo = value => t(value ? "yes" : "no");
const pill = (text, kind) => el("span", { class: "pill " + (kind || "") }, text);

function formatUptime(seconds) {
  const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60);
  return (d ? d + " d " : "") + (h || d ? h + " h " : "") + m + " min";
}
const fmtBytes = b => b >= 1048576 ? (Math.round(b / 104857.6) / 10) + " MB" : b >= 1024 ? Math.round(b / 1024) + " kB" : b + " B";

// ---- the status page (live, read-only) --------------------------------------------------------------------------------------------------

function barBox(id, labelId) {
  const fill = el("div", { class: "fill", id: id + "-fill" });
  return el("div", { class: "bar-track", id, role: "progressbar", "aria-labelledby": labelId, "aria-valuemin": "0", "aria-valuemax": "100", "aria-valuenow": "0" }, fill);
}

// One measure of the table under the head: the median of the good readings of the laser.
async function measureTable() {
  const button = document.getElementById("st-measure");
  if (button) button.disabled = true;
  const r = await api("POST", "head/measure?samples=7", {});
  S.measure = r.ok ? r.data : { ok: false, why: "error" };
  if (!r.ok) S.message = { kind: "bad", text: errorText(r.data.error) };
  render();
}

function statusPage() {
  const m = S.measure;
  return el("div", { class: "grid" },
    el("div", { class: "span-all" }, note(t("statusNoControl"), "info")),
    el("div", { class: "span-all", id: "st-lost" }),
    card(t("stCamera"), el("div", { class: "pills" }, el("span", { id: "st-p-camera" })), el("p", { class: "muted", id: "st-camera-model" }), el("p", { class: "muted" }, t("stCameraHelp"))),
    card(t("stLaser"), el("div", { class: "pills" }, el("span", { id: "st-p-laser" })),
      isAdmin() ? el("div", { class: "actions" }, el("button", { class: "b", type: "button", id: "st-measure", onclick: measureTable }, t("stMeasure"))) : null,
      m ? (m.ok ? el("p", { class: "big" }, t("stMeasureResult", m.distance_mm.toFixed(3), m.sigma_mm.toFixed(3), m.used, m.asked)) : note(t("stMeasureFail", m.why || "?"), "bad")) : null,
      el("p", { class: "muted" }, t("stLaserHelp"))),
    card(t("stSpindle"), el("p", { class: "big" }, el("span", { id: "sp-rpm" }, "—")), el("span", { id: "sp-label", class: "hint" }, t("stSpindleBar")), barBox("sp-bar", "sp-label"),
      el("p", { class: "muted", id: "sp-max" }), el("div", { class: "pills" }, el("span", { id: "st-p-heat" })), el("p", { class: "big" }, el("span", { id: "sp-temp" }, "—"))),
    card(t("stProtect"), el("div", { class: "pills" }, el("span", { id: "st-p-protect" })), el("p", { class: "muted" }, t("stProtectHelp"))),
    card(t("stNode"), kv([[t("firmware"), el("span", { id: "st-version" }, "—")], [t("uptime"), el("span", { id: "st-uptime" }, "—")], [t("address"), el("span", { id: "st-ip" }, "—")]])));
}

function setBar(id, fraction) {
  const box = document.getElementById(id), fill = document.getElementById(id + "-fill");
  if (!box || !fill) return;
  const pct = Math.max(0, Math.min(100, Math.round(fraction * 100)));
  fill.style.width = pct + "%";
  box.setAttribute("aria-valuenow", String(pct));
}
function setText(id, text) { const node = document.getElementById(id); if (node && node.textContent !== text) node.textContent = text; }
function setPill(id, text, kind) { const node = document.getElementById(id); if (node) node.replaceChildren(pill(text, kind)); }

// Draws the polled values into the status page without rebuilding it.
function patchStatus() {
  const lost = document.getElementById("st-lost");
  if (!lost) return;
  const m = S.ms;
  lost.replaceChildren(...(m ? [] : [note(t("stLost"), "bad")]));
  if (!m) return;
  setPill("st-p-camera", t(m.camera ? "stOn" : "stOff"), m.camera ? "ok" : "");
  setText("st-camera-model", m.camera && m.camera_model ? m.camera_model.toUpperCase() : "");
  setPill("st-p-laser", t(m.laser ? "stOn" : "stOff"), m.laser ? "ok" : "");
  const max = S.head ? S.head.rpm_max : 0;
  setText("sp-rpm", m.rpm_sensor ? m.rpm + " rpm" : "—");
  setBar("sp-bar", max && m.rpm_sensor ? m.rpm / max : 0);
  setText("sp-max", !m.rpm_sensor ? t("spNoSensor") : max ? t("spMax", max) : "");
  const heat = m.heat === "critical" ? [t("stHeatCritical"), "bad"] : m.heat === "warn" ? [t("stHeatWarn"), "warn"] : [t("stHeatOk"), "ok"];
  setPill("st-p-heat", heat[0], heat[1]);
  setText("sp-temp", typeof m.temp_c === "number" ? m.temp_c.toFixed(1) + " °C" : t("stNoTemp"));
  setPill("st-p-protect", t(m.protect_open ? "stProtectOpen" : "stProtectClosed"), m.protect_open ? "warn" : "ok");
  setText("st-version", m.version); setText("st-ip", S.status && S.status.network && S.status.network.ip ? S.status.network.ip : "—"); setText("st-uptime", formatUptime(m.uptime_s));
}

// ---- the head page --------------------------------------------------------------------------------------------------------------------

const hf = (labelKey, path, options) => field(labelKey, "head", path, options);
const num = (labelKey, path, min, max, placeholder, extra) => hf(labelKey, path, Object.assign({ type: "number", min, max, placeholder }, extra || {}));
const secret = (labelKey, path, hint) => [hf(labelKey, path, { type: "password", hint }),
  isAdmin() && S.head[path + "_set"] ? hf("fSecretClear", path + "_clear", { type: "checkbox" }) : null];

// Tries the ring from the panel: the session of the panel is enough, the access token of the programs is not needed.
async function lightCall(query) {
  let ok = false;
  try {
    const r = await fetch("api/light?" + query, { method: "POST", credentials: "same-origin", headers: { "X-Requested-With": REQUEST_MARK } });
    ok = r.ok;
  } catch (error) { /* the node did not answer */ }
  S.message = ok ? null : { kind: "bad", text: t("lightTestFailed") };
  refreshBar();
}

function headPage() {
  if (!S.head) return el("p", { class: "muted" }, t("loading"));
  const h = S.head;
  return el("div", {}, note(t("hRestartNote"), "info"), !isAdmin() ? note(t("adminOnly"), "info") : null,
    el("div", { class: "grid wide" },
      card(t("hCamera"), el("p", { class: "muted" }, t("hCameraIntro")),
        hf("fCameraOn", "camera_enabled", { type: "checkbox", rerender: true }),
        h.camera_enabled ? [
          hf("fCameraModel", "camera_model", { type: "select", options: [["auto", t("modelAuto")], ["ov2640", "OV2640"], ["ov5640", "OV5640"]] }),
          el("div", { class: "row" }, num("fWidth", "camera_width", 160, 2592, "640"), num("fHeight", "camera_height", 120, 1944, "480")),
          el("div", { class: "row" }, num("fQuality", "camera_quality", 4, 63, "12", { hint: t("fQualityHint") }), num("fFps", "camera_fps_max", 1, 30, "10"))] : null),
      card(t("hStream"), el("p", { class: "muted" }, t("hStreamIntro")),
        hf("fRtspOn", "rtsp_enabled", { type: "checkbox", rerender: true }),
        h.rtsp_enabled ? num("fRtspPort", "rtsp_port", 1, 65535, "8554") : null,
        hf("fOnvifOn", "onvif_enabled", { type: "checkbox" }),
        hf("fStreamUser", "stream_user", { max: 32 }),
        secret("fStreamPassword", "stream_password", t("fStreamPasswordHint")),
        secret("fToken", "token", t("fTokenHint")),
        !h.token_set ? note(t("noTokenNote"), "info") : null),
      card(t("hLaser"), el("p", { class: "muted" }, t("hLaserIntro")),
        hf("fLaserOn", "laser_enabled", { type: "checkbox", rerender: true }),
        h.laser_enabled ? [
          el("div", { class: "row" }, num("fSamples", "laser_samples", 1, 31, "7"), num("fLaserTimeout", "laser_timeout_ms", 20, 5000, "500")),
          el("div", { class: "row" }, num("fLaserOffset", "laser_offset_mm", -1000, 1000, "0", { step: "any" }), num("fLaserSigma", "laser_sigma_max_mm", 0, 50, "0.2", { step: "any" })),
          el("div", { class: "row" }, num("fLaserMin", "laser_range_min_mm", 0, 4000, "10", { step: "any" }), num("fLaserMax", "laser_range_max_mm", 1, 4000, "400", { step: "any" }))] : null),
      card(t("hLight"), el("p", { class: "muted" }, t("hLightIntro")),
        hf("fLightOn", "light_enabled", { type: "checkbox", rerender: true }),
        h.light_enabled ? [
          el("div", { class: "row" }, num("fLightLeds", "light_leds", 1, 64, "8"), num("fLightLevel", "light_level", 0, 100, "60"), num("fLightMax", "light_max_level", 1, 100, "80")),
          hf("fLightTone", "light_tone", { type: "select", options: [["white", t("toneWhite")], ["cool", t("toneCool")], ["warm", t("toneWarm")], ["red", t("toneRed")], ["green", t("toneGreen")], ["blue", t("toneBlue")]] }),
          el("div", { class: "row" }, num("fLightAutoOff", "light_auto_off_s", 0, 86400, "600"),
            hf("fLightOrder", "light_order", { type: "select", options: [["grb", "GRB (WS2812)"], ["rgb", "RGB"]] })),
          hf("fLightAtStart", "light_on_at_start", { type: "checkbox" }),
          el("div", { class: "row" },
            el("button", { class: "b", type: "button", onclick: () => lightCall("on=1&tone=white") }, t("lightBtnOn")),
            el("button", { class: "b", type: "button", onclick: () => lightCall("on=0") }, t("lightBtnOff")),
            el("button", { class: "b", type: "button", onclick: () => lightCall("cue=ok") }, t("lightBtnOk")),
            el("button", { class: "b", type: "button", onclick: () => lightCall("cue=nok") }, t("lightBtnNok")))] : null),
      card(t("hSpindle"), el("p", { class: "muted" }, t("hSpindleIntro")),
        hf("fRpmOn", "rpm_enabled", { type: "checkbox", rerender: true }),
        h.rpm_enabled ? el("div", { class: "row" }, num("fPulses", "rpm_pulses_per_rev", 1, 64, "1"), num("fRpmMax", "rpm_max", 100, 60000, "24000"), num("fRpmStale", "rpm_stale_ms", 100, 10000, "1000")) : null,
        hf("fTempSensor", "temp_sensor", { type: "select", rerender: true, options: [["none", t("tempNone")], ["ds18b20", "DS18B20"], ["ntc", "NTC"], ["mlx90614", "MLX90614"]] }),
        h.temp_sensor === "ntc" ? el("div", { class: "row" }, num("fNtcSeries", "ntc_series_ohm", 100, 1000000, "10000", { step: "any" }), num("fNtcR25", "ntc_r25_ohm", 100, 1000000, "10000", { step: "any" }), num("fNtcBeta", "ntc_beta", 1000, 10000, "3950", { step: "any" })) : null,
        h.temp_sensor !== "none" ? el("div", { class: "row" }, num("fTempWarn", "temp_warn_c", 0, 200, "55", { step: "any" }), num("fTempCritical", "temp_critical_c", 0, 250, "70", { step: "any" }), num("fTempHyst", "temp_hysteresis_c", 0, 50, "3", { step: "any" })) : null,
        hf("fProtect", "protect_output", { type: "checkbox", hint: t("fProtectHint") }),
        note(t("hSpindleSafety"), "bad"))));
}

// ---- the network page -----------------------------------------------------------------------------------------------------------------

async function scanNetworks() {
  S.scan = { busy: true, list: null, error: "" }; render();
  const r = await api("GET", "wifi/scan");
  S.scan = r.ok ? { busy: false, list: r.data.networks || [], error: "" } : { busy: false, list: null, error: errorText(r.data.error) };
  render();
}

function scanResults() {
  const scan = S.scan;
  if (scan.busy) return el("p", { class: "muted", role: "status" }, t("scanning"));
  if (scan.error) return el("p", { class: "err" }, scan.error);
  if (!scan.list) return null;
  if (!scan.list.length) return el("p", { class: "muted" }, t("noNetworks"));
  return el("div", { class: "scroll" }, el("table", {}, el("thead", {}, el("tr", {}, el("th", {}, t("ssid")), el("th", {}, t("signal")), el("th", {}, t("wifiChannel")), el("th", {}, t("security")), el("th", {}))),
    el("tbody", {}, scan.list.map(n => el("tr", {}, el("td", {}, n.ssid), el("td", { class: "mono" }, n.rssi + " dBm"), el("td", {}, n.channel), el("td", {}, n.security),
      el("td", {}, el("button", { class: "b", type: "button", disabled: !isAdmin(), onclick: () => { setValue("cfg", "sta.enabled", true); setValue("cfg", "sta.ssid", n.ssid); render(); } }, t("useNetwork"))))))));
}

function currentNetworkCard() {
  const n = S.status && S.status.network;
  if (!n) return null;
  return card(t("netNow"), kv([[t("link"), n.link_up ? t("linkUp") : t("linkDown")], [t("address"), n.has_ip ? n.ip : "—"], [t("netmask"), n.netmask || "—"], [t("gateway"), n.gateway || "—"], [t("mac"), n.mac],
    n.sta_ssid ? [t("station"), n.sta_ssid + (n.sta_connected ? " · " + n.sta_rssi + " dBm" : " · " + t("notConnected"))] : null,
    n.sta_error ? [t("staError"), L["se_" + n.sta_error] ? t("se_" + n.sta_error) : String(n.sta_error)] : null,
    n.ap_active ? [t("apActive"), "“" + n.ap_ssid + "” · " + t("channel") + " " + n.ap_channel + " · " + n.ap_clients + " " + t("apClients") + (n.ap_setup ? " · " + t("setupNetwork") : "")] : null]),
    n.ethernet_available && n.ethernet_ok === false ? note(t("ethernetMissing"), "bad") : null);
}

// ---- the summary: what the node is, its hardware, its clock, its flash and its network (the first page of the node) ----------------------------

const TIME_ZONES = [
  ["UTC0", "UTC"], ["WET0WEST,M3.5.0/1,M10.5.0", "Lisbon · London · Dublin"], ["CET-1CEST,M3.5.0,M10.5.0/3", "Madrid · Paris · Berlin · Rome (CET/CEST)"],
  ["EET-2EEST,M3.5.0/3,M10.5.0/4", "Athens · Helsinki · Kyiv (EET/EEST)"], ["MSK-3", "Moscow · Istanbul"], ["GMT0", "Reykjavik · Dakar"],
  ["<-03>3", "Buenos Aires · São Paulo"], ["EST5EDT,M3.2.0,M11.1.0", "New York · Toronto"], ["CST6CDT,M3.2.0,M11.1.0", "Chicago · Mexico City"],
  ["MST7MDT,M3.2.0,M11.1.0", "Denver"], ["PST8PDT,M3.2.0,M11.1.0", "Los Angeles · Vancouver"], ["<-05>5", "Bogotá · Lima"], ["<-04>4", "Caracas · La Paz"],
  ["GST-4", "Dubai"], ["IST-5:30", "India"], ["<+07>-7", "Bangkok · Jakarta"], ["CST-8", "Beijing · Singapore · Hong Kong"], ["JST-9", "Tokyo · Seoul"],
  ["AEST-10AEDT,M10.1.0,M4.1.0/3", "Sydney · Melbourne"], ["NZST-12NZDT,M9.5.0,M4.1.0/3", "Auckland"], ["<+02>-2", "Cairo · Johannesburg"],
];
const zoneName = rule => { const hit = TIME_ZONES.find(z => z[0] === rule); return hit ? hit[1] + " (" + rule + ")" : rule; };
const utcLabel = minutes => "UTC" + (minutes < 0 ? "-" : "+") + String(Math.floor(Math.abs(minutes) / 60)).padStart(2, "0") + ":" + String(Math.abs(minutes) % 60).padStart(2, "0");
const BOARD_NAMES = { "s3-eth": "Waveshare ESP32-S3-ETH", "s3-wifi": "ESP32-S3-WROOM-1 N16R8", "p4": "ESP32-P4" };
const reasonText = code => { const key = "reason" + (code ? code.charAt(0).toUpperCase() + code.slice(1) : ""); return L[key] ? t(key) : t("reasonOther"); };
const kvf = rows => { const d = kv(rows); d.className = "kv framed"; return d; };

function flashCard(flash) {
  const parts = flash.partitions || [];
  const rows = [[t("flashTotal"), fmtBytes(flash.total)], [t("flashAllocated"), fmtBytes(flash.allocated) + " (" + Math.round(flash.allocated * 100 / flash.total) + " %)"], [t("flashUnallocated"), fmtBytes(flash.total - flash.allocated)]];
  parts.forEach(p => {
    const label = p.label.toUpperCase();
    if (p.app) {
      const state = p.running ? " · " + t("slotRunning") : p.next_boot ? " · " + t("slotNextBoot") : "";
      rows.push([label, p.used ? fmtBytes(p.used) + " / " + fmtBytes(p.size) + " (" + Math.round(p.used * 100 / p.size) + " %) · " + fmtBytes(p.size - p.used) + " " + t("flashFree") + " · v" + p.version + state : t("slotEmpty") + " · " + fmtBytes(p.size)]);
    } else rows.push([label, fmtBytes(p.size)]);
  });
  return card(t("ovFlash"), kvf(rows));
}

function overviewPage() {
  const s = S.status;
  if (!s) return el("p", { class: "muted" }, t("loading"));
  const n = s.network || {}, m = S.ms;
  const layoutNote = n.ap_setup ? t("setupNetwork") : n.ap_active ? t("ownNetwork") : "";
  return el("div", { class: "grid" },
    card(t("ovNode"), kvf([[t("nodeId"), s.node_id], [t("nodeName"), s.name], [t("firmware"), s.version + (s.partition ? " (" + s.partition + ")" : "")], [t("uptime"), formatUptime(s.uptime_s)],
      [t("resetReason"), reasonText(s.reset_reason)], [t("memory"), Math.round(s.heap_free / 1024) + " kB" + (s.psram_free ? " + " + Math.round(s.psram_free / 1048576 * 10) / 10 + " MB PSRAM" : "")]])),
    m ? card(t("ovHead"), kvf([[t("stCamera"), t(m.camera ? "stOn" : "stOff") + (m.camera && m.camera_model ? " · " + m.camera_model.toUpperCase() : "")], [t("stLaser"), t(m.laser ? "stOn" : "stOff")],
      [t("stSpindle"), m.rpm_sensor ? m.rpm + " rpm" : "—"], [t("stTemp"), typeof m.temp_c === "number" ? m.temp_c.toFixed(1) + " °C" : "—"]])) : null,
    s.hardware ? card(t("ovHardware"), kvf([[t("chip"), s.hardware.chip.toUpperCase() + " rev " + s.hardware.chip_revision + " · " + s.hardware.cores + " " + t("cores")],
      [t("flash"), s.hardware.flash_mb + " MB"], [t("psram"), s.hardware.psram_mb ? s.hardware.psram_mb + " MB" : t("none")],
      s.hardware.cpu_mhz ? [t("cpuFreq"), s.hardware.cpu_mhz + " MHz"] : null,
      typeof s.hardware.wifi === "boolean" ? [t("radios"), [s.hardware.wifi ? "Wi-Fi" : null, s.hardware.ble ? "Bluetooth LE" : null].filter(Boolean).join(" · ") || t("none")] : null,
      typeof s.hardware.brownout_level === "number" ? [t("brownoutLevel"), String(s.hardware.brownout_level)] : null,
      typeof s.hardware.secure_boot === "boolean" ? [t("secureBoot"), s.hardware.secure_boot ? t("yes") : t("no")] : null,
      typeof s.hardware.flash_encryption === "boolean" ? [t("flashEncryption"), s.hardware.flash_encryption ? t("yes") : t("no")] : null,
      [t("appIdf"), s.hardware.app_idf], [t("bootloaderIdf"), s.hardware.bootloader_idf]])) : null,
    s.time ? card(t("ovTime"), kvf([[t("localTime"), s.time.set ? s.time.local + " (" + utcLabel(s.time.utc_offset_min) + ")" : t("clockNotSet")],
      [t("timeSource"), !s.time.set ? "—" : s.time.synced ? t("timeFromNtp") : s.time.ntp ? t("timeNtpWaiting") : t("timeManual")], [t("timeZone"), zoneName(s.time.zone)]])) : null,
    s.flash ? flashCard(s.flash) : null,
    card(t("ovNetwork"), kvf([[t("board"), BOARD_NAMES[n.board] || n.board || "—"], [t("layout"), n.layout], [t("link"), n.link_up ? t("linkUp") : t("linkDown")], [t("address"), n.has_ip ? n.ip : "—"], [t("netmask"), n.netmask || "—"], [t("gateway"), n.gateway || "—"],
      [t("dns"), n.dns || "—"], [t("mac"), n.mac],
      n.ap_active ? [t("apActive"), "“" + n.ap_ssid + "” · " + t("channel") + " " + n.ap_channel + " · " + n.ap_clients + " " + t("apClients") + " · " + layoutNote] : null,
      n.sta_ssid ? [t("station"), n.sta_ssid + (n.sta_connected ? " · " + n.sta_rssi + " dBm" : " · " + t("notConnected"))] : null]),
      n.ethernet_available && n.ethernet_ok === false ? note(t("ethernetMissing"), "bad") : null));
}

// ---- Wi-Fi: the node's own access point, the network it joins and the backup networks it tries when that one is lost ------------------------------

const MAX_BACKUP_NETWORKS = 3;
function backupNetworks() {
  const list = S.cfg.sta.backup || (S.cfg.sta.backup = []);
  return el("div", {}, el("h3", {}, t("backupNetworksTitle")), el("p", { class: "hint" }, t("backupNetworksHint")),
    list.map((network, i) => {
      const base = "sta.backup." + i + ".";
      return el("div", { class: "row" }, field("ssid", "cfg", base + "ssid", { max: 32 }), field("wifiPassword", "cfg", base + "password", { type: "password", hint: network.password_set ? t("passwordSet") : "" }),
        el("button", { class: "b danger", type: "button", disabled: !isAdmin(), onclick: () => { list.splice(i, 1); refreshBar(); render(); } }, t("removeNetwork")));
    }),
    isAdmin() && list.length < MAX_BACKUP_NETWORKS
      ? el("div", { class: "actions" }, el("button", { class: "b", type: "button", onclick: () => { list.push({ ssid: "", password: "" }); refreshBar(); render(); } }, t("addBackupNetwork")))
      : null);
}

function wifiPage() {
  if (!S.cfg) return el("p", { class: "muted" }, t("loading"));
  const cfg = S.cfg, wired = isWired(), ap = cfg.ap;
  const channels = [[0, t("channelAuto")]].concat(Array.from({ length: 13 }, (_, i) => [i + 1, String(i + 1)]));
  return el("div", {}, note(t("netRestartNote"), "info"),
    el("div", { class: "grid wide" },
      wired ? card(t("wifiSta"), note(t("wiredNote"), "info")) : card(t("wifiSta"), field("staEnable", "cfg", "sta.enabled", { type: "checkbox", rerender: true }),
        cfg.sta.enabled ? el("div", { class: "row" }, field("ssid", "cfg", "sta.ssid", { max: 32 }), field("wifiPassword", "cfg", "sta.password", { type: "password", hint: cfg.sta.password_set ? t("passwordSet") : "" })) : null,
        el("div", { class: "actions" }, el("button", { class: "b", type: "button", disabled: !isAdmin() || S.scan.busy, onclick: scanNetworks }, t("scanNetworks"))), scanResults(),
        el("p", { class: "hint" }, t("scanNote")), note(t("staNote"), "info"),
        cfg.sta.enabled ? backupNetworks() : null),
      card(t("wifiAp"), field("apEnable", "cfg", "ap.enabled", { type: "checkbox", rerender: true }),
        ap.enabled ? [
          el("div", { class: "row" }, field("ssid", "cfg", "ap.ssid", { max: 32 }),
            field("security", "cfg", "ap.security", { type: "select", rerender: true, options: [["wpa2", t("secWpa2")], ["wpa3", t("secWpa3")], ["wpa2wpa3", t("secWpa2Wpa3")], ["open", t("secOpen")]] })),
          ap.security !== "open" ? field("wifiPassword", "cfg", "ap.password", { type: "password", hint: ap.password_set ? t("passwordSet") : "" }) : null,
          field("wifiChannel", "cfg", "ap.channel", { type: "select", number: true, options: channels, hint: ap.channel === 0 ? t("channelNow", S.channelAuto) : "" }),
          field("hidden", "cfg", "ap.hidden", { type: "checkbox" })] : null,
        !wired && !ap.enabled && !cfg.sta.enabled ? note(t("noWifiWarn"), "bad") : null,
        note(t("apNote"), "info"))));
}

// ---- help and about ----------------------------------------------------------------------------------------------------------------------------

const HELP_TOPICS = ["overview", "status", "head", "network", "wifi", "users", "update", "log", "system"];
let helpTopic = "overview";
function helpPage() {
  const paragraphs = t("help_" + helpTopic + "_text").split("\n\n");
  return el("div", { class: "help" },
    el("div", { class: "help-nav" }, HELP_TOPICS.map(topic => el("button", { type: "button", class: topic === helpTopic ? "active" : "", onclick: () => { helpTopic = topic; render(); } }, t("navHelpTab_" + topic)))),
    card(t("help_" + helpTopic + "_title"), ...paragraphs.map(p => el("p", {}, p))));
}

function aboutPage() {
  const s = S.status;
  return el("div", { class: "grid wide" },
    card(null,
      el("p", { class: "eyebrow" }, "EH3D · ELECTRO HOBBY 3D"),
      el("h2", {}, t("appName")),
      el("p", {}, t("aboutDescription")),
      kv([[t("nodeId"), s ? s.node_id : "—"], [t("nodeName"), s ? s.name : "—"], [t("firmware"), s ? s.version : "—"],
        [t("author"), "JuanenRac · Electro Hobby 3D"], [t("license"), "GPL-3.0-or-later"]]),
      el("p", { class: "muted mono" }, "github.com/JuanenRac/" + REPO_NAME),
      el("p", { class: "muted" }, t("aboutIndependent"))));
}

// ---- the update from GitHub: the node looks for the newest release of its own repository and installs it ---------------------------------------

async function githubCheck() {
  S.github.busy = true; S.github.error = ""; render();
  // The look for a release is done by the node in a task of its own: it is started, and asked about every second or so.
  const start = await api("POST", "ota/check", {});
  if (!start.ok) { S.github.busy = false; S.github.checked = true; S.github.available = false; S.github.error = errorText((start.data && start.data.error) || "network"); render(); return; }
  const poll = async (tries) => {
    const r = await api("GET", "ota/check");
    if (r.ok && r.data.state === "checking" && tries < 40) { setTimeout(() => poll(tries + 1), 800); return; }
    S.github.busy = false;
    if (r.ok && r.data.ok) { S.github.checked = true; S.github.available = r.data.update_available; S.github.latest = r.data.latest_version; }
    else { S.github.checked = true; S.github.available = false; S.github.error = errorText((r.data && r.data.error) || "network"); }
    render();
  };
  setTimeout(() => poll(0), 800);
}

// The install runs in the node (a task of its own): this starts it and asks, twice a second, how far it is.
async function githubInstall() {
  S.github.installing = true; S.github.error = ""; S.github.state = "downloading"; S.github.pct = 0; render();
  const r = await api("POST", "ota/install", {});
  if (!r.ok) { S.github.installing = false; S.github.error = errorText(r.data.error); render(); return; }
  const poll = async () => {
    const q = await api("GET", "ota/progress");
    if (!q.ok) { S.rebooting = true; render(); waitForNode(6000); return; }   // the node is already restarting into the new version
    const d = q.data;
    S.github.state = d.state;
    S.github.pct = d.total ? Math.round(d.got * 100 / d.total) : 0;
    if (d.state === "done") { S.rebooting = true; render(); waitForNode(6000); return; }
    if (d.state === "failed") { S.github.installing = false; S.github.error = errorText(d.error); render(); return; }
    render(); setTimeout(poll, 500);
  };
  setTimeout(poll, 500);
}

function githubCard() {
  return card(t("githubUpdateTitle"),
    el("div", { class: "actions" }, el("button", { class: "b", type: "button", disabled: !isAdmin() || S.github.busy, onclick: githubCheck }, S.github.busy ? t("checking") : t("checkGithub"))),
    S.github.checked && !S.github.error ? el("p", { class: "muted" }, S.github.available ? t("githubAvailable", S.github.latest) : t("githubUpToDate")) : null,
    S.github.error ? el("p", { class: "err" }, S.github.error) : null,
    S.github.available ? el("div", { class: "actions" }, el("button", { class: "b danger", type: "button", disabled: !isAdmin() || S.github.installing, onclick: githubInstall }, S.github.installing ? t("installing") : t("installUpdate"))) : null,
    S.github.installing ? el("div", { class: "progress" }, el("i", { style: "width:" + S.github.pct + "%" })) : null,
    S.github.installing ? el("p", { class: "muted", role: "status" }, S.github.state === "verifying" ? t("githubVerifying") : t("githubDownloading", S.github.pct)) : null,
    note(t("githubUpdateNote"), "info"));
}

function networkPage() {
  if (!S.cfg) return el("p", { class: "muted" }, t("loading"));
  const cfg = S.cfg, wired = isWired(), ap = cfg.ap;
  const channels = [[0, t("channelAuto")]].concat(Array.from({ length: 13 }, (_, i) => [i + 1, String(i + 1)]));
  return el("div", {}, note(t("netRestartNote"), "info"),
    el("div", { class: "grid wide" },
      currentNetworkCard(),
      card(t("nodeTitle"), field("hostname", "cfg", "node.hostname", { max: 32, hint: t("hostnameHint") })),
      card(t("netTitle"),
        hasEthernet() ? field("uplink", "cfg", "uplink", { type: "select", rerender: true, options: [["ethernet", t("uplinkEthernet")], ["wifi", t("uplinkWifi")]] }) : null,
        field("dhcp", "cfg", "ip.dhcp", { type: "checkbox", rerender: true }),
        !cfg.ip.dhcp ? el("div", { class: "row" }, field("address", "cfg", "ip.address", { max: 15 }), field("netmask", "cfg", "ip.netmask", { max: 15 }), field("gateway", "cfg", "ip.gateway", { max: 15 }),
          field("dns1", "cfg", "ip.dns1", { max: 15 }), field("dns2", "cfg", "ip.dns2", { max: 15 })) : null,
        wired ? note(t("wiredNote"), "info") : null),
      wired ? null : note(t("wifiMoved"), "info")));
}

// ---- users ----------------------------------------------------------------------------------------------------------------------------

const usersMessage = { text: "", kind: "" };
async function usersAction(promise, done) {
  const r = await promise;
  usersMessage.text = r.ok ? (done || t("saved")) : errorText(r.data.error); usersMessage.kind = r.ok ? "ok" : "bad";
  const list = await api("GET", "users");
  if (list.ok) S.users = list.data;
  render();
  return r;
}

function usersPage() {
  const form = { name: "", password: "", role: "viewer" }, mine = { current: "", password: "" };
  const rows = S.users.map((u, i) => {
    const pw = el("input", { type: "password", id: "upw-" + i, placeholder: t("newPassword"), "aria-label": t("newPassword") + " - " + u.name, autocomplete: "new-password" });
    return el("tr", {}, el("td", {}, u.name, u.name === S.session.user ? el("span", { class: "hint" }, " (" + t("you") + ")") : null),
      el("td", {}, el("select", { "aria-label": t("role") + " - " + u.name, onchange: e => usersAction(api("PUT", "users/" + u.name, { role: e.target.value })) },
        [["admin", t("roleAdmin")], ["viewer", t("roleViewer")]].map(([v, text]) => el("option", { value: v, selected: u.role === v }, text)))),
      el("td", {}, pw), el("td", { class: "actions" }, el("button", { class: "b", type: "button", onclick: () => usersAction(api("PUT", "users/" + u.name, { password: pw.value })) }, t("setPassword")),
        el("button", { class: "b danger", type: "button", onclick: () => confirm(t("confirmAsk")) && usersAction(api("DELETE", "users/" + u.name)) }, t("delete"))));
  });
  const labelled = (key, input) => el("div", { class: "field" }, el("label", {}, t(key), input));
  return el("div", { class: "grid wide" },
    isAdmin() ? card(t("usersTitle"), el("div", { class: "scroll" }, el("table", {}, el("thead", {}, el("tr", {}, el("th", {}, t("user")), el("th", {}, t("role")), el("th", {}, t("newPassword")), el("th", {}))), el("tbody", {}, rows))),
      el("p", { class: "muted" }, t("rolesHelp")),
      el("h3", {}, t("addUser")),
      el("div", { class: "row" }, labelled("user", el("input", { autocomplete: "off", oninput: e => { form.name = e.target.value; } })),
        labelled("newPassword", el("input", { type: "password", autocomplete: "new-password", oninput: e => { form.password = e.target.value; } })),
        labelled("role", el("select", { onchange: e => { form.role = e.target.value; } }, [["viewer", t("roleViewer")], ["admin", t("roleAdmin")]].map(([v, text]) => el("option", { value: v }, text))))),
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", onclick: () => usersAction(api("POST", "users", form)) }, t("add"))),
      usersMessage.text ? el("p", { class: usersMessage.kind === "ok" ? "hint" : "err", role: "status" }, usersMessage.text) : null) : null,
    card(t("myAccount"),
      labelled("currentPassword", el("input", { type: "password", autocomplete: "current-password", oninput: e => { mine.current = e.target.value; } })),
      labelled("newPassword", el("input", { type: "password", autocomplete: "new-password", oninput: e => { mine.password = e.target.value; } })),
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", onclick: async () => {
        const r = await api("PUT", "account", mine);
        if (r.ok) { alert(t("passwordChanged")); S.session.authenticated = false; start(); } else { usersMessage.text = errorText(r.data.error); usersMessage.kind = "bad"; render(); }
      } }, t("changePassword"))), !isAdmin() && usersMessage.text ? el("p", { class: "err", role: "status" }, usersMessage.text) : null));
}

// ---- update, log, system --------------------------------------------------------------------------------------------------------------

function uploadFirmware(file, progressBar, label, done) {
  const request = new XMLHttpRequest();
  request.open("POST", "api/v1/ota");
  request.setRequestHeader("X-Requested-With", REQUEST_MARK);
  request.upload.onprogress = e => { if (e.lengthComputable) { const pct = Math.round(e.loaded * 100 / e.total); progressBar.style.width = pct + "%"; label.textContent = t("uploading", pct); } };
  request.onload = () => {
    let data = {};
    try { data = JSON.parse(request.responseText); } catch (error) { /* keep {} */ }
    done(request.status === 200 ? { ok: true, version: data.version } : { ok: false, error: data.error || "network" });
  };
  request.onerror = () => done({ ok: false, error: "network" });
  request.send(file);
}

// The two firmware slots and the switch between them: the node boots the other one at the next restart (the way back to the previous version, or forward to the new one).
function slotCard() {
  const parts = ((S.status && S.status.flash && S.status.flash.partitions) || []).filter(p => p.app);
  const other = parts.find(p => !p.running), running = parts.find(p => p.running);
  const usable = !!(other && other.used);
  const result = el("p", { class: "muted", role: "status" });
  return card(t("slotsTitle"), el("p", { class: "muted" }, t("slotsHelp")),
    parts.length ? kv(parts.map(p => [p.label, p.used ? t("slotVersion", p.version) + (p.running ? " · " + t("slotRunning") : p.next_boot ? " · " + t("slotNextBoot") : "") : t("slotEmpty")])) : el("p", { class: "muted" }, t("slotsUnknown")), result,
    el("div", { class: "actions" }, el("button", { class: "b danger", type: "button", disabled: !isAdmin() || !usable, onclick: async () => {
      if (!usable || !confirm(t("switchAsk", other.label, other.version))) return;
      const r = await api("POST", "ota/switch", {});
      if (!r.ok) { result.textContent = errorText(r.data.error); result.className = "err"; return; }
      S.rebooting = true; render(); waitForNode(4000);
    } }, usable ? t("switchSlot", other.label, other.version) : t("switchSlotNone")),
    el("button", { class: "b", type: "button", disabled: !isAdmin() || !running, onclick: async () => {
      if (!running || !confirm(t("copyAsk", running.label, running.version, other ? other.label : ""))) return;
      const start = await api("POST", "ota/copy", {});
      if (!start.ok) { result.textContent = errorText(start.data.error); result.className = "err"; return; }
      result.className = "muted";
      const follow = async () => {
        const q = await api("GET", "ota/copy");
        if (!q.ok) { result.textContent = errorText(q.data.error); result.className = "err"; return; }
        const d = q.data;
        if (d.state === "copying") { result.textContent = t("copyRunning", d.total ? Math.round(d.done * 100 / d.total) : 0); setTimeout(follow, 700); return; }
        if (d.state === "done") { result.textContent = t("copyDone", d.version || ""); await refreshNodeStatus(); render(); return; }
        result.textContent = errorText(d.error || "unknown"); result.className = "err";
      };
      result.textContent = t("copyRunning", 0);
      setTimeout(follow, 700);
    } }, t("copySlot", running ? running.label : "", other ? other.label : ""))));
}

function updatePage() {
  const m = S.ms, progress = el("i"), label = el("p", { class: "muted", role: "status" }), file = el("input", { type: "file", accept: ".bin", id: "fw-file" });
  const result = el("p", { class: "muted", role: "status" });
  return el("div", { class: "grid wide" },
    card(t("updateTitle"), m ? kv([[t("firmware"), m.version], [t("slot"), (S.status && S.status.partition) || "—"]]) : null, el("p", { class: "muted" }, t("updateHelp")),
      el("div", { class: "field" }, el("label", { for: "fw-file" }, t("chooseFile")), file), el("div", { class: "progress" }, progress), label, result,
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", disabled: !isAdmin(), onclick: () => {
        if (!file.files[0]) { result.textContent = t("chooseFirst"); return; }
        uploadFirmware(file.files[0], progress, label, r => {
          if (r.ok) { result.textContent = t("updateDone", r.version); S.rebooting = true; render(); waitForNode(6000); }
          else { result.textContent = errorText(r.error); result.className = "err"; }
        });
      } }, t("upload")))),
    githubCard(),
    slotCard());
}

// Copies text to the clipboard. navigator.clipboard only exists on a secure page (HTTPS) and the panel is often opened over plain HTTP, so there is a fallback through a hidden text area.
function copyText(text, button) {
  const done = ok => {
    const out = button.querySelector(".copy-note");
    out.textContent = ok ? t("copied") : t("copyFailed"); button.classList.toggle("bad", !ok);
    setTimeout(() => { out.textContent = ""; button.classList.remove("bad"); }, 1800);
  };
  const fallback = () => {
    const area = el("textarea", { class: "offscreen", "aria-hidden": "true", tabIndex: -1 }); area.value = text;
    document.body.appendChild(area); area.select();
    let ok = false;
    try { ok = document.execCommand("copy"); } catch (error) { ok = false; }
    area.remove(); done(ok);
  };
  if (navigator.clipboard && window.isSecureContext) navigator.clipboard.writeText(text).then(() => done(true), fallback); else fallback();
}

function copyButton(getText) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24"); svg.setAttribute("width", "16"); svg.setAttribute("height", "16"); svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("fill", "none"); svg.setAttribute("stroke", "currentColor"); svg.setAttribute("stroke-width", "1.8"); svg.setAttribute("stroke-linecap", "round"); svg.setAttribute("stroke-linejoin", "round");
  svg.innerHTML = '<rect x="8" y="8" width="12" height="12" rx="2"></rect><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"></path>';
  return el("button", { class: "b copy-btn", type: "button", "aria-label": t("copyLog"), title: t("copyLog"), onclick: e => copyText(getText(), e.currentTarget) }, svg, el("span", { class: "copy-note", role: "status" }));
}

function logPage() {
  return el("div", { class: "grid wide" }, card(t("logTitle"), el("pre", { class: "log", id: "log-box", tabIndex: 0 }, S.log.text),
    el("div", { class: "actions" }, el("button", { class: "b", type: "button", onclick: async () => { S.log = { next: 0, text: "" }; await refreshLog(); } }, t("refresh")), copyButton(() => S.log.text))));
}

function systemPage() {
  const m = S.ms, confirmBox = el("input", { id: "reset-confirm", placeholder: "RESET", autocomplete: "off", "aria-describedby": "reset-help" });
  return el("div", { class: "grid wide" },
    card(t("stNode"), kv([[t("nodeId"), S.session.node_id], [t("firmware"), m ? m.version : "—"], [t("uptime"), m ? formatUptime(m.uptime_s) : "—"], [t("address"), m && m.ip ? m.ip : "—"], [t("mac"), S.session.mac || "—"], [t("author"), "JuanenRac · Electro Hobby 3D"], [t("license"), "GPL-3.0-or-later"]])),
    card(t("maintenance"), el("div", { class: "actions" }, el("button", { class: "b", type: "button", disabled: !isAdmin(), onclick: () => confirm(t("confirmAsk")) && reboot() }, t("rebootNode"))),
      el("h3", {}, t("factoryTitle")), el("p", { class: "muted", id: "reset-help" }, t("factoryHelp")),
      el("div", { class: "actions" }, el("label", { class: "sr-only", for: "reset-confirm" }, t("factoryTitle")), confirmBox, el("button", { class: "b danger", type: "button", disabled: !isAdmin(), onclick: async () => {
        const r = await api("POST", "factory-reset", { confirm: confirmBox.value });
        if (r.ok) { S.rebooting = true; render(); setTimeout(() => location.reload(), 6000); } else alert(errorText(r.data.error));
      } }, t("factoryTitle")))));
}

// ---- data, polling and the shell -------------------------------------------------------------------------------------------------------

async function refreshStatus() {
  const r = await api("GET", "head/status");
  S.ms = r.ok ? r.data : null;
}
async function refreshNodeStatus() {
  const r = await api("GET", "status");
  if (r.ok) S.status = r.data;
}
async function refreshLog() {
  const r = await api("GET", "log?from=" + S.log.next);
  if (!r.ok) return;
  S.log.text = (S.log.text + r.data.text).slice(-24000); S.log.next = r.data.next;
  const box = document.getElementById("log-box");
  if (box) { const bottom = box.scrollTop + box.clientHeight >= box.scrollHeight - 30; box.textContent = S.log.text; if (bottom) box.scrollTop = box.scrollHeight; }
}

let pollTimer = null, tick = 0;
async function poll() {
  if (!S.session || !S.session.authenticated || S.rebooting) return;
  tick += 1;
  if (S.page !== "status" && tick % 3 !== 0) return;   // the status page is read every second, the others every third
  await refreshStatus();
  if (S.page === "status") patchStatus();
  else if (S.page === "log") await refreshLog();
  updatePills();
}

function updatePills() {
  const box = document.getElementById("pills");
  if (!box) return;
  const m = S.ms, ip = S.status && S.status.network && S.status.network.has_ip ? S.status.network.ip : "";
  box.replaceChildren(...(m ? [pill(ip || t("linkDown"), ip ? "ok" : "bad"), pill(t("stCamera") + ": " + t(m.camera ? "stOn" : "stOff"), m.camera ? "ok" : ""), pill("v" + m.version)] : [pill(t("stLost"), "bad")]).filter(Boolean));
}

function shell(content) {
  const groups = [...new Set(PAGES.map(p => p.group))];
  const nav = el("nav", { class: "nav", "aria-label": t("navMenu") }, groups.map(g => [el("div", { class: "nav-title" }, t(g)),
    PAGES.filter(p => p.group === g).map(p => el("button", { type: "button", class: p.id === S.page ? "active" : "", "aria-current": p.id === S.page ? "page" : undefined, onclick: () => go(p.id) }, el("span", { class: "ico", "aria-hidden": "true" }, p.icon), el("span", { class: "nav-label" }, t(p.label))))]));
  const page = PAGES.find(p => p.id === S.page);
  barNode = el("div", { class: "savebar", hidden: true });
  const foot = el("div", { class: "side-foot" },
    el("span", { class: "who" }, el("span", { class: "dot " + (S.status && S.status.network && S.status.network.has_ip ? "ok" : "bad"), "aria-hidden": "true" }), S.session.user + " · " + roleName(S.session.role)),
    langPicker(true), themeButton(),
    el("button", { class: "b", type: "button", onclick: async () => { await api("POST", "logout", {}); S.session.authenticated = false; start(); } }, t("signOut")));
  return el("div", { class: "shell" },
    el("a", { class: "skip", href: "#main", onclick: e => { e.preventDefault(); const m = document.getElementById("main"); if (m) m.focus(); } }, t("skip")),
    // the menu is on the left of a wide screen and on top of a narrow one (a phone): the brand, the pages in their groups and, at the foot, who is signed in, the language and the sign-out
    el("aside", { class: "side" },
      el("div", { class: "brand" }, el("span", { class: "brand-mark", "aria-hidden": "true" }, "EH3D"), el("div", {}, el("strong", {}, t("appName")), el("small", {}, S.session.node_id))),
      nav, foot),
    el("main", { id: "main", tabIndex: -1 }, el("div", { class: "page-head" }, el("div", {}, el("p", { class: "eyebrow" }, t(page.group)), el("h1", {}, t(page.label))), el("div", { class: "pills", id: "pills" })), content),
    barNode);
}

const themeButton = () => el("button", { class: "b", type: "button", onclick: toggleTheme }, themeName());

function langPicker(persist) {
  return el("select", { "aria-label": t("language"), class: "lang", onchange: e => {
    lang = Number(e.target.value);
    try { localStorage.setItem("eh3d_lang", LANGS[lang][0]); } catch (error) { /* ignore */ }
    document.documentElement.lang = LANGS[lang][0];
    if (persist) persistLanguage();
    render();
  } }, LANGS.map((l, i) => el("option", { value: String(i), selected: i === lang }, l[1])));
}

function go(id) { S.page = id; S.message = S.message && S.message.kind === "ok" ? null : S.message; S.touched = new Set(); location.hash = "#/" + id; }

async function enterPage(id) {
  if (id === "status" || id === "head") { if (!S.head) await loadHead(); }
  if (id === "update" || id === "network" || id === "wifi" || id === "overview" || id === "about" || id === "status" || id === "machine" || id === "head") await refreshNodeStatus();
  if (id === "overview") await refreshStatus();
  if (id === "log") await refreshLog();
  if (id === "status") await refreshStatus();
}

function render() {
  const active = document.activeElement, focusId = active && active.id ? active.id : "";
  const selection = active && typeof active.selectionStart === "number" && active.type !== "number" && active.type !== "password" ? [active.selectionStart, active.selectionEnd] : null;
  if (S.rebooting) { $app.replaceChildren(el("div", { class: "center" }, el("div", { class: "login", role: "status" }, el("h1", {}, t("restarting")), el("p", { class: "muted" }, t("loading"))))); return; }
  if (!S.session || S.session.setup) { $app.replaceChildren(setupScreen()); return; }
  if (!S.session.authenticated) { $app.replaceChildren(loginScreen()); return; }
  let content;
  switch (S.page) {
    case "head": content = headPage(); break;
    case "network": content = networkPage(); break;
    case "wifi": content = wifiPage(); break;
    case "help": content = helpPage(); break;
    case "about": content = aboutPage(); break;
    case "users": content = usersPage(); break;
    case "update": content = updatePage(); break;
    case "log": content = logPage(); break;
    case "system": content = systemPage(); break;
    case "status": content = statusPage(); break;
    default: content = overviewPage();
  }
  $app.replaceChildren(shell(content));
  document.title = t(PAGES.find(p => p.id === S.page).label) + " - " + t("appName");
  updatePills();
  if (S.page === "status") patchStatus();
  refreshBar();
  const again = focusId ? document.getElementById(focusId) : null;
  if (again && again !== document.activeElement) { again.focus(); if (selection && typeof again.setSelectionRange === "function") try { again.setSelectionRange(selection[0], selection[1]); } catch (error) { /* not a text field */ } }
}

// ---- login and set-up screens -----------------------------------------------------------------------------------------------------------

// A password field with a button to show what was typed: the login and set-up screens are where a mistyped password locks someone out.
function passwordField(labelKey, inputAttrs) {
  const input = el("input", Object.assign({ type: "password" }, inputAttrs));
  const toggle = el("button", { type: "button", class: "b eye-toggle", "aria-pressed": "false", onclick: () => {
    input.type = input.type === "password" ? "text" : "password"; toggle.setAttribute("aria-pressed", input.type === "text" ? "true" : "false");
  } }, t("showShort"));
  toggle.setAttribute("aria-label", t("showPassword"));
  return el("div", { class: "field" }, el("label", { for: inputAttrs.id }, t(labelKey)), el("div", { class: "password-row" }, input, toggle));
}

// The panel remembers, if asked, only the user name (never the password: that belongs to the browser's own password manager).
const REMEMBER_USER_KEY = "eh3d_remember_user";
function rememberedUser() { try { return localStorage.getItem(REMEMBER_USER_KEY) || ""; } catch (error) { return ""; } }
function rememberUserName(on, user) { try { if (on && user) localStorage.setItem(REMEMBER_USER_KEY, user); else localStorage.removeItem(REMEMBER_USER_KEY); } catch (error) { /* storage may be blocked: the panel works without it */ } }

function brandBlock() {
  return el("div", { class: "brand center-brand" }, el("span", { class: "brand-mark", "aria-hidden": "true" }, "EH3D"), el("div", {}, el("strong", {}, t("appName")), el("small", {}, S.session.node_id)));
}

function loginScreen() {
  const form = { user: rememberedUser(), password: "", remember: false };
  let keepUser = form.user !== "";
  const message = el("p", { class: "err", role: "alert" });
  const submit = async e => {
    e.preventDefault();
    const r = await api("POST", "login", form);
    if (r.ok) { rememberUserName(keepUser, form.user.trim()); await start(); return; }
    message.textContent = r.data.error === "too_many_attempts" && r.data.wait_s ? errorText("too_many_attempts") + " (" + r.data.wait_s + " s)" : errorText(r.data.error);
  };
  return el("div", { class: "center" }, el("form", { class: "login", id: "login-form", name: "login", method: "post", action: "#", onsubmit: submit },
    brandBlock(), el("h1", {}, t("signIn")),
    el("div", { class: "field" }, el("label", { for: "login-user" }, t("user")), el("input", { id: "login-user", name: "username", value: form.user, autocomplete: "username", autocapitalize: "none", spellcheck: false, autofocus: true, required: true, oninput: e => { form.user = e.target.value; } })),
    passwordField("password", { id: "login-password", name: "password", autocomplete: "current-password", required: true, oninput: e => { form.password = e.target.value; } }),
    el("label", { class: "check", for: "login-remember-user" }, el("input", { type: "checkbox", id: "login-remember-user", name: "remember_user", checked: keepUser, onchange: e => { keepUser = e.target.checked; if (!keepUser) rememberUserName(false, ""); } }), el("span", {}, t("rememberUser"))),
    el("label", { class: "check", for: "login-remember" }, el("input", { type: "checkbox", id: "login-remember", name: "remember", onchange: e => { form.remember = e.target.checked; } }), el("span", {}, t("rememberMe"))),
    el("small", { class: "muted" }, t("loginNote")),
    message, el("button", { class: "b primary", type: "submit" }, t("signIn")),
    el("div", { class: "row" }, langPicker(false), themeButton())));
}

function setupScreen() {
  const form = { code: "", user: "admin", password: "", language: LANGS[lang][0], wifi_ssid: "", wifi_password: "" };
  const message = el("p", { class: "err", role: "alert" });
  const submit = async e => {
    e.preventDefault();
    form.language = LANGS[lang][0];
    const r = await api("POST", "setup", form);
    if (r.ok) { S.rebooting = true; render(); waitForNode(6000); return; }
    message.textContent = errorText(r.data.error);
  };
  return el("div", { class: "center" }, el("form", { class: "login", id: "setup-form", onsubmit: submit },
    brandBlock(), el("h1", {}, t("setupTitle")),
    el("p", { class: "muted" }, t("setupIntro")), S.session.setup_ssid ? el("p", { class: "hint" }, t("setupWifi") + ": " + S.session.setup_ssid) : null,
    S.session.mac ? el("p", { class: "hint mono" }, t("mac") + ": " + S.session.mac) : null,
    el("div", { class: "field" }, el("label", { for: "setup-code" }, t("setupCode")), el("input", { id: "setup-code", autocomplete: "off", autocapitalize: "characters", spellcheck: false, required: true, oninput: e => { form.code = e.target.value.trim().toUpperCase(); } })),
    el("div", { class: "field" }, el("label", { for: "setup-user" }, t("adminName")), el("input", { id: "setup-user", name: "username", value: "admin", autocomplete: "username", required: true, oninput: e => { form.user = e.target.value; } })),
    passwordField("newPassword", { id: "setup-password", name: "password", autocomplete: "new-password", required: true, oninput: e => { form.password = e.target.value; } }),
    el("h3", {}, t(hasEthernet() ? "setupWifiOptionalTitle" : "setupWifiTitle")), el("p", { class: "hint" }, t(hasEthernet() ? "setupWifiOptionalHelp" : "setupWifiHelp")),
    el("div", { class: "field" }, el("label", { for: "setup-ssid" }, t("ssid")), el("input", { id: "setup-ssid", autocomplete: "off", maxLength: 32, oninput: e => { form.wifi_ssid = e.target.value; } })),
    passwordField("wifiPassword", { id: "setup-wifi-password", autocomplete: "off", oninput: e => { form.wifi_password = e.target.value; } }),
    message, el("button", { class: "b primary", type: "submit" }, t("createAdmin")),
    el("div", { class: "row" }, langPicker(false), themeButton())));
}

// ---- start ----------------------------------------------------------------------------------------------------------------------------

async function start() {
  const r = await api("GET", "session");
  if (!r.ok) { $app.replaceChildren(el("div", { class: "center" }, el("p", { class: "note bad", role: "alert" }, t("unreachable")))); setTimeout(start, 3000); return; }
  S.session = r.data;
  pickLanguage(S.session.language);
  if (S.session.authenticated) {
    await Promise.all([loadConfig(), loadHead(), refreshStatus()]);
    const users = isAdmin() ? await api("GET", "users") : null;
    if (users && users.ok) S.users = users.data;
    S.page = (location.hash.replace("#/", "") || "overview");
    if (!PAGES.some(p => p.id === S.page)) S.page = "overview";
    await enterPage(S.page);
  }
  render();
  clearInterval(pollTimer);
  pollTimer = setInterval(poll, 1000);
}

window.addEventListener("hashchange", async () => {
  const id = location.hash.replace("#/", "");
  if (PAGES.some(p => p.id === id) && S.session && S.session.authenticated) { S.page = id; await enterPage(id); render(); }
});
start();
