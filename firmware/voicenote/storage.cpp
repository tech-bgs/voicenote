#include "storage.h"

#include <SD_MMC.h>

#include "pins.h"

namespace storage {

static const char *kDir = "/notes";
static const uint32_t kBytesPerSec = 16000 * 2; // 16kHz, 16비트, 모노
static const uint32_t kWavHeader = 44;

bool begin() {
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if (!SD_MMC.begin("/sdcard", true)) { // 1선 모드
    Serial.println("[sd] mount failed");
    return false;
  }
  if (!SD_MMC.exists(kDir)) SD_MMC.mkdir(kDir);
  Serial.printf("[sd] ok, %llu MB free\n", (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024));
  return true;
}

// "rec_013.wav" → 13, 아니면 -1
static int parseNumber(const char *name) {
  const char *base = strrchr(name, '/');
  base = base ? base + 1 : name;
  int n = -1;
  if (sscanf(base, "rec_%d.wav", &n) == 1 && strstr(base, ".wav") && !strstr(base, ".wav.")) return n;
  return -1;
}

static bool hasText(int number) {
  char p[48];
  snprintf(p, sizeof(p), "%s/rec_%03d.txt", kDir, number);
  if (SD_MMC.exists(p)) return true;
  snprintf(p, sizeof(p), "%s/rec_%03d.wav.txt", kDir, number);
  return SD_MMC.exists(p);
}

int list(Note *notes, int max) {
  File dir = SD_MMC.open(kDir);
  if (!dir) return 0;
  int count = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const int n = parseNumber(f.name());
    if (n < 0) continue;
    Note note;
    note.number = n;
    note.seconds = f.size() > kWavHeader ? (f.size() - kWavHeader) / kBytesPerSec : 0;
    note.when = f.getLastWrite();
    note.converted = false;
    if (count < max) {
      notes[count++] = note;
    } else {
      // 꽉 찼으면 가장 오래된(번호 작은) 것과 바꿔치기
      int minIdx = 0;
      for (int i = 1; i < count; i++)
        if (notes[i].number < notes[minIdx].number) minIdx = i;
      if (n > notes[minIdx].number) notes[minIdx] = note;
    }
  }
  dir.close();
  // 번호 큰(최신) 것이 앞으로
  for (int i = 0; i < count; i++)
    for (int j = i + 1; j < count; j++)
      if (notes[j].number > notes[i].number) std::swap(notes[i], notes[j]);
  for (int i = 0; i < count; i++) notes[i].converted = hasText(notes[i].number);
  return count;
}

int nextNumber() {
  int maxN = 0;
  File dir = SD_MMC.open(kDir);
  if (!dir) return 1;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const int n = parseNumber(f.name());
    if (n > maxN) maxN = n;
  }
  dir.close();
  return maxN + 1;
}

void wavPath(int number, char *out, size_t n) { snprintf(out, n, "%s/rec_%03d.wav", kDir, number); }

uint32_t freeMinutes() {
  const uint64_t freeBytes = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  return freeBytes / kBytesPerSec / 60;
}

} // namespace storage
