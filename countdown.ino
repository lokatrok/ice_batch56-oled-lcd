
//=====SETCOUNTDOWN=====

// Di countdown.ino, di bagian atas
extern bool breathingGuideActive;

// Di countdown.ino, di bagian atas (setelah extern declarations)
enum BuzzerState {
  BUZZER_OFF,
  BUZZER_ON
};

BuzzerState buzzerState = BUZZER_OFF;
unsigned long buzzerTimer = 0;
int beepCount = 0;
const int TOTAL_BEEPS = 3;
const unsigned long BEEP_DURATION = 500; // 500ms ON, 500ms OFF

// Di countdown.ino, di bagian paling atas
void updateOledDisplay(); // Prototype untuk fungsi dari oled.ino

// Di countdown.ino, GANTI SELURUH fungsi handleCountdownButton()
void handleCountdownButton() {
  static bool lastButtonState = HIGH;
  static bool stableButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  
  bool currentState = digitalRead(COUNTDOWN_BUTTON);
  
  // Logika debounce
  if (currentState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 20) { // Diperkecil untuk lebih sensitif
    if (currentState != stableButtonState) {
      stableButtonState = currentState;
      
      // Aksi saat tombol DITEKAN (LOW)
      if (stableButtonState == LOW) {
        
        // Jika nilai countdown belum diatur, tampilkan peringatan
        if (!valueSet) {
          Serial2.print("tCountTime.txt=\"Set Value\"");
          sendFF();
          return;
        }

        // --- LOGIKA YANG BENAR SESUAI KEINGINAN ANDA ---
        if (!isCounting) {
          // TEKAN PERTAMA: Mulai timer utama saja
          isCounting = true;
          //digitalWrite(COUNTDOWN_LED, HIGH);
          prevMillis = millis(); 
          
          // Set OLED ke status timer-only
          oledCurrentState = OLED_TIMER_ONLY;
          breathingGuideActive = false; // Pastikan panduan pernapasan mati
          
        } else {
          // TEKAN KEDUA, KETIGA, DAN SETERUSNYA: Toggle panduan pernapasan
          breathingGuideActive = !breathingGuideActive; 

          if (breathingGuideActive) {
            // Nyalakan panduan pernapasan, mulai dari READY
            oledCurrentState = OLED_READY;
            oledTransitionTimerMillis = millis();
            
            // !!! PERBAIKAN KRUSIAL: Paksa tampilan OLED untuk update SEKARANG juga !!!
            // Ini memastikan "READY" langsung muncul tanpa menunggu loop berikutnya
            updateOledDisplay(); 
            
          } else {
            // Matikan panduan pernapasan
            oledCurrentState = OLED_STOP;
            // Paksa update untuk STOP juga agar responsif
            updateOledDisplay();
          }
        }
      }
    }
  }
  
  lastButtonState = currentState;
}

// Di countdown.ino
void runCountdown() {
  // Fungsi ini hanya berjalan jika flag isCounting TRUE
  if (isCounting) {
    if (millis() - prevMillis >= COUNTDOWN_INTERVAL) {
      count--;
      
      // Update tampilan di Nextion
      Serial2.print("tCountTime.txt=\"");
      Serial2.print(count);
      Serial2.print("\"");
      sendFF();
      
      prevMillis = millis();
      
      // Cek apakah countdown sudah selesai
      if (count <= 0) {
        finishCountdown();
      }
    }
  }
}

// Di countdown.ino, GANTI fungsi finishCountdown()
void finishCountdown() {
  // Hentikan timer utama
  isCounting = false;
  //digitalWrite(COUNTDOWN_LED, LOW);

  // --- TAMBAHKAN INI UNTUK MEMATIKAN LED ---
  strip.clear();
  strip.show();
  
  // Set OLED ke status FINISHED
  oledCurrentState = OLED_FINISHED;
  oledTransitionTimerMillis = millis();
  
  // Reset flag panduan pernapasan
  breathingGuideActive = false;

  // Reset ke nilai awal
  count = initialCount;
  Serial2.print("tCountTime.txt=\"");
  Serial2.print(count);
  Serial2.print("\"");
  sendFF();

  // --- MULAI PROSES BUZZER TANPA MEMBLOKIR SISTEM ---
  beepCount = 0;
  buzzerState = BUZZER_ON;
  buzzerTimer = millis();
  digitalWrite(BUZZER_PIN, HIGH); // Nyalakan buzzer untuk beep pertama
}

// Di countdown.ino, tambahkan fungsi baru ini
void handleBuzzer() {
  if (buzzerState == BUZZER_OFF) {
    return; // Tidak melakukan apa-apa jika buzzer sedang off
  }

  if (millis() - buzzerTimer >= BEEP_DURATION) {
    if (buzzerState == BUZZER_ON) {
      // Waktu beep habis, matikan buzzer
      digitalWrite(BUZZER_PIN, LOW);
      buzzerState = BUZZER_OFF;
      beepCount++;
      
      // Cek apakah masih ada beep yang tersisa
      if (beepCount < TOTAL_BEEPS) {
        // Jika ada, atur timer untuk beep berikutnya
        buzzerTimer = millis();
        buzzerState = BUZZER_ON;
        digitalWrite(BUZZER_PIN, HIGH);
      }
    }
  }
}

// Di countdown.ino, tambahkan fungsi ini di bagian bawah
void updateBreathingLED() {
  // Gunakan gelombang sinus untuk transisi yang sangat halus
  // rumus: sin( (waktu / kecepatan) * 2 * PI )
  long waktu = millis();
  float sineValue = (sin(waktu / (float)BREATH_SPEED * 2.0 * PI) + 1.0) / 2.0;
  
  // Petakan nilai sinus (0.0 - 1.0) ke rentang kecerahan (BREATH_MIN - BREATH_MAX)
  int brightness = BREATH_MIN + (int)(sineValue * (BREATH_MAX - BREATH_MIN));
  
  // Atur warna (misalnya, warna putih hangat atau biru santai)
  // Anda bisa ganti R, G, B sesuai keinginan
  strip.setPixelColor(strip.numPixels(), strip.Color(brightness, brightness, brightness)); // Putih
  
  // Isi seluruh strip dengan warna dan kecerahan yang sudah dihitung
  strip.fill(strip.Color(brightness, brightness, brightness)); // Putih
  strip.show();
}