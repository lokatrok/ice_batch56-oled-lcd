// ====== COUNTDOWN SYSTEM (Optimized + Synchronized Breathing) ======

// --- Externs & Enums ---
extern bool breathingGuideActive;
extern OledState oledCurrentState;  // tambahkan untuk sinkronisasi LED dengan OLED

enum BuzzerState {
  BUZZER_OFF,
  BUZZER_ON,
  BUZZER_PAUSE
};

BuzzerState buzzerState = BUZZER_OFF;
unsigned long buzzerTimer = 0;
int beepCount = 0;

constexpr int TOTAL_BEEPS = 3;
constexpr unsigned long BEEP_DURATION = 500; // ms ON
constexpr unsigned long PAUSE_DURATION = 500; // ms OFF

// OLED
void updateOledDisplay(); // from oled.ino

// --- Countdown Button Handler ---
void handleCountdownButton() {
  static bool lastButtonState = HIGH;
  static bool stableButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;

  bool currentState = digitalRead(COUNTDOWN_BUTTON);

  // Debounce
  if (currentState != lastButtonState) lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > 30) {
    if (currentState != stableButtonState) {
      stableButtonState = currentState;

      // Saat ditekan (LOW)
      if (stableButtonState == LOW) {

        if (!valueSet) {
          Serial2.print("tCountTime.txt=\"Set Value\"");
          sendFF();
          return;
        }

        // === TEKANAN PERTAMA: MULAI COUNTDOWN + BREATHING ===
        if (!isCounting) {
          isCounting = true;
          prevMillis = millis();

          oledCurrentState = OLED_READY;           // langsung masuk panduan napas
          breathingGuideActive = true;             // aktifkan LED breathing
          oledTransitionTimerMillis = millis();

          strip.clear();
          strip.show();

          buzzerState = BUZZER_OFF;                // reset buzzer
          beepCount = 0;
        }

        // === TEKANAN KEDUA, KETIGA, DST: PAUSE / RESUME VISUAL ===
        else {
          // Toggle breathing animasi (tanpa hentikan countdown)
          breathingGuideActive = !breathingGuideActive;

          if (breathingGuideActive) {
            oledCurrentState = OLED_READY;         // lanjut animasi
            oledTransitionTimerMillis = millis();
          } else {
            oledCurrentState = OLED_STOP;          // berhenti visual
          }

          updateOledDisplay();                     // update OLED segera
        }
      }
    }
  }
  lastButtonState = currentState;
}

// --- Countdown Core ---
void runCountdown() {
  if (isCounting && millis() - prevMillis >= COUNTDOWN_INTERVAL) {
    if (count > 0) {
      count--;
      Serial2.print("tCountTime.txt=\"");
      Serial2.print(count);
      Serial2.print("\"");
      sendFF();
      prevMillis = millis();
    } else {
      finishCountdown();
    }
  }
}

// --- Countdown Finish Handler ---
void finishCountdown() {
  isCounting = false;

  // 🟢 Matikan efek breathing LED secara halus
  breathingGuideActive = false;   // hentikan animasi LED
  for (int fade = BREATH_MAX; fade >= BREATH_MIN; fade -= 5) {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(fade, fade, fade));
    }
    strip.show();
    delay(10);
  }
  strip.clear();
  strip.show();

  // 🧠 Update tampilan OLED seperti biasa
  oledCurrentState = OLED_FINISHED;
  oledTransitionTimerMillis = millis();

  // 🔁 Reset nilai dan tampilkan kembali di Nextion
  count = initialCount;
  Serial2.print("tCountTime.txt=\"");
  Serial2.print(count);
  Serial2.print("\"");
  sendFF();

  // 🔔 Jalankan urutan bunyi buzzer
  beepCount = 0;
  buzzerState = BUZZER_ON;
  buzzerTimer = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

// --- Buzzer Handler ---
void handleBuzzer() {
  if (buzzerState == BUZZER_OFF) return;

  unsigned long now = millis();

  if (buzzerState == BUZZER_ON && now - buzzerTimer >= BEEP_DURATION) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = BUZZER_PAUSE;
    buzzerTimer = now;
  } 
  else if (buzzerState == BUZZER_PAUSE && now - buzzerTimer >= PAUSE_DURATION) {
    beepCount++;
    if (beepCount < TOTAL_BEEPS) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerState = BUZZER_ON;
      buzzerTimer = now;
    } else {
      buzzerState = BUZZER_OFF;
    }
  }
}

// --- Breathing LED Update (Sinkron dengan OLED State) ---
void updateBreathingLED() {
  if (!breathingGuideActive || oledCurrentState == OLED_FINISHED || !isCounting) {
    // Matikan LED sepenuhnya jika countdown selesai atau pause
    strip.clear();
    strip.show();
    return;
  }

  int brightness = BREATH_MIN; // default

  switch (oledCurrentState) {
    case OLED_READY:
      brightness = map((millis() % 1000), 0, 1000, BREATH_MIN, BREATH_MAX);
      break;

    case OLED_INHALE:
      brightness = BREATH_MIN + (int)((sin((millis() / (float)BREATH_SPEED) * PI)) * (BREATH_MAX - BREATH_MIN));
      break;

    case OLED_HOLD:
      brightness = BREATH_MAX;
      break;

    case OLED_EXHALE:
      brightness = BREATH_MAX - (int)((sin((millis() / (float)BREATH_SPEED) * PI)) * (BREATH_MAX - BREATH_MIN));
      break;

    case OLED_STOP:
    case OLED_FINISHED:
    case OLED_IDLE:
      brightness = BREATH_MIN;
      break;

    default:
      break;
  }

for (int i = 0; i < strip.numPixels(); i++) {
  // Ganti putih menjadi biru
  strip.setPixelColor(i, strip.Color(0, 0, brightness));
}
strip.show();
}
