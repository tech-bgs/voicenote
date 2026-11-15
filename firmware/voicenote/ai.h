// 글 요약 (OpenAI) · 화자 나누기 (AssemblyAI). 결과는 SD에 저장해 두고 다시 쓴다.
//   /notes/rec_013.sum.txt  요약
//   /notes/rec_013.spk.txt  화자별 글 ("A: 안녕하세요" 한 줄씩)
//   /notes/rec_013.job      화자 나누기 작업 번호 (끝나면 지움)
#pragma once
#include <Arduino.h>

namespace ai {

bool hasSpeakerKey(); // AssemblyAI 키를 넣었는지

void summaryPath(int n, char *out, size_t len);
void speakersPath(int n, char *out, size_t len);
void jobPath(int n, char *out, size_t len);

// 요약을 만들어 저장. 실패하면 false, err 에 이유
bool summarize(int n, String &err);

// 화자 나누기: 처음 부르면 녹음을 올리고 작업을 시작, 다음부터는 진행 상황 확인.
enum class Speakers { Working, Done, Failed };
// progress: 녹음 올리는 동안 (보낸 바이트, 전체) 로 불림. false 를 돌려주면 그만둠 (nullptr 가능)
typedef bool (*Progress)(uint32_t done, uint32_t total);
Speakers speakers(int n, String &err, Progress progress = nullptr);

} // namespace ai
