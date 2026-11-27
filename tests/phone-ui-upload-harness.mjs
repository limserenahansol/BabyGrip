import { createServer } from 'node:http';
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { deflateSync } from 'node:zlib';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Browser-only disposable upload harness. No hardware access or outbound requests.
// / is the unchanged production UI. /__qa/upload-harness adds visible test-only
// FileList preparation controls; the real production validation/upload handlers run.
// Admin: POST /__qa/settings with the fixture token and JSON
// {offline:boolean,rejectNextUpload:boolean}; GET /__qa/audit always works.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const fixtureDir = path.join(root, 'qa/upload-harness');
const token = '0123456789abcdef0123456789abcdef';
const PHOTO_BYTES = 412 * 412 * 2;
const MAX_JPEG_BYTES = 196608;
const allocation = n => Math.ceil(n / 4096) * 4096;
const MAX_MP3_BYTES = 2 * 1024 * 1024;
const STORAGE_TOTAL = 10235904;
const STORAGE_BASELINE = 1372160;
const ORIGINAL_MP3_BYTES = 540647;
const header = await readFile(path.join(root, 'sketch/BabyGrip_WiFi/BabyGrip_PhoneUI.h'), 'utf8');
const match = /R"BGHTML\(([\s\S]*?)\)BGHTML";/.exec(header);
if (!match || match[1].split('__BG_TOKEN__').length !== 2) throw new Error('HTML/token marker not found exactly once');
const html = match[1].replace('__BG_TOKEN__', token);
const harnessControls = `
<aside aria-label="자동검사 전용 파일 준비" style="margin:20px auto;max-width:724px;padding:18px;border:3px dashed #8a4b12;background:#fff6de;color:#442500">
  <h2>자동검사 전용 · 실제 제품 기능 아님</h2>
  <p>생성한 검사 파일을 선택한 상태만 만듭니다. 아래에서 준비한 뒤 위쪽 실제 사진/MP3 업로드 버튼을 누르세요. 휴대폰 파일 선택창이나 Wi-Fi 무선을 시험하는 기능은 아닙니다.</p>
  <div style="display:flex;flex-wrap:wrap;gap:8px;margin-top:12px">
    <button type="button" data-qa-prepare="photos">사진 두 장 준비</button>
    <button type="button" data-qa-prepare="bulk">사진 30장 준비</button>
    <button type="button" data-qa-prepare="folder">폴더 사진과 동영상 준비</button>
    <button type="button" data-qa-prepare="mp3">정상 MP3 준비</button>
    <button type="button" data-qa-prepare="extension">확장자 오류 MP3 준비</button>
    <button type="button" data-qa-prepare="oversize">용량 초과 MP3 준비</button>
    <button type="button" data-qa-prepare="corruptMp3">손상 MP3 준비</button>
    <button type="button" data-qa-prepare="corruptPhoto">손상 사진 준비</button>
    <button type="button" data-qa-prepare="clear">선택 비우기</button>
  </div>
  <p id="qa-file-status" role="status" aria-live="polite">검사 파일 준비 대기</p>
</aside>
<script>
(() => {
  const status = document.getElementById('qa-file-status');
  const buttons = Array.from(document.querySelectorAll('[data-qa-prepare]'));
  async function localFile(name, type) {
    const response = await fetch('/__qa/files/' + name, {cache:'no-store'});
    if (!response.ok) throw new Error('검사 파일을 읽지 못했습니다: ' + name);
    return new File([await response.blob()], name, {type, lastModified:0});
  }
  function selectFiles(id, files) {
    const input = document.getElementById(id);
    const selected = new DataTransfer();
    files.forEach(file => selected.items.add(file));
    input.files = selected.files;
    input.dispatchEvent(new Event('change', {bubbles:true}));
  }
  async function prepare(kind) {
    buttons.forEach(button => { button.disabled = true; });
    status.textContent = '검사 파일을 준비하는 중…';
    try {
      if (kind === 'photos') {
        const files = await Promise.all([localFile('colors.png','image/png'),localFile('crop.png','image/png')]);
        selectFiles('photos', files);
        status.textContent = '사진 2장 선택 완료 · 위쪽 사진 업로드 버튼을 누르세요.';
      } else if (kind === 'bulk') {
        const source = await localFile('colors.png','image/png');
        selectFiles('photos', Array.from({length:30},(_,i)=>new File([source], 'color-'+i+'.png', {type:'image/png',lastModified:0})));
        status.textContent = '사진 30장 선택 완료';
      } else if (kind === 'folder') {
        const source = await localFile('crop.png','image/png');
        selectFiles('photoFolder', [source, new File(['test-video'], 'clip.mov', {type:'video/quicktime'}), new File(['meta'],'.DS_Store')]);
        status.textContent = '폴더 사진 1장과 제외할 파일 2개 선택 완료';
      } else if (kind === 'mp3') {
        selectFiles('noiseFile', [await localFile('lullaby.mp3','audio/mpeg')]);
        status.textContent = '정상 MP3 선택 완료 · 1,646,357바이트';
      } else if (kind === 'extension') {
        selectFiles('noiseFile', [new File(['not an MP3'],'invalid.wav',{type:'audio/wav',lastModified:0})]);
        status.textContent = '확장자 오류 파일 선택 완료';
      } else if (kind === 'oversize') {
        selectFiles('noiseFile', [new File([new Uint8Array(2000001)],'too-large.mp3',{type:'audio/mpeg',lastModified:0})]);
        status.textContent = '용량 초과 MP3 선택 완료 · 2,000,001바이트';
      } else if (kind === 'corruptMp3') {
        selectFiles('noiseFile', [new File(['bad'],'corrupt.mp3',{type:'audio/mpeg',lastModified:0})]);
        status.textContent = '손상 MP3 선택 완료 · 3바이트';
      } else if (kind === 'corruptPhoto') {
        selectFiles('photos', [await localFile('broken.jpg','image/jpeg')]);
        status.textContent = '손상 사진 선택 완료';
      } else if (kind === 'clear') {
        selectFiles('photos', []); selectFiles('noiseFile', []);
        status.textContent = '파일 선택 비움';
      } else throw new Error('알 수 없는 검사 종류');
    } catch (error) { status.textContent = '검사 파일 준비 실패: ' + error.message; }
    finally { buttons.forEach(button => { button.disabled = false; }); }
  }
  buttons.forEach(button => button.addEventListener('click', () => prepare(button.dataset.qaPrepare)));
})();
</script>`;
const harnessHtml = html.replace('</body>', harnessControls + '\n</body>');
const assets = new Map([
  ['/__qa/files/colors.png', {file:'colors.png', type:'image/png'}],
  ['/__qa/files/crop.png', {file:'crop.png', type:'image/png'}],
  ['/__qa/files/broken.jpg', {file:'broken.jpg', type:'image/jpeg'}],
  ['/__qa/files/lullaby.mp3', {file:'lullaby.mp3', type:'audio/mpeg'}]
]);
const state = {
  napRunning:false, napElapsed:0, totalSecs:0, clockAvailable:false,
  noisePlaying:false, decoderRunning:false, audioSeconds:0, noiseRemaining:0,
  volume:3, volumeCap:5, timerMinutes:30, soundMessage:'Tap PLAY to start',
  track:'white', lullabyReady:true, photoCount:0, photoIndex:0, autoplay:false,
  storageReady:true, storageTotal:STORAGE_TOTAL, storageUsed:STORAGE_BASELINE,
  ssid:'BabyGrip-QA-LOCAL-FIXTURE', photos:[], photoFormat:'jpeg', photoLimit:1024,
  photoMaxBytes:MAX_JPEG_BYTES, storageAllocationUnit:4096, storageReserve:65536
};
const calls = [], uploads = [], rejected = [];
let offline = false, rejectNextUpload = false, nextPhoto = 1, storageOverhead = 0;
const photoSizes = new Map();
let napStart = 0, noiseStart = 0, noiseEnd = 0, uploadedNoiseBytes = ORIGINAL_MP3_BYTES;
let statePolls = 0;
function stopNoise() { state.noisePlaying=false; state.decoderRunning=false; state.noiseRemaining=0; state.soundMessage='Stopped'; }
function snapshot() {
  state.napElapsed = state.napRunning ? Math.floor((Date.now()-napStart)/1000) : 0;
  if (state.noisePlaying && Date.now() >= noiseEnd) stopNoise();
  state.noiseRemaining = state.noisePlaying ? Math.max(0,Math.floor((noiseEnd-Date.now())/1000)) : 0;
  state.audioSeconds = state.noisePlaying ? Math.floor((Date.now()-noiseStart)/1000) : 0;
  state.photoCount=state.photos.length;
  state.storageUsed=STORAGE_BASELINE + [...photoSizes.values()].reduce((a,b)=>a+allocation(b),0) + allocation(uploadedNoiseBytes)-allocation(ORIGINAL_MP3_BYTES) + storageOverhead;
  state.storageFree=state.storageTotal-state.storageUsed;
  return {...state, totalSecs:state.totalSecs+state.napElapsed, photos:[...state.photos]};
}
function reply(res,status,body) {
  res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});
  res.end(JSON.stringify(body));
}
function bad(res,status,message) { reply(res,status,{error:message}); }
function integer(form,key,min,max) {
  const raw=form.get(key);
  if (!raw || !/^-?\d+$/.test(raw)) return null;
  const value=Number(raw);
  return Number.isSafeInteger(value)&&value>=min&&value<=max ? value : null;
}
function filePart(req,raw) {
  const boundary=/^multipart\/form-data;.*boundary=(?:"([^"]+)"|([^;\s]+))/i.exec(req.headers['content-type']||'');
  if (!boundary) throw new Error('multipart/form-data 파일이 필요합니다.');
  const separator=Buffer.from('--'+(boundary[1]||boundary[2]));
  const start=raw.indexOf(Buffer.from('\r\n\r\n'));
  const end=raw.lastIndexOf(Buffer.concat([Buffer.from('\r\n'),separator,Buffer.from('--')]));
  if (start<0||end<start+4||raw.indexOf(Buffer.concat([Buffer.from('\r\n'),separator,Buffer.from('\r\n')]),start+4)>=0) throw new Error('파일은 한 번에 하나씩 올려 주세요.');
  return raw.subarray(start+4,end);
}
function uploadInfo(route,file) {
  const result={path:route,bytes:file.length,sha256:createHash('sha256').update(file).digest('hex')};
  if (route==='/api/photo' && file.length===PHOTO_BYTES) {
    result.row200={redProbe:file.readUInt16LE((200*412+10)*2),greenProbe:file.readUInt16LE((200*412+205)*2),blueProbe:file.readUInt16LE((200*412+400)*2)};
    let nonGreenInterior=0,nonGreenEdge=0;
    for(let i=0;i<file.length;i+=2) if(file.readUInt16LE(i)!==0x07e0) { const x=(i/2)%412; x>1&&x<410 ? nonGreenInterior++ : nonGreenEdge++; }
    result.nonGreenInterior=nonGreenInterior; result.nonGreenEdge=nonGreenEdge;
  }
  return result;
}
const server=createServer(async(req,res)=>{
  try {
    if(req.method==='GET'&&(req.url==='/'||req.url==='/__qa/upload-harness')) {
      res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store','Content-Security-Policy':"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"}); res.end(req.url==='/'?html:harnessHtml);return;
    }
    if(req.method==='GET'&&req.url.startsWith('/__qa/files/')) {
      const asset=assets.get(req.url);
      if(!asset){bad(res,404,'허용되지 않은 검사 파일');return;}
      res.writeHead(200,{'Content-Type':asset.type,'Cache-Control':'no-store'});
      res.end(await readFile(path.join(fixtureDir,asset.file)));return;
    }
    if(req.method==='GET'&&req.url==='/__qa/audit') { reply(res,200,{scope:'Production browser upload handlers with synthetic FileList; not ESP32 or phone radio',productionHeaderSha256:createHash('sha256').update(header).digest('hex'),state:snapshot(),statePolls,offline,rejectNextUpload,calls,uploads,rejected});return; }
    if(req.method==='POST'&&req.headers['x-babygrip-token']!==token) { rejected.push({path:req.url,status:403,reason:'token'});bad(res,403,'인증 실패');return; }
    const admin=req.method==='POST'&&req.url==='/__qa/settings';
    if(offline&&!admin) { bad(res,503,'테스트 연결 끊김');return; }
    if(req.method==='GET'&&req.url==='/api/state') { statePolls++;reply(res,200,snapshot());return; }
    if(req.method!=='POST') { bad(res,404,'경로 없음');return; }
    const chunks=[];let length=0;
    for await(const chunk of req) { length+=chunk.length;if(length>3000000){bad(res,413,'파일 크기 초과');return;}chunks.push(chunk); }
    const raw=Buffer.concat(chunks);
    if(admin) {
      const settings=JSON.parse(raw.toString('utf8'));
      if(typeof settings.offline==='boolean') offline=settings.offline;
      if(typeof settings.rejectNextUpload==='boolean') rejectNextUpload=settings.rejectNextUpload;
      if(Number.isSafeInteger(settings.storageOverhead) && settings.storageOverhead>=0 && settings.storageOverhead<STORAGE_TOTAL) storageOverhead=settings.storageOverhead;
      reply(res,200,{ok:true,offline,rejectNextUpload,storageOverhead});return;
    }
    const form=new URLSearchParams(raw.toString('utf8'));
    calls.push({path:req.url,action:form.get('action'),value:form.get('value'),bytes:raw.length,authenticated:true});
    if(req.url==='/api/time') {
      const epoch=integer(form,'epoch',1735689600,4102444800),offset=integer(form,'offset',-840,840);
      if(epoch===null||offset===null){bad(res,400,'휴대폰 시간 정보를 확인해 주세요.');return;}
      state.clockAvailable=true;reply(res,200,{ok:true});return;
    }
    if(req.url==='/api/control') {
      const action=form.get('action'),value=form.get('value');
      switch(action) {
        case 'nap_start':if(!state.napRunning){state.napRunning=true;napStart=Date.now();}break;
        case 'nap_stop':if(state.napRunning){snapshot();state.totalSecs+=state.napElapsed;state.napRunning=false;state.napElapsed=0;}break;
        case 'noise_start':if(!state.storageReady){bad(res,409,'내장 저장소를 사용할 수 없습니다.');return;}if(!state.noisePlaying){state.noisePlaying=true;state.decoderRunning=true;noiseStart=Date.now();noiseEnd=Date.now()+state.timerMinutes*60000;state.soundMessage='Playing - stops automatically';}break;
        case 'noise_stop':stopNoise();break;
        case 'volume':{const n=integer(form,'value',0,5);if(n===null){bad(res,400,'음량 범위를 확인해 주세요.');return;}state.volume=n;break;}
        case 'timer':{const n=integer(form,'value',15,60);if(![15,30,60].includes(n)){bad(res,400,'타이머는 15/30/60분 중 선택해 주세요.');return;}state.timerMinutes=n;if(state.noisePlaying)noiseEnd=Date.now()+n*60000;break;}
        case 'track':if(value!=='white'&&value!=='lullaby'){bad(res,400,'음원을 선택해 주세요.');return;}if(value==='lullaby'&&!state.lullabyReady){bad(res,409,'자장가 파일을 사용할 수 없습니다.');return;}stopNoise();state.track=value;state.soundMessage=value==='lullaby'?'Lullaby ready':'White noise ready';break;
        case 'previous':case 'next':if(!state.photos.length){bad(res,409,'먼저 사진을 올려 주세요.');return;}state.photoIndex=(state.photoIndex+state.photos.length+(action==='next'?1:-1))%state.photos.length;state.autoplay=false;break;
        case 'slideshow':{const n=integer(form,'value',0,1);if(n===null){bad(res,400,'잘못된 앨범 설정입니다.');return;}if(!state.photos.length){bad(res,409,'먼저 사진을 올려 주세요.');return;}state.autoplay=Boolean(n);break;}
        default:bad(res,400,'알 수 없는 명령입니다.');return;
      }
      reply(res,200,snapshot());return;
    }
    if(req.url==='/api/photo'||req.url==='/api/noise') {
      const photo=req.url==='/api/photo';
      let file;
      try{file=filePart(req,raw);}catch(error){bad(res,400,error.message);return;}
      if(photo&&state.photos.length>=state.photoLimit){bad(res,400,'사진 수 제한입니다.');return;}
      stopNoise();state.autoplay=false;
      if(rejectNextUpload){rejectNextUpload=false;rejected.push({path:req.url,status:400,reason:'injected storage error'});bad(res,400,'저장소 테스트 오류');return;}
      const mp3Header=file.length>=3&&(file.subarray(0,3).equals(Buffer.from('ID3'))||(file[0]===255&&(file[1]&0xe0)===0xe0));
      const jpeg = file.length>=128 && file.length<=MAX_JPEG_BYTES && file[0]===255 && file[1]===216 && file.at(-2)===255 && file.at(-1)===217;
      if((photo&&!jpeg)||(!photo&&(file.length<128||file.length>MAX_MP3_BYTES||!mp3Header))){rejected.push({path:req.url,status:400,reason:'invalid file',bytes:file.length});bad(res,400,'파일 형식 또는 크기가 올바르지 않습니다.');return;}
      if(snapshot().storageTotal-state.storageUsed<allocation(file.length)+65536){bad(res,400,'기기 저장 공간이 부족합니다.');return;}
      const info=uploadInfo(req.url,file);
      let name='white_noise.mp3';
      if(photo){name=`p${String(nextPhoto++).padStart(8,'0')}.jpg`;await writeFile(path.join(fixtureDir,name),file);photoSizes.set(name,file.length);state.photos.push(name);state.photoIndex=Math.min(state.photoIndex,state.photos.length-1);}
      else{uploadedNoiseBytes=file.length;state.track='white';}
      uploads.push(info);
      state.soundMessage='Ready - tap PLAY';snapshot();reply(res,200,{ok:true,name});return;
    }
    if(req.url==='/api/delete') {
      const name=form.get('name');if(!name||!/^[A-Za-z0-9_-]+\.(rgb|jpg)$/.test(name)){bad(res,400,'잘못된 사진 이름입니다.');return;}
      const index=state.photos.indexOf(name);if(index<0){bad(res,404,'사진을 찾거나 지우지 못했습니다.');return;}
      const previous=state.photos[state.photoIndex];state.photos.splice(index,1);photoSizes.delete(name);state.photoIndex=Math.max(0,state.photos.indexOf(previous));state.autoplay=false;snapshot();reply(res,200,{ok:true});return;
    }
    bad(res,404,'경로 없음');
  }catch(error){if(!res.headersSent)bad(res,500,error.message);else res.destroy();}
});

// Synthetic PNGs, generated locally without a browser or image packages.
const crcTable=Array.from({length:256},(_,n)=>{let c=n;for(let i=0;i<8;i++)c=c&1?0xedb88320^(c>>>1):c>>>1;return c>>>0;});
function crc32(bytes){let c=0xffffffff;for(const b of bytes)c=crcTable[(c^b)&255]^(c>>>8);return(c^0xffffffff)>>>0;}
function pngChunk(type,data){const name=Buffer.from(type),size=Buffer.alloc(4),crc=Buffer.alloc(4);size.writeUInt32BE(data.length);crc.writeUInt32BE(crc32(Buffer.concat([name,data])));return Buffer.concat([size,name,data,crc]);}
function png(width,height,colorAt){const raw=Buffer.alloc(height*(1+width*4));for(let y=0;y<height;y++){const row=y*(1+width*4);for(let x=0;x<width;x++){const [r,g,b]=colorAt(x,y),i=row+1+x*4;raw[i]=r;raw[i+1]=g;raw[i+2]=b;raw[i+3]=255;}}const ihdr=Buffer.alloc(13);ihdr.writeUInt32BE(width);ihdr.writeUInt32BE(height,4);ihdr[8]=8;ihdr[9]=6;return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),pngChunk('IHDR',ihdr),pngChunk('IDAT',deflateSync(raw)),pngChunk('IEND',Buffer.alloc(0))]);}
await mkdir(fixtureDir,{recursive:true});
await Promise.all([
  writeFile(path.join(fixtureDir,'colors.png'),png(412,412,x=>x<137?[255,0,0]:x<275?[0,255,0]:[0,0,255])),
  writeFile(path.join(fixtureDir,'crop.png'),png(800,400,x=>x<200?[255,0,0]:x<600?[0,255,0]:[0,0,255])),
  writeFile(path.join(fixtureDir,'broken.jpg'),'not a decodable image'),
  copyFile(path.join(root,'audio/Lullaby_Alex_McCulloch_CC0.mp3'),path.join(fixtureDir,'lullaby.mp3'))
]);
server.listen(8769,'127.0.0.1',()=>console.log(`BabyGrip album fixture ready at http://127.0.0.1:8769 ; files ${fixtureDir}`));
server.on('error',error=>{console.error(error.message);process.exitCode=1;});
for(const signal of ['SIGINT','SIGTERM'])process.on(signal,()=>server.close(()=>process.exit(0)));
