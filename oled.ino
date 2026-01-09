//=====OLED INTEGRATED=====
// Deklarasi objek OLED dari icebath.ino
extern Adafruit_SSD1306 display;
// Di oled.ino, di bagian deklarasi variabel eksternal
extern bool breathingGuideActive;

// --- Deklarasi Variabel dari icebath.ino ---
extern OledState oledCurrentState;
extern unsigned long oledMainTimerMillis;
extern int oledRemainingMainTime;
extern bool oledMainTimerRunning;
extern unsigned long oledBreathingTimerMillis;
extern int oledCurrentStepCountdown;
extern unsigned long oledTransitionTimerMillis;
extern unsigned long oledStopTimerMillis;
extern bool oledShowHeartbeats;
extern int oledHeartbeatsX;
extern unsigned long oledHeartbeatDrawMillis;

// Variabel dari file lain
extern float currentTemp;
extern int count;
extern int initialCount;
extern bool isCounting;

// Di oled.ino, di bagian deklarasi variabel
bool isInhaleLabelPhase = true;
bool isHoldLabelPhase = true;
bool isExhaleLabelPhase = true;

// Konstanta waktu untuk panduan nafas
const int INHALE_TIME = 4;
const int HOLD_TIME = 3;
const int EXHALE_TIME = 4;
const int READY_DELAY = 2;
const int FINISHED_DELAY = 3;
const int STOP_SCREENSAVER_DELAY = 5;

// --- Ikon Play dan Stop (8x8 pixels) ---
const unsigned char PROGMEM playIcon8x8[] = {
  0x00, 0x7C, 0x7E, 0x7F, 0x7F, 0x7E, 0x7C, 0x00
};

const unsigned char PROGMEM stopIcon8x8[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// --- Variabel untuk Animasi Equalizer ---
int equalizerHeights[8];
unsigned long equalizerAnimMillis = 0;
const int ANIM_SPEED_MS = 100; // Kecepatan animasi dalam milidetik


// --- Fungsi untuk Menggambar Elemen-elemen UI ---

void drawTopBar() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Suhu di Pojok Kiri
  display.setCursor(0, 0);
  if (currentTemp > -99.0) {
    // PERUBAHAN: Tanpa desimal, dengan spasi, tanpa simbol derajat
    display.printf("%.0f C", currentTemp);
  } else {
    display.print("ERR C");
  }

  // Timer di Pojok Kanan dengan tambahan "SEC"
  String timerText = String(count) + " SEC";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w, 0);
  display.print(timerText);
}

void drawBottomBar() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // "HUMAN+" di Pojok Kanan Bawah
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("HUMAN+", 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w, SCREEN_HEIGHT - 8);
  display.print("HUMAN+");
}

/**
 * @brief Menggambar ikon (8x8) di bagian bawah tengah layar.
 * 
 * @param icon Pointer ke data ikon (PROGMEM).
 */
void drawBottomCenterIcon(const unsigned char *icon) {
  int iconSize = 8;
  // PERUBAHAN: Geser ikon 2 pixel ke kiri dari posisi tengah
  int x = (SCREEN_WIDTH - iconSize) / 2 - 1; 
  int y = SCREEN_HEIGHT - 8; // Sejajar dengan teks "HUMAN+"
  display.drawBitmap(x, y, icon, iconSize, iconSize, SSD1306_WHITE);
}
void drawCenterVisualization() {
  // Koordinat dan tinggi untuk kotak equalizer
  const int barWidth = 8;
  const int barSpacing = 15; 
  const int baseY = SCREEN_HEIGHT - 30; 
  const int numBars = 8;

  int totalWidth = numBars * barWidth + (numBars - 1) * (barSpacing - barWidth);
  int startX = (SCREEN_WIDTH - totalWidth) / 2;

  for (int i = 0; i < numBars; ++i) {
    int x = startX + i * barSpacing;
    int y = baseY - equalizerHeights[i];
    display.fillRect(x, y, barWidth, equalizerHeights[i], SSD1306_WHITE);
  }
}

// Fungsi utama untuk dipanggil dari loop() utama
void updateOLEDSystem() {
  // Jalankan mesin status OLED
  handleOledTimers();
  
  // Update tampilan OLED
  updateOledDisplay();
}


// Di oled.ino, GANTI SELURUH isi fungsi handleOledTimers()
void handleOledTimers() {
  // Sinkronisasi waktu utama dengan countdown dari sistem utama
  oledRemainingMainTime = count;
  
  // Cek apakah countdown utama sudah selesai
  if (oledRemainingMainTime <= 0 && oledMainTimerRunning) {
    oledCurrentState = OLED_FINISHED;
    oledTransitionTimerMillis = millis();
    oledMainTimerRunning = false;
    // Reset fase internal saat selesai
    isInhaleLabelPhase = true;
    isHoldLabelPhase = true;
    isExhaleLabelPhase = true;
    return;
  }
  
  switch (oledCurrentState) {
    case OLED_READY:
      if (millis() - oledTransitionTimerMillis >= (READY_DELAY * 1000)) {
        oledCurrentState = OLED_INHALE;
        isInhaleLabelPhase = true; // Mulai dari fase label untuk siklus baru
        oledTransitionTimerMillis = millis();
      }
      break;
      
    case OLED_INHALE:
      if (isInhaleLabelPhase) {
        // Tampilkan label "INHALE" selama 1 detik
        if (millis() - oledTransitionTimerMillis >= 1000) {
          isInhaleLabelPhase = false; // Beralih ke fase countdown
          oledCurrentStepCountdown = INHALE_TIME;
          oledBreathingTimerMillis = millis();
        }
      } else {
        // Tampilkan countdown
        if (millis() - oledBreathingTimerMillis >= 1000) {
          oledBreathingTimerMillis = millis();
          oledCurrentStepCountdown--;
          if (oledCurrentStepCountdown <= 0) {
            // Selesai inhale, pindah ke hold
            oledCurrentState = OLED_HOLD;
            isHoldLabelPhase = true;
            oledTransitionTimerMillis = millis();
          }
        }
      }
      break;
      
    case OLED_HOLD:
      if (isHoldLabelPhase) {
        if (millis() - oledTransitionTimerMillis >= 1000) {
          isHoldLabelPhase = false;
          oledCurrentStepCountdown = HOLD_TIME;
          oledBreathingTimerMillis = millis();
        }
      } else {
        if (millis() - oledBreathingTimerMillis >= 1000) {
          oledBreathingTimerMillis = millis();
          oledCurrentStepCountdown--;
          if (oledCurrentStepCountdown <= 0) {
            // Selesai hold, pindah ke exhale
            oledCurrentState = OLED_EXHALE;
            isExhaleLabelPhase = true;
            oledTransitionTimerMillis = millis();
          }
        }
      }
      break;
      
    case OLED_EXHALE:
      if (isExhaleLabelPhase) {
        if (millis() - oledTransitionTimerMillis >= 1000) {
          isExhaleLabelPhase = false;
          oledCurrentStepCountdown = EXHALE_TIME;
          oledBreathingTimerMillis = millis();
        }
      } else {
        if (millis() - oledBreathingTimerMillis >= 1000) {
          oledBreathingTimerMillis = millis();
          oledCurrentStepCountdown--;
          if (oledCurrentStepCountdown <= 0) {
            // Selesai exhale, loop kembali ke INHALE
            oledCurrentState = OLED_INHALE;
            isInhaleLabelPhase = true;
            oledTransitionTimerMillis = millis();
          }
        }
      }
      break;
      
    case OLED_STOP:
      // Tidak ada transisi otomatis, menunggu tombol ditekan lagi
      break;
      
    case OLED_FINISHED:
      if (millis() - oledTransitionTimerMillis >= (FINISHED_DELAY * 1000)) {
        oledCurrentState = OLED_IDLE;
      }
      break;
      
    default:
      // OLED_IDLE dan OLED_TIMER_ONLY tidak memiliki transisi otomatis
      break;
  }
}

// Di oled.ino, ganti seluruh fungsi updateOledDisplay() dengan ini
void updateOledDisplay() {
  display.clearDisplay();
  int16_t x1, y1;
  uint16_t w, h;

  // Gambar bar atas dan bawah untuk semua state
  drawTopBar();
  drawBottomBar();

  // Logika untuk animasi dan status equalizer
  // Animasi aktif untuk semua state kecuali IDLE, STOP, dan FINISHED
  if (oledCurrentState == OLED_IDLE || oledCurrentState == OLED_STOP || oledCurrentState == OLED_FINISHED) {
    // Equalizer statis saat IDLE, STOP, atau FINISHED
    const int staticHeights[] = {6, 12, 9, 15, 10, 8, 14, 11};
    for (int i = 0; i < 8; i++) {
      equalizerHeights[i] = staticHeights[i];
    }
  } else {
    // Animasikan equalizer untuk status lainnya (READY, INHALE, HOLD, EXHALE, TIMER_ONLY)
    if (millis() - equalizerAnimMillis >= ANIM_SPEED_MS) {
      equalizerAnimMillis = millis();
      for (int i = 0; i < 8; i++) {
        equalizerHeights[i] = random(4, 16); // Tinggi acak antara 4 dan 15
      }
    }
  }
  
  // Gambar visualisasi tengah
  drawCenterVisualization();

  // Tentukan posisi Y untuk teks status di bagian bawah
  int statusTextY = SCREEN_HEIGHT - 24; 
  String statusText = "";
  const unsigned char* iconToDraw = nullptr;

  // Tentukan teks dan ikon berdasarkan state yang baru
  switch (oledCurrentState) {
    case OLED_IDLE:
      statusText = "PRESS BUTTON";
      iconToDraw = playIcon8x8;
      break;
    
    case OLED_TIMER_ONLY:
      statusText = ""; // Tidak ada teks di tengah
      break;
    
    case OLED_READY:
      statusText = "READY";
      iconToDraw = stopIcon8x8;
      break;
    
    case OLED_INHALE:
      // Cek apakah sedang fase label atau countdown
      if (isInhaleLabelPhase) {
        statusText = "INHALE";
      } else {
        statusText = String(oledCurrentStepCountdown);
      }
      iconToDraw = stopIcon8x8;
      break;
      
    case OLED_HOLD:
      if (isHoldLabelPhase) {
        statusText = "HOLD";
      } else {
        statusText = String(oledCurrentStepCountdown);
      }
      iconToDraw = stopIcon8x8;
      break;
      
    case OLED_EXHALE:
      if (isExhaleLabelPhase) {
        statusText = "EXHALE";
      } else {
        statusText = String(oledCurrentStepCountdown);
      }
      iconToDraw = stopIcon8x8;
      break;
      
    case OLED_STOP:
      statusText = "STOP";
      iconToDraw = playIcon8x8;
      break;
      
    case OLED_FINISHED:
      statusText = "FINISHED";
      // Tidak ada ikon untuk state FINISHED
      break;
  }

  // Gambar teks status di bagian bawah
  if (statusText != "") {
    // Atur ukuran teks secara kondisional
    if (oledCurrentState == OLED_IDLE) {
      display.setTextSize(1); // Kecilkan hanya untuk "PRESS BUTTON"
    } else {
      display.setTextSize(2); // Ukuran lainnya tetap besar
    }

    // Hitung posisi agar teks berada di tengah
    display.getTextBounds(statusText, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, statusTextY);
    display.println(statusText);
  }
  
  // Gambar ikon di bagian bawah tengah jika ada
  if (iconToDraw != nullptr) {
    drawBottomCenterIcon(iconToDraw);
  }

  // Kirim semua buffer ke layar untuk ditampilkan
  display.display();
}