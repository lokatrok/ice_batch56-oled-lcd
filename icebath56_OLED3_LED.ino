// ======== LIBRARY & PIN SETUP icebatch56_oleed_led.ino ========
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>          
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions
#define TEMP_SENSOR_PIN     32
#define VALVE_DRAIN_PIN     33
#define VALVE_INLET_PIN     26
#define FLOAT_SENSOR_PIN    5
#define FLOW_SENSOR_PIN     14
#define TDS_SENSOR_PIN      34
#define COMPRESSOR_PIN      25
#define PUMP_UV_PIN         27
#define RTC_SDA_PIN         21
#define RTC_SCL_PIN         22
#define BUZZER_PIN          18
#define COUNTDOWN_BUTTON    23
//#define COUNTDOWN_LED       4
#define FLOW_SWITCH_PIN     35  // Pin untuk flow switch
#define OZONE_PIN           12
// --- Definisi untuk WS2812B ---
#define LED_PIN         4       // Pin yang sama dengan COUNTDOWN_LED
#define LED_COUNT       50      // Jumlah LED yang Anda punya, SESUAIKAN!
#define BRIGHTNESS      255     // Kecerahan maksimal (0-255)

// Parameter untuk efek breathing
#define BREATH_MIN      10      // Kecerahan minimum saat breathing
#define BREATH_MAX      BRIGHTNESS // Kecerahan maksimal saat breathing
#define BREATH_SPEED    3000    // Kecepatan breathing (dalam milidetik untuk satu siklus penuh)

// Constants
#define FLOAT_DEBOUNCE_DELAY   500  // 0.5 detik debounce untuk float sensor
#define GRACE_PERIOD           5000   // 5 detik grace period
#define TARGET_CHANGE_DEBOUNCE  3000   // 3 detik debounce untuk perubahan target
#define COUNTDOWN_INTERVAL     1000   // 1 detik untuk countdown
#define UPDATE_INTERVAL        1000   // Update interval untuk display
#define STATUS_PRINT_INTERVAL  2000   // Interval untuk print status ke serial
#define PRE_FILL_MINUTES       360    // 6 jam dalam menit
#define MAX_CIRCULATION_TIME   10800000 // 3 jam dalam milidetik

// EEPROM Address
#define EEPROM_ADDR 0
#define EEPROM_MANUAL_ADDR sizeof(ScheduleData) // Address untuk menyimpan data manual

// Definisi konstanta layar OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Di icebath.ino, di bagian deklarasi objek global
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Structure for EEPROM data
struct ScheduleData {
  uint32_t lastChangeTimestamp;  // Timestamp pergantian air terakhir
  uint32_t lastPreFillTimestamp; // Timestamp pre-fill terakhir
  uint8_t autoChangeDay;         // Setting hari ganti air
  bool preFillExecuted;          // Status pre-fill
  
  // Parameter auto mode
  uint8_t autoTempTarget;        // Target suhu auto
  uint8_t autoHour1;             // Jam digit 1
  uint8_t autoHour2;             // Jam digit 2
  uint8_t autoMin1;              // Menit digit 1
  uint8_t autoMin2;              // Menit digit 2

  // Tambahkan checksum untuk validasi data
  uint16_t checksum;             // Checksum untuk validasi integritas data
};

// Structure for Manual Mode EEPROM data
struct ManualData {
  uint8_t manualTempTarget;      // Target suhu manual
};

// Global objects
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
RTC_DS3231 rtc;

// Sensor variables
float currentTemp = 0.0;
float tdsValue = 0.0;
float currentFlowRate = 0.0; 
DeviceAddress tempDeviceAddress;
volatile unsigned long pulseCount = 0;

// Control variables
bool fillingActive = false;
bool drainingActive = false;
bool sistemAktif = false;
bool statusKompresor = false;
int suhuTarget = 29;
int activeProcess = 0;
bool debug = true;

// Auto mode variables
bool autoMode = false;
bool manualCirculationOn = false;
bool manualCirculationOff = false;
int autoHour1 = 0;
int autoHour2 = 0;
int autoMin1 = 0;
int autoMin2 = 0;
int autoTempTarget = 3;
int autoChangeDay = 7;
unsigned long lastAutoCheck = 0;
unsigned long autoCheckInterval = 60000; // Cek setiap menit
DateTime lastChangeDate; // Tanggal pergantian air terakhir
bool waterChangeScheduled = false; // Flag untuk proses water change
bool preFillScheduled = false;
bool preFillExecuted = false;
DateTime lastPreFillDate;
unsigned long autoCirculationStartTime = 0;
// Constants for auto mode
const float TEMP_HYSTERESIS = 3.0; // 3 degree

// Filling variables
bool fillingStarted = false;
bool fillingStoppedBySensor = false;
unsigned long drainStartTime = 0;
unsigned long lastFillingDebug = 0;
bool lastFloatState = LOW;
unsigned long lastFloatTriggerTime = 0;
bool floatSensorStable = false;
int fillingStage = 0; // 0=idle, 1=draining, 2=filling
bool fillingError = false; // Flag untuk error filling

// Temperature control variables
const int histeresis = 3;
bool initialCoolingMode = false;
bool targetReached = false;
unsigned long waktuTerakhirCek = 0;
unsigned long intervalCek = 3000;

// Tambahkan variabel-variabel ini untuk stabilisasi suhu
unsigned long lastCompressorChange = 0;  // Waktu terakhir kompresor berubah status
const unsigned long COMPRESSOR_STABILIZE_TIME = 30000;  // 30 detik stabilisasi
float lastStableTemp = 0.0;  // Suhu terakhir yang stabil
unsigned long lastTempStableTime = 0;  // Waktu terakhir suhu stabil
const float TEMP_STABILITY_THRESHOLD = 0.5;  // Threshold untuk suhu stabil
const unsigned long TEMP_STABILITY_TIME = 10000;  // 10 detik untuk menentukan suhu stabil

// Draining variables
bool graceActive = false;
unsigned long graceStart = 0;
bool flowOK = false;
unsigned long lastFlowCheckTime = 0;
int lowFlowCount = 0;

// Timing variables
unsigned long lastSensorUpdate = 0;
unsigned long intervalSensorUpdate = 2000;
unsigned long lastStatusPrint = 0; // Untuk print status ke serial

// Nextion communication variables
String cmd = "";
int ffCount = 0;
unsigned long lastNextionRead = 0;
unsigned long intervalNextionRead = 25;
int cachedActiveProcess = -1;
int cachedSuhuTarget = -1;
int lastValidTarget = 29;
unsigned long lastTargetChange = 0;
bool receivingAutoParams = false;
int paramIndex = 0;
String autoParams[6]; // Untuk menyimpan jam, menit, suhu, hari

// Duration tracking variables
unsigned long coolingStartTime = 0;
bool coolingActive = false;
unsigned long lastUpdateTime = 0;

// Tambahkan variabel ini untuk durasi pendinginan manual
int manualCoolingDuration = 30; // Default durasi 30 menit

// Countdown variables
int count = 0;
int initialCount = 0;
bool isCounting = false;
bool valueSet = false;
unsigned long prevMillis = 0;

// RTC variables
bool rtcInitialized = false;

// Flow Sensor FS300A specific constants
const float FS300A_CALIBRATION = 480.0; // Pulses per liter untuk FS300A
const int FLOW_MAX_RATE = 60;          // Maksimal 60 L/min (sesuai spesifikasi FS300A)
const int FLOW_SAMPLES = 5;          // Jumlah sampel untuk filter
float flowReadings[FLOW_SAMPLES];    // Array untuk menyimpan sampel flow rate
int flowIndex = 0;                   // Indeks untuk array
unsigned long lastInterruptTime = 0; // Untuk debounce interrupt
const int DEBOUNCE_TIME = 2;         // 2ms debounce time untuk YF-B5

// Variabel untuk tracking flow rate
unsigned long lastPulseCount = 0;
unsigned long lastPulseCountAuto = 0;
unsigned long lastFlowCalcTime = 0;
unsigned long lastFlowCalcTimeAuto = 0;

// PROSES CIRCULATION
bool circulationActive = false;
bool circulationStarted = false;
int circulationStage = 0; // 0=idle, 1=draining, 2=filling, 3=cooling
bool circulationErrorActive = false;
unsigned long circulationStartTime = 0;
bool initialCirculationMode = false;

// Variabel untuk prefill otomatis
bool prefillActive = false;
bool prefillStarted = false;
int prefillStage = 0; // 0=idle, 1=draining, 2=filling
bool prefillErrorActive = false;

// Tambahkan jika belum ada
bool waterChangeActive = false;
int waterChangeStage = 0;

// Variabel global untuk menyimpan state antar pemanggilan fungsi
unsigned long graceStartGlobal = 0;
unsigned long lastFlowCalcTimeGlobal = 0;
unsigned long fullDetectedTimeGlobal = 0;
unsigned long fillingStartTimeGlobal = 0;
unsigned long drainStartTimeGlobal = 0;
unsigned long pulseCountGlobal = 0;
unsigned long lastPulseCountGlobal = 0;
int lowFlowCountGlobal = 0;

// Variabel untuk mode eksklusif
bool modeActive = false; // Flag untuk menandakan mode sedang aktif (manual atau auto)

// Flag untuk mengontrol tombol berdasarkan kondisi sistem
bool fillingErrorActive = false;   // Filling dalam keadaan error

// Untuk menyimpan state circulation sebelum error
int circulationStateBeforeError = 0; // 0=OFF, 1=ON

// Untuk melacak intervensi user saat error circulation
bool userIntervenedDuringCirculationError = false;

// Variabel untuk melacak bagaimana sirkulasi dimulai
bool circulationStartedAutomatically = false;

// Circulation status tracking
int circulationStatus = 0; // 0=OFF, 1=ON, 2=ERROR, 3=WAITING FOR PRE-FILL

// Tambahkan di bagian variabel global
bool firstBoot = true; // Flag untuk menandai boot pertama kali

// Di icebath.ino, di bagian variabel global
unsigned long autoOffCooldownTime = 0;

// Variabel untuk kontrol Ozon
bool ozoneActive = false;
unsigned long ozoneStartTime = 0;
const unsigned long OZONE_DURATION = 900000; // 15 menit dalam milidetik (15 * 60 * 1000)
// Di icebath.ino, di bagian variabel global (misalnya di bawah variabel ozone)
bool ozoneInitializedForThisCycle = false;

// Bypass System Variables - CUMA 4 VARIBEL INI!
bool bypassInlet = false;
bool bypassDrain = false;
bool bypassPumpUV = false;
bool bypassCompre = false;
bool bypassOzone = false;

// Di icebin.ino, di bagian variabel global
bool circulationInitialized = false;

// Di icebath.ino, di bagian variabel global (misalnya di bawah variabel ozone)
bool breathingGuideActive = false; // Flag untuk mengontrol panduan pernapasan

// --- VARIABEL UNTUK OLED (TAMBAHKAN BLOK INI)
enum OledState {
  OLED_IDLE,
  OLED_TIMER_ONLY,
  OLED_READY,
  OLED_INHALE,       // Mengganti INHALE_LABEL dan INHALE_COUNTDOWN
  OLED_HOLD,         // Mengganti HOLD_LABEL dan HOLD_COUNTDOWN
  OLED_EXHALE,       // Mengganti EXHALE_LABEL dan EXHALE_COUNTDOWN
  OLED_STOP,
  OLED_FINISHED
};

OledState oledCurrentState = OLED_IDLE;
unsigned long oledMainTimerMillis = 0;
int oledRemainingMainTime = 0;
bool oledMainTimerRunning = false;
unsigned long oledBreathingTimerMillis = 0;
int oledCurrentStepCountdown = 0;
unsigned long oledTransitionTimerMillis = 0;
unsigned long oledStopTimerMillis = 0;
bool oledShowHeartbeats = false;
int oledHeartbeatsX = 0;
unsigned long oledHeartbeatDrawMillis = 0;

// Function declarations untuk mode auto
void AUTOSCHEDULE();
void STARTWATERCHANGE();
void STARTPREFILL();
void STARTCIRCULATION();
void STOPPREFILLCIRCULATION();
void CIRCULATIONSAFETY();
bool isWaterChangeDay();
bool preFillExecutedToday(DateTime now);
int getMinutesUntilNextSchedule(int targetHour, int targetMinute);
void setupRTC();
void completeWaterChange();

// Function declarations untuk mode manual
void STARTFILLING();
void STOPFILLINGBYSENSOR();
void STOPFILLINGBYMANUAL();
void STARTDRAINING();
void STOPDRAININGBYSENSOR();
void STOPDRAININGBYMANUAL();
void STARTCOOLING();
void STOPCOOLING();

// Function declarations untuk sensor
void setupSensors();
void readSensors();
void testSensors();
float getAverageFlowRate(float newReading);
void updateFlowRateToNextion(float flowRate);
void IRAM_ATTR flowISR();

// Function declarations untuk Nextion
void handleNextionCommunication();
void readNextion();
void processNextionCommand(String command);
void sendFF();
void updateNextionDisplay();
void updateNextionClock();
void sendToNextion(String cmd);
int bacaKomponenWaktu(String namaKomponen, int minVal, int maxVal);
String bacaTeksDariNextion(String namaKomponen);
void bacaParameterDariNextion();
void processAutoParameters();
int bacaStatusDariNextion();
int bacaTargetDariNextion();
void readAutoParametersBeforeActivation();

// Function declarations untuk countdown
void handleCountdownButton();
//void startCountdown();
void runCountdown();
void finishCountdown();

// Function declarations untuk utilitas
void printSystemStatus();
void handleDateSetting();
void loadScheduleData();
void saveScheduleData();
void loadManualData();
void saveManualData();
int getDaysSinceLastChange();
bool isTimeForWaterChange();
void updateNextionStatus(String statusText, int picValue);
void checkErrorRecovery();
void restoreSettingsToNextion();
void updateNDaysOnAllPages(int days); // Tambahkan ini

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // --- INISIALISASI OLED (TAMBAHKAN BLOK INI) ---
  // Initialize I2C for OLED (gunakan pin 21/22 yang sama dengan RTC)
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Alamat I2C biasanya 0x3C
    Serial.println(F("SSD1306 allocation failed"));
    // Hentikan program jika OLED tidak ditemukan
    for(;;);
  }
  display.clearDisplay();
  display.display();

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show(); // Bersihkan tampilan LED
  
  // Initialize pins
  pinMode(COUNTDOWN_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  //pinMode(COUNTDOWN_LED, OUTPUT);
  pinMode(FLOW_SWITCH_PIN, INPUT_PULLUP);
  pinMode(FLOAT_SENSOR_PIN, INPUT_PULLUP);
  pinMode(OZONE_PIN, OUTPUT);
  digitalWrite(OZONE_PIN, LOW); // Pastikan ozon mati saat startup
  digitalWrite(BUZZER_PIN, LOW);
  //digitalWrite(COUNTDOWN_LED, LOW);
  
  // Initialize countdown system
  count = 0;
  initialCount = 0;
  valueSet = false;
  isCounting = false;
  prevMillis = 0;
  
  // Kirim nilai awal ke Nextion
  Serial2.print("tCountTime.txt=\"0\"");
  sendFF();
  
  // Initialize filling states
  fillingActive = false;
  fillingStarted = false;
  fillingStage = 0;
  lastFloatState = LOW;
  lastFloatTriggerTime = 0;
  floatSensorStable = false;
  fillingError = false;
  
  // Initialize error flags
  fillingErrorActive = false;
  
  // Initialize coolduration state
  coolingActive = false;
  lastUpdateTime = 0;
  
  // Setup sensors and RTC
  setupSensors();
  setupRTC();
  
  // Initialize EEPROM
  EEPROM.begin(512);
  loadScheduleData();
  loadManualData();
  
  // Check RTC validity
  DateTime now = rtc.now();
  if (debug) {
    Serial.print(">> RTC Time: ");
    Serial.print(now.hour());
    Serial.print(":");
    if (now.minute() < 10) Serial.print("0");
    Serial.println(now.minute());
  }
  
  if (now.year() >= 2023) {
    rtcInitialized = true;
    if (debug) Serial.println("RTC has valid time");
  } else {
    rtcInitialized = false;
    if (debug) Serial.println("RTC time invalid");
  }
  
  // Tunggu Nextion siap
  delay(1000);

  // Restore settings to Nextion after power on
  restoreSettingsToNextion();

  // Kirim perintah tambahan untuk memastikan Nextion menampilkan data dengan benar
  Serial2.print("automenu.nHourAuto1.val=");
  Serial2.print(autoHour1);
  sendFF();
  
  Serial2.print("automenu.nHourAuto2.val=");
  Serial2.print(autoHour2);
  sendFF();
  
  Serial2.print("automenu.nMinAuto1.val=");
  Serial2.print(autoMin1);
  sendFF();
  
  Serial2.print("automenu.nMinAuto2.val=");
  Serial2.print(autoMin2);
  sendFF();
  
  // Send initial days since last change
  int initialDays = getDaysSinceLastChange();
  updateNDaysOnAllPages(initialDays);
  
  if (debug) {
    Serial.print(">> Initial days since last change: ");
    Serial.println(initialDays);
  }
  
  // Test hardware
  Serial.println("=== TESTING HARDWARE ===");
  testSensors();
  Serial.println("=== TESTING COMPLETE ===");
  
  if (debug) {
    Serial.println("=== SISTEM KONTROL TANK TERINTEGRASI ===");
    Serial.println("Sistem siap...");
    Serial.println("Kirim 'setdate' untuk mengatur tanggal");
    Serial.println("Format: SETDATE=DD/MM/YYYY");
  }
  
  // Send initial message to Nextion
  sendToNextion("tCountTime.txt=\"0\"");
  
  if (debug) {
    Serial.println("=====================================");
    Serial.println("  Countdown Buzzer System Ready");
    Serial.println("=====================================");
    Serial.println("Waiting for countdown value from Nextion...");
    Serial.println("=====================================");
  }
  // Di akhir fungsi setup()
  firstBoot = true; // Set flag first boot
  
  // --- TAMBAHKAN INISIALISASI LED STRIP DI AKHIR SETUP ---
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear(); // Matikan semua LED saat startup
  strip.show();
  
  if (debug) {
    Serial.println(">> WS2812B LED Strip Initialized");
  }
}


void loop() {
  // Prioritas UTAMA: komunikasi Nextion dan tombol fisik
  handleNextionCommunication();
  handleCountdownButton();
  
  // Prioritas KEDUA: Jalankan proses yang sedang aktif
  // Gunakan else if untuk memastikan hanya SATU proses auto yang berjalan
  if (autoMode) {
    if (waterChangeActive) {
      STARTWATERCHANGE();
    } else if (prefillActive) {
      STARTPREFILL();
    } else if (circulationActive) {
      STARTCIRCULATION();
      CIRCULATIONSAFETY();
    }
  } else {
    // Mode Manual: proses bisa berjalan bersamaan, tapi sudah ada cek konflik di masing-masing fungsi
    if (fillingActive) STARTFILLING();
    if (drainingActive) STARTDRAINING();
    if (activeProcess == 1) STARTCOOLING();
  }
  
  // Prioritas KETIGA: Tugas-tugas berkala (non-blocking)
  // Gunakan millis() agar tidak menghentikan seluruh program
  
  // Proses ringan setiap 50ms
  static unsigned long lastProcessTime = 0;
  if (millis() - lastProcessTime >= 50) {
    handleDateSetting();
    validateAndFixSystemStatus();
    //updateBypassActuators();
    lastProcessTime = millis();
  }
  
  // Baca sensor setiap 500ms
  static unsigned long lastSensorTime = 0;
  if (millis() - lastSensorTime >= 500) {
    readSensors();
    lastSensorTime = millis();
  }
  
  // Update display setiap 200ms
  static unsigned long lastDisplayTime = 0;
  if (millis() - lastDisplayTime >= 200) {
    updateNextionDisplay();
    lastDisplayTime = millis();
  }

  // --- UPDATE OLED (TAMBAHKAN INI) ---
  updateOLEDSystem();

  handleBuzzer();
  
  // Jalankan countdown jika aktif
  if (isCounting) {
    runCountdown();
    updateBreathingLED();
  }

  // Cek jadwal otomatis setiap 30 detik
  static unsigned long lastScheduleCheck = 0;
  if (millis() - lastScheduleCheck >= 30000) {
    if (autoMode) {
      AUTOSCHEDULE();
    }
    lastScheduleCheck = millis();
  }

  // Update jam di Nextion setiap 1 detik
  static unsigned long lastClockUpdate = 0;
  if (millis() - lastClockUpdate >= 1000) {
    updateNextionClock();
    lastClockUpdate = millis();
  }

  
}

// Save data to EEPROM
void saveScheduleData() {
  ScheduleData data;
  data.lastChangeTimestamp = lastChangeDate.unixtime();
  data.lastPreFillTimestamp = lastPreFillDate.unixtime();
  data.autoChangeDay = autoChangeDay;
  data.preFillExecuted = preFillExecuted;
  
  // Parameter auto mode - pastikan semua tersimpan
  data.autoTempTarget = autoTempTarget;
  data.autoHour1 = autoHour1;
  data.autoHour2 = autoHour2;
  data.autoMin1 = autoMin1;
  data.autoMin2 = autoMin2;
  
  // Hitung checksum sederhana
  data.checksum = data.autoHour1 + data.autoHour2 + data.autoMin1 + data.autoMin2 + 
                   data.autoTempTarget + data.autoChangeDay;
  
  EEPROM.put(EEPROM_ADDR, data);
  if (EEPROM.commit()) {
    if (debug) {
      Serial.println(">> Schedule data saved to EEPROM");
      Serial.print("  - Temperature: ");
      Serial.print(data.autoTempTarget);
      Serial.println("°C");
      Serial.print("  - Time: ");
      Serial.print(data.autoHour1);
      Serial.print(data.autoHour2);
      Serial.print(":");
      Serial.print(data.autoMin1);
      Serial.print(data.autoMin2);
      Serial.println();
      Serial.print("  - Change Day: ");
      Serial.print(data.autoChangeDay);
      Serial.println(" days");
      Serial.print("  - Checksum: ");
      Serial.println(data.checksum);
    }
  } else {
    if (debug) Serial.println(">> ERROR: Failed to save data to EEPROM");
  }
}

// Load data from EEPROM
void loadScheduleData() {
  ScheduleData data;
  EEPROM.get(EEPROM_ADDR, data);
  
  // Debug - show raw values from EEPROM
  if (debug) {
    Serial.println(">> Reading EEPROM data:");
    Serial.print("  autoChangeDay: ");
    Serial.println(data.autoChangeDay);
    Serial.print("  autoTempTarget: ");
    Serial.println(data.autoTempTarget);
    Serial.print("  autoHour1: ");
    Serial.println(data.autoHour1);
    Serial.print("  autoHour2: ");
    Serial.println(data.autoHour2);
    Serial.print("  autoMin1: ");
    Serial.println(data.autoMin1);
    Serial.print("  autoMin2: ");
    Serial.println(data.autoMin2);
    Serial.print("  checksum: ");
    Serial.println(data.checksum);
  }
  
  // Hitung checksum untuk validasi
  uint16_t calculatedChecksum = data.autoHour1 + data.autoHour2 + data.autoMin1 + data.autoMin2 + 
                                  data.autoTempTarget + data.autoChangeDay;
  
  // Check if data is valid - gunakan validasi yang lebih longgar
  bool dataValid = true;
  
  // Validasi checksum
  if (data.checksum != calculatedChecksum) {
    dataValid = false;
    if (debug) {
      Serial.print(">> Checksum mismatch! Expected: ");
      Serial.print(calculatedChecksum);
      Serial.print(" Got: ");
      Serial.println(data.checksum);
    }
  }
  
  // Validasi dasar - pastikan nilai tidak kosong
  if (data.autoChangeDay == 0 || data.autoChangeDay > 365) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoChangeDay in EEPROM");
  }
  
  if (data.autoTempTarget == 0 || data.autoTempTarget > 50) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoTempTarget in EEPROM");
  }
  
  // Validasi jam - pastikan jam valid
  if (data.autoHour1 < 0 || data.autoHour1 > 2) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoHour1 in EEPROM");
  }
  
  if (data.autoHour2 < 0 || data.autoHour2 > 9) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoHour2 in EEPROM");
  }
  
  if (data.autoMin1 < 0 || data.autoMin1 > 5) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoMin1 in EEPROM");
  }
  
  if (data.autoMin2 < 0 || data.autoMin2 > 9) {
    dataValid = false;
    if (debug) Serial.println(">> Invalid autoMin2 in EEPROM");
  }
  
  if (dataValid) {
    // Data valid, gunakan nilai dari EEPROM
    
    // Validate timestamp
    if (data.lastChangeTimestamp > 0) {
      lastChangeDate = DateTime(data.lastChangeTimestamp);
      
      // Check if date is reasonable
      DateTime now = rtc.now();
      if (abs((now - lastChangeDate).days()) > 3650) { // More than 10 years difference
        if (debug) Serial.println(">> Invalid date in EEPROM, using current date");
        lastChangeDate = DateTime(now.year(), now.month(), now.day());
      }
    } else {
      lastChangeDate = DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day());
    }
    
    if (data.lastPreFillTimestamp > 0) {
      lastPreFillDate = DateTime(data.lastPreFillTimestamp);
    } else {
      lastPreFillDate = DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day());
    }
    
    // Load parameter auto mode tanpa validasi berlebihan
    autoChangeDay = data.autoChangeDay;
    preFillExecuted = data.preFillExecuted;
    autoTempTarget = data.autoTempTarget;
    autoHour1 = data.autoHour1;
    autoHour2 = data.autoHour2;
    autoMin1 = data.autoMin1;
    autoMin2 = data.autoMin2;
    
    if (debug) {
      Serial.println(">> Schedule data loaded from EEPROM");
      Serial.print("  - Temperature: ");
      Serial.print(autoTempTarget);
      Serial.println("°C");
      Serial.print("  - Time: ");
      Serial.print(autoHour1);
      Serial.print(autoHour2);
      Serial.print(":");
      Serial.print(autoMin1);
      Serial.print(autoMin2);
      Serial.println();
      Serial.print("  - Change Day: ");
      Serial.print(autoChangeDay);
      Serial.println(" days");
      Serial.print("  - Last Change: ");
      Serial.print(lastChangeDate.day());
      Serial.print("/");
      Serial.print(lastChangeDate.month());
      Serial.print("/");
      Serial.println(lastChangeDate.year());
    }
  } else {
    // Data tidak valid, gunakan default dan simpan ke EEPROM
    DateTime now = rtc.now();
    lastChangeDate = DateTime(now.year(), now.month(), now.day());
    lastPreFillDate = DateTime(now.year(), now.month(), now.day());
    autoChangeDay = 7;
    preFillExecuted = false;
    autoTempTarget = 3;
    autoHour1 = 0;
    autoHour2 = 1;
    autoMin1 = 0;
    autoMin2 = 0;
    
    // Simpan default ke EEPROM
    saveScheduleData();
    
    if (debug) {
      Serial.println(">> Invalid EEPROM data, using defaults and saving to EEPROM");
    }
  }
}

// Save manual data to EEPROM
void saveManualData() {
  ManualData data;
  data.manualTempTarget = suhuTarget;
  
  EEPROM.put(EEPROM_MANUAL_ADDR, data);
  if (EEPROM.commit()) {
    if (debug) {
      Serial.println(">> Manual data saved to EEPROM");
      Serial.print("  - Temperature: ");
      Serial.print(data.manualTempTarget);
      Serial.println("°C");
    }
  } else {
    if (debug) Serial.println(">> ERROR: Failed to save manual data to EEPROM");
  }
}

// Load manual data from EEPROM
void loadManualData() {
  ManualData data;
  EEPROM.get(EEPROM_MANUAL_ADDR, data);
  
  // Check if data is valid
  if (data.manualTempTarget >= 1 && data.manualTempTarget <= 70) {
    suhuTarget = data.manualTempTarget;
    cachedSuhuTarget = suhuTarget;
    lastValidTarget = suhuTarget;
    
    if (debug) {
      Serial.println("Manual data loaded from EEPROM");
      Serial.print("  - Temperature: ");
      Serial.print(suhuTarget);
      Serial.println("°C");
    }
  } else {
    // If data is invalid, set to default
    suhuTarget = 29;
    cachedSuhuTarget = suhuTarget;
    lastValidTarget = suhuTarget;
    
    if (debug) {
      Serial.println("Invalid manual EEPROM data, using default");
    }
  }
}

// Restore settings to Nextion after power on - PERBAIKAN
void restoreSettingsToNextion() {
  if (debug) {
    Serial.println(">> Restoring settings to Nextion...");
  }
  
  // Tunggu Nextion siap menerima perintah
  delay(1000);
  
  // Restore Auto Mode Settings
  // Temperature setting - HANYA kirim angka saja
  Serial2.print("automenu.tSetTempAu.txt=\"");
  Serial2.print(autoTempTarget);
  Serial2.print("\"");
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  // Day setting - HANYA kirim angka saja
  Serial2.print("automenu.tSetDayAuto.txt=\"");
  Serial2.print(autoChangeDay);
  Serial2.print("\"");
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  // Time setting - kirim ke komponen number dengan format yang benar
  Serial2.print("automenu.nHourAuto1.val=");
  Serial2.print(autoHour1);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  Serial2.print("automenu.nHourAuto2.val=");
  Serial2.print(autoHour2);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  Serial2.print("automenu.nMinAuto1.val=");
  Serial2.print(autoMin1);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  Serial2.print("automenu.nMinAuto2.val=");
  Serial2.print(autoMin2);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  // Restore Manual Mode Settings
  // Temperature setting
  Serial2.print("manualmenu.nTargetTempMan.val=");
  Serial2.print(suhuTarget);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  // Restore days counter
  int daysSince = getDaysSinceLastChange();
  Serial2.print("nDay.val=");
  Serial2.print(daysSince);
  sendFF();
  delay(100); // Tunggu perintah selesai
  
  // Debug output
  if (debug) {
    Serial.println(">> Settings restored to Nextion:");
    Serial.print("  - Auto Temp: ");
    Serial.println(autoTempTarget);
    Serial.print("  - Auto Day: ");
    Serial.println(autoChangeDay);
    Serial.print("  - Auto Time: ");
    Serial.print(autoHour1);
    Serial.print(autoHour2);
    Serial.print(":");
    Serial.print(autoMin1);
    Serial.print(autoMin2);
    Serial.println();
    Serial.print("  - Manual Temp: ");
    Serial.println(suhuTarget);
    Serial.print("  - Days Since Last Change: ");
    Serial.println(daysSince);
  }
}

// Check if it's time for water change
bool isTimeForWaterChange() {
  DateTime now = rtc.now();
  uint32_t nowTimestamp = now.unixtime();
  uint32_t lastChangeTimestamp = lastChangeDate.unixtime();
  
  // Calculate days since last change
  int daysSinceLastChange = (nowTimestamp - lastChangeTimestamp) / 86400; // 86400 seconds = 1 day
  
  if (debug) {
    Serial.print("Days since last change: ");
    Serial.println(daysSinceLastChange);
    Serial.print("Change every: ");
    Serial.println(autoChangeDay);
  }
  
  return (daysSinceLastChange >= autoChangeDay);
}

// Get days since last change
int getDaysSinceLastChange() {
  DateTime now = rtc.now();
  uint32_t nowTimestamp = now.unixtime();
  uint32_t lastChangeTimestamp = lastChangeDate.unixtime();
  
  // Debug - tampilkan timestamp untuk troubleshooting
  if (debug) {
    Serial.print(">> Now: ");
    Serial.print(now.year());
    Serial.print("-");
    Serial.print(now.month());
    Serial.print("-");
    Serial.print(now.day());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(" | Timestamp: ");
    Serial.println(nowTimestamp);
    
    Serial.print(">> Last Change: ");
    Serial.print(lastChangeDate.year());
    Serial.print("-");
    Serial.print(lastChangeDate.month());
    Serial.print("-");
    Serial.print(lastChangeDate.day());
    Serial.print(" ");
    Serial.print(lastChangeDate.hour());
    Serial.print(":");
    Serial.print(lastChangeDate.minute());
    Serial.print(" | Timestamp: ");
    Serial.println(lastChangeTimestamp);
  }
  
  // Validate timestamp
  if (nowTimestamp < lastChangeTimestamp) {
    // If RTC is set to a time earlier than lastChange
    if (debug) Serial.println(">> Warning: RTC time is earlier than last change time");
    return 0;
  }
  
  // Calculate days since last change
  int daysSinceLastChange = (nowTimestamp - lastChangeTimestamp) / 86400; // 86400 seconds = 1 day
  
  // Debug
  if (debug) {
    Serial.print(">> Days since last change: ");
    Serial.println(daysSinceLastChange);
  }
  
  return daysSinceLastChange;
}

// Send data to Nextion
void sendToNextion(String cmd) {
  Serial2.print(cmd);
  Serial2.write(0xFF);
  Serial2.write(0xFF);
  Serial2.write(0xFF);
  
  if (debug) {
    Serial.print("Sent to Nextion: ");
    Serial.println(cmd);
  }
}

// Update Nextion clock
void updateNextionClock() {
  DateTime now = rtc.now();
  
  // Format time: "HH:MM"
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
  
  // Send to all Nextion pages
  sendToNextion("homemenu.tClock.txt=\"" + String(timeStr) + "\"");
  sendToNextion("manualmenu.tClock.txt=\"" + String(timeStr) + "\"");
  sendToNextion("automenu.tClock.txt=\"" + String(timeStr) + "\"");
  sendToNextion("settingmenu.tClock.txt=\"" + String(timeStr) + "\"");
  sendToNextion("bypassmenu.tClock.txt=\"" + String(timeStr) + "\"");
  
  if (debug) {
    Serial.print("Update Nextion Clock: ");
    Serial.println(timeStr);
  }
}

// Handle date setting via Serial
void handleDateSetting() {
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    
    if (input.equalsIgnoreCase("SETDATE")) {
      Serial.println(">> DATE SETTING MODE ACTIVATED");
      Serial.println(">> Enter date (DD/MM/YYYY):");
      return;
    }
    
    if (input.startsWith("SETDATE=")) {
      String dateStr = input.substring(8); // Skip "SETDATE="
      
      // Validate format DD/MM/YYYY
      if (dateStr.length() == 10 && dateStr.charAt(2) == '/' && dateStr.charAt(5) == '/') {
        int day = dateStr.substring(0, 2).toInt();
        int month = dateStr.substring(3, 5).toInt();
        int year = dateStr.substring(6, 10).toInt();
        
        // Validate date
        if (day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 2000 && year <= 2100) {
          // Get current time
          DateTime now = rtc.now();
          
          // Set RTC with new date but keep current time
          rtc.adjust(DateTime(year, month, day, now.hour(), now.minute(), now.second()));
          
          Serial.print(">> Date successfully set to: ");
          Serial.print(day);
          Serial.print("/");
          Serial.print(month);
          Serial.print("/");
          Serial.println(year);
          
          // Mark RTC as set
          rtcInitialized = true;
        } else {
          Serial.println(">> Invalid date! Please enter date in DD/MM/YYYY format");
        }
      } else {
        Serial.println(">> Invalid format! Please enter date in DD/MM/YYYY format");
      }
    }
  }
}

// Print system status
void printSystemStatus() {
  DateTime now = rtc.now();
  
  // Format tanggal dan waktu
  char dateTimeStr[20];
  sprintf(dateTimeStr, "%02d:%02d %02d/%02d/%04d", 
          now.hour(), now.minute(), now.day(), now.month(), now.year());
  Serial.println(dateTimeStr);
  
  // Print sensor values with fixed formatting
  Serial.print("TEMP SENSOR: ");
  if (currentTemp == -99.0) {
    Serial.print("ERR");
  } else {
    Serial.print(currentTemp, 1);
  }
  Serial.print(" C | TDS SENSOR: ");
  if (tdsValue == -1) {
    Serial.print("ERR");
  } else {
    Serial.print(tdsValue, 0);
  }
  Serial.print(" PPM | FLOW SENSOR: ");
  if (currentFlowRate < 0) {
    Serial.print("ERR");
  } else {
    Serial.print(currentFlowRate, 2);
  }
  Serial.println(" L/MIN");
  
  // ===MANUAL=== section
  Serial.println("===MANUAL===");
  
  // Filling status
  Serial.print("FILLING: ");
  if (fillingActive) {
    if (fillingError) {
      Serial.println("OFF ERROR");
    } else {
      Serial.println("ON");
    }
  } else {
    if (fillingStoppedBySensor) {
      Serial.println("OFF BY SENSORS");
    } else {
      Serial.println("OFF MANUALLY");
    }
  }
  
  // Flow switch status
  Serial.print("FLOW SWITCH: ");
  Serial.println(digitalRead(FLOW_SWITCH_PIN) ? "ON" : "OFF");
  
  // Valve status
  Serial.print("VALVE DRAIN: ");
  Serial.println(digitalRead(VALVE_DRAIN_PIN) ? "ON" : "OFF");
  Serial.print("VALVE INLET: ");
  Serial.println(digitalRead(VALVE_INLET_PIN) ? "ON" : "OFF");
  
  // Filling error
  Serial.print("FILLING ERROR: ");
  Serial.println(fillingError ? "YES" : "NO");
  
  // Float sensor
  Serial.print("FLOAT SENSOR: ");
  Serial.println(!readFloatSensor() ? "LOW WATER" : "HIGH WATER");
  
  // Draining status
  Serial.print("DRAINING: ");
  if (drainingActive) {
    Serial.println("ON");
  } else {
    if (graceActive || flowOK) {
      Serial.println("OFF BY SENSORS");
    } else {
      Serial.println("OFF MANUALLY");
    }
  }
  
  // Valve and pump status for draining
  Serial.print("VALVE DRAIN: ");
  Serial.println(digitalRead(VALVE_DRAIN_PIN) ? "ON" : "OFF");
  Serial.print("PUMP UV: ");
  Serial.println(digitalRead(PUMP_UV_PIN) ? "ON" : "OFF");
  
  // Cooling status
  Serial.print("TARGET TEMP: ");
  Serial.print(suhuTarget);
  Serial.println(" C");
  Serial.print("COOLING: ");
  Serial.println(activeProcess == 1 ? "ON" : "OFF");
  Serial.print("PUMP UV: ");
  Serial.println(digitalRead(PUMP_UV_PIN) ? "ON" : "OFF");
  Serial.print("COMPRESSOR: ");
  Serial.println(statusKompresor ? "ON" : "OFF");
  
  // Cooling duration
  Serial.print("COOL DUR MAN: ");
  if (coolingActive && activeProcess == 1) {
    unsigned long coolingMinutes = (millis() - coolingStartTime) / 60000;
    Serial.print(coolingMinutes);
  } else {
    Serial.print("0");
  }
  Serial.println(" MIN");
  
  // ===AUTO=== section
  Serial.println("===AUTO===");
  
  // Current temp
  Serial.print("CURRENT TEMP: ");
  if (currentTemp == -99.0) {
    Serial.print("ERR");
  } else {
    Serial.print(currentTemp, 1);
  }
  Serial.println(" C");
  
  // Current clock
  Serial.print("CURRENT CLOCK: ");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.println(now.minute());
  
  // Count day
  Serial.print("COUNT DAY: ");
  Serial.println(getDaysSinceLastChange());
  
  // Target temp
  Serial.print("TARGET TEMP: ");
  Serial.print(autoTempTarget);
  Serial.println(" C");
  
  // Target clock
  Serial.print("TARGET CLOCK: ");
  Serial.print(autoHour1);
  Serial.print(autoHour2);
  Serial.print(":");
  Serial.print(autoMin1);
  Serial.print(autoMin2);
  Serial.println();
  
  // Target day
  Serial.print("TARGET DAY: ");
  Serial.println(autoChangeDay);
  
  // Auto status
  Serial.print("AUTO: ");
  Serial.println(autoMode ? "ON" : "OFF");
  
  // Circulation status
  Serial.print("CIRCULATION: ");
  if (circulationActive) {
    if (manualCirculationOn) {
      Serial.println("ON MANUALLY");
    } else {
      Serial.println("ON AUTOMATIC");
    }
  } else {
    if (manualCirculationOff) {
      Serial.println("OFF MANUALLY");
    } else {
      Serial.println("OFF AUTOMATIC");
    }
  }
  
  // Cooling duration auto
  Serial.print("COOL DUR AUTO: ");
  if (circulationActive) {
    unsigned long circulationMinutes = (millis() - circulationStartTime) / 60000;
    Serial.print(circulationMinutes);
  } else {
    Serial.print("0");
  }
  Serial.println(" MIN");
  
  // Circulation error
  Serial.print("CIRCULATION ERROR: ");
  Serial.println(circulationErrorActive ? "YES" : "NO");
  
  // ===SETTING=== section
  Serial.println("===SETTING===");
  
  // Clock
  Serial.print("CLOCK: ");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.println(now.minute());
  
  // Countdown time
  Serial.print("COUNTDOWN TIME: ");
  if (isCounting) {
    Serial.print(count);
  } else {
    Serial.print(initialCount);
  }
  Serial.println(" SEC");
  
  Serial.println(); // Empty line for readability
}

// Complete water change process with filling
void completeWaterChange() {
  if (debug) {
    Serial.println(">> Completing water change/fill process");
  }
  
  // Reset water change scheduled flag
  waterChangeScheduled = false;
  
  // Update last change date to now
  lastChangeDate = rtc.now();
  
  // Save to EEPROM immediately
  saveScheduleData();
  
  // Check float sensor
  bool floatState = readFloatSensor();
  if (debug) {
    Serial.print(">> Float sensor state: ");
    Serial.println(floatState ? "LOW WATER" : "HIGH WATER");
  }
  
  if (floatState == HIGH) { // Water not enough
    fillingActive = true;
    fillingStarted = false;
    fillingStage = 0;
    lastFloatState = floatState;
    
    if (debug) {
      Serial.println(">> Filling started after water change/pre-fill");
    }
  } else {
    if (debug) {
      Serial.println(">> Water level already sufficient after water change/pre-fill");
    }
    
    // Jika ini pre-fill manual, lanjutkan ke circulation
    if (preFillScheduled) {
      if (debug) Serial.println(">> Pre-fill scheduled is true, starting circulation");
      preFillScheduled = false;
      
      // Mulai circulation setelah pre-fill selesai
      circulationActive = true;
      manualCirculationOn = true;
      manualCirculationOff = false;
      circulationStatus = 1; // ON state
      
      // Reset temperature control mode
      initialCoolingMode = true;
      targetReached = false;
      
      // Turn on UV pump
      digitalWrite(PUMP_UV_PIN, HIGH);
      
      // Set start time untuk circulation
      circulationStartTime = millis();
      autoCirculationStartTime = millis();
      lastUpdateTime = millis();
      
      // Aktifkan cooling duration
      coolingActive = true;
      
      // Reset duration display
      Serial2.print("nCoolDurAuto.val=0");
      sendFF();
      
      Serial2.print("pStopCir.pic=9"); // ON state (biru)
      sendFF();
      Serial2.print("blinkingSC.val=1"); // Enable blinking
      sendFF();
      Serial2.print("tBlinkSC.en=1");
      sendFF();
      Serial2.print("cirActive.val=1"); // Update Nextion variable
      sendFF();
      
      if (debug) Serial.println(">> Circulation started after manual pre-fill");
    } else {
      if (debug) Serial.println(">> Pre-fill scheduled is false, not starting circulation");
    }
  }
  
  // Update nDay di Nextion
  int daysSince = getDaysSinceLastChange();
  Serial2.print("nDay.val=");
  Serial2.print(daysSince);
  sendFF();
  
  if (debug) {
    Serial.print(">> Updated nDay to: ");
    Serial.println(daysSince);
  }
}

// Check error recovery for circulation
void checkErrorRecovery() {
  if (circulationErrorActive) {
    bool flowStatus = digitalRead(FLOW_SWITCH_PIN);
    
    if (flowStatus) {
      // Flow switch kembali normal, coba recovery
      if (debug) Serial.println(">> Circulation error recovery detected");
      
      // Reset error status
      circulationErrorActive = false;
      circulationStatus = 1; // ON state
      
      // Restart circulation
      digitalWrite(PUMP_UV_PIN, HIGH);
      circulationStartTime = millis();
      autoCirculationStartTime = millis();
      
      // Update Nextion
      Serial2.print("cirActive.val=1");
      sendFF();
      Serial2.print("pStopCir.pic=9"); // ON state
      sendFF();
      Serial2.print("blinkingSC.val=1"); // Enable blinking
      sendFF();
      Serial2.print("tBlinkSC.en=1");
      sendFF();
      Serial2.print("blinkingEC.val=0"); // Disable error blinking
      sendFF();
      Serial2.print("tBlinkEC.en=0");
      sendFF();
      
      // Enable auto process button again
      Serial2.print("pAutoProcess.en=1");
      sendFF();
    }
  }
}

// Update Nextion status
void updateNextionStatus(String statusText, int picValue) {
  Serial2.print("tStatus.txt=\"" + statusText + "\"");
  sendFF();
  Serial2.print("pStatus.pic=" + String(picValue));
  sendFF();
}
