#!/bin/bash
# 와이파이 이름·비밀번호, API 키를 물어보고 firmware/voicenote/secrets.h 를 만든다.
# 입력한 비밀번호와 키는 화면에 안 보이고, secrets.h 는 git에 올라가지 않는다.
#
# 실행: ~/voicenote/tools/setup_secrets.sh
set -e
cd "$(dirname "$0")/../firmware/voicenote"

if [ -f secrets.h ]; then
  read -r -p "secrets.h 가 이미 있어요. 새로 만들까요? (y/n) " yn
  [ "$yn" = "y" ] || { echo "그대로 둘게요."; exit 0; }
fi

# C 문자열 안에 넣을 수 있게 \ 와 " 앞에 \ 를 붙인다
esc() { printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'; }

ask() { # $1=질문 $2=숨김여부(1이면 숨김)
  local v
  if [ "$2" = "1" ]; then
    read -r -s -p "$1: " v
    echo >&2
  else
    read -r -p "$1: " v
  fi
  printf '%s' "$v"
}

echo "== 1순위: 집 와이파이 =="
HOME_SSID=$(ask "와이파이 이름" 0)
HOME_PASS=$(ask "와이파이 비밀번호 (입력해도 안 보여요)" 1)
echo "== 2순위: 휴대폰 핫스팟 (없으면 그냥 엔터) =="
PHONE_SSID=$(ask "핫스팟 이름" 0)
PHONE_PASS=$(ask "핫스팟 비밀번호 (입력해도 안 보여요)" 1)
echo "== API 키 =="
OPENAI=$(ask "OpenAI API 키 (sk-로 시작, 안 보여요)" 1)
ASSEMBLY=$(ask "AssemblyAI API 키 (없으면 엔터, 안 보여요)" 1)

umask 077
cat > secrets.h <<EOF
// tools/setup_secrets.sh 로 만든 파일. git에 올라가지 않음. 절대 공개하지 말 것.
#pragma once

#define WIFI_HOME_SSID   "$(esc "$HOME_SSID")"
#define WIFI_HOME_PASS   "$(esc "$HOME_PASS")"
#define WIFI_PHONE_SSID  "$(esc "$PHONE_SSID")"
#define WIFI_PHONE_PASS  "$(esc "$PHONE_PASS")"

#define OPENAI_API_KEY      "$(esc "$OPENAI")"
#define ASSEMBLYAI_API_KEY  "$(esc "$ASSEMBLY")"
EOF

echo
echo "만들었어요: $(pwd)/secrets.h"
echo "- 집 와이파이: ${HOME_SSID:-(비어 있음)}"
echo "- 핫스팟: ${PHONE_SSID:-(비어 있음)}"
echo "- OpenAI 키: $([ -n "$OPENAI" ] && echo 입력됨 || echo 비어 있음)"
echo "- AssemblyAI 키: $([ -n "$ASSEMBLY" ] && echo 입력됨 || echo 비어 있음)"
