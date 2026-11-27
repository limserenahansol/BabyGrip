import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Local stand-in for the BabyGrip HTTP API (album build). Serves the unchanged
// production page at / and the same page plus test-only file pickers at
// /__qa/upload-harness. No hardware, no outbound requests.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const fixtures = path.join(root, 'qa/fixtures');
const port = Number(process.argv[2] || 8770);
const token = '0123456789abcdef0123456789abcdef';
const MAX_JPEG = 196608, UNIT = 4096, RESERVE = 65536, LIMIT = 2000;
const header = await readFile(path.join(root, 'sketch/BabyGrip_WiFi/BabyGrip_PhoneUI.h'), 'utf8');
const match = /R"BGHTML\(([\s\S]*?)\)BGHTML";/.exec(header);
if (!match || match[1].split('__BG_TOKEN__').length !== 2) throw new Error('HTML/token marker not found exactly once');
const html = match[1].replace('__BG_TOKEN__', token);
const manifest = (await readFile(path.join(fixtures, 'manifest.tsv'), 'utf8')).trim().split('\n').map(l => l.split('\t'));
const harness = `
<aside aria-label="자동검사 전용" style="margin:20px auto;max-width:724px;padding:18px;border:3px dashed #8a4b12;background:#fff6de;color:#442500">
  <h2>자동검사 전용 · 실제 제품 기능 아님</h2>
  <div style="display:flex;flex-wrap:wrap;gap:8px;margin-top:12px">
    <button type="button" data-qa="thirty">사진 30장 (섞인 순서)</button>
    <button type="button" data-qa="mixed">버스트 2 + 스크린샷 + 손상 1</button>
    <button type="button" data-qa="folder">폴더: 사진 5 + 동영상 + 숨김파일</button>
    <button type="button" data-qa="mp3">MP3 준비</button>
  </div>
  <p id="qa-status" role="status">대기</p>
</aside>
<script>
(() => {
  window.confirm = () => true;  // test-only: accept the delete-all dialog
  const thirty = ${JSON.stringify(manifest.map(m => m[0]))};
  const status = document.getElementById('qa-status');
  async function file(name, type) {
    const r = await fetch('/__qa/files/' + encodeURIComponent(name), {cache:'no-store'});
    if (!r.ok) throw new Error(name);
    return new File([await r.blob()], name, {type, lastModified: Date.UTC(2026, 10, 20, 12, 0, 0)});
  }
  function select(id, files) {
    const t = new DataTransfer(); files.forEach(f => t.items.add(f));
    const input = document.getElementById(id); input.files = t.files;
    input.dispatchEvent(new Event('change', {bubbles:true}));
  }
  document.querySelectorAll('[data-qa]').forEach(b => b.addEventListener('click', async () => {
    status.textContent = '준비 중';
    const kind = b.dataset.qa;
    if (kind === 'thirty') select('photos', await Promise.all(thirty.map(n => file(n, 'image/jpeg'))));
    if (kind === 'mixed') select('photos', [await file('BURST_0.JPG','image/jpeg'), await file('BURST_1.JPG','image/jpeg'), await file('Screenshot.png','image/png'), await file('broken.jpg','image/jpeg')]);
    if (kind === 'folder') select('photoFolder', [...await Promise.all(thirty.slice(0, 5).map(n => file(n, 'image/jpeg'))), await file('clip.mov','video/quicktime'), new File(['x'], '.DS_Store')]);
    if (kind === 'mp3') select('noiseFile', [new File([new Uint8Array([0x49,0x44,0x33,4,0,0,0,0,0,0, ...new Array(4000).fill(0xff)])], 'sleep.mp3', {type:'audio/mpeg'})]);
    status.textContent = kind + ' 선택 완료';
  }));
})();
</script>`;
const harnessHtml = html.replace('</body>', harness + '\n</body>');

const state = {
  napRunning:false, napElapsed:0, totalSecs:0, clockAvailable:false, noisePlaying:false, decoderRunning:false,
  audioSeconds:0, noiseRemaining:0, volume:3, volumeCap:5, timerMinutes:30, track:'white', lullabyReady:true,
  customNoise:false, soundMessage:'Tap PLAY to start', photoIndex:0, photoFormat:'jpeg', photoNaming:'time-id',
  photoLimit:LIMIT, photoMaxBytes:MAX_JPEG, storageAllocationUnit:UNIT, storageReserve:RESERVE, autoplay:false,
  storageReady:true, soundStorageReady:true, volumes:2, ssid:'BabyGrip-QA-LOCAL',
  night:false, nightAuto:true, nightStart:'20:00', nightEnd:'07:00', dayLight:50, nightLight:6, glanceAuto:true
};
const builtInTracks = [{key:'white', title:'WHITE NOISE', user:false}];
const userTracks = ['01-rain','02-ocean','03-stream','04-pink_noise','05-brown_noise','06-fan','07-heartbeat','08-shush'];
const lullabyTrack = {key:'lullaby', title:'LULLABY', user:false};
const soundTitle = base => base.replace(/^\d\d-/, '').replace(/[_-]/g, ' ').toUpperCase();
const tracks = () => [...builtInTracks, ...userTracks.slice().sort().map(b => ({key:'f:' + b, title:soundTitle(b), user:true})), lullabyTrack];
// Feeding log mock: same record fields as the device.
const logs = []; let logRev = 1, nextId = 1, running = null;
const nowSecs = () => Math.floor(Date.now() / 1000);
function sideSecs(r, right) { let s = right ? r.right : r.left; if (running === r && r.onRight === right) s += nowSecs() - r.segStart; return s; }
function feedSnapshot() {
  const feeds = logs.filter(r => r.kind <= 2), diapers = logs.filter(r => r.kind >= 3);
  const last = feeds.at(-1), breast = logs.filter(r => r.kind === 1).at(-1), diaper = diapers.at(-1);
  const f = {rev: logRev, count: logs.length, running: Boolean(running), saved: true,
    today: {feeds: feeds.length, breast: feeds.filter(r => r.kind === 1).length, bottles: feeds.filter(r => r.kind === 2).length,
      ml: feeds.reduce((a, r) => a + r.ml, 0), diapers: diapers.length, wet: diapers.filter(r => r.kind === 3 || r.kind === 5).length,
      dirty: diapers.filter(r => r.kind === 4 || r.kind === 5).length}};
  if (running) Object.assign(f, {side: running.onRight ? 'right' : 'left', left: sideSecs(running, false), right: sideSecs(running, true), runningId: running.id});
  if (last) Object.assign(f, {lastId: last.id, lastAgo: nowSecs() - last.start, lastKind: last.kind});
  if (breast) f.lastSide = breast.onRight ? 'right' : 'left';
  if (diaper) f.diaperAgo = nowSecs() - diaper.start;
  return f;
}
function addLog(kind, extra = {}) { const r = {id: nextId++, kind, start: nowSecs(), left: 0, right: 0, ml: 0, onRight: false, flags: 0, ...extra}; logs.push(r); logRev++; return r; }
function endSegment() { if (!running) return; const s = nowSecs() - running.segStart; if (running.onRight) running.right += s; else running.left += s; }
const settings = { storageTotal: 12873728, failNext: 0, fullAfter: -1, delayMs: 0 };
const photos = new Map(); // name -> {size, width, height, progressive, sha256}
const uploads = [], rejected = [], calls = [];
let rev = 1;
const allocation = n => Math.ceil(n / UNIT) * UNIT;
const used = () => [...photos.values()].reduce((a, p) => a + allocation(p.size), 0) + (state.customNoise ? allocation(4010) : 0);
const free = () => Math.max(0, settings.storageTotal - used() - 2 * RESERVE);
function snapshot() {
  const names = [...photos.keys()].sort();
  return {...state, photoCount: names.length, photosRev: rev, storageTotal: settings.storageTotal,
          storageUsed: used(), storageFree: free(), photoIndex: Math.min(state.photoIndex, Math.max(0, names.length - 1)),
          tracks: tracks(), maxTracks: 24, feed: feedSnapshot()};
}
function reply(res, status, body) { res.writeHead(status, {'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'}); res.end(JSON.stringify(body)); }
function bad(res, status, error, code) { rejected.push({status, error, code}); reply(res, status, {error, code}); }
function sof(buf) {
  if (buf[0] !== 0xff || buf[1] !== 0xd8) return null;
  let i = 2;
  while (i + 9 < buf.length) {
    if (buf[i] !== 0xff) return null;
    const marker = buf[i + 1], length = buf.readUInt16BE(i + 2);
    if (marker >= 0xc0 && marker <= 0xc3) return {progressive: marker === 0xc2, height: buf.readUInt16BE(i + 5), width: buf.readUInt16BE(i + 7)};
    i += 2 + length;
  }
  return null;
}
function part(req, raw) {
  const b = /boundary=(?:"([^"]+)"|([^;\s]+))/i.exec(req.headers['content-type'] || '');
  if (!b) return null;
  const sep = Buffer.from('--' + (b[1] || b[2]));
  const head = raw.indexOf(Buffer.from('\r\n\r\n'));
  const end = raw.lastIndexOf(Buffer.concat([Buffer.from('\r\n'), sep, Buffer.from('--')]));
  if (head < 0 || end < head) return null;
  const disposition = raw.subarray(0, head).toString('utf8');
  const filename = /filename="([^"]*)"/.exec(disposition);
  return {filename: filename ? filename[1] : '', data: raw.subarray(head + 4, end)};
}
const server = createServer(async (req, res) => {
  try {
    const url = new URL(req.url, 'http://x');
    if (settings.delayMs) await new Promise(r => setTimeout(r, settings.delayMs));
    if (req.method === 'GET' && (url.pathname === '/' || url.pathname === '/__qa/upload-harness')) {
      res.writeHead(200, {'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store',
        'Content-Security-Policy':"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"});
      res.end(url.pathname === '/' ? html : harnessHtml); return;
    }
    if (req.method === 'GET' && url.pathname.startsWith('/__qa/files/')) {
      const name = path.basename(decodeURIComponent(url.pathname.slice(12)));
      res.writeHead(200, {'Cache-Control':'no-store'}); res.end(await readFile(path.join(fixtures, name))); return;
    }
    if (req.method === 'GET' && url.pathname === '/__qa/audit') {
      reply(res, 200, {settings, state: snapshot(), photos: [...photos.entries()].sort().map(([name, p]) => ({name, ...p,
        taken: /^p[0-9a-f]{16}\.jpg$/.test(name) ? new Date(parseInt(name.slice(1, 9), 16) * 1000).toISOString().slice(0, 19) : null})),
        uploads, rejected, calls}); return;
    }
    const chunks = []; for await (const c of req) chunks.push(c);
    const raw = Buffer.concat(chunks);
    if (req.method === 'POST' && url.pathname === '/__qa/settings') {
      Object.assign(settings, JSON.parse(raw.toString('utf8') || '{}'));
      if (settings.reset) { photos.clear(); uploads.length = 0; rejected.length = 0; calls.length = 0; rev++; delete settings.reset; }
      reply(res, 200, settings); return;
    }
    if (req.method === 'GET' && url.pathname === '/api/state') { reply(res, 200, snapshot()); return; }
    if (req.method === 'GET' && url.pathname === '/api/photos') { const names = [...photos.keys()].sort(); reply(res, 200, {rev, count: names.length, photos: names}); return; }
    if (req.method === 'GET' && url.pathname === '/api/feeds') {
      reply(res, 200, {rev: logRev, clock: state.clockAvailable, now: nowSecs(), items: logs.slice().reverse().map(r => ({id: r.id, kind: r.kind,
        ago: nowSecs() - r.start, left: sideSecs(r, false), right: sideSecs(r, true), ml: r.ml, flags: (r === running ? 1 : 0) | (r.onRight ? 2 : 0)}))});
      return;
    }
    if (req.method !== 'POST') { bad(res, 404, '경로 없음', 'missing'); return; }
    if (req.headers['x-babygrip-token'] !== token) { bad(res, 403, '인증 실패', 'auth'); return; }
    const form = new URLSearchParams(raw.toString('utf8'));
    calls.push({path: url.pathname, action: form.get('action'), value: form.get('value')});
    if (url.pathname === '/api/time') { state.clockAvailable = true; reply(res, 200, {ok: true}); return; }
    if (url.pathname === '/api/control') {
      const action = form.get('action');
      if (action === 'noise_start') { state.noisePlaying = true; state.soundMessage = 'Playing - stops automatically'; }
      else if (action === 'noise_stop') { state.noisePlaying = false; state.soundMessage = 'Stopped'; }
      else if (action === 'noise_default') { state.customNoise = false; state.noisePlaying = false; state.track = 'white'; state.soundMessage = 'White noise ready'; }
      else if (action === 'nap_start') state.napRunning = true;
      else if (action === 'nap_stop') state.napRunning = false;
      else if (action === 'track') { if (!tracks().some(t => t.key === form.get('value'))) { bad(res, 400, '음원을 선택해 주세요.', 'error'); return; } state.track = form.get('value'); state.noisePlaying = false; state.soundMessage = 'Sound selected'; }
      else if (action === 'sound_delete') { const i = userTracks.indexOf((form.get('value') || '').slice(2)); if (i < 0) { bad(res, 404, '지울 음원을 찾지 못했습니다.', 'missing'); return; } userTracks.splice(i, 1); if (state.track === form.get('value')) state.track = 'white'; }
      else if (action === 'night') state.night = form.get('value') === '1';
      else if (action === 'night_auto') { if (!/^\d\d:\d\d$/.test(form.get('start')) || form.get('start') === form.get('end')) { bad(res, 400, '밤 모드 시간을 확인해 주세요.', 'error'); return; } Object.assign(state, {nightAuto: form.get('value') === '1', nightStart: form.get('start'), nightEnd: form.get('end')}); }
      else if (action === 'brightness') Object.assign(state, {dayLight: Number(form.get('day')), nightLight: Number(form.get('night'))});
      else if (action === 'glance') state.glanceAuto = form.get('value') === '1';
      else if (action === 'look') {}
      else if (action === 'timer') state.timerMinutes = Number(form.get('value'));
      else if (action === 'volume') state.volume = Number(form.get('value'));
      else if (action === 'slideshow') state.autoplay = form.get('value') === '1';
      else if (action === 'next' || action === 'previous') {}
      else { bad(res, 400, '알 수 없는 명령입니다.', 'error'); return; }
      reply(res, 200, snapshot()); return;
    }
    if (url.pathname === '/api/delete') {
      const name = form.get('name');
      if (!photos.delete(name)) { bad(res, 404, '사진을 찾지 못했습니다.', 'missing'); return; }
      rev++; reply(res, 200, {ok: true, photoCount: photos.size}); return;
    }
    if (url.pathname === '/api/delete_all') {
      if (form.get('confirm') !== 'all') { bad(res, 400, '확인 필요', 'confirm'); return; }
      const removed = photos.size; photos.clear(); rev++;
      reply(res, 200, {ok: true, removed, failed: 0, photoCount: 0}); return;
    }
    if (url.pathname === '/api/feed') {
      const action = form.get('action');
      if (action === 'left' || action === 'right') {
        const right = action === 'right';
        if (running && running.onRight === right) { endSegment(); running = null; }
        else if (running) { endSegment(); running.onRight = right; running.segStart = nowSecs(); }
        else { running = addLog(1, {onRight: right, segStart: nowSecs()}); }
        logRev++;
      } else if (action === 'stop') { endSegment(); running = null; logRev++; }
      else if (action === 'bottle') addLog(2, {ml: Number(form.get('value')) || 0});
      else if (action === 'diaper') addLog({wet: 3, dirty: 4, both: 5, any: 6}[form.get('value')] || 6);
      else if (action === 'delete') { const i = logs.findIndex(r => r.id === Number(form.get('id'))); if (i < 0) { bad(res, 404, '기록을 찾지 못했습니다.', 'missing'); return; } if (logs[i] === running) running = null; logs.splice(i, 1); logRev++; }
      else if (action === 'update') { const r = logs.find(x => x.id === Number(form.get('id'))); if (!r) { bad(res, 404, '기록을 찾지 못했습니다.', 'missing'); return; } if (form.has('ml')) r.ml = Number(form.get('ml')); logRev++; }
      else { bad(res, 400, '알 수 없는 기록 명령입니다.', 'error'); return; }
      reply(res, 200, snapshot()); return;
    }
    if (url.pathname === '/api/noise') {
      const p = part(req, raw);
      const name = url.searchParams.get('name') || '';
      if (!/^[a-z0-9_-]{1,20}$/.test(name)) { bad(res, 400, '음원 이름을 확인해 주세요.', 'name'); return; }
      if (!p || p.data.length < 128 || !(p.data.subarray(0, 3).toString() === 'ID3' || (p.data[0] === 0xff && (p.data[1] & 0xe0) === 0xe0))) { bad(res, 400, 'MP3 형식의 파일을 선택해 주세요.', 'invalid'); return; }
      if (!userTracks.includes(name)) userTracks.push(name);
      state.track = 'f:' + name; state.noisePlaying = false; state.soundMessage = 'Ready - tap PLAY';
      reply(res, 200, {ok: true, key: 'f:' + name}); return;
    }
    if (url.pathname === '/api/photo') {
      if (settings.failNext > 0) { settings.failNext--; rejected.push({status: 'dropped'}); req.socket.destroy(); return; }
      const p = part(req, raw);
      const size = Number(url.searchParams.get('size'));
      if (!p) { bad(res, 400, '파일 없음', 'invalid'); return; }
      if (!/^p[0-9a-f]{16}\.jpg$/.test(p.filename)) { bad(res, 400, '이름 오류', 'name'); return; }
      if (photos.has(p.filename)) { uploads.push({name: p.filename, duplicate: true}); reply(res, 200, {ok: true, duplicate: true, name: p.filename, photoCount: photos.size, storageFree: free()}); return; }
      if (photos.size >= LIMIT) { bad(res, 409, '한도', 'limit'); return; }
      if (settings.fullAfter === 0 || allocation(p.data.length) + RESERVE > free() + RESERVE) { bad(res, 507, '기기 저장 공간이 가득 찼습니다.', 'full'); return; }
      if (p.data.length !== size || size > MAX_JPEG) { bad(res, 400, '크기 불일치', 'incomplete'); return; }
      const info = sof(p.data);
      if (!info || info.width !== 412 || info.height !== 412 || info.progressive || p.data.at(-2) !== 0xff || p.data.at(-1) !== 0xd9) { bad(res, 400, '사진을 열지 못했습니다.', 'invalid'); return; }
      if (settings.fullAfter > 0) settings.fullAfter--;
      photos.set(p.filename, {size, ...info, sha256: createHash('sha256').update(p.data).digest('hex').slice(0, 16)});
      uploads.push({name: p.filename, size}); rev++;
      reply(res, 200, {ok: true, duplicate: false, name: p.filename, photoCount: photos.size, storageFree: free()}); return;
    }
    bad(res, 404, '경로 없음', 'missing');
  } catch (error) { if (!res.headersSent) reply(res, 500, {error: error.message}); else res.destroy(); }
});
server.listen(port, '127.0.0.1', () => console.log(`BabyGrip album fixture on http://127.0.0.1:${port}/__qa/upload-harness`));
