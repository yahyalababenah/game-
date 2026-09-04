/*
  ============================================================
  Digital Tug of War  -  شد الحبل الرقمي
  Arduino Uno / Nano  ->  Serial  ->  صفحة ويب
  ============================================================
  التوصيل:
    Arcade Button 1 : Pin 2  -> GND   (INPUT_PULLUP)
    Arcade Button 2 : Pin 3  -> GND   (INPUT_PULLUP)
    Start Button    : Pin 4  -> GND   (INPUT_PULLUP)
    Buzzer          : Pin 8  -> GND
    LED  (اختياري)  : Pin 9  -> 220R -> GND
  ============================================================
  البروتوكول المُرسل للويب (سطر واحد كل 50ms):
    S:<state>,P:<pos>,A:<count1>,B:<count2>,T:<ms_left>
      state = 0 IDLE | 1 COUNTDOWN | 2 RUNNING | 3 FINISHED
      pos   = -100 (فوز اللاعب 1)  ..  +100 (فوز اللاعب 2)
  ============================================================
*/

// ---------- Pins ----------
const uint8_t PIN_P1     = 2;
const uint8_t PIN_P2     = 3;
const uint8_t PIN_START  = 4;
const uint8_t PIN_BUZZER = 8;
const uint8_t PIN_LED    = 9;

// ---------- Settings ----------
const int           WIN_POS      = 100;   // مسافة الفوز من المنتصف
const float         PULL_STEP    = 1.6;   // كم تتحرك العقدة مع كل ضغطة
const unsigned long ROUND_MS     = 20000; // اقصى مدة للجولة
const unsigned long DEBOUNCE_MS  = 18;    // مهم جدا مع الضغط السريع
const unsigned long SEND_EVERY   = 50;    // معدل الارسال للويب
const unsigned long COUNTDOWN_MS = 3000;
const unsigned long RESULT_MS    = 6000;

// ---------- State ----------
enum GameState { IDLE = 0, COUNTDOWN = 1, RUNNING = 2, FINISHED = 3 };
GameState state = IDLE;

float         pos = 0;            // موقع العقدة
unsigned int  count1 = 0, count2 = 0;
unsigned long stateStart = 0;
unsigned long lastSend   = 0;

// debounce لكل زر على حدة
unsigned long lastEdge1 = 0, lastEdge2 = 0;
bool prev1 = false, prev2 = false;

int   lastBeepZone = 0;
int   lastCountdownBeep = -1;

inline bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }

void setup() {
  pinMode(PIN_P1,    INPUT_PULLUP);
  pinMode(PIN_P2,    INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED,    OUTPUT);

  Serial.begin(115200);
  enterState(IDLE);
}

void loop() {
  // ---- اوامر قادمة من الويب (اختياري) ----
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'R' || c == 'r') startRound();
  }

  // ---- زر البداية ----
  if (pressed(PIN_START) && (state == IDLE || state == FINISHED)) {
    while (pressed(PIN_START)) { /* hold */ }
    delay(40);
    startRound();
  }

  switch (state) {

    case COUNTDOWN: {
      unsigned long elapsed = millis() - stateStart;
      int secLeft = 3 - (int)(elapsed / 1000);
      if (secLeft != lastCountdownBeep && secLeft >= 1) {
        lastCountdownBeep = secLeft;
        tone(PIN_BUZZER, 800, 100);
      }
      if (elapsed >= COUNTDOWN_MS) {
        tone(PIN_BUZZER, 1600, 250);
        digitalWrite(PIN_LED, HIGH);
        enterState(RUNNING);
      }
      break;
    }

    case RUNNING: {
      readPulls();

      if (pos <= -WIN_POS || pos >= WIN_POS) {
        finishRound();
      } else if (millis() - stateStart >= ROUND_MS) {
        finishRound();   // انتهى الوقت، الاقرب للفوز يكسب
      } else {
        beepOnZoneChange();
      }
      break;
    }

    case FINISHED:
      if (millis() - stateStart >= RESULT_MS) enterState(IDLE);
      break;

    case IDLE:
    default:
      break;
  }

  sendState();
}

// ============================================================
//  Game logic
// ============================================================

void startRound() {
  pos = 0;
  count1 = 0;
  count2 = 0;
  lastBeepZone = 0;
  lastCountdownBeep = -1;
  prev1 = pressed(PIN_P1);
  prev2 = pressed(PIN_P2);
  digitalWrite(PIN_LED, LOW);
  enterState(COUNTDOWN);
}

// قراءة الضغطات: نحسب الحافة (من مرفوع الى مضغوط) فقط
void readPulls() {
  unsigned long now = millis();

  bool now1 = pressed(PIN_P1);
  if (now1 && !prev1 && (now - lastEdge1) > DEBOUNCE_MS) {
    lastEdge1 = now;
    count1++;
    pos -= PULL_STEP;
  }
  prev1 = now1;

  bool now2 = pressed(PIN_P2);
  if (now2 && !prev2 && (now - lastEdge2) > DEBOUNCE_MS) {
    lastEdge2 = now;
    count2++;
    pos += PULL_STEP;
  }
  prev2 = now2;

  if (pos < -WIN_POS) pos = -WIN_POS;
  if (pos >  WIN_POS) pos =  WIN_POS;
}

void finishRound() {
  digitalWrite(PIN_LED, LOW);
  tone(PIN_BUZZER, 1900, 400);
  enterState(FINISHED);
}

// صفير خفيف كل ما دخلت العقدة منطقة جديدة  -> يرفع التوتر
void beepOnZoneChange() {
  int zone = (int)(pos / 33);
  if (zone != lastBeepZone) {
    lastBeepZone = zone;
    tone(PIN_BUZZER, 500 + abs(zone) * 300, 60);
  }
}

void enterState(GameState s) {
  state = s;
  stateStart = millis();
}

// ============================================================
//  Serial output
// ============================================================

void sendState() {
  if (millis() - lastSend < SEND_EVERY) return;
  lastSend = millis();

  long msLeft = 0;
  if (state == RUNNING) {
    long e = (long)(millis() - stateStart);
    msLeft = (long)ROUND_MS - e;
    if (msLeft < 0) msLeft = 0;
  } else if (state == COUNTDOWN) {
    long e = (long)(millis() - stateStart);
    msLeft = (long)COUNTDOWN_MS - e;
    if (msLeft < 0) msLeft = 0;
  }

  Serial.print(F("S:"));  Serial.print((int)state);
  Serial.print(F(",P:")); Serial.print((int)pos);
  Serial.print(F(",A:")); Serial.print(count1);
  Serial.print(F(",B:")); Serial.print(count2);
  Serial.print(F(",T:")); Serial.println(msLeft);
}
