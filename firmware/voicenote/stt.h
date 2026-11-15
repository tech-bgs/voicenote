// 녹음 → 글자 (OpenAI Whisper). 파일을 메모리에 다 올리지 않고 SD에서 조금씩 읽어 보낸다.
// 10분이 넘는 녹음은 10분씩 나눠 보내고 글을 이어 붙인다 (Whisper 한 번에 25MB 제한).
#pragma once
#include <Arduino.h>

namespace stt {

// 진행 알림. done/total 은 보낸 바이트. false 를 돌려주면 그만둔다.
// 다 보내고 결과를 기다리는 동안에도 (total, total) 로 계속 불린다.
typedef bool (*Progress)(uint32_t done, uint32_t total);

enum class Result { Ok, Cancelled, Failed };

Result transcribe(const char *wavPath, const char *txtPath, Progress cb);

const char *lastError(); // 실패 이유 (로그용)

} // namespace stt
