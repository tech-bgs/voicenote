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
.demo{font-size:12px;color:var(--warn);text-align:center;margin-top:20px}
</style>
</head>
<body>
<header>
  <button class="back" id="back" aria-label="목록으로">← 목록</button>
  <h1 id="title">VOICE NOTE</h1>
  <span class="count" id="count"></span>
</header>
<main id="main"></main>
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

async function showList() {
  history.replaceState(null, "", location.pathname + location.search);
  $("back").style.display = "none";
  $("title").textContent = "VOICE NOTE";
  const main = $("main");
  main.innerHTML = '<p class="empty">불러오는 중…</p>';
  let notes;
  try { notes = await getNotes(); } catch (e) { main.innerHTML = '<p class="empty">기기와 연결이 끊겼어요.<br>기기에서 전송 모드를 다시 켜 주세요.</p>'; return; }
  $("count").textContent = `녹음 ${notes.length}개`;
  if (!notes.length) { main.innerHTML = '<p class="empty">녹음이 없어요</p>'; return; }
  main.innerHTML = notes.map((x) => `
    <button class="note" data-n="${x.n}">
      <div class="top"><span class="num">${pad3(x.n)}</span><span class="date">${when(x.when)} · ${len(x.sec) || "0초"}</span>${x.conv ? "" : '<span class="badge wait">변환 전</span>'}</div>
      <div class="prev ${x.prev ? "" : "none"}">${x.prev ? esc(x.prev) : x.conv ? "(글이 비어 있어요)" : "아직 글로 바꾸지 않았어요"}</div>
    </button>`).join("") + (DEMO ? '<p class="demo">예시 화면 (기기 데이터 아님)</p>' : "");
  main.querySelectorAll(".note").forEach((b) => b.onclick = () => showDetail(notes.find((x) => x.n == b.dataset.n)));
}

async function showDetail(x) {
  history.pushState({n: x.n}, "", "#" + x.n);
  $("back").style.display = "block";
  $("title").textContent = "";
  $("count").textContent = "";
  const text = x.conv ? await getText(x.n) : "";
  $("main").innerHTML = `
    <div class="detail">
      <h2>${pad3(x.n)}</h2>
      <div class="when">${when(x.when)} · ${len(x.sec) || "0초"}</div>
      ${DEMO ? "" : `<audio controls preload="none" src="/audio?n=${x.n}"></audio>`}
      ${text ? `<div class="actions">
        <button class="btn primary" id="copy">글 복사</button>
        <a class="btn" ${DEMO ? "" : `href="/txt?n=${x.n}"`} download="voicenote_${String(x.n).padStart(3, "0")}.txt">TXT 저장</a>
      </div>` : ""}
      <div class="text ${text ? "" : "none"}">${text ? esc(text) : "아직 글로 바꾸지 않았어요.<br>기기에서 전송 모드를 켜면 바뀌어요."}</div>
    </div>`;
  const c = $("copy");
  if (c) c.onclick = async () => {
    try { await navigator.clipboard.writeText(text); toast("복사했어요"); }
    catch (e) { // http 주소에서는 복사 기능이 막힐 수 있어 예전 방식으로 한 번 더
      const ta = document.createElement("textarea");
      ta.value = text; document.body.appendChild(ta); ta.select();
      document.execCommand("copy"); ta.remove(); toast("복사했어요");
    }
  };
  window.scrollTo(0, 0);
}

$("back").onclick = () => history.back();
window.onpopstate = () => showList();
showList();
</script>
</body>
</html>
)VN";
