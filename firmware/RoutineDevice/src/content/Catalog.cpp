#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\content\\Catalog.cpp"
#include "Catalog.h"

namespace routine::content {
const char* foodName(uint8_t id) {
  // Figma 466:2033, ordered by the number printed in each catalogue frame.
  static const char* const names[] = {
    "사과","감","바나나","멜론","블루베리","핫도그","솜사탕","딸기 케이크",
    "체리","오렌지","레몬","키위","팬케이크","쿠키","복숭아","김밥",
    "석류","애플망고","파인애플","옥수수","크루아상","바닐라 아이스크림",
    "새우","무화과","용과","초밥","감자튀김","당고","빵","초코 아이스크림","버섯"
  };
  return id && id<=kFoodCount ? names[id-1] : "???";
}
const char* characterName(uint8_t id) {
  // IDs 0 and 1 must remain stable across existing saved progress.
  // Descriptive names for seal/blue robot await the designer's canonical names.
  static const char* const names[] = {"몽실이","또비","메에","저스티스","냐요미","맘뭉이","물범","파랑 로봇","북극곰","깡충이"};
  return id<kCharacterCount ? names[id] : "???";
}
uint8_t drawFood(uint32_t& state) {
  // Persist both result and generator state in the completion transaction.
  // Equal weighting is a replaceable content policy, not a confirmed probability table.
  if (!state) state=0x6d2b79f5;
  state ^= state<<13; state ^= state>>17; state ^= state<<5;
  return uint8_t(state%kFoodCount+1);
}
}
