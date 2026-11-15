"""web/index.html → firmware/voicenote/web_page.h (기기에 들어가는 웹페이지)

웹페이지를 고친 뒤 이걸 실행하고 다시 컴파일: python3 tools/make_web.py
"""

from pathlib import Path

root = Path(__file__).resolve().parent.parent
html = (root / "web" / "index.html").read_text(encoding="utf-8")
assert ")VN\"" not in html
out = root / "firmware" / "voicenote" / "web_page.h"
out.write_text(
    "// tools/make_web.py 가 web/index.html 로 만든 파일. 직접 고치지 말 것.\n"
    "#pragma once\n"
    f'static const char kIndexHtml[] = R"VN({html})VN";\n',
    encoding="utf-8",
)
print("saved", out, len(html.encode()), "bytes")
