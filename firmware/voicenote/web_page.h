// tools/make_web.py 가 web/index.html 로 만든 파일. 직접 고치지 말 것.
#pragma once
static const char kIndexHtml[] = R"VN(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>VOICE NOTE</title>
<style>
:root{--bg:#f4f4f1;--card:#fff;--ink:#141414;--sub:#6b6b66;--line:#e2e2dc;--accent:#141414;--ok:#1f7a4a;--warn:#a15c00}
@media (prefers-color-scheme:dark){:root{--bg:#121212;--card:#1d1d1c;--ink:#f1f1ee;--sub:#9b9b95;--line:#2e2e2c;--accent:#f1f1ee;--ok:#5cc58c;--warn:#e0a24a}}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.6 -apple-system,BlinkMacSystemFont,"Apple SD Gothic Neo","Noto Sans KR",sans-serif;-webkit-text-size-adjust:100%}
header{position:sticky;top:0;z-index:2;background:var(--bg);padding:16px 16px 10px;padding-top:max(16px,env(safe-area-inset-top));border-bottom:1px solid var(--line);display:flex;align-items:center;gap:10px}
header h1{font-size:19px;margin:0;letter-spacing:.04em;font-weight:800;flex:1}
header .count{color:var(--sub);font-size:14px}
.back{border:0;background:none;color:var(--ink);font-size:16px;padding:6px 8px 6px 0;cursor:pointer;display:none}
main{max-width:640px;margin:0 auto;padding:12px 16px 40px}
.note{display:block;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px;margin:10px 0;cursor:pointer;width:100%;text-align:left;color:inherit;font:inherit}
.note:active{transform:scale(.99)}
.top{display:flex;align-items:center;gap:8px;font-size:14px;color:var(--sub)}
.num{font-weight:800;color:var(--ink);font-variant-numeric:tabular-nums}
.top .date{flex:1}
.prev{margin-top:6px;font-size:16px;line-height:1.5;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;word-break:keep-all;overflow-wrap:anywhere}
.prev.none{color:var(--sub);font-size:15px}
.badge{font-size:12px;font-weight:700;padding:3px 8px;border-radius:99px;white-space:nowrap}
.badge.ok{color:var(--ok);border:1px solid currentColor}
.badge.wait{color:var(--warn);border:1px solid currentColor}
.empty{color:var(--sub);text-align:center;padding:60px 0}
.detail h2{font-size:24px;margin:8px 0 2px}
.detail .when{color:var(--sub);font-size:14px;margin-bottom:14px}
audio{width:100%;margin:4px 0 14px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:14px}
.btn{flex:1;min-width:120px;border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:12px;padding:12px 10px;font:inherit;font-weight:700;cursor:pointer;text-align:center;text-decoration:none}
.btn.primary{background:var(--accent);color:var(--bg);border-color:var(--accent)}
.text{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;white-space:pre-wrap;word-break:keep-all;overflow-wrap:anywhere;font-size:17px;line-height:1.75}
.text.none{color:var(--sub)}
.toast{position:fixed;left:50%;bottom:28px;transform:translateX(-50%);background:var(--accent);color:var(--bg);padding:10px 18px;border-radius:99px;font-weight:700;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
.hbtn{border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:99px;padding:5px 14px;font:inherit;font-size:14px;font-weight:700;cursor:pointer}
.note.sel{border-color:var(--ink);box-shadow:inset 0 0 0 1px var(--ink)}
.tick{width:22px;height:22px;border-radius:50%;border:2px solid var(--sub);flex:none;display:inline-flex;align-items:center;justify-content:center;font-size:13px;font-weight:900;color:var(--bg)}
.note.sel .tick{background:var(--ink);border-color:var(--ink)}
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--bg);border-top:1px solid var(--line);padding:12px 16px;padding-bottom:max(12px,env(safe-area-inset-bottom));display:none;gap:8px;z-index:3}
.bar.show{display:flex}
.btn.danger{color:#c62f2f;border-color:#c62f2f55}
.btn.danger.fill{background:#c62f2f;color:#fff;border-color:#c62f2f}
.btn:disabled{opacity:.4}
.delrow{margin-top:28px}
main.selecting{padding-bottom:100px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px;margin-bottom:14px}
.card h3{margin:0 0 6px;font-size:14px;color:var(--sub);font-weight:700}
.sum{white-space:pre-wrap;word-break:keep-all;overflow-wrap:anywhere;font-size:16px;line-height:1.7}
.tabs{display:flex;gap:6px;margin-bottom:10px}
.tab{border:1px solid var(--line);background:none;color:var(--sub);border-radius:99px;padding:6px 14px;font:inherit;font-size:14px;font-weight:700;cursor:pointer}
.tab.on{background:var(--ink);color:var(--bg);border-color:var(--ink)}
.utt{display:flex;gap:10px;margin:0 0 12px}
.who{flex:none;width:30px;height:30px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:800;font-size:13px;color:#fff}
.say{flex:1;word-break:keep-all;overflow-wrap:anywhere;line-height:1.7}
.note2{color:var(--sub);font-size:13px;margin:-6px 0 14px}
.demo{font-size:12px;color:var(--warn);text-align:center;margin-top:20px}
</style>
</head>
<body>
<header>
  <button class="back" id="back" aria-label="목록으로">← 목록</button>
  <h1 id="title">VOICE NOTE</h1>
  <span class="count" id="count"></span>
  <button class="hbtn" id="selbtn" style="display:none">선택</button>
</header>
<main id="main"></main>
<div class="bar" id="bar">
  <button class="btn" id="cancelSel">취소</button>
  <button class="btn danger fill" id="delSel" disabled>삭제</button>
</div>
<div class="toast" id="toast"></div>
<script>
// 기기(voicenote.local)에서 열면 기기 데이터를, 그냥 파일로 열면 예시 데이터를 보여 준다.
const DEMO = location.protocol === "file:" || location.search.includes("demo");
const demoNotes = [
  {n: 19, when: 1763190600, sec: 10, conv: true, prev: "오늘 회의에서 정한 것 정리. 다음 주 화요일까지 케이스 시안 두 개 만들고, 수요일에 촬영 일정 잡기."},
  {n: 18, when: 1763187000, sec: 31, conv: true, prev: "장 볼 것. 우유, 계란, 두부, 대파. 세탁소에 맡긴 코트 찾아오기."},
  {n: 17, when: 1763100000, sec: 312, conv: false, prev: ""},
  {n: 16, when: 1763013600, sec: 65, conv: true, prev: "스택찬 영상 아이디어. 사람이 들어오면 인사하고 나가면 어디 가냐고 묻는 장면을 첫 장면으로."},
];
const demoSummary = "회의에서 케이스 시안과 촬영 일정을 정함.\n- 다음 주 화요일까지 케이스 시안 2개\n- 수요일 촬영\n- 배터리 1000mAh로 바꿔 재시험\n할 일:\n- 케이스 시안 만들기\n- 촬영 일정 잡기";
const demoSpeakers = "A: 다음 주 화요일까지 케이스 시안 두 개 만들 수 있어요?\nB: 네, 화요일까지 할게요. 촬영은 수요일 괜찮아요?\nA: 좋아요. 배터리는 1000mAh로 바꿔서 다시 해 봐요.";
const demoText = "오늘 회의에서 정한 것 정리. 다음 주 화요일까지 케이스 시안 두 개 만들고, 수요일에 촬영 일정 잡기. 배터리는 1000mAh 로 바꿔서 다시 시험해 보기로 함.";

const $ = (id) => document.getElementById(id);
const pad3 = (n) => "#" + String(n).padStart(3, "0");
const len = (s) => (s >= 60 ? Math.floor(s / 60) + "분 " : "") + (s % 60 ? (s % 60) + "초" : "");
function when(t) {
  if (!t || t < 1735689600) return "날짜 없음";
  const d = new Date(t * 1000);
  const hh = String(d.getHours()).padStart(2, "0"), mm = String(d.getMinutes()).padStart(2, "0");
  return `${d.getMonth() + 1}월 ${d.getDate()}일 ${hh}:${mm}`;
}
function toast(msg) {
  const t = $("toast");
  t.textContent = msg;
  t.classList.add("show");
  setTimeout(() => t.classList.remove("show"), 1600);
}
async function getNotes() {
  if (DEMO) return demoNotes;
  const r = await fetch("/api/notes");
  return r.json();
}
async function getText(n) {
  if (DEMO) return demoText;
  const r = await fetch("/api/text?n=" + n);
  return r.ok ? r.text() : "";
}
const esc = (s) => s.replace(/[&<>]/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;"}[c]));

let notes = [];
let selecting = false;
const picked = new Set();

async function deleteNotes(nums) {
  if (DEMO) { notes = notes.filter((x) => !nums.includes(x.n)); demoNotes.splice(0, demoNotes.length, ...notes); return true; }
  const r = await fetch("/api/delete", {method: "POST", headers: {"Content-Type": "application/x-www-form-urlencoded"}, body: "n=" + nums.join(",")});
  return r.ok;
}

function renderList() {
  const main = $("main");
  main.classList.toggle("selecting", selecting);
  $("count").textContent = selecting ? `${picked.size}개 선택` : `녹음 ${notes.length}개`;
  $("selbtn").style.display = notes.length ? "inline-block" : "none";
  $("selbtn").textContent = selecting ? "모두" : "선택";
  $("bar").classList.toggle("show", selecting);
  $("delSel").disabled = picked.size === 0;
  $("delSel").textContent = picked.size ? `${picked.size}개 삭제` : "삭제";
  if (!notes.length) { main.innerHTML = '<p class="empty">녹음이 없어요</p>'; return; }
  main.innerHTML = notes.map((x) => `
    <button class="note ${picked.has(x.n) ? "sel" : ""}" data-n="${x.n}">
      <div class="top">${selecting ? '<span class="tick">✓</span>' : ""}<span class="num">${pad3(x.n)}</span><span class="date">${when(x.when)} · ${len(x.sec) || "0초"}</span>${x.conv ? "" : '<span class="badge wait">변환 전</span>'}</div>
      <div class="prev ${x.prev ? "" : "none"}">${x.prev ? esc(x.prev) : x.conv ? "(글이 비어 있어요)" : "아직 글로 바꾸지 않았어요"}</div>
    </button>`).join("") + (DEMO ? '<p class="demo">예시 화면 (기기 데이터 아님)</p>' : "");
  main.querySelectorAll(".note").forEach((b) => b.onclick = () => {
    const n = +b.dataset.n;
    if (selecting) { picked.has(n) ? picked.delete(n) : picked.add(n); renderList(); }
    else showDetail(notes.find((x) => x.n === n));
  });
}

function setSelecting(on) {
  selecting = on;
  picked.clear();
  renderList();
}

async function showList() {
  history.replaceState(null, "", location.pathname + location.search);
  $("back").style.display = "none";
  $("title").textContent = "VOICE NOTE";
  const main = $("main");
  main.innerHTML = '<p class="empty">불러오는 중…</p>';
  try { notes = await getNotes(); } catch (e) { main.innerHTML = '<p class="empty">기기와 연결이 끊겼어요.<br>기기에서 전송 모드를 다시 켜 주세요.</p>'; return; }
  renderList();
}

let info = {speakers: DEMO};
async function getInfo() {
  if (DEMO) return;
  try { info = await (await fetch("/api/info")).json(); } catch (e) {}
}
async function getStored(kind, n) { // kind: summary | speakers
  if (DEMO) return "";
  const r = await fetch(`/api/${kind}?n=${n}`);
  return r.ok ? r.text() : "";
}
const COLORS = ["#2f6fdf", "#d4572a", "#2e8b57", "#8a4fd1", "#b08800", "#c2307a"];
function speakersHtml(t) {
  return t.trim().split("\n").map((line) => {
    const i = line.indexOf(": ");
    const who = i > 0 ? line.slice(0, i) : "?";
    const say = i > 0 ? line.slice(i + 2) : line;
    const c = COLORS[(who.charCodeAt(0) - 65 + COLORS.length) % COLORS.length];
    return `<div class="utt"><span class="who" style="background:${c}">${esc(who)}</span><span class="say">${esc(say)}</span></div>`;
  }).join("");
}

async function showDetail(x) {
  history.pushState({n: x.n}, "", "#" + x.n);
  $("back").style.display = "block";
  $("selbtn").style.display = "none";
  $("bar").classList.remove("show");
  $("title").textContent = "";
  $("count").textContent = "";
  const text = x.conv ? await getText(x.n) : "";
  let summary = text ? await getStored("summary", x.n) : "";
  let spk = text && info.speakers ? await getStored("speakers", x.n) : "";
  let view = "all"; // all | spk

  const main = $("main");
  function render() {
    main.innerHTML = `
    <div class="detail">
      <h2>${pad3(x.n)}</h2>
      <div class="when">${when(x.when)} · ${len(x.sec) || "0초"}</div>
      ${DEMO ? "" : `<audio controls preload="none" src="/audio?n=${x.n}"></audio>`}
      ${text ? `<div class="actions">
        <button class="btn primary" id="copy">글 복사</button>
        <a class="btn" ${DEMO ? "" : `href="/txt?n=${x.n}"`} download="voicenote_${String(x.n).padStart(3, "0")}.txt">TXT 저장</a>
      </div>
      <div class="actions">
        <button class="btn" id="sumBtn">${summary ? "다시 요약" : "요약하기"}</button>
        ${info.speakers && !spk ? '<button class="btn" id="spkBtn">화자 나누기</button>' : ""}
      </div>
      <p class="note2" id="busy" style="display:none"></p>` : ""}
      ${summary ? `<div class="card"><h3>요약</h3><div class="sum">${esc(summary)}</div></div>` : ""}
      ${spk ? `<div class="tabs"><button class="tab ${view === "all" ? "on" : ""}" data-v="all">전체 글</button><button class="tab ${view === "spk" ? "on" : ""}" data-v="spk">화자별</button></div>` : ""}
      ${spk && view === "spk" ? `<div class="text">${speakersHtml(spk)}</div>`
        : `<div class="text ${text ? "" : "none"}">${text ? esc(text) : "아직 글로 바꾸지 않았어요.<br>기기에서 전송 모드를 켜면 바뀌어요."}</div>`}
      <div class="actions delrow"><button class="btn danger" id="del">이 녹음 삭제</button></div>
    </div>`;
    bind();
  }
  // 진행 안내. fail=true 면 실패 이유를 남겨 두고 버튼은 다시 누를 수 있게
  function busy(msg, fail) {
    const b = $("busy");
    b.style.display = msg ? "block" : "none";
    b.textContent = msg || "";
    b.style.color = fail ? "#c62f2f" : "";
    main.querySelectorAll("#sumBtn,#spkBtn").forEach((e) => e.disabled = !!msg && !fail);
  }
  function bind() {
    main.querySelectorAll(".tab").forEach((t) => t.onclick = () => { view = t.dataset.v; render(); });
    const c = $("copy");
    if (c) c.onclick = async () => {
      const what = spk && view === "spk" ? spk.replace(/^(\S+): /gm, "화자 $1: ") : text;
      try { await navigator.clipboard.writeText(what); toast("복사했어요"); }
      catch (e) { // http 주소에서는 복사 기능이 막힐 수 있어 예전 방식으로 한 번 더
        const ta = document.createElement("textarea");
        ta.value = what; document.body.appendChild(ta); ta.select();
        document.execCommand("copy"); ta.remove(); toast("복사했어요");
      }
    };
    const sb = $("sumBtn");
    if (sb) sb.onclick = async () => {
      busy("요약하는 중… 10초쯤 걸려요");
      if (DEMO) { await new Promise((r) => setTimeout(r, 800)); summary = demoSummary; return render(); }
      try {
        const r = await fetch("/api/summary?n=" + x.n, {method: "POST"});
        const t = await r.text();
        if (r.ok) { summary = t; render(); } else busy("요약 실패: " + (t || "이유 모름"), true);
      } catch (e) { busy("기기와 연결이 끊겼어요. 전송 모드가 켜져 있는지 봐 주세요", true); }
    };
    const kb = $("spkBtn");
    if (kb) kb.onclick = async () => {
      busy("1/2 녹음을 AssemblyAI에 올리는 중… (2분 녹음이면 1분 30초쯤)");
      if (DEMO) { await new Promise((r) => setTimeout(r, 800)); spk = demoSpeakers; view = "spk"; return render(); }
      // 첫 요청: 녹음을 올리고 작업 시작. 그다음부터 5초마다 끝났는지 확인
      const t0 = Date.now();
      for (let i = 0; i < 240; i++) {
        let j;
        try { j = await (await fetch("/api/speakers?n=" + x.n, {method: "POST"})).json(); }
        catch (e) { return busy("기기와 연결이 끊겼어요. 다시 누르면 이어서 확인해요", true); }
        if (j.state === "done") { spk = await getStored("speakers", x.n); view = "spk"; return render(); }
        if (j.state === "failed") return busy("화자 나누기 실패: " + (j.error || "이유 모름"), true);
        busy(`2/2 AssemblyAI가 화자를 나누는 중… (${Math.round((Date.now() - t0) / 1000)}초)`);
        await new Promise((r) => setTimeout(r, 5000));
      }
      busy("20분이 넘었어요. 잠시 뒤 다시 누르면 이어서 확인해요", true);
    };
    $("del").onclick = async () => {
      if (!confirm(`${pad3(x.n)} 녹음과 글을 지울까요?\n지우면 되돌릴 수 없어요.`)) return;
      if (await deleteNotes([x.n])) { toast("지웠어요"); history.back(); }
      else toast("지우지 못했어요");
    };
  }
  render();
  window.scrollTo(0, 0);
}

$("back").onclick = () => history.back();
$("selbtn").onclick = () => {
  if (!selecting) return setSelecting(true);
  // 선택 중엔 "모두" 버튼: 전부 고르기 / 전부 풀기
  if (picked.size === notes.length) picked.clear(); else notes.forEach((x) => picked.add(x.n));
  renderList();
};
$("cancelSel").onclick = () => setSelecting(false);
$("delSel").onclick = async () => {
  const nums = [...picked];
  if (!nums.length) return;
  if (!confirm(`녹음 ${nums.length}개와 글을 지울까요?\n지우면 되돌릴 수 없어요.`)) return;
  if (await deleteNotes(nums)) toast(`${nums.length}개 지웠어요`); else toast("일부를 지우지 못했어요");
  selecting = false;
  picked.clear();
  showList();
};
window.onpopstate = () => { selecting = false; picked.clear(); showList(); };
getInfo().then(showList);
</script>
</body>
</html>
)VN";
