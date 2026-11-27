#pragma once
#include <Arduino.h>

static const char BG_PHONE_HTML[] PROGMEM = R"BGHTML(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light">
<title>BabyGrip</title>
<style>
:root{font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:#29372f;background:#f6f4ed;line-height:1.5;font-synthesis:none;--green:#315f49;--muted:#5c675e;--line:#d9ded3;--paper:#fffefa;--soft:#e8eee4}
*{box-sizing:border-box}body{margin:0;background:#f6f4ed}main{max-width:760px;margin:auto;padding:26px 18px 48px}h1,h2,h3,p{margin:0}h1{font-family:Georgia,serif;font-weight:normal;font-size:2.65rem;line-height:1.15;letter-spacing:-.04em}h2{font-size:1.12rem;font-weight:650}h3{font-size:1rem;font-weight:650;margin-bottom:10px}header{margin-bottom:22px}.subtitle{color:var(--muted);margin-top:8px}.eyebrow{text-transform:uppercase;font-size:.69rem;letter-spacing:.16em;font-weight:700;color:var(--green);margin-bottom:8px}.connection{display:flex;align-items:center;gap:8px;margin:18px 0 8px;font-size:.85rem;color:var(--muted)}.dot{width:8px;height:8px;border-radius:50%;background:#a3792f;flex:none}.connected .dot{background:#3d774b}.connection strong{font-weight:550}.ssid{overflow-wrap:anywhere}.grid{display:grid;gap:16px}.card{background:var(--paper);border:1px solid var(--line);border-radius:20px;padding:20px}.card-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px}.tag{font-size:.72rem;border-radius:20px;background:var(--soft);padding:4px 9px;color:#375342;white-space:nowrap}.stats{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:18px}.stat-label,.helper{font-size:.81rem;color:var(--muted)}.stat-value{font-variant-numeric:tabular-nums;font-size:1.8rem;letter-spacing:-.04em;margin-top:3px}.helper{margin-top:9px}.row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}.spread{justify-content:space-between}.block{display:block}.field{margin-top:17px}.field>label{display:block;font-size:.87rem;font-weight:600;margin-bottom:8px}.button,button,select,input[type=file]{font:inherit}.button,button{border:1px solid transparent;min-height:46px;padding:10px 16px;border-radius:12px;background:var(--green);color:white;font-weight:600;font-size:.9rem;cursor:pointer;touch-action:manipulation}button.secondary,.button.secondary{background:#f0f2ea;color:#304c3b;border-color:var(--line)}button.small{padding:8px 12px;min-height:44px;font-size:.81rem}button.danger{background:transparent;border-color:#e0d2c9;color:#854331}button:disabled{opacity:.47;cursor:not-allowed}button.full{width:100%}button:focus-visible,input:focus-visible,select:focus-visible,summary:focus-visible{outline:3px solid #a36527;outline-offset:3px}select{width:100%;min-height:46px;background:white;border:1px solid #aab5a7;border-radius:10px;padding:8px 12px;color:inherit}input[type=range]{width:100%;min-height:44px;margin:0;accent-color:var(--green)}input[type=file]{display:block;width:100%;max-width:100%;padding:6px 0;font-size:.83rem;color:var(--muted)}input[type=file]::file-selector-button{font:inherit;padding:11px 13px;min-height:44px;margin-right:10px;border:1px solid #aab5a7;border-radius:10px;color:#304c3b;background:#f0f2ea;cursor:pointer}input[type=file]:disabled{opacity:.5}input.file-input{position:absolute;width:1px;height:1px;padding:0;margin:0;opacity:0;overflow:hidden;pointer-events:none}.pick{display:flex;align-items:center;justify-content:center;text-align:center;width:100%;font-size:1rem;min-height:52px}.file-input:disabled+.pick{opacity:.47;pointer-events:none}.file-input:focus-visible+.pick{outline:3px solid #a36527;outline-offset:3px}output{font-variant-numeric:tabular-nums;font-size:.85rem;color:var(--muted)}.album-buttons{display:grid;grid-template-columns:1fr 1.15fr 1fr;gap:9px}.album-count{text-align:center;font-variant-numeric:tabular-nums;font-size:1.5rem;letter-spacing:-.03em;margin-bottom:14px}.photo-list{list-style:none;padding:0;margin:10px 0 0;max-height:360px;overflow:auto}.photo-list li{display:flex;align-items:center;justify-content:space-between;gap:10px;border-top:1px solid var(--line);padding:6px 0;font-size:.83rem}.photo-list span{overflow-wrap:anywhere}.notice{font-size:.86rem;padding:12px 14px;background:#e9efe4;border:1px solid #cbd8c4;border-radius:12px;margin-bottom:16px;overflow-wrap:anywhere}.error{background:#fff0e9;border-color:#e5bda8;color:#7d321c}.offline{background:#fff7e7;border-color:#dfcda8;color:#72511c}[hidden]{display:none!important}.storage{margin-top:16px;padding-top:14px;border-top:1px solid var(--line)}meter{display:block;width:100%;height:12px;margin:8px 0;accent-color:var(--green)}details{margin-top:16px;border-top:1px solid var(--line);padding-top:12px}summary{cursor:pointer;min-height:44px;display:list-item;padding:9px 0;font-size:.86rem;font-weight:600}footer{margin-top:22px;font-size:.77rem;color:var(--muted)}.time-row{margin-top:14px}.volume-row{margin-top:4px}.volume-row input{flex:1;min-width:150px}.upload-action{margin-top:12px;width:100%}.no-photos{font-size:.85rem;color:var(--muted);text-align:center;padding:2px 0 10px}progress{width:100%;margin:4px 0;accent-color:var(--green)}.plan{margin-top:12px;font-size:.86rem;padding:11px 13px;border-radius:12px;background:#f3f1e6;border:1px solid var(--line);overflow-wrap:anywhere}.upload-panel{display:flex;gap:12px;align-items:center;margin-top:12px}.upload-panel img{width:64px;height:64px;border-radius:50%;object-fit:cover;background:var(--soft);flex:none}.upload-panel>div{flex:1;min-width:0}.upload-panel .helper{margin-top:2px}.delete-all{margin-top:12px}.feed-buttons{display:grid;grid-template-columns:1fr 1fr;gap:10px}.feed-buttons button{min-height:64px;font-size:1.1rem}.feed-buttons button.on{background:#a36527}.feed-extra{display:grid;gap:12px;margin-top:16px;padding-top:14px;border-top:1px solid var(--line)}.bottle-row label,.diaper-row span{font-size:.87rem;font-weight:600;min-width:64px}.bottle-row input,.text-input{font:inherit;min-height:46px;width:96px;border:1px solid #aab5a7;border-radius:10px;padding:8px 10px}.text-input{width:100%}.diaper-row{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.diaper-row button{flex:1;min-width:70px}.check{display:flex;align-items:center;gap:10px;min-height:44px;font-size:.9rem}.check input{width:22px;height:22px;accent-color:var(--green)}.time-pair{display:grid;grid-template-columns:1fr 1fr;gap:10px}.time-pair input{font:inherit;width:100%;min-height:46px;border:1px solid #aab5a7;border-radius:10px;padding:8px}.feed-list li .actions{display:flex;gap:6px;flex:none}
@media(min-width:620px){main{padding-top:38px}.grid{grid-template-columns:1fr 1fr}.wide{grid-column:1/-1}.card{padding:23px}.album-panel{display:grid;grid-template-columns:1fr 1fr;gap:28px}.album-main{padding-top:8px}.upload-box{border-right:1px solid var(--line);padding-right:24px}}
@media(max-width:619px){.album-main{margin-top:20px;padding-top:18px;border-top:1px solid var(--line)}}
@media(max-width:359px){main{padding:20px 12px 36px}.card{padding:16px}.stat-value{font-size:1.55rem}.album-buttons button{padding-left:8px;padding-right:8px}}
@media(prefers-reduced-motion:reduce){*{scroll-behavior:auto!important}}
</style>
</head>
<body>
<main>
<header>
  <p class="eyebrow">아기와 함께하는 일상</p>
  <h1>BabyGrip</h1>
  <p class="subtitle">수유 기록, 사진, 낮잠, 편안한 소리.</p>
  <div class="connection" id="connection" role="status" aria-live="polite"><span class="dot" aria-hidden="true"></span><strong id="connectionText">BabyGrip에 연결하는 중…</strong></div>
  <p class="helper ssid" id="networkName"></p>
</header>
<div class="notice offline" id="offlineNotice" hidden>BabyGrip Wi-Fi에 연결해 주세요. 휴대폰에 ‘인터넷 연결 없음’이 표시되어도 연결 유지를 선택하세요. <button class="small secondary" id="retryButton" type="button">다시 연결</button></div>
<p class="notice error" id="errorNotice" role="alert" hidden></p>
<p class="notice" id="notice" role="status" aria-live="polite" hidden></p>
<div class="grid" id="controls" aria-busy="true">
  <section class="card wide" aria-labelledby="feedHeading">
    <div class="card-head"><h2 id="feedHeading">수유·기저귀 기록</h2><span class="tag" id="feedTag">기록 대기</span></div>
    <div class="stats"><div><p class="stat-label" id="feedAgoLabel">마지막 수유 시작부터</p><p class="stat-value" id="feedAgo">--</p><p class="helper" id="feedLastInfo"></p></div><div><p class="stat-label" id="feedTodayLabel">오늘</p><p class="stat-value" id="feedToday">0회</p><p class="helper" id="feedTodayInfo"></p></div></div>
    <div class="feed-buttons"><button id="feedLeft" type="button" data-control disabled>왼쪽</button><button id="feedRight" type="button" data-control disabled>오른쪽</button></div>
    <p class="helper">왼쪽·오른쪽을 누르면 시간이 시작돼요. 반대쪽을 누르면 쪽이 바뀌고, 같은 쪽을 다시 누르면 끝나요. 한쪽이 60분을 넘으면 자동으로 멈춰요. BabyGrip 화면의 FEED에서도 똑같이 할 수 있어요.</p>
    <div class="feed-extra">
      <div class="row bottle-row"><label for="bottleMl">분유·젖병</label><input id="bottleMl" type="number" inputmode="numeric" min="0" max="400" step="10" placeholder="ml" data-control disabled><button class="secondary" id="bottleButton" type="button" data-control disabled>기록</button></div>
      <div class="diaper-row"><span>기저귀</span><button class="secondary" type="button" data-diaper="wet" data-control disabled>소변</button><button class="secondary" type="button" data-diaper="dirty" data-control disabled>대변</button><button class="secondary" type="button" data-diaper="both" data-control disabled>둘 다</button></div>
    </div>
    <details id="feedHistory"><summary id="feedSummary">기록 목록</summary><ul class="photo-list feed-list" id="feedList" aria-label="수유·기저귀 기록"></ul><button class="secondary small delete-all" id="feedExport" type="button" data-control disabled>CSV 파일로 저장</button></details>
  </section>
  <section class="card wide" aria-labelledby="albumHeading">
    <div class="card-head"><h2 id="albumHeading">소중한 순간들</h2><span class="tag" id="albumTag">사진 앨범</span></div>
    <div class="album-panel">
      <div class="upload-box">
        <h3>아이폰 사진 넣기</h3>
        <input id="photos" class="file-input" type="file" accept="image/*" multiple data-control disabled aria-describedby="photoHelp">
        <label class="button pick" for="photos">사진 보관함에서 여러 장 선택</label>
        <p class="helper" id="photoHelp">선택 화면에서 사진 위를 손가락으로 쓸면 여러 장이 한 번에 선택돼요. 몇백 장도 한 번에 올릴 수 있어요.</p>
        <details id="folderPicker" hidden><summary>앨범을 통째로 넣기 (폴더)</summary><p class="helper" id="folderHelp">iOS 18.4 이상: 사진 앱에서 앨범 → 선택 → 모두 선택 → 공유 → ‘파일에 저장’으로 한 폴더에 담은 뒤, 아래에서 그 폴더를 고르세요. iCloud에만 있는 사진은 집 Wi-Fi에서 먼저 내려받아 두세요.</p><input id="photoFolder" class="file-input" type="file" webkitdirectory multiple data-control disabled aria-describedby="folderHelp"><label class="button secondary pick" for="photoFolder">파일 앱에서 폴더 선택</label></details>
        <p class="plan" id="planText" role="status" aria-live="polite" hidden></p>
        <button class="upload-action" id="photoUploadButton" type="button" data-control disabled>사진 업로드</button>
        <div class="upload-panel" id="uploadPanel" hidden><img id="uploadPreview" alt=""><div><progress id="uploadProgress" value="0" max="1" aria-label="사진 업로드 진행 상황"></progress><p class="helper" id="uploadStatus" role="status" aria-live="polite"></p></div></div>
        <p class="helper">사진은 412×412 크기로 줄여 BabyGrip에 저장해요. 아이폰 원본은 바뀌지 않아요. 올리는 동안 이 화면을 켜 두세요. 소리와 앨범 재생은 그대로 이어집니다.</p>
      </div>
      <div class="album-main">
        <p class="album-count" id="photoCount">0 / 0</p>
        <p class="no-photos" id="emptyAlbum">좋아하는 아기 사진을 액자에 추가해 보세요.</p>
        <div class="album-buttons"><button class="secondary" id="previousButton" type="button" data-control disabled>이전</button><button id="slideshowButton" type="button" data-control disabled>앨범 재생</button><button class="secondary" id="nextButton" type="button" data-control disabled>다음</button></div>
        <label class="check"><input id="slideAuto" type="checkbox" data-control disabled>사진 보기를 기본 화면으로 (2분 동안 안 만지면 글자 없이 사진만 넘기기)</label><div class="field"><label for="slideSeconds">넘기는 간격</label><select id="slideSeconds" data-control disabled><option value="5">5초</option><option value="10">10초</option><option value="30">30초</option><option value="60">1분</option><option value="300">5분</option></select></div><p class="helper" id="slideHelp">찍은 날짜 순서로 5초마다 사진이 바뀝니다. 사진만 나올 때 화면을 톡 치면 홈으로 돌아와요. 밤에는 사진을 어둡고 따뜻한 색으로 보여 줘요.</p>
        <div class="storage"><p class="stat-label" id="storageText">내부 저장 공간 확인 중…</p><meter id="storageMeter" min="0" max="1" value="0" aria-label="내부 저장 공간 사용량"></meter><p class="helper" id="roomText">사진은 이 BabyGrip 안에 저장됩니다. microSD 카드나 클라우드 계정이 필요하지 않아요.</p></div>
        <details id="savedPhotos"><summary id="savedSummary">저장된 사진 목록</summary><ul class="photo-list" id="photoList" aria-label="BabyGrip에 저장된 사진"></ul><button class="danger small delete-all" id="deleteAllButton" type="button" data-control disabled>사진 모두 지우기</button></details>
      </div>
    </div>
  </section>
  <section class="card" aria-labelledby="napHeading">
    <div class="card-head"><h2 id="napHeading">낮잠 기록</h2><span class="tag" id="napTag">기록 대기</span></div>
    <div class="stats"><div><p class="stat-label">이번 낮잠</p><p class="stat-value" id="napElapsed">00:00:00</p></div><div><p class="stat-label" id="totalTitle">전원을 켠 뒤 누적</p><p class="stat-value" id="napTotal">00:00:00</p></div></div>
    <button class="full" id="napButton" type="button" data-control disabled>낮잠 시작</button>
    <p class="helper">페이지를 닫아도 기기에서 시간을 계속 기록합니다.</p>
  </section>
  <section class="card" aria-labelledby="noiseHeading">
    <div class="card-head"><h2 id="noiseHeading">소리 재생</h2><span class="tag" id="noiseTag">정지</span></div>
    <div class="field"><label for="track">음원 선택</label><select id="track" data-control disabled><option value="white">백색소음</option><option value="lullaby">자장가</option></select></div>
    <p class="helper" id="noiseStatus">연결하면 스피커 상태를 확인할 수 있습니다.</p>
    <p class="helper" id="noiseRemaining"></p>
    <div class="field"><button class="full" id="noiseButton" type="button" data-control disabled>소리 재생</button></div>
    <div class="field"><label for="volume">스피커 음량 <output id="volumeValue" for="volume">0 / 5</output></label><div class="row volume-row"><input id="volume" type="range" min="0" max="5" step="1" value="0" data-control disabled></div></div>
    <div class="field"><label for="timer">자동 정지 타이머</label><select id="timer" data-control disabled><option value="15">15분</option><option value="30" selected>30분</option><option value="60">60분</option></select></div>
    <p class="helper">가장 낮은 음량부터 시작하고, BabyGrip은 아기와 멀리(아기 침대 안이나 위가 아닌 곳) 두세요. 소리는 타이머로 짧게 쓰는 것이 좋아요.</p>
    <details><summary>내 MP3 추가하기</summary><label class="block helper" for="noiseFile">최대 2 MB의 MP3를 고르면 새 음원으로 추가돼요. 같은 이름이면 바뀝니다.</label><input id="noiseFile" type="file" accept="audio/mpeg,.mp3" data-control disabled><div class="field"><label for="noiseName">음원 이름 (영문 소문자·숫자·-·_ 20자)</label><input class="text-input" id="noiseName" type="text" maxlength="20" autocapitalize="off" autocomplete="off" spellcheck="false" data-control disabled></div><button class="secondary upload-action" id="noiseUploadButton" type="button" data-control disabled>MP3 추가</button><button class="danger upload-action" id="noiseDeleteButton" type="button" data-control disabled>선택한 음원 지우기</button><button class="secondary upload-action" id="noiseDefaultButton" type="button" data-control disabled hidden>예전에 올린 백색소음 지우고 기본으로</button></details>
  </section>
  <section class="card" aria-labelledby="nightHeading">
    <div class="card-head"><h2 id="nightHeading">밤 모드·화면</h2><span class="tag" id="nightTag">낮</span></div>
    <button class="full" id="nightButton" type="button" data-control disabled>밤 모드 켜기</button>
    <p class="helper">밤 모드는 화면을 아주 어둡게 하고 따뜻한 주황색만 써요. 사진 대신 마지막 수유부터 지난 시간을 크게 보여 줘요.</p>
    <label class="check"><input id="nightAuto" type="checkbox" data-control disabled>정해진 시간에 자동으로 켜고 끄기</label>
    <div class="time-pair"><label class="block helper">켜기<input id="nightStart" type="time" value="20:00" data-control disabled></label><label class="block helper">끄기<input id="nightEnd" type="time" value="07:00" data-control disabled></label></div>
    <button class="secondary upload-action" id="nightSave" type="button" data-control disabled>시간 저장</button>
    <div class="field"><label for="dayLight">낮 밝기 <output id="dayLightValue">50</output></label><input id="dayLight" type="range" min="10" max="100" step="5" value="50" data-control disabled></div>
    <div class="field"><label for="nightLight">밤 밝기 <output id="nightLightValue">6</output></label><input id="nightLight" type="range" min="1" max="40" step="1" value="6" data-control disabled></div>
    <label class="check"><input id="glanceAuto" type="checkbox" data-control disabled>휴대폰을 엎어 두면 시계·마지막 수유 화면 보기</label>
    <p class="helper">엎어 둔 채 1분이 지나면 화면이 꺼지고, 움직이거나 만지면 다시 켜져요. 이 화면이 바닥을 보면 5초 뒤 꺼져 배터리를 아껴요.</p>
    <button class="secondary upload-action" id="calibrateButton" type="button" data-control disabled>엎어 둔 방향 맞추기</button>
    <p class="helper" id="calibrateHelp">엎어 두었는데 시계 화면이 안 나오고 화면이 꺼지면 한 번 맞춰 주세요. 버튼을 누르고 5초 안에 휴대폰을 탁자에 엎어 두세요.</p>
    <button class="secondary upload-action" id="lookButton" type="button" data-control disabled>‘여기 봐!’ 화면 켜기</button>
    <p class="helper">사진 찍을 때 BabyGrip 쪽이 아기를 향해요. 홈 화면의 LOOK을 누르면 흑백 무늬와 짧은 소리로 아기의 시선을 끌어요. 화면을 톡 치면 무늬가 바뀌고, 길게 누르면 끝나요.</p>
    <div class="field"><label for="crySensitivity">울음 감지 (BabyGrip 마이크)</label><select id="crySensitivity" data-control disabled><option value="0">끄기</option><option value="1">켜기 · 둔감</option><option value="2">켜기 · 보통</option><option value="3">켜기 · 민감</option></select></div>
    <div class="field"><label for="cryTrack">울음이 들리면 틀 소리</label><select id="cryTrack" data-control disabled></select></div>
    <p class="helper" id="cryHelp">아기가 5초 넘게 크게 울면 고른 소리를 지금 음량과 타이머로 틀고, 기록 목록에 ‘울음 감지’로 남겨요. 소리를 트는 동안과 끝난 뒤 20초는 듣지 않아요. 소리는 기기 안에서만 판단하고, 녹음하거나 어디로 보내지 않아요. TV나 대화 소리가 크면 잘못 켜질 수 있으니 그럴 땐 ‘둔감’으로 바꾸세요. 기기의 SET → NIGHT &amp; CRY에서도 바꿀 수 있어요.</p>
  </section>
  <section class="card wide" aria-labelledby="clockHeading">
    <div class="row spread"><div><h2 id="clockHeading">시간 및 연결</h2><p class="helper" id="clockStatus">기기 시간 확인 중입니다.</p></div><button class="secondary small time-row" id="syncButton" type="button" data-control disabled>휴대폰 시간으로 맞추기</button></div>
    <p class="helper">휴대폰의 날짜와 현지 시간을 기기에 전달합니다. 연결 후 잠시 페이지를 열어 두세요.</p>
  </section>
</div>
<footer>휴대폰과 BabyGrip을 직접 연결합니다. 인터넷은 필요하지 않습니다.</footer>
</main>
<script>
(() => {
'use strict';
const TOKEN = '__BG_TOKEN__';
const MAX_PHOTO_BYTES = 196608;
const MAX_MP3_BYTES = 2000000;
const DEFAULT_PHOTO_BYTES = 36864;  // planning guess until real photos are measured
const NAME_PATTERN = /^p[0-9a-f]{16}\.jpg$/;
const SOUND_NAME = /^[a-z0-9_-]{1,20}$/;
let feedRev = null;
let feedItems = [];
let feedNow = 0;
let trackKey = '';
let editingLight = false;
const $ = id => document.getElementById(id);
let state = null;
let connected = false;
let busy = true;
let pollInFlight = false;
let revision = 0;
let editingVolume = false;
let devicePhotos = [];
let deviceSet = new Set();
let photosRev = null;
let photoListKey = '';
let plan = null;
let planning = false;
let planToken = 0;
let measuredPhotoBytes = 0;

class DeviceError extends Error {
  constructor(message, status = 0, code = '') { super(message); this.name = 'DeviceError'; this.status = status; this.code = code; }
}
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
function duration(seconds) {
  const value = Math.max(0, Math.floor(Number(seconds) || 0));
  return [Math.floor(value / 3600), Math.floor(value / 60) % 60, value % 60].map(n => String(n).padStart(2, '0')).join(':');
}
function bytes(value) { return ((Number(value) || 0) / 1000000).toFixed(1) + ' MB'; }
function count(value) { return Number(value || 0).toLocaleString('ko-KR'); }
function waitText(seconds) {
  const s = Math.max(1, Math.round(seconds));
  if (s < 60) return s + '초';
  const m = Math.floor(s / 60), r = s % 60;
  return m + '분' + (r && m < 10 ? ' ' + r + '초' : '');
}
function allocation(size) { return Math.ceil(size / 4096) * 4096; }
function perPhotoBytes() { return (measuredPhotoBytes || DEFAULT_PHOTO_BYTES) * 1.1 + 160; }
function maxBytes() { return Math.min(MAX_PHOTO_BYTES, Number(state && state.photoMaxBytes) || MAX_PHOTO_BYTES); }
function speakerMessage(message) {
  const messages = {
    'Ready to connect':'연결 준비가 되었습니다.',
    'Tap PLAY to start':'소리 재생을 누르면 시작합니다.',
    'Playing - stops automatically':'재생 중입니다. 타이머가 끝나면 자동으로 정지합니다.',
    'Stopped':'정지 상태입니다.',
    'Lullaby ready':'자장가를 선택했습니다. 소리 재생을 눌러 주세요.',
    'White noise ready':'백색소음을 선택했습니다.',
    'Sound selected':'음원을 선택했습니다. 소리 재생을 눌러 주세요.',
    'Ready - tap PLAY':'준비되었습니다. 소리 재생을 눌러 주세요.',
    'Audio decoder could not start':'음원 재생을 시작하지 못했습니다. MP3 파일을 확인해 주세요.',
    'Audio decoder could not restart':'음원 재생이 멈췄습니다. 다시 재생을 눌러 주세요.',
    'Configure Wi-Fi / Home Assistant':'Wi-Fi와 Home Assistant 설정을 확인해 주세요.',
    'Home Assistant URL error':'Home Assistant 주소를 확인해 주세요.'
  };
  if (!message) return '재생 준비가 되었습니다.';
  if (Object.prototype.hasOwnProperty.call(messages, message)) return messages[message];
  if (/^HA error \d+$/.test(message)) return 'Home Assistant 오류: ' + message.slice(9);
  return '기기 상태: ' + message;
}
function notice(message) { $('notice').textContent = message || ''; $('notice').hidden = !message; }
function showError(error) { $('errorNotice').textContent = error.message || String(error); $('errorNotice').hidden = false; }
function clearError() { $('errorNotice').hidden = true; $('errorNotice').textContent = ''; }
function showPlan(message) { $('planText').textContent = message || ''; $('planText').hidden = !message; }
function setConnection(value) {
  const reconnected = value && !connected;
  connected = value;
  if (reconnected) clearError();
  $('connection').classList.toggle('connected', value);
  const message = value ? 'BabyGrip에 연결됨' : (state ? '연결이 끊겼습니다 · 마지막 확인 상태 표시 중' : 'BabyGrip 연결을 기다리는 중');
  if ($('connectionText').textContent !== message) $('connectionText').textContent = message;
  $('offlineNotice').hidden = value;
  updateDisabled();
}
async function request(path, form, timeout = 8000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeout);
  try {
    const options = {cache:'no-store', signal:controller.signal, redirect:'error'};
    if (form !== undefined) {
      options.method = 'POST';
      options.headers = {'X-BabyGrip-Token':TOKEN};
      options.body = form instanceof FormData ? form : new URLSearchParams(form);
    }
    const response = await fetch(path, options);
    let body;
    try { body = await response.json(); } catch (_) { throw new DeviceError('기기 응답을 읽을 수 없습니다. 다시 연결하고 페이지를 새로고침해 주세요.', response.status, 'reply'); }
    if (!response.ok || body.error) {
      const code = body.code || (response.status === 403 ? 'auth' : 'error');
      const message = code === 'auth' ? '연결 세션이 바뀌었습니다. 페이지를 새로고침한 뒤 다시 시도해 주세요.' : (body.error ? '기기 응답: ' + body.error : '기기가 요청을 완료하지 못했습니다.');
      throw new DeviceError(message, response.status, code);
    }
    return body;
  } catch (error) {
    if (error.name === 'AbortError') throw new DeviceError('기기 응답 시간이 초과되었습니다. BabyGrip Wi-Fi 연결을 확인해 주세요.', 0, 'timeout');
    if (error instanceof DeviceError) throw error;
    throw new DeviceError('BabyGrip에 연결할 수 없습니다. BabyGrip Wi-Fi에 연결한 상태에서 다시 시도해 주세요.', 0, 'network');
  } finally { clearTimeout(timer); }
}
function updateDisabled() {
  const unavailable = busy || !connected;
  $('controls').setAttribute('aria-busy', String(busy));
  document.querySelectorAll('[data-control]').forEach(el => { el.disabled = unavailable; });
  if (unavailable) return;
  const storage = Boolean(state && state.storageReady);
  const soundStorage = Boolean(state && state.soundStorageReady);
  ['photos','photoFolder'].forEach(id => { $(id).disabled = !storage; });
  $('photoUploadButton').disabled = !storage || planning || !plan || !plan.chosen.length;
  $('noiseFile').disabled = !soundStorage;
  $('noiseName').disabled = !soundStorage;
  $('noiseUploadButton').disabled = !soundStorage || !$('noiseFile').files.length || !SOUND_NAME.test($('noiseName').value);
  $('noiseDefaultButton').disabled = !(state && state.customNoise);
  $('noiseDefaultButton').hidden = !(state && state.customNoise);
  $('noiseDeleteButton').disabled = !selectedUserTrack();
  ['nightStart','nightEnd'].forEach(id => { $(id).disabled = !$('nightAuto').checked; });
  ['previousButton','nextButton'].forEach(id => { $(id).disabled = !state || state.photoCount < 1; });
  $('slideshowButton').disabled = !state || (state.photoCount < 2 && !state.autoplay);
  $('deleteAllButton').disabled = !state || state.photoCount < 1;
}
function photoLabel(name) {
  if (!NAME_PATTERN.test(name)) return '이전 버전에서 올린 사진';
  const seconds = parseInt(name.slice(1, 9), 16);
  return new Date(seconds * 1000).toLocaleString('ko-KR', {timeZone:'UTC', year:'numeric', month:'long', day:'numeric', hour:'numeric', minute:'2-digit'});
}
function renderPhotos() {
  const key = photosRev + ':' + devicePhotos.length;
  $('savedSummary').textContent = '저장된 사진 목록 (' + count(devicePhotos.length) + '장)';
  if (key === photoListKey) return;
  photoListKey = key;
  const rows = devicePhotos.slice().reverse().map(name => {
    const row = document.createElement('li');
    const title = document.createElement('span'); title.textContent = photoLabel(name);
    const remove = document.createElement('button');
    remove.type = 'button'; remove.className = 'danger small'; remove.textContent = '삭제';
    remove.setAttribute('aria-label', photoLabel(name) + ' 사진 삭제'); remove.setAttribute('data-control', ''); remove.setAttribute('data-photo-delete', '');
    remove.disabled = busy || !connected;
    remove.addEventListener('click', () => operation(async () => { await request('/api/delete', {name}); await refreshPhotoList(true); return '사진을 삭제했습니다.'; }));
    row.append(title, remove); return row;
  });
  $('photoList').replaceChildren(...rows);
}
// ---- feeding log -----------------------------------------------------------
const SOUND_TITLES = {'WHITE NOISE':'백색소음','LULLABY':'자장가 (Alex McCulloch)','RAIN':'빗소리','OCEAN':'파도 소리','STREAM':'시냇물',
  'PINK NOISE':'핑크 노이즈','BROWN NOISE':'브라운 노이즈','FAN':'선풍기','HEARTBEAT':'엄마 배 속 심장 소리','SHUSH':'쉬- 소리'};
function selectedUserTrack() {
  if (!state || !Array.isArray(state.tracks)) return null;
  return state.tracks.find(t => t.key === $('track').value && t.user) || null;
}
function agoText(seconds) {
  if (typeof seconds !== 'number' || seconds < 0) return '--';
  const m = Math.floor(seconds / 60);
  if (m < 1) return '방금';
  if (m < 60) return m + '분 전';
  return Math.floor(m / 60) + '시간 ' + (m % 60) + '분 전';
}
function minutes(seconds) { return Math.round((Number(seconds) || 0) / 60) + '분'; }
function kindText(item) {
  switch (item.kind) {
    case 1: return '모유 · 왼쪽 ' + minutes(item.left) + ' · 오른쪽 ' + minutes(item.right) + ((item.flags & 1) ? ' (진행 중)' : '') + ((item.flags & 4) ? ' (60분 자동 종료)' : '');
    case 2: return '분유·젖병' + (item.ml ? ' ' + item.ml + ' ml' : ' (양 미입력)');
    case 3: return '기저귀 · 소변';
    case 4: return '기저귀 · 대변';
    case 5: return '기저귀 · 소변+대변';
    case 6: return '기저귀';
    case 7: return '울음 감지 → 소리 자동 재생';
    default: return '기록';
  }
}
function itemTime(item) {
  if (item.ago < 0 || !feedNow) return item.ago < 0 ? '시간 모름' : agoText(item.ago);
  return new Date((feedNow - item.ago) * 1000).toLocaleString('ko-KR', {month:'numeric', day:'numeric', hour:'numeric', minute:'2-digit'});
}
function renderFeed() {
  const f = state.feed;
  if (!f) return;
  const running = Boolean(f.running);
  $('feedTag').textContent = running ? '수유 중' : (f.saved ? '기록 대기' : '저장 공간 없음 · 전원을 끄면 사라져요');
  if (running) {
    $('feedAgoLabel').textContent = '지금 수유 중 · ' + (f.side === 'right' ? '오른쪽' : '왼쪽');
    $('feedAgo').textContent = duration((f.left || 0) + (f.right || 0)).replace(/^00:/, '');
    $('feedLastInfo').textContent = '왼쪽 ' + minutes(f.left) + ' · 오른쪽 ' + minutes(f.right);
  } else {
    $('feedAgoLabel').textContent = '마지막 수유 시작부터';
    $('feedAgo').textContent = typeof f.lastAgo === 'number' ? agoText(f.lastAgo) : '기록 없음';
    $('feedLastInfo').textContent = (f.lastKind === 2 ? '마지막: 분유·젖병' : f.lastSide ? '마지막 쪽: ' + (f.lastSide === 'right' ? '오른쪽' : '왼쪽') : '') +
      (typeof f.diaperAgo === 'number' && f.diaperAgo >= 0 ? ' · 기저귀 ' + agoText(f.diaperAgo) : '');
  }
  const t = f.today || {};
  $('feedTodayLabel').textContent = state.clockAvailable ? '오늘' : '전원을 켠 뒤';
  $('feedToday').textContent = '수유 ' + (t.feeds || 0) + '회';
  $('feedTodayInfo').textContent = '모유 ' + (t.breast || 0) + ' · 젖병 ' + (t.bottles || 0) + (t.ml ? ' (' + t.ml + ' ml)' : '') +
    ' · 기저귀 ' + (t.diapers || 0) + ' (소변 ' + (t.wet || 0) + ' · 대변 ' + (t.dirty || 0) + ')' + (t.cries ? ' · 울음 감지 ' + t.cries + '회' : '');
  const left = $('feedLeft'), right = $('feedRight');
  left.textContent = running ? (f.side === 'left' ? '왼쪽 끝내기' : '왼쪽으로 바꾸기') : '왼쪽 시작';
  right.textContent = running ? (f.side === 'right' ? '오른쪽 끝내기' : '오른쪽으로 바꾸기') : '오른쪽 시작';
  left.classList.toggle('on', running && f.side === 'left');
  right.classList.toggle('on', running && f.side === 'right');
  $('feedSummary').textContent = '기록 목록 (' + count(f.count || 0) + '개)';
}
function renderFeedList() {
  const rows = feedItems.map(item => {
    const row = document.createElement('li');
    const title = document.createElement('span'); title.textContent = itemTime(item) + ' · ' + kindText(item);
    const actions = document.createElement('span'); actions.className = 'actions';
    if (item.kind === 2) {
      const edit = document.createElement('button');
      edit.type = 'button'; edit.className = 'secondary small'; edit.textContent = '양'; edit.setAttribute('data-control', '');
      edit.setAttribute('aria-label', itemTime(item) + ' 분유 양 입력');
      edit.disabled = busy || !connected;
      edit.addEventListener('click', () => {
        const value = window.prompt('분유·젖병 양을 ml로 입력하세요 (0~400)', item.ml ? String(item.ml) : '');
        if (value === null) return;
        const ml = parseInt(value, 10);
        if (!(ml >= 0 && ml <= 400)) { showError(new DeviceError('0에서 400 사이의 숫자를 입력해 주세요.')); return; }
        operation(async () => { await request('/api/feed', {action:'update', id:String(item.id), ml:String(ml)}); await refreshFeedList(true); return '분유 양을 저장했습니다.'; });
      });
      actions.append(edit);
    }
    const remove = document.createElement('button');
    remove.type = 'button'; remove.className = 'danger small'; remove.textContent = '삭제'; remove.setAttribute('data-control', '');
    remove.setAttribute('aria-label', itemTime(item) + ' ' + kindText(item) + ' 기록 삭제');
    remove.disabled = busy || !connected;
    remove.addEventListener('click', () => {
      if (!window.confirm(itemTime(item) + ' · ' + kindText(item) + ' 기록을 지울까요?')) return;
      operation(async () => { await request('/api/feed', {action:'delete', id:String(item.id)}); await refreshFeedList(true); return '기록을 지웠습니다.'; });
    });
    actions.append(remove);
    row.append(title, actions);
    return row;
  });
  if (!rows.length) { const empty = document.createElement('li'); empty.textContent = '아직 기록이 없어요.'; rows.push(empty); }
  $('feedList').replaceChildren(...rows);
}
async function refreshFeedList(force) {
  if (!state || !state.feed) return;
  if (!force && feedRev === state.feed.rev && !(state.feed.running && $('feedHistory').open)) return;
  const body = await request('/api/feeds', undefined, 15000);
  feedItems = Array.isArray(body.items) ? body.items.filter(x => x && typeof x.id === 'number') : [];
  feedNow = body.clock ? Number(body.now) || 0 : 0;
  feedRev = body.rev;
  renderFeedList();
}
function exportFeedCsv() {
  const lines = ['시각,종류,왼쪽(분),오른쪽(분),분유(ml)'];
  const names = {1:'모유',2:'분유·젖병',3:'기저귀 소변',4:'기저귀 대변',5:'기저귀 소변+대변',6:'기저귀',7:'울음 감지'};
  feedItems.slice().reverse().forEach(item => {
    const when = item.ago < 0 || !feedNow ? '시간 모름' : new Date((feedNow - item.ago) * 1000).toLocaleString('ko-KR');
    lines.push(['"' + when + '"', names[item.kind] || '', item.kind === 1 ? Math.round(item.left / 60) : '', item.kind === 1 ? Math.round(item.right / 60) : '', item.kind === 2 ? item.ml : ''].join(','));
  });
  const blob = new Blob(['﻿' + lines.join('\n')], {type:'text/csv;charset=utf-8'});
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = 'babygrip-feeding-' + new Date().toISOString().slice(0, 10) + '.csv';
  document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 10000);
}
function renderTracks() {
  const list = Array.isArray(state.tracks) ? state.tracks : [];
  const key = list.map(t => t.key).join('|');
  if (key !== trackKey) {
    trackKey = key;
    $('track').replaceChildren(...list.map(t => {
      const option = document.createElement('option');
      option.value = t.key;
      option.textContent = SOUND_TITLES[t.title] || (t.user ? '내 음원: ' + t.title.toLowerCase() : t.title);
      if (t.key === 'white' && state.customNoise) option.textContent = '예전에 올린 MP3 (백색소음 자리)';
      return option;
    }));
  }
  if (document.activeElement !== $('track')) $('track').value = state.track || 'white';
}
function renderNight() {
  $('nightTag').textContent = state.night ? '밤 모드' : '낮';
  $('nightButton').textContent = state.night ? '밤 모드 끄기' : '밤 모드 켜기';
  if (document.activeElement !== $('nightAuto')) $('nightAuto').checked = Boolean(state.nightAuto);
  if (document.activeElement !== $('nightStart')) $('nightStart').value = state.nightStart || '20:00';
  if (document.activeElement !== $('nightEnd')) $('nightEnd').value = state.nightEnd || '07:00';
  if (!editingLight) {
    $('dayLight').value = String(state.dayLight || 50); $('dayLightValue').textContent = String(state.dayLight || 50);
    $('nightLight').value = String(state.nightLight || 6); $('nightLightValue').textContent = String(state.nightLight || 6);
  }
  if (document.activeElement !== $('glanceAuto')) $('glanceAuto').checked = Boolean(state.glanceAuto);
  if (document.activeElement !== $('slideAuto')) $('slideAuto').checked = state.slideAuto !== false;
  if (document.activeElement !== $('crySensitivity')) $('crySensitivity').value = String(state.cry || 0);
  const tracks = Array.isArray(state.tracks) ? state.tracks : [];
  const cryKeys = tracks.map(x => x.key).join('|');
  if ($('cryTrack').dataset.keys !== cryKeys) {
    $('cryTrack').dataset.keys = cryKeys;
    $('cryTrack').replaceChildren(...tracks.map(x => new Option(SOUND_TITLES[x.title] || (x.user ? '내 음원: ' + x.title.toLowerCase() : x.title), x.key)));
  }
  if (document.activeElement !== $('cryTrack')) $('cryTrack').value = tracks.some(x => x.key === state.cryTrack) ? state.cryTrack : 'white';
  if (state.slideSeconds && document.activeElement !== $('slideSeconds')) {
    const s = String(state.slideSeconds);
    if (![...$('slideSeconds').options].some((o) => o.value === s)) $('slideSeconds').add(new Option(s + '초', s));
    $('slideSeconds').value = s;
    $('slideHelp').textContent = '찍은 날짜 순서로 ' + secondsText(state.slideSeconds) + '마다 사진이 바뀝니다. 사진만 나올 때 화면을 톡 치면 홈으로 돌아와요. 밤에는 사진을 어둡고 따뜻한 색으로 보여 줘요.';
  }
}
function render(next) {
  state = next;
  $('napTag').textContent = state.napRunning ? '낮잠 중' : '기록 대기';
  $('napElapsed').textContent = duration(state.napElapsed);
  $('napTotal').textContent = duration(state.totalSecs);
  $('totalTitle').textContent = state.clockAvailable ? '오늘 누적' : '전원을 켠 뒤 누적';
  $('napButton').textContent = state.napRunning ? '낮잠 종료' : '낮잠 시작';
  $('noiseTag').textContent = state.noisePlaying ? '재생 중' : '정지';
  $('noiseStatus').textContent = speakerMessage(state.soundMessage);
  $('noiseRemaining').textContent = state.noisePlaying ? duration(state.noiseRemaining) + ' 후 자동 정지' : '';
  $('noiseButton').textContent = state.noisePlaying ? '소리 정지' : '소리 재생';
  $('volume').max = String(state.volumeCap);
  if (!editingVolume) { $('volume').value = String(state.volume); $('volumeValue').textContent = state.volume + ' / ' + state.volumeCap; }
  if (document.activeElement !== $('timer')) $('timer').value = String(state.timerMinutes);
  renderTracks();
  renderFeed();
  renderNight();
  $('photoCount').textContent = state.photoCount ? (state.photoIndex + 1) + ' / ' + count(state.photoCount) : '0 / 0';
  $('albumTag').textContent = '사진 ' + count(state.photoCount) + '장';
  $('emptyAlbum').hidden = state.photoCount > 0;
  $('slideshowButton').textContent = state.autoplay ? '앨범 정지' : '앨범 재생';
  if (state.storageReady) {
    const room = Math.floor((Number(state.storageFree) || 0) / perPhotoBytes());
    $('storageText').textContent = '사진 공간 ' + bytes(state.storageUsed) + ' / ' + bytes(state.storageTotal) + ' 사용 중';
    $('roomText').textContent = room > 0 ? '약 ' + count(room) + '장 더 넣을 수 있어요 (사진마다 크기가 달라 대략적인 수입니다). microSD 카드는 필요 없어요.' : '저장 공간이 거의 찼습니다. 오래된 사진을 지우면 새 사진을 넣을 수 있어요.';
  } else {
    $('storageText').textContent = '내부 저장 공간을 사용할 수 없습니다. 낮잠 기록과 소리는 사용할 수 있습니다.';
  }
  $('storageMeter').value = state.storageTotal ? Math.min(1, state.storageUsed / state.storageTotal) : 0;
  $('storageMeter').hidden = !state.storageReady;
  $('clockStatus').textContent = state.clockAvailable ? '기기 시간이 설정되어 오늘의 낮잠 합계를 표시합니다.' : '날짜별 합계를 보려면 휴대폰 시간으로 맞춰 주세요.';
  $('networkName').textContent = state.ssid ? 'Wi-Fi: ' + state.ssid : '';
  setConnection(true);
}
async function refreshState() {
  const startedRevision = revision;
  try {
    const next = await request('/api/state');
    if (!next || typeof next.napRunning !== 'boolean' || typeof next.storageReady !== 'boolean') throw new DeviceError('기기 상태를 모두 읽지 못했습니다. 페이지를 새로고침해 주세요.');
    if (startedRevision === revision) render(next);
  } catch (error) {
    if (startedRevision === revision) setConnection(false);
    throw error;
  }
}
async function refreshPhotoList(force) {
  if (!force && state && state.photosRev === photosRev) return;
  const body = await request('/api/photos', undefined, 15000);
  devicePhotos = Array.isArray(body.photos) ? body.photos.filter(name => typeof name === 'string') : [];
  deviceSet = new Set(devicePhotos);
  photosRev = body.rev;
  renderPhotos();
}
async function operation(work) {
  if (busy) return;
  const previousFocus = document.activeElement;
  busy = true; revision++; clearError(); notice('처리 중…'); updateDisabled();
  try {
    const message = await work();
    await refreshState();
    notice(message || '변경 사항을 적용했습니다.');
  } catch (error) {
    showError(error); notice('');
    try { await refreshState(); } catch (_) { /* The connection banner remains visible. */ }
  } finally {
    busy = false; updateDisabled();
    if (connected && (document.activeElement === document.body || document.activeElement === previousFocus)) {
      let target = previousFocus;
      if (target && target.hasAttribute('data-photo-delete') && !target.isConnected) target = $('photoList').querySelector('button') || $('photos');
      if (target === $('photoUploadButton') && target.disabled) target = $('photos');
      if (target && target.isConnected && !target.disabled && target.matches('[data-control]')) target.focus({preventScroll:true});
    }
  }
}
function control(action, value) { return request('/api/control', value === undefined ? {action} : {action, value:String(value)}); }
function syncClock() { return request('/api/time', {epoch:String(Math.floor(Date.now() / 1000)), offset:String(-new Date().getTimezoneOffset())}); }

// ---- photo identity: capture time + id, so the album plays in date order and
// a photo picked again is recognised and skipped.
function fnv1a(bytesIn, seed) {
  let hash = seed === undefined ? 0x811c9dc5 : seed;
  for (let i = 0; i < bytesIn.length; i++) { hash ^= bytesIn[i]; hash = Math.imul(hash, 0x01000193) >>> 0; }
  return hash >>> 0;
}
function hex8(value) { return (value >>> 0).toString(16).padStart(8, '0'); }
function exifInfo(buffer) {
  const view = new DataView(buffer);
  if (view.byteLength < 4 || view.getUint16(0) !== 0xFFD8) return null;
  let offset = 2;
  while (offset + 4 <= view.byteLength) {
    if (view.getUint8(offset) !== 0xFF) return null;
    const marker = view.getUint8(offset + 1);
    if (marker === 0xFF) { offset++; continue; }
    if (marker === 0xDA || marker === 0xD9) return null;
    const length = view.getUint16(offset + 2);
    if (length < 2) return null;
    if (marker === 0xE1 && offset + 10 <= view.byteLength && view.getUint32(offset + 4) === 0x45786966 && view.getUint16(offset + 8) === 0)
      return tiffInfo(view, offset + 10, Math.min(view.byteLength, offset + 2 + length));
    offset += 2 + length;
  }
  return null;
}
function tiffInfo(view, start, end) {
  if (start + 8 > end) return null;
  const order = view.getUint16(start), little = order === 0x4949;
  if (!little && order !== 0x4D4D) return null;
  const u16 = at => view.getUint16(at, little), u32 = at => view.getUint32(at, little);
  if (u16(start + 2) !== 42) return null;
  const entries = ifd => {
    const found = new Map();
    if (ifd < start || ifd + 2 > end) return found;
    const total = u16(ifd);
    for (let i = 0; i < total && ifd + 14 + i * 12 <= end; i++) { const at = ifd + 2 + i * 12; found.set(u16(at), at); }
    return found;
  };
  const ascii = at => {
    if (at === undefined || u16(at + 2) !== 2) return '';
    const length = u32(at + 4), data = length > 4 ? start + u32(at + 8) : at + 8;
    if (data < start || data + length > end) return '';
    let text = '';
    for (let i = 0; i < length; i++) { const c = view.getUint8(data + i); if (!c) break; text += String.fromCharCode(c); }
    return text.trim();
  };
  const zero = entries(start + u32(start + 4));
  let date = '', sub = '';
  if (zero.has(0x8769)) {
    const exif = entries(start + u32(zero.get(0x8769) + 8));
    date = ascii(exif.get(0x9003)); sub = ascii(exif.get(0x9291));
  }
  if (!date) date = ascii(zero.get(0x0132));
  const match = /^(\d{4}):(\d{2}):(\d{2}) (\d{2}):(\d{2}):(\d{2})/.exec(date);
  if (!match) return null;
  const seconds = Date.UTC(+match[1], +match[2] - 1, +match[3], +match[4], +match[5], +match[6]) / 1000;
  if (!Number.isFinite(seconds) || seconds <= 0) return null;
  return {seconds, key:'E|' + date + '|' + sub};
}
async function describe(file) {
  let head = new Uint8Array(0);
  try { head = new Uint8Array(await file.slice(0, 65536).arrayBuffer()); } catch (_) { /* unreadable: decided later */ }
  let info = null;
  try { info = exifInfo(head.buffer); } catch (_) { info = null; }
  const modified = file.lastModified || Date.now();
  const localSeconds = modified / 1000 - new Date(modified).getTimezoneOffset() * 60;
  const seconds = Math.max(1, Math.min(0xffffffff, Math.floor(info ? info.seconds : localSeconds)));
  const id = info ? fnv1a(new TextEncoder().encode(info.key)) : fnv1a(new TextEncoder().encode('F|' + file.size), fnv1a(head));
  return {file, seconds, name:'p' + hex8(seconds) + hex8(id) + '.jpg'};
}
function spread(items, wanted) {
  if (wanted <= 0) return [];
  if (wanted >= items.length) return items.slice();
  if (wanted === 1) return [items[Math.floor(items.length / 2)]];
  const picked = [];
  for (let k = 0; k < wanted; k++) picked.push(items[Math.round(k * (items.length - 1) / (wanted - 1))]);
  return picked;
}
function loadImage(file) {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const image = new Image();
    image.decoding = 'async';
    image.onload = () => resolve({image, url});
    image.onerror = () => { URL.revokeObjectURL(url); reject(new DeviceError('사진을 열 수 없습니다 (' + file.name + ').', 0, 'unreadable')); };
    image.src = url;
  });
}
function canvasJpeg(canvas, quality) {
  return new Promise(resolve => canvas.toBlob(resolve, 'image/jpeg', quality));
}
async function photoBlob(file, maximumBytes) {
  if (file.size > 80000000) throw new DeviceError('사진 파일이 너무 큽니다 (' + file.name + ').', 0, 'unreadable');
  const {image, url} = await loadImage(file);
  const canvas = document.createElement('canvas');
  let middle = null;
  try {
    const width = image.naturalWidth, height = image.naturalHeight;
    if (!width || !height) throw new DeviceError('사진 크기를 읽지 못했습니다 (' + file.name + ').', 0, 'unreadable');
    canvas.width = canvas.height = 412;
    const context = canvas.getContext('2d');
    if (!context) throw new DeviceError('이 브라우저에서는 사진을 준비할 수 없습니다. Safari나 Chrome에서 다시 시도해 주세요.', 0, 'browser');
    context.fillStyle = '#000'; context.fillRect(0, 0, 412, 412);
    context.imageSmoothingEnabled = true; context.imageSmoothingQuality = 'high';
    const crop = Math.min(width, height), x = (width - crop) / 2, y = (height - crop) / 2;
    if (crop > 1648) {
      // Two smaller steps keep fine detail (hair, fabric) from shimmering.
      middle = document.createElement('canvas'); middle.width = middle.height = 824;
      const step = middle.getContext('2d');
      step.imageSmoothingEnabled = true; step.imageSmoothingQuality = 'high';
      step.drawImage(image, x, y, crop, crop, 0, 0, 824, 824);
      context.drawImage(middle, 0, 0, 824, 824, 0, 0, 412, 412);
    } else context.drawImage(image, x, y, crop, crop, 0, 0, 412, 412);
    let quality = 0.8, blob = null;
    for (;;) {
      blob = await canvasJpeg(canvas, quality);
      if (!blob || !blob.size || blob.type !== 'image/jpeg') throw new DeviceError('JPEG 사진을 만들 수 없습니다. Safari나 Chrome에서 다시 시도해 주세요.', 0, 'browser');
      if (blob.size <= maximumBytes || quality < 0.6) break;
      quality -= 0.15;
    }
    if (blob.size > maximumBytes) throw new DeviceError('압축한 사진이 너무 큽니다 (' + file.name + ').', 0, 'unreadable');
    return blob;
  } finally {
    URL.revokeObjectURL(url);
    image.removeAttribute('src');
    canvas.width = canvas.height = 0;
    if (middle) middle.width = middle.height = 0;
  }
}
function pickedPhotos(source) {
  const files = Array.from($(source).files || []);
  const photos = files.filter(file => !file.name.startsWith('.') && !/^video\//i.test(file.type) &&
    (/^image\//i.test(file.type) || /\.(jpe?g|png|webp|gif|bmp|heic|heif|avif|tiff?)$/i.test(file.name)));
  return {photos, skipped:files.length - photos.length};
}
function describePlan(p) {
  const lines = [];
  if (!p.fresh) lines.push(p.total ? '선택한 사진은 모두 이미 BabyGrip에 있어요.' : '올릴 수 있는 사진이 없습니다.');
  else if (!p.chosen.length) lines.push('저장 공간이 가득 찼습니다. 저장된 사진을 일부 지운 뒤 다시 선택해 주세요.');
  else if (p.chosen.length < p.fresh) lines.push('새 사진 ' + count(p.fresh) + '장 중 약 ' + count(p.chosen.length) + '장이 들어갈 공간이 있어요. 찍은 날짜가 골고루 퍼지도록 ' + count(p.chosen.length) + '장을 골라 올립니다.');
  else lines.push('새 사진 ' + count(p.chosen.length) + '장을 올립니다 (약 ' + bytes(p.chosen.length * perPhotoBytes()) + ', 남은 공간 ' + bytes(state.storageFree) + ').');
  if (p.fresh && p.already) lines.push('이미 BabyGrip에 있는 ' + count(p.already) + '장은 건너뜁니다.');
  if (p.skipped) lines.push('동영상 등 사진이 아닌 파일 ' + count(p.skipped) + '개는 제외합니다.');
  if (p.chosen.length) lines.push('예상 시간 약 ' + waitText(p.chosen.length * 0.9) + '.');
  showPlan(lines.join(' '));
}
function updateUploadButton() {
  const chosen = plan ? plan.chosen.length : 0;
  $('photoUploadButton').textContent = !chosen ? '사진 업로드' : (chosen < plan.fresh ? '골고루 ' + count(chosen) + '장 올리기' : count(chosen) + '장 올리기');
  updateDisabled();
}
async function planSelection(source) {
  const token = ++planToken;
  $(source === 'photos' ? 'photoFolder' : 'photos').value = '';
  const {photos, skipped} = pickedPhotos(source);
  plan = null; planning = photos.length > 0; clearError(); updateUploadButton();
  if (!photos.length) { planning = false; showPlan(skipped ? '선택한 파일 중 사진이 없습니다.' : ''); updateUploadButton(); return; }
  showPlan(count(photos.length) + '장을 확인하는 중…');
  try {
    const described = [];
    for (let i = 0; i < photos.length; i += 8) {
      described.push(...await Promise.all(photos.slice(i, i + 8).map(describe)));
      if (token !== planToken) return;
      if (photos.length > 40) showPlan(count(photos.length) + '장을 확인하는 중… ' + Math.round(described.length / photos.length * 100) + '%');
    }
    described.sort((a, b) => a.seconds - b.seconds || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    const unique = [], seen = new Set();
    for (const item of described) if (!seen.has(item.name)) { seen.add(item.name); unique.push(item); }
    await refreshState();
    await refreshPhotoList(true);
    if (token !== planToken) return;
    const fresh = unique.filter(item => !deviceSet.has(item.name));
    const cache = new Map(), samples = [];
    const sampleCount = Math.min(fresh.length, 4);
    for (let k = 0; k < sampleCount; k++) {
      const item = fresh[Math.round(k * (fresh.length - 1) / Math.max(1, sampleCount - 1))];
      if (cache.has(item.name)) continue;
      showPlan('사진 크기를 재는 중… ' + (k + 1) + ' / ' + sampleCount);
      try { const blob = await photoBlob(item.file, maxBytes()); cache.set(item.name, blob); samples.push(allocation(blob.size)); } catch (_) { /* counted during upload */ }
      if (token !== planToken) return;
    }
    if (samples.length) measuredPhotoBytes = samples.reduce((a, b) => a + b, 0) / samples.length;
    const room = Math.floor((Number(state.storageFree) || 0) / perPhotoBytes());
    const slots = Math.max(0, Math.min(room, (Number(state.photoLimit) || 2000) - state.photoCount));
    plan = {chosen:spread(fresh, slots), fresh:fresh.length, already:unique.length - fresh.length, skipped, total:unique.length, cache};
    describePlan(plan);
  } catch (error) {
    if (token === planToken) { plan = null; showPlan(''); showError(error); }
  } finally {
    if (token === planToken) { planning = false; updateUploadButton(); if (state) render(state); }
  }
}
async function uploadPlan(p) {
  const items = p.chosen;
  const panel = $('uploadPanel'), bar = $('uploadProgress'), status = $('uploadStatus'), preview = $('uploadPreview');
  panel.hidden = false; bar.max = items.length; bar.value = 0; status.textContent = '첫 사진을 준비하는 중…';
  let saved = 0, duplicates = 0, unreadable = 0, done = 0, stop = null, previewUrl = '';
  const started = performance.now();
  const prepare = item => (p.cache.has(item.name) ? Promise.resolve(p.cache.get(item.name)) : photoBlob(item.file, maxBytes()))
    .then(blob => ({blob}), error => ({error}));
  let pending = prepare(items[0]);
  try {
    for (let i = 0; i < items.length && !stop; i++) {
      const item = items[i];
      const prepared = await pending;
      p.cache.delete(item.name);
      pending = i + 1 < items.length ? prepare(items[i + 1]) : null;
      if (prepared.error) { unreadable++; done++; bar.value = done; continue; }
      if (previewUrl) URL.revokeObjectURL(previewUrl);
      previewUrl = URL.createObjectURL(prepared.blob); preview.src = previewUrl;
      let reply = null;
      for (let attempt = 0; !reply && !stop; attempt++) {
        try {
          const form = new FormData(); form.append('file', prepared.blob, item.name);
          reply = await request('/api/photo?size=' + prepared.blob.size, form, 30000);
        } catch (error) {
          if (error.code === 'invalid' || error.code === 'incomplete' && attempt >= 1) reply = {unreadable:true};
          else if (['full','limit','auth','memory','storage','name'].includes(error.code) || attempt >= 2) stop = error;
          else await sleep(1500 * (attempt + 1));
        }
      }
      if (stop) break;
      done++;
      if (reply.unreadable) unreadable++;
      else if (reply.duplicate) duplicates++;
      else { saved++; deviceSet.add(item.name); }
      if (state && typeof reply.photoCount === 'number') { state.photoCount = reply.photoCount; state.storageFree = reply.storageFree; $('albumTag').textContent = '사진 ' + count(reply.photoCount) + '장'; }
      bar.value = done;
      const elapsed = (performance.now() - started) / 1000;
      status.textContent = count(done) + ' / ' + count(items.length) + '장' + (done < items.length ? ' · 남은 시간 약 ' + waitText(elapsed / done * (items.length - done)) : ' · 마무리 중');
    }
  } finally {
    if (previewUrl) URL.revokeObjectURL(previewUrl);
    preview.removeAttribute('src'); panel.hidden = true;
  }
  const parts = ['사진 ' + count(saved) + '장을 저장했습니다.'];
  if (duplicates) parts.push('이미 있던 ' + count(duplicates) + '장은 건너뛰었습니다.');
  if (unreadable) parts.push('열 수 없는 사진 ' + count(unreadable) + '장은 제외했습니다.');
  if (stop && (stop.code === 'full' || stop.code === 'limit')) {
    parts.push(stop.code === 'full' ? '저장 공간이 가득 차서 나머지는 올리지 않았습니다.' : '사진 수 한도에 도달해 나머지는 올리지 않았습니다.');
    return parts.join(' ');
  }
  if (stop) throw new DeviceError(parts.join(' ') + ' ' + stop.message + ' 같은 사진을 다시 고르면 이미 저장된 사진은 건너뛰고 이어서 올립니다.', stop.status, stop.code);
  return parts.join(' ') + ' Wi-Fi를 끊어도 BabyGrip에서 계속 재생됩니다.';
}
$('napButton').addEventListener('click', () => operation(async () => { await control(state.napRunning ? 'nap_stop' : 'nap_start'); return '낮잠 기록 상태를 변경했습니다.'; }));
$('noiseButton').addEventListener('click', () => operation(async () => { await control(state.noisePlaying ? 'noise_stop' : 'noise_start'); return '스피커 상태를 변경했습니다.'; }));
$('volume').addEventListener('input', () => { editingVolume = true; $('volumeValue').textContent = $('volume').value + ' / ' + (state ? state.volumeCap : 5); });
$('volume').addEventListener('change', () => { const value = $('volume').value; editingVolume = false; operation(async () => { await control('volume', value); return '음량을 변경했습니다.'; }); });
$('volume').addEventListener('blur', () => { editingVolume = false; });
$('timer').addEventListener('change', () => { const value = $('timer').value; operation(async () => { await control('timer', value); return '자동 정지 시간을 변경했습니다.'; }); });
$('track').addEventListener('change', () => { const value = $('track').value; operation(async () => { await control('track', value); return '음원을 선택했습니다. 소리 재생을 눌러 시작하세요.'; }); });
$('previousButton').addEventListener('click', () => operation(async () => { await control('previous'); return '이전 사진을 선택했습니다.'; }));
$('nextButton').addEventListener('click', () => operation(async () => { await control('next'); return '다음 사진을 선택했습니다.'; }));
$('slideshowButton').addEventListener('click', () => operation(async () => { await control('slideshow', state.autoplay ? 0 : 1); return '앨범 재생 상태를 변경했습니다.'; }));
$('syncButton').addEventListener('click', () => operation(async () => { await syncClock(); return '기기를 휴대폰 시간에 맞췄습니다.'; }));
$('retryButton').addEventListener('click', () => operation(async () => { await refreshState(); await refreshPhotoList(true); return '다시 연결했습니다.'; }));
$('photos').addEventListener('change', () => planSelection('photos'));
$('photoFolder').addEventListener('change', () => planSelection('photoFolder'));
$('folderPicker').hidden = !('webkitdirectory' in $('photoFolder'));
$('noiseFile').addEventListener('change', () => {
  const file = $('noiseFile').files[0];
  if (file) $('noiseName').value = file.name.replace(/\.mp3$/i, '').toLowerCase().replace(/[^a-z0-9_-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 20) || 'my-sound';
  updateDisabled();
});
$('noiseName').addEventListener('input', updateDisabled);
$('noiseDeleteButton').addEventListener('click', () => {
  const track = selectedUserTrack();
  if (!track || !window.confirm('‘' + track.title.toLowerCase() + '’ 음원을 BabyGrip에서 지울까요?')) return;
  operation(async () => { await control('sound_delete', track.key); return '음원을 지웠습니다.'; });
});
['feedLeft','feedRight'].forEach(id => $(id).addEventListener('click', () => operation(async () => {
  await request('/api/feed', {action: id === 'feedLeft' ? 'left' : 'right'});
  await refreshFeedList(true);
  return '수유 기록을 바꿨습니다.';
})));
$('bottleButton').addEventListener('click', () => {
  const value = $('bottleMl').value.trim();
  const ml = value === '' ? 0 : parseInt(value, 10);
  if (!(ml >= 0 && ml <= 400)) { showError(new DeviceError('분유 양은 0에서 400 ml 사이로 입력해 주세요.')); return; }
  operation(async () => { await request('/api/feed', {action:'bottle', value:String(ml)}); $('bottleMl').value = ''; await refreshFeedList(true); return '분유·젖병 수유를 기록했습니다.'; });
});
document.querySelectorAll('[data-diaper]').forEach(button => button.addEventListener('click', () => operation(async () => {
  await request('/api/feed', {action:'diaper', value:button.getAttribute('data-diaper')});
  await refreshFeedList(true);
  return '기저귀를 기록했습니다.';
})));
$('feedHistory').addEventListener('toggle', () => { if ($('feedHistory').open) refreshFeedList(true).catch(() => {}); });
$('feedExport').addEventListener('click', () => operation(async () => { await refreshFeedList(true); exportFeedCsv(); return 'CSV 파일을 만들었습니다.'; }));
$('nightButton').addEventListener('click', () => operation(async () => { await control('night', state.night ? 0 : 1); return state.night ? '밤 모드를 껐습니다.' : '밤 모드를 켰습니다.'; }));
function saveNightSchedule() {
  const start = $('nightStart').value, end = $('nightEnd').value, on = $('nightAuto').checked;
  operation(async () => {
    if (!/^\d\d:\d\d$/.test(start) || !/^\d\d:\d\d$/.test(end) || start === end) throw new DeviceError('켜는 시간과 끄는 시간을 다르게 정해 주세요.');
    await request('/api/control', {action:'night_auto', value:on ? '1' : '0', start, end});
    return on ? '밤 모드를 ' + start + '에 켜고 ' + end + '에 끕니다.' : '밤 모드 자동 전환을 껐습니다.';
  });
}
$('nightAuto').addEventListener('change', () => { updateDisabled(); saveNightSchedule(); });
$('nightSave').addEventListener('click', saveNightSchedule);
['dayLight','nightLight'].forEach(id => {
  $(id).addEventListener('input', () => { editingLight = true; $(id + 'Value').textContent = $(id).value; });
  $(id).addEventListener('change', () => {
    const day = $('dayLight').value, night = $('nightLight').value;
    editingLight = false;
    operation(async () => { await request('/api/control', {action:'brightness', day, night}); return '화면 밝기를 바꿨습니다.'; });
  });
});
function secondsText(s) { return s >= 60 && s % 60 === 0 ? (s / 60) + '분' : s + '초'; }
$('crySensitivity').addEventListener('change', () => { const v = Number($('crySensitivity').value); operation(async () => { await control('cry', v); return v ? '울음 감지를 켰습니다. 아기가 5초 넘게 크게 울면 소리를 틀어요.' : '울음 감지를 껐습니다. 마이크도 꺼집니다.'; }); });
$('cryTrack').addEventListener('change', () => { const key = $('cryTrack').value; operation(async () => { await control('cry_track', key); return '울음이 들리면 이 소리를 틉니다.'; }); });
$('slideAuto').addEventListener('change', () => { const on = $('slideAuto').checked; operation(async () => { await control('slide_auto', on ? 1 : 0); return on ? '2분 동안 안 만지면 글자 없이 사진만 넘깁니다.' : '사진 보기 기본 화면을 껐습니다.'; }); });
$('slideSeconds').addEventListener('change', () => { const s = Number($('slideSeconds').value); operation(async () => { await control('slide_seconds', s); return secondsText(s) + '마다 사진을 넘깁니다.'; }); });
$('glanceAuto').addEventListener('change', () => { const on = $('glanceAuto').checked; operation(async () => { await control('glance', on ? 1 : 0); return on ? '엎어 두면 시계 화면을 보여 줍니다.' : '엎어 둘 때 화면 바꾸기를 껐습니다.'; }); });
$('calibrateButton').addEventListener('click', () => operation(async () => {
  await control('glance_calibrate');
  for (let s = 5; s > 0; s--) { notice(s + '초 안에 휴대폰을 탁자에 엎어 두세요…'); await sleep(1000); }
  await sleep(1500);
  await refreshState();
  if (state.calibration !== 'ok') throw new DeviceError('BabyGrip이 평평하게 놓이지 않았어요. 휴대폰을 탁자에 엎어 둔 채 다시 해 주세요.');
  return '방향을 맞췄습니다. 이제 엎어 두면 시계·마지막 수유 화면이 나와요.';
}));
$('lookButton').addEventListener('click', () => operation(async () => { await control('look'); return 'BabyGrip에 ‘여기 봐!’ 화면을 켰습니다. 길게 누르면 끝나요.'; }));
$('photoUploadButton').addEventListener('click', () => {
  const current = plan;
  if (!current || !current.chosen.length || planning) return;
  operation(async () => {
    notice('사진을 올리는 중입니다. 이 화면을 켜 두세요.');
    try { return await uploadPlan(current); }
    finally {
      plan = null; planToken++; $('photos').value = ''; $('photoFolder').value = ''; showPlan(''); updateUploadButton();
      try { await refreshPhotoList(true); } catch (_) { /* shown by the connection banner */ }
    }
  });
});
$('deleteAllButton').addEventListener('click', () => {
  if (!state || !state.photoCount) return;
  if (!window.confirm('BabyGrip에 저장된 사진 ' + count(state.photoCount) + '장을 모두 지울까요? 아이폰의 원본 사진은 지워지지 않습니다.')) return;
  operation(async () => {
    const result = await request('/api/delete_all', {confirm:'all'}, 120000);
    await refreshPhotoList(true);
    return 'BabyGrip의 사진 ' + count(result.removed) + '장을 지웠습니다.';
  });
});
$('noiseUploadButton').addEventListener('click', () => {
  const file = $('noiseFile').files[0];
  operation(async () => {
    const name = $('noiseName').value;
    if (!file || !/\.mp3$/i.test(file.name)) throw new DeviceError('MP3 음원 파일을 선택해 주세요.');
    if (!file.size || file.size > MAX_MP3_BYTES) throw new DeviceError('MP3 파일의 크기는 0바이트보다 크고 최대 2 MB여야 합니다.');
    if (!SOUND_NAME.test(name)) throw new DeviceError('음원 이름은 영문 소문자·숫자·-·_로 20자 이하여야 합니다.');
    notice('새 음원을 업로드하는 중…');
    const form = new FormData(); form.append('file', file, name + '.mp3');
    await request('/api/noise?name=' + encodeURIComponent(name), form, 60000); $('noiseFile').value = ''; $('noiseName').value = '';
    return '‘' + name + '’ 음원을 추가했습니다. 소리 재생을 눌러 주세요.';
  });
});
$('noiseDefaultButton').addEventListener('click', () => operation(async () => { await control('noise_default'); return '기본 백색소음으로 되돌렸습니다.'; }));
async function poll() {
  if (!busy && !pollInFlight) {
    pollInFlight = true;
    try { await refreshState(); await refreshPhotoList(false); await refreshFeedList(false); } catch (_) { /* The persistent connection banner explains recovery. */ }
    finally { pollInFlight = false; }
  }
  setTimeout(poll, 2000);
}
async function start() {
  try {
    await refreshState();
    try { await syncClock(); await refreshState(); }
    catch (error) { notice('자동 시간 설정을 확인하지 못했습니다. 연결된 상태에서 ‘휴대폰 시간으로 맞추기’를 눌러 주세요.'); }
    await refreshPhotoList(true);
    await refreshFeedList(true);
  } catch (_) { /* The connection banner provides a manual retry. */ }
  finally { busy = false; updateDisabled(); setTimeout(poll, 2000); }
}
start();
})();
</script>
</body>
</html>)BGHTML";
