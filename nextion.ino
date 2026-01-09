//=====NEXTION=====

void sendFF() {
  Serial2.write(0xFF);
  Serial2.write(0xFF);
  Serial2.write(0xFF);
}

// Handle Nextion communication
void handleNextionCommunication() {
  readNextion();
  
  // Read parameters from Nextion with interval
  if (millis() - lastNextionRead >= intervalNextionRead) {
    bacaParameterDariNextion();
    lastNextionRead = millis();
  }
}

// Ganti fungsi readNextion() dengan versi yang lebih responsif
void readNextion() {
  // Proses semua data yang tersedia di buffer
  while (Serial2.available()) {
    uint8_t ch = Serial2.read();
    
    if (ch == 0xFF) {
      ffCount++;
      if (ffCount == 3) {
        cmd.trim();
        
        // Debug: Tampilkan perintah yang diterima
        if (debug && cmd.length() > 0) {
          Serial.print(">> Nextion command received: '");
          Serial.print(cmd);
          Serial.println("'");
        }
        
        if (cmd.length() > 0) {
          // Tambahkan validasi perintah sebelum diproses
          if (isValidCommand(cmd)) {
            processNextionCommand(cmd);
          } else {
            if (debug) {
              Serial.print(">> Invalid command ignored: '");
              Serial.print(cmd);
              Serial.println("'");
            }
          }
        }
        cmd = "";
        ffCount = 0;
      }
    } else {
      cmd += (char)ch;
      ffCount = 0;
    }
  }
}

// Fungsi untuk memvalidasi perintah (VERSII LEBIH KUAT)
bool isValidCommand(String command) {
  // 1. Hapus spasi di awal dan akhir
  command.trim();

  // 2. Hapus karakter aneh (non-printable) di akhir string
  while (command.length() > 0 && !isPrintable(command.charAt(command.length() - 1))) {
    command.remove(command.length() - 1);
  }

  // 3. Jika setelah dibersihkan kosong, anggap tidak valid
  if (command.length() == 0) {
    return false;
  }
  
  // Daftar perintah yang valid
  String validCommands[] = {
    "AUTO_ON", "AUTO_OFF",
    "CIRCULATION_ON", "CIRCULATION_OFF", "CIRCULATION_OFF_DURING_ERROR",
    "FILLING_ON", "FILLING_OFF",
    "DRAINING_ON", "DRAINING_OFF",
    "COOLING_ON", "COOLING_OFF",
    "SAVE_AUTO_SETTINGS",
    "settime=",
    "bypassInlet",
    "bypassDrain",
    "bypassPumpUV",
    "bypassCompre",
    "bypassOzone",
    "GOTO_BYPASS_MENU",
    "EXIT_BYPASS_MENU"
  };
  
  // 4. Gunakan equals() untuk pencocokan yang persis setelah string dibersihkan
  for (String validCmd : validCommands) {
    if (command.indexOf(validCmd)) {
      return true;
    }
  }
  
  // Untuk countdown value, harus berupa digit
  if (isDigit(command.charAt(0))) {
    return true;
  }
  
  // Untuk auto parameters, harus dalam mode receiving
  if (receivingAutoParams) {
    return true;
  }
  
  return false;
}


// Fungsi untuk membersihkan teks dari karakter non-digit
String cleanNumberText(String input) {
  String result = "";
  
  for (int i = 0; i < input.length(); i++) {
    if (isDigit(input.charAt(i))) {
      result += input.charAt(i);
    }
  }
  
  return result;
}

void processNextionCommand(String command) {
  // Hapus karakter non-printable
  command.trim();
  
  // Tambahkan filter untuk perintah kosong atau tidak valid
  if (command.length() == 0) {
    return;
  }
  
  // Tambahkan mekanisme untuk mencegah double-trigger
  static unsigned long lastCommandTime = 0;
  static String lastCommand = "";
  
  unsigned long now = millis();
  if (now - lastCommandTime < 300) { // 300ms debounce
    if (command == lastCommand) {
      if (debug) {
        Serial.println(">> Command ignored - debounce");
      }
      return;
    }
  }
  
  lastCommandTime = now;
  lastCommand = command;

  // --- PROSES MASTER STOP TERLEBIH DAHULU ---
  if (command.indexOf("GOTO_BYPASS_MENU") >=0) {
    handleGotoBypassMenu();
    return; // Keluar, tidak perlu proses lainnya
  }
  else if (command.indexOf("EXIT_BYPASS_MENU") >=0) { // <-- TAMBAHKAN INI
    handleExitBypassMenu();
    return;
  }

  // Proses perintah dengan penanganan yang lebih baik
  if (command.indexOf("AUTO_ON") >= 0) {
    handleAutoOn();
  }
  if (command.indexOf("AUTO_OFF") >= 0) {
    handleAutoOff();
  }
  if (command.indexOf("CIRCULATION_ON") >= 0) {
    handleCirculationOn();
  }
  if (command.indexOf("CIRCULATION_OFF") >= 0) {
    handleCirculationOff();
  }
  if (command.indexOf("FILLING_ON") >= 0) {
    handleFillingOn();
  }
  if (command.indexOf("FILLING_OFF") >= 0) {
    handleFillingOff();
  }
  if (command.indexOf("DRAINING_ON") >= 0) {
    handleDrainingOn();
  }
  if (command.indexOf("DRAINING_OFF") >= 0) {
    handleDrainingOff();
  }
  if (command.indexOf("COOLING_ON") >= 0) {
    handleCoolingOn();
  }
  if (command.indexOf("COOLING_OFF") >= 0) {
    handleCoolingOff();
  }
  // --- PANGGIL HANDLER BYPASS SECARA LANGSUNG ---
  if (command.indexOf("bypassInlet") >= 0) {
    handleBypassInlet(); // <-- Langsung panggil fungsinya
  }
  if (command.indexOf("bypassDrain") >= 0) {
    handleBypassDrain(); // <-- Langsung panggil fungsinya
  }
  if (command.indexOf("bypassPumpUV") >= 0) {
    handleBypassPumpUV(); // <-- Langsung panggil fungsinya
  }
  if (command.indexOf("bypassCompre") >= 0) {
    handleBypassCompre(); // <-- Langsung panggil fungsinya
  }
  if (command.indexOf("bypassOzone") >= 0) {
    handleBypassOzone(); // <-- Langsung panggil fungsinya
  }
  if (receivingAutoParams) {
    handleAutoParams(command);
  }
  if (command.length() > 0 && isDigit(command.charAt(0))) {
    handleCountdownValue(command);
  }
  if (command.indexOf("SAVE_AUTO_SETTINGS") >= 0) {
    handleSaveAutoSettings();
  }
  if (command.startsWith("settime=")) {
    handleSetTime(command);
  }
}

void handleAutoOn() {
  // Cek apakah sistem sudah dalam mode auto
  if (autoMode) {
    if (debug) Serial.println(">> AUTO_ON ignored - already in auto mode");
    return;
  }
  
  // Cek apakah ada proses aktif yang harus dihentikan dulu
  /*if (fillingActive || drainingActive || circulationActive) {
    if (debug) Serial.println(">> AUTO_ON ignored - active process running");
    return;
  }*/
  
  // Matikan SEMUA sistem dulu untuk reset
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(OZONE_PIN, LOW);
  
  // Reset SEMUA status INTERNAL ESP
  autoMode = false;
  circulationActive = false;
  fillingActive = false;
  drainingActive = false;
  fillingStarted = false;
  fillingStage = 0;
  graceActive = false;
  flowOK = false;
  statusKompresor = false;
  sistemAktif = false;
  manualCirculationOn = false;
  manualCirculationOff = false;
  circulationStartedAutomatically = false;
  circulationErrorActive = false;
  circulationStatus = 0;
  
  // Aktifkan auto mode
  autoMode = true;
  
  // Reset pre-fill flag
  preFillScheduled = false;
  preFillExecuted = false;
  
  // Baca parameter auto dari Nextion sebelum aktivasi
  readAutoParametersBeforeActivation();
  
  // Kirim feedback ke Nextion HANYA setelah semua proses internal selesai
  sendNextionFeedback("AUTO_ON");
  
  if (debug) {
    Serial.println(">> AUTO mode activated successfully");
  }
}

void handleAutoOff() {
  // Cek apakah sistem sudah dalam mode manual
  if (!autoMode) {
    if (debug) Serial.println(">> AUTO_OFF ignored - already in manual mode");
    return;
  }
  
  // Matikan SEMUA aktuator TANPA KONDISI
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(OZONE_PIN, LOW);
  
  // Reset SEMUA status INTERNAL ESP
  autoMode = false;
  circulationActive = false;
  fillingActive = false;
  drainingActive = false;
  fillingStarted = false;
  fillingStage = 0;
  graceActive = false;
  flowOK = false;
  statusKompresor = false;
  sistemAktif = false;
  manualCirculationOn = false;
  manualCirculationOff = false;
  circulationStartedAutomatically = false;
  circulationErrorActive = false;
  circulationStatus = 0;
  receivingAutoParams = false;
  paramIndex = 0;
  ozoneActive = false;
  ozoneInitializedForThisCycle = false;
  
  // Kirim feedback ke Nextion HANYA setelah semua proses internal selesai
  sendNextionFeedback("AUTO_OFF");

  autoOffCooldownTime = millis();
  
  if (debug) {
    Serial.println(">> AUTO mode deactivated successfully");
  }
}

void handleCirculationOn() {
  if (!autoMode) {
    if (debug) Serial.println(">> CIRCULATION_ON ignored - not in auto mode");
    return;
  }
  
  if (circulationActive) {
    if (debug) Serial.println(">> CIRCULATION_ON ignored - already active");
    return;
  }
  
  if (fillingActive || drainingActive) {
    if (debug) Serial.println(">> CIRCULATION_ON ignored - another process active");
    return;
  }
  
  // Cek flow switch
  bool flowStatus = digitalRead(FLOW_SWITCH_PIN);
  
  if (flowStatus) {
    // Set error status
    circulationActive = false;
    manualCirculationOn = false;
    manualCirculationOff = false;
    circulationStartedAutomatically = false;
    circulationErrorActive = true;
    circulationStatus = 2; // 2 = ERROR
    
    // Kirim feedback error ke Nextion
    sendNextionFeedback("CIRCULATION_ERROR");
    
    if (debug) {
      Serial.println(">> CIRCULATION ERROR - Flow switch problem");
    }
    return;
  }
  
  // Cek float sensor
  bool floatStatus = !readFloatSensor();
  
  // Matikan semua aktuator dulu untuk reset
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(OZONE_PIN, LOW);

  // Set status circulation aktif
  circulationActive = true;
  manualCirculationOn = true;
  manualCirculationOff = false;
  circulationStartedAutomatically = false;
  circulationErrorActive = false;
  circulationStatus = 1; // 1 = ON
  
  // Reset stage ke 0 untuk memastikan pemeriksaan ulang
  circulationStage = 0;
  
  // Reset sistem aktif untuk memastikan inisialisasi ulang
  sistemAktif = false;
  
  // Set start time
  circulationStartTime = millis();
  
  // Kirim feedback ke Nextion HANYA setelah semua status diatur
  sendNextionFeedback("CIRCULATION_ON");
  
  // Debug output
  if (debug) {
    Serial.println(">> CIRCULATION ON - Status set to active");
    Serial.print(">> Float sensor status: ");
    Serial.println(floatStatus ? "LOW (air rendah)" : "HIGH (air penuh)");
  }
  
  // Panggil STARTCIRCULATION setelah semua status diatur
  STARTCIRCULATION();
}

void handleCirculationOff() {
  if (!circulationActive && !circulationErrorActive) {
    if (debug) Serial.println(">> CIRCULATION_OFF ignored - not active");
    return;
  }
  
  // Matikan sirkulasi
  STOPPREFILLCIRCULATION();
  
  // Kirim feedback ke Nextion HANYA setelah proses selesai
  sendNextionFeedback("CIRCULATION_OFF");
  
  if (debug) {
    Serial.println(">> CIRCULATION OFF - Status set to inactive");
  }
}

// Di NEXTION.INO - perbaiki handleFillingOn
void handleFillingOn() {
  if (autoMode) {
    if (debug) Serial.println(">> FILLING_ON ignored - in auto mode");
    return;
  }
  
  if (fillingActive) {
    if (debug) Serial.println(">> FILLING_ON ignored - already active");
    return;
  }
  
  // Cek konflik dengan proses lain
  if (coolingActive || drainingActive) {
    if (debug) Serial.println(">> FILLING_ON ignored - another process active");
    return;
  }
  
  // Set status filling aktif
  fillingActive = true;
  fillingStarted = false;
  fillingStage = 0;
  
  // Kirim feedback ke Nextion SEGERA
  sendNextionFeedback("FILLING_ON");
  
  if (debug) {
    Serial.println(">> FILLING ON - Status set to active");
  }
}

void handleFillingOff() {
  if (!fillingActive) {
    if (debug) Serial.println(">> FILLING_OFF ignored - not active");
    return;
  }
  
  STOPFILLINGBYMANUAL();
  
  // Kirim feedback ke Nextion HANYA setelah proses selesai
  sendNextionFeedback("FILLING_OFF");
  
  if (debug) {
    Serial.println(">> FILLING OFF - Status set to inactive");
  }
}

// Fungsi handler untuk DRAINING
void handleDrainingOn() {
  if (autoMode) {
    if (debug) Serial.println(">> DRAINING_ON ignored - in auto mode");
    return;
  }
  
  if (drainingActive) {
    if (debug) Serial.println(">> DRAINING_ON ignored - already active");
    return;
  }
  
  if (coolingActive || fillingActive) {
    if (debug) Serial.println(">> DRAINING_ON ignored - another process active");
    return;
  }
  
  // Set status draining aktif
  drainingActive = true;
  graceActive = false;
  flowOK = false;
  
  // Kirim feedback ke Nextion HANYA setelah status diatur
  sendNextionFeedback("DRAINING_ON");
  
  if (debug) {
    Serial.println(">> DRAINING ON - Status set to active");
  }
}

void handleDrainingOff() {
  if (!drainingActive) {
    if (debug) Serial.println(">> DRAINING_OFF ignored - not active");
    return;
  }
  
  STOPDRAININGBYMANUAL();
  
  // Kirim feedback ke Nextion HANYA setelah proses selesai
  sendNextionFeedback("DRAINING_OFF");
  
  if (debug) {
    Serial.println(">> DRAINING OFF - Status set to inactive");
  }
}

// Fungsi handler untuk COOLING
void handleCoolingOn() {
  if (autoMode) {
    if (debug) Serial.println(">> COOLING_ON ignored - in auto mode");
    return;
  }
  
  if (coolingActive) {
    if (debug) Serial.println(">> COOLING_ON ignored - already active");
    return;
  }
  
  if (fillingActive || drainingActive) {
    if (debug) Serial.println(">> COOLING_ON ignored - another process active");
    return;
  }

  // Tambahkan baris ini untuk membaca durasi dari Nextion
  manualCoolingDuration = bacaDurasiPendinginManual();
  
  // Set status cooling aktif
  coolingActive = true;
  sistemAktif = false;
  statusKompresor = false;
  initialCoolingMode = true;
  targetReached = false;
  
  // Simpan suhu target ke EEPROM
  saveManualData();
  
  // Kirim feedback ke Nextion HANYA setelah status diatur
  sendNextionFeedback("COOLING_ON");
  
  if (debug) {
    Serial.println(">> COOLING ON - Status set to active");
  }
}

void handleCoolingOff() {
  if (!coolingActive) {
    if (debug) Serial.println(">> COOLING_OFF ignored - not active");
    return;
  }
  
  STOPCOOLING();
  
  // Kirim feedback ke Nextion HANYA setelah proses selesai
  sendNextionFeedback("COOLING_OFF");
  
  if (debug) {
    Serial.println(">> COOLING OFF - Status set to inactive");
  }
}

// Fungsi handler untuk Auto Parameters
void handleAutoParams(String command) {
  autoParams[paramIndex] = command;
  paramIndex++;
  if (debug) {
    Serial.print(">> Received auto param ");
    Serial.print(paramIndex);
    Serial.print(": '");
    Serial.print(command);
    Serial.println("'");
  }
  if (paramIndex >= 6) {
    processAutoParameters();
    receivingAutoParams = false;
    paramIndex = 0;
  }
}

// Fungsi handler untuk Countdown Value
void handleCountdownValue(String command) {
  int newValue = command.toInt();
  if (newValue >= 0 && newValue <= 999) {
    count = newValue;
    initialCount = newValue;
    valueSet = true;
    Serial2.print("tCountTime.txt=\"");
    Serial2.print(count);
    Serial2.print("\"");
    sendFF();
    if (debug) {
      Serial.print("Countdown value set to: ");
      Serial.println(count);
    }
  }
}

// Fungsi handler untuk Save Auto Settings
void handleSaveAutoSettings() {
  if (debug) Serial.println(">> Saving auto settings command received");
  
  // Baca parameter dari Nextion
  readAutoParametersBeforeActivation();
  
  // Kirim konfirmasi ke Nextion
  Serial2.print("tSaveStatus.txt=\"Settings Saved\"");
  sendFF();
  
  if (debug) Serial.println(">> Auto settings saved successfully");
}

// Fungsi handler untuk Set Time
void handleSetTime(String command) {
  if (debug) Serial.println(">> DEBUG: Set clock command detected");
  if (command.length() >= 13) {
    String timeData = command.substring(8, 13);
    if (debug) {
      Serial.print(">> Time data: ");
      Serial.println(timeData);
    }
    if (timeData.length() == 5 && timeData.charAt(2) == ':') {
      int hour = timeData.substring(0, 2).toInt();
      int minute = timeData.substring(3, 5).toInt();
      if (debug) {
        Serial.print(">> Hour: ");
        Serial.print(hour);
        Serial.print(", Minute: ");
        Serial.println(minute);
      }
      if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        DateTime now = rtc.now();
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), hour, minute, 0));
        rtcInitialized = true;
        if (debug) {
          Serial.print(">> RTC set to: ");
          Serial.print(hour);
          Serial.print(":");
          Serial.println(minute);
        }
      } else {
        if (debug) Serial.println(">> Invalid time format!");
      }
    } else {
      if (debug) Serial.println(">> Invalid time format!");
    }
  } else {
    if (debug) Serial.println(">> Command too short!");
  }
}

void sendNextionFeedback(String command) {
  if (command == "AUTO_ON") {
    Serial2.print("pAutoProcess.pic=15"); // ON state
    sendFF();
    Serial2.print("manualmenu.activeProcess.val=5"); // Set auto mode active
    sendFF();
    Serial2.print("autoModeActive.val=1"); // Set auto mode active
    sendFF();
    Serial2.print("blinkingAP.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkAP.en=1"); // Enable blinking
    sendFF();
    
    // Reset tombol manual
    Serial2.print("pFillingMan.pic=6"); // OFF state
    sendFF();
    Serial2.print("pCoolingMan.pic=8"); // OFF state
    sendFF();
    Serial2.print("pDrainingMan.pic=10"); // OFF state
    sendFF();
    
    // Reset status sirkulasi
    Serial2.print("cirActive.val=0"); // Reset circulation status
    sendFF();
    Serial2.print("blinkingSC.val=0"); // Reset circulation blinking
    sendFF();
    Serial2.print("tBlinkSC.en=0"); // Reset circulation blinking enable
    sendFF();
    Serial2.print("pStopCir.pic=8"); // Reset circulation button to OFF
    sendFF();
    
    // Aktifkan tombol pStopCir
    Serial2.print("pStopCir.en=1"); // Enable pStopCir button
    sendFF();
    
    // Non-aktifkan tombol manual
    Serial2.print("pFillingMan.en=0"); // Disable pFillingMan button
    sendFF();
    Serial2.print("pCoolingMan.en=0"); // Disable pCoolingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=0"); // Disable pDrainingMan button
    sendFF();
  }
  else if (command == "AUTO_OFF") {
    Serial2.print("pAutoProcess.pic=14"); // OFF state
    sendFF();
    Serial2.print("manualmenu.activeProcess.val=0"); // Reset active process
    sendFF();
    Serial2.print("autoModeActive.val=0"); // Reset auto mode active
    sendFF();
    Serial2.print("blinkingAP.val=0"); // Stop blinking
    sendFF();
    Serial2.print("tBlinkAP.en=0"); // Disable blinking
    sendFF();
    
    // Reset button images
    Serial2.print("pFillingMan.pic=6"); // OFF state
    sendFF();
    Serial2.print("pCoolingMan.pic=8"); // OFF state
    sendFF();
    Serial2.print("pDrainingMan.pic=10"); // OFF state
    sendFF();
    
    // Reset status sirkulasi
    Serial2.print("cirActive.val=0"); // Reset circulation status
    sendFF();
    Serial2.print("blinkingSC.val=0"); // Reset circulation blinking
    sendFF();
    Serial2.print("tBlinkSC.en=0"); // Reset circulation blinking enable
    sendFF();
    Serial2.print("pStopCir.pic=8"); // Reset circulation button to OFF
    sendFF();
    
    // Non-aktifkan tombol pStopCir
    Serial2.print("pStopCir.en=0"); // Disable pStopCir button
    sendFF();
    
    // Aktifkan tombol manual
    Serial2.print("pFillingMan.en=1"); // Enable pFillingMan button
    sendFF();
    Serial2.print("pCoolingMan.en=1"); // Enable pCoolingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=1"); // Enable pDrainingMan button
    sendFF();
  }
  else if (command == "CIRCULATION_ON") {
    Serial2.print("cirActive.val=1"); // Set circulation active
    sendFF();
    Serial2.print("blinkingSC.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkSC.en=1"); // Enable blinking
    sendFF();
    Serial2.print("pStopCir.pic=9"); // ON state
    sendFF();
  }
  else if (command == "CIRCULATION_OFF") {
    Serial2.print("cirActive.val=0"); // Reset circulation status
    sendFF();
    Serial2.print("blinkingSC.val=0"); // Reset circulation blinking
    sendFF();
    Serial2.print("tBlinkSC.en=0"); // Reset circulation blinking enable
    sendFF();
    Serial2.print("pStopCir.pic=8"); // OFF state
    sendFF();
    
    // Matikan juga error blinking jika aktif
    Serial2.print("blinkingEC.val=0"); // Reset error blinking
    sendFF();
    Serial2.print("tBlinkEC.en=0"); // Reset error blinking enable
    sendFF();
    
    // Aktifkan kembali tombol pAutoProcess
    Serial2.print("pAutoProcess.en=1"); // Enable pAutoProcess button
    sendFF();
  }
  else if (command == "CIRCULATION_ERROR") {
    Serial2.print("cirActive.val=2"); // Set circulation error
    sendFF();
    Serial2.print("pStopCir.pic=17"); // ERROR picture
    sendFF();
    Serial2.print("blinkingEC.val=1"); // Start error blinking
    sendFF();
    Serial2.print("tBlinkEC.en=1"); // Enable error blinking
    sendFF();
  }
  else if (command == "FILLING_ON") {
    Serial2.print("pFillingMan.pic=7"); // ON state
    sendFF();
    Serial2.print("blinkingFM.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkFM.en=1"); // Enable blinking
    sendFF();
    
    // Non-aktifkan tombol lain
    Serial2.print("pCoolingMan.en=0"); // Disable pCoolingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=0"); // Disable pDrainingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=0"); // Disable pAutoProcess button
    sendFF();
    Serial2.print("pStopCir.en=0"); // Disable pStopCir button
    sendFF();
  }
  else if (command == "FILLING_OFF") {
    Serial2.print("pFillingMan.pic=6"); // OFF state
    sendFF();
    Serial2.print("blinkingFM.val=0"); // Stop blinking
    sendFF();
    Serial2.print("tBlinkFM.en=0"); // Disable blinking
    sendFF();
    
    // Aktifkan kembali tombol lain
    Serial2.print("pCoolingMan.en=1"); // Enable pCoolingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=1"); // Enable pDrainingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=1"); // Enable pAutoProcess button
    sendFF();
  }
  else if (command == "DRAINING_ON") {
    Serial2.print("pDrainingMan.pic=11"); // ON state
    sendFF();
    Serial2.print("blinkingDM.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkDM.en=1"); // Enable blinking
    sendFF();
    
    // Non-aktifkan tombol lain
    Serial2.print("pFillingMan.en=0"); // Disable pFillingMan button
    sendFF();
    Serial2.print("pCoolingMan.en=0"); // Disable pCoolingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=0"); // Disable pAutoProcess button
    sendFF();
    Serial2.print("pStopCir.en=0"); // Disable pStopCir button
    sendFF();
  }
  else if (command == "DRAINING_OFF") {
    Serial2.print("pDrainingMan.pic=10"); // OFF state
    sendFF();
    Serial2.print("blinkingDM.val=0"); // Stop blinking
    sendFF();
    Serial2.print("tBlinkDM.en=0"); // Disable blinking
    sendFF();
    
    // Aktifkan kembali tombol lain
    Serial2.print("pFillingMan.en=1"); // Enable pFillingMan button
    sendFF();
    Serial2.print("pCoolingMan.en=1"); // Enable pCoolingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=1"); // Enable pAutoProcess button
    sendFF();
  }
  else if (command == "COOLING_ON") {
    Serial2.print("pCoolingMan.pic=9"); // ON state
    sendFF();
    Serial2.print("blinkingCM.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkCM.en=1"); // Enable blinking
    sendFF();
    
    // Non-aktifkan tombol lain
    Serial2.print("pFillingMan.en=0"); // Disable pFillingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=0"); // Disable pDrainingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=0"); // Disable pAutoProcess button
    sendFF();
    Serial2.print("pStopCir.en=0"); // Disable pStopCir button
    sendFF();
  }
  else if (command == "COOLING_OFF") {
    Serial2.print("pCoolingMan.pic=8"); // OFF state
    sendFF();
    Serial2.print("blinkingCM.val=0"); // Stop blinking
    sendFF();
    Serial2.print("tBlinkCM.en=0"); // Disable blinking
    sendFF();
    
    // Aktifkan kembali tombol lain
    Serial2.print("pFillingMan.en=1"); // Enable pFillingMan button
    sendFF();
    Serial2.print("pDrainingMan.en=1"); // Enable pDrainingMan button
    sendFF();
    Serial2.print("pAutoProcess.en=1"); // Enable pAutoProcess button
    sendFF();
  }
}

void readAutoParametersBeforeActivation() {
  if (debug) Serial.println(">> Reading auto parameters before activation...");
  
  // Jika ini first boot, gunakan nilai dari EEPROM dan jangan baca dari Nextion
  /*if (firstBoot) {
    if (debug) Serial.println(">> First boot detected, using EEPROM values for time settings");
    
    // Kirim nilai dari EEPROM ke Nextion
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
    
    Serial2.print("automenu.tSetTempAu.txt=\"");
    Serial2.print(autoTempTarget);
    Serial2.print("\"");
    sendFF();
    
    Serial2.print("automenu.tSetDayAuto.txt=\"");
    Serial2.print(autoChangeDay);
    Serial2.print("\"");
    sendFF();
    
    // Reset flag first boot
    firstBoot = false;
    
    if (debug) {
      Serial.print(">> Auto Parameters from EEPROM - Time: ");
      Serial.print(autoHour1);
      Serial.print(autoHour2);
      Serial.print(":");
      Serial.print(autoMin1);
      Serial.print(autoMin2);
      Serial.print(" | Temp: ");
      Serial.print(autoTempTarget);
      Serial.print(" | Day: ");
      Serial.println(autoChangeDay);
    }
    
    return; // Keluar dari fungsi, jangan baca dari Nextion
  }*/
  
  // Baca waktu dari komponen Nextion menggunakan fungsi yang diperbaiki
  autoHour1 = bacaDataNumerik("automenu.nHourAuto1", 0, 2);
  autoHour2 = bacaDataNumerik("automenu.nHourAuto2", 0, 9);
  autoMin1 = bacaDataNumerik("automenu.nMinAuto1", 0, 5);
  autoMin2 = bacaDataNumerik("automenu.nMinAuto2", 0, 9);
  
  // Baca suhu dari tSetTempAu dengan penanganan khusus
  String tempText = bacaTeksDariNextion("automenu.tSetTempAu");
  if (debug) {
    Serial.print(">> Read temp text: '");
    Serial.print(tempText);
    Serial.println("'");
  }
  
  // Ekstrak angka dari teks suhu
  String tempNum = cleanNumberText(tempText);
  
  if (tempNum.length() > 0) {
    int newTempTarget = tempNum.toInt();
    if (newTempTarget >= 1 && newTempTarget <= 50) {
      autoTempTarget = newTempTarget;
      if (debug) {
        Serial.print(">> Parsed temperature: ");
        Serial.println(autoTempTarget);
      }
    }
  }
  
  // Baca hari dari tSetDayAuto dengan penanganan khusus
  String dayText = bacaTeksDariNextion("automenu.tSetDayAuto");
  if (debug) {
    Serial.print(">> Read day text: '");
    Serial.print(dayText);
    Serial.println("'");
  }
  
  // Ekstrak angka dari teks hari
  String dayNum = cleanNumberText(dayText);
  
  if (dayNum.length() > 0) {
    int newChangeDay = dayNum.toInt();
    if (newChangeDay >= 1 && newChangeDay <= 365) {
      // JIKA HARI BERUBAH, RESET nDay KE 0
      if (newChangeDay != autoChangeDay) {
        if (debug) {
          Serial.print(">> Day changed from ");
          Serial.print(autoChangeDay);
          Serial.print(" to ");
          Serial.println(newChangeDay);
        }
        autoChangeDay = newChangeDay;
        lastChangeDate = rtc.now(); // Reset ke hari ini
        
        // KIRIM PERINTAH RESET nDay KE NEXTION
        // Reset nDay when day setting changes
        updateNDaysOnAllPages(0);
        if (debug) Serial.println(">> nDay reset to 0 on all pages");
      } else {
        autoChangeDay = newChangeDay;
      }
      
      if (debug) {
        Serial.print(">> Parsed day: ");
        Serial.println(autoChangeDay);
      }
    }
  }
  
  // SIMPAN PARAMETER KE EEPROM SETELAH MEMBACA
  saveScheduleData();
  
  // Debug output
  if (debug) {
    Serial.print(">> Auto Parameters Read & Saved - Time: ");
    Serial.print(autoHour1);
    Serial.print(autoHour2);
    Serial.print(":");
    Serial.print(autoMin1);
    Serial.print(autoMin2);
    Serial.print(" | Temp: ");
    Serial.print(autoTempTarget);
    Serial.print(" | Day: ");
    Serial.println(autoChangeDay);
  }
}  // Tambahkan kurung kurawal tutup ini

// Read time component from Nextion - PERBAIKAN
int bacaKomponenWaktu(String namaKomponen, int minVal, int maxVal) {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get ");
  Serial2.print(namaKomponen);
  Serial2.print(".val");
  sendFF();
  
  // Tunggu respons lebih lama untuk memastikan data diterima
  delay(200);
  
  int value = 0; // Default value
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (header == 0x71) { // 0x71 adalah header untuk respons nilai numerik
      uint8_t lowByte = Serial2.read();
      uint8_t highByte = Serial2.read();
      Serial2.read(); // Baca byte ketiga (biasanya 0)
      
      // Gabungkan byte untuk mendapatkan nilai 16-bit
      value = lowByte | (highByte << 8);
      
      // Debug read value
      if (debug) {
        Serial.print(">> Read ");
        Serial.print(namaKomponen);
        Serial.print(": ");
        Serial.println(value);
        Serial.print("  (lowByte: ");
        Serial.print(lowByte);
        Serial.print(", highByte: ");
        Serial.print(highByte);
        Serial.println(")");
      }
      
      // Validate value
      if (value < minVal || value > maxVal) {
        if (debug) {
          Serial.print(">> Invalid ");
          Serial.print(namaKomponen);
          Serial.print(": ");
          Serial.print(value);
          Serial.print(", using default: ");
          Serial.println(minVal);
        }
        value = minVal;
      }
    } else {
      if (debug) {
        Serial.print(">> Invalid header for ");
        Serial.print(namaKomponen);
        Serial.print(": 0x");
        Serial.println(header, HEX);
      }
    }
  } else {
    if (debug) {
      Serial.print(">> No data for ");
      Serial.println(namaKomponen);
    }
  }
  
  return value;
}

// Fungsi khusus untuk membaca data numerik dari Nextion
int bacaDataNumerik(String namaKomponen, int minVal, int maxVal) {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get ");
  Serial2.print(namaKomponen);
  Serial2.print(".val");
  sendFF();
  
  // Tunggu respons
  delay(200);
  
  int value = 0;
  int bytesAvailable = Serial2.available();
  
  if (debug) {
    Serial.print(">> Bytes available for ");
    Serial.print(namaKomponen);
    Serial.print(": ");
    Serial.println(bytesAvailable);
  }
  
  if (bytesAvailable >= 4) {
    uint8_t header = Serial2.read();
    if (debug) {
      Serial.print(">> Header: 0x");
      Serial.println(header, HEX);
    }
    
    if (header == 0x71) { // 0x71 adalah header untuk respons nilai numerik
      uint8_t byte1 = Serial2.read();
      uint8_t byte2 = Serial2.read();
      uint8_t byte3 = Serial2.read();
      
      if (debug) {
        Serial.print(">> Bytes: ");
        Serial.print(byte1);
        Serial.print(", ");
        Serial.print(byte2);
        Serial.print(", ");
        Serial.println(byte3);
      }
      
      // Untuk nilai 8-bit, hanya byte1 yang digunakan
      // Untuk nilai 16-bit, byte1 adalah low byte, byte2 adalah high byte
      value = byte1;
      
      // Debug read value
      if (debug) {
        Serial.print(">> Read ");
        Serial.print(namaKomponen);
        Serial.print(": ");
        Serial.println(value);
      }
      
      // Validate value
      if (value < minVal || value > maxVal) {
        if (debug) {
          Serial.print(">> Invalid ");
          Serial.print(namaKomponen);
          Serial.print(": ");
          Serial.print(value);
          Serial.print(", using default: ");
          Serial.println(minVal);
        }
        value = minVal;
      }
    } else {
      if (debug) {
        Serial.print(">> Invalid header for ");
        Serial.print(namaKomponen);
        Serial.print(": 0x");
        Serial.println(header, HEX);
      }
    }
  } else {
    if (debug) {
      Serial.print(">> Insufficient data for ");
      Serial.print(namaKomponen);
      Serial.println();
    }
  }
  
  return value;
}

// Di NEXTION.INO, perbaiki fungsi bacaTeksDariNextion
String bacaTeksDariNextion(String namaKomponen) {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get ");
  Serial2.print(namaKomponen);
  Serial2.print(".txt");
  sendFF();
  
  // Tunggu respons
  delay(300);
  
  String result = "";
  unsigned long startTime = millis();
  
  // Baca data dari Nextion dengan timeout
  while (millis() - startTime < 1000) {
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == 0xFF) {
        break;
      }
      result += c;
    }
  }
  
  // Hapus awalan 'p' jika ada
  if (result.length() > 0 && result.charAt(0) == 'p') {
    result = result.substring(1);
  }
  
  // Debug output
  if (debug) {
    Serial.print(">> Read ");
    Serial.print(namaKomponen);
    Serial.print(": '");
    Serial.print(result);
    Serial.println("'");
  }
  
  return result;
}

// Read status from Nextion
int bacaStatusDariNextion() {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get activeProcess.val");
  sendFF();
  delay(100);
  
  int value = cachedActiveProcess != -1 ? cachedActiveProcess : 0;
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (header == 0x71) {
      value = Serial2.read();
      Serial2.read();
      Serial2.read();
    }
  }
  
  return value;
}


// Di NEXTION.INO, ganti fungsi bacaTargetDariNextion() dengan versi yang lebih robust:

int bacaTargetDariNextion() {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get manualmenu.nTargetTempMan.val");
  sendFF();
  
  // Tunggu respons lebih lama
  delay(200);
  
  int value = cachedSuhuTarget;
  bool validResponse = false;
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (debug) {
      Serial.print(">> Target temp header: 0x");
      Serial.println(header, HEX);
    }
    
    if (header == 0x71) {
      uint8_t lowByte = Serial2.read();
      uint8_t highByte = Serial2.read();
      uint8_t endByte = Serial2.read(); // Baca byte terakhir
      
      if (debug) {
        Serial.print(">> Target temp bytes: ");
        Serial.print(lowByte);
        Serial.print(", ");
        Serial.print(highByte);
        Serial.print(", ");
        Serial.println(endByte);
      }
      
      // Untuk nilai 8-bit, hanya lowByte yang digunakan
      int rawValue = lowByte;
      
      // Validasi nilai yang masuk akal
      if (rawValue >= 1 && rawValue <= 70) {
        value = rawValue;
        validResponse = true;
        
        if (debug) {
          Serial.print(">> Valid target temp read: ");
          Serial.println(value);
        }
      } else {
        if (debug) {
          Serial.print(">> Invalid target temp value: ");
          Serial.println(rawValue);
        }
      }
    } else {
      if (debug) {
        Serial.println(">> Invalid header for target temp");
      }
    }
  } else {
    if (debug) {
      Serial.println(">> Insufficient data for target temp");
    }
  }
  
  // Jika respons tidak valid, gunakan nilai terakhir yang valid
  if (!validResponse) {
    value = lastValidTarget;
    if (debug) {
      Serial.print(">> Using last valid target temp: ");
      Serial.println(value);
    }
  }
  
  return value;
}

// Di NEXTION.INO, perbarui fungsi bacaParameterDariNextion():
void bacaParameterDariNextion() {
  static unsigned long lastParamRead = 0;
  
  // Batasi pembacaan parameter hanya setiap 2 detik
  if (millis() - lastParamRead < 2000) {
    return;
  }
  
  lastParamRead = millis();
  
  // Read status only if needed
  int newActiveProcess = bacaStatusDariNextion();
  if (newActiveProcess != cachedActiveProcess) {
    activeProcess = newActiveProcess;
    cachedActiveProcess = newActiveProcess;
    
    if (debug) {
      Serial.print(">> Status changed to ");
      Serial.println(activeProcess);
    }
  }
  
  // Read target with validation
  int newSuhuTarget = bacaTargetDariNextion();
  
  // Tambahkan validasi ekstra untuk mencegah perubahan tiba-tiba
  if (newSuhuTarget >= 1 && newSuhuTarget <= 70) {
    // Cek apakah perubahan terlalu drastis (mungkin error)
    int tempDiff = abs(newSuhuTarget - cachedSuhuTarget);
    
    if (tempDiff > 10) {
      // Jika perubahan terlalu drastis, mungkin ada error komunikasi
      if (debug) {
        Serial.print(">> WARNING: Large temperature change detected (");
        Serial.print(cachedSuhuTarget);
        Serial.print(" -> ");
        Serial.print(newSuhuTarget);
        Serial.println("), ignoring...");
      }
      
      // Gunakan nilai terakhir yang valid
      suhuTarget = lastValidTarget;
    } else if (newSuhuTarget != cachedSuhuTarget) {
      // Perubahan normal, terima nilai baru
      if (sistemAktif && newSuhuTarget != suhuTarget) {
        initialCoolingMode = true;
        targetReached = false;
        if (debug) Serial.println(">> TARGET CHANGED - RESET TO INITIAL COOLING MODE");
      }
      
      lastValidTarget = newSuhuTarget;
      suhuTarget = newSuhuTarget;
      cachedSuhuTarget = newSuhuTarget;
      
      // Simpan suhu target ke EEPROM
      saveManualData();
      
      if (debug) {
        Serial.print(">> Target temperature changed to ");
        Serial.print(suhuTarget);
        Serial.println("°C");
      }
    }
  } else {
    // Invalid target - use last valid value
    suhuTarget = lastValidTarget;
    cachedSuhuTarget = lastValidTarget;
    
    if (debug) {
      Serial.print(">> Invalid target temp, using last valid: ");
      Serial.println(suhuTarget);
    }
  }
}

// Di NEXTION.INO, tambahkan fungsi ini:

void bacaAutoParameterDariNextion() {
  static unsigned long lastAutoParamRead = 0;
  
  // Batasi pembacaan parameter auto hanya setiap 10 detik
  if (millis() - lastAutoParamRead < 10000) {
    return;
  }
  
  lastAutoParamRead = millis();
  
  // Baca parameter auto menggunakan fungsi yang diperbaiki
  int tempAutoHour1 = bacaDataNumerik("automenu.nHourAuto1", 0, 2);
  int tempAutoHour2 = bacaDataNumerik("automenu.nHourAuto2", 0, 9);
  int tempAutoMin1 = bacaDataNumerik("automenu.nMinAuto1", 0, 5);
  int tempAutoMin2 = bacaDataNumerik("automenu.nMinAuto2", 0, 9);
  
  String tempAutoTemp = bacaTeksDariNextion("automenu.tSetTempAu");
  String tempAutoDay = bacaTeksDariNextion("automenu.tSetDayAuto");
  
  // Ekstrak nilai
  tempAutoTemp = cleanNumberText(tempAutoTemp);
  tempAutoDay = cleanNumberText(tempAutoDay);
  
  int newAutoTemp = tempAutoTemp.toInt();
  int newAutoDay = tempAutoDay.toInt();
  
  // Cek apakah ada perubahan
  if (tempAutoHour1 != autoHour1 || tempAutoHour2 != autoHour2 || 
      tempAutoMin1 != autoMin1 || tempAutoMin2 != autoMin2 ||
      newAutoTemp != autoTempTarget || newAutoDay != autoChangeDay) {
    
    if (debug) {
      Serial.println(">> Auto parameters changed, updating...");
    }
    
    // Update nilai
    autoHour1 = tempAutoHour1;
    autoHour2 = tempAutoHour2;
    autoMin1 = tempAutoMin1;
    autoMin2 = tempAutoMin2;
    
    if (newAutoTemp >= 1 && newAutoTemp <= 50) {
      autoTempTarget = newAutoTemp;
    }
    
    if (newAutoDay >= 1 && newAutoDay <= 365) {
      autoChangeDay = newAutoDay;
    }
    
    // Simpan ke EEPROM
    saveScheduleData();
    
    if (debug) {
      Serial.println(">> Auto parameters saved due to change");
    }
  }
}

// Send data to Nextion
void updateFlowRateToNextion(float flowRate) {
  // Batasi nilai maksimal sesuai FS300A
  if (flowRate > FLOW_MAX_RATE) flowRate = FLOW_MAX_RATE;
  
  // Debug: Cek nilai flow rate
  if (debug) {
    Serial.print(">> FS300A Flow to Nextion: ");
    Serial.println(flowRate, 2);
  }
  
  // Format string
  char flowStr[10];
  dtostrf(flowRate, 5, 2, flowStr);
  
  // Kirim ke Nextion
  Serial2.print("manualmenu.tFlow.txt=\"");
  Serial2.print(flowStr);
  Serial2.print("\"");
  sendFF();
}

void processAutoParameters() {
  // Debug received parameters
  if (debug) {
    Serial.println(">> Processing auto parameters:");
    for (int i = 0; i < 6; i++) {
      Serial.print(" Param ");
      Serial.print(i+1);
      Serial.print(": '");
      Serial.print(autoParams[i]);
      Serial.println("'");
    }
  }
  
  // Parameter 1: First hour digit
  if (autoParams[0].length() > 0) {
    autoHour1 = autoParams[0].toInt();
    if (autoHour1 < 0 || autoHour1 > 2) autoHour1 = 0;
  } else {
    autoHour1 = 1;
  }
  
  // Parameter 2: Second hour digit
  if (autoParams[1].length() > 0) {
    autoHour2 = autoParams[1].toInt();
    if (autoHour2 < 0 || autoHour2 > 9) autoHour2 = 0;
  } else {
    autoHour2 = 1;
  }
  
  // Parameter 3: First minute digit
  if (autoParams[2].length() > 0) {
    autoMin1 = autoParams[2].toInt();
    if (autoMin1 < 0 || autoMin1 > 5) autoMin1 = 0;
  } else {
    autoMin1 = 0;
  }
  
  // Parameter 4: Second minute digit
  if (autoParams[3].length() > 0) {
    autoMin2 = autoParams[3].toInt();
    if (autoMin2 < 0 || autoMin2 > 9) autoMin2 = 0;
  } else {
    autoMin2 = 0;
  }
  
  // Parameter 5: Temperature
  if (autoParams[4].length() > 0) {
    String tempStr = autoParams[4];
    tempStr.replace("°C", "");
    tempStr.replace(" ", "");
    autoTempTarget = tempStr.toInt();
    if (autoTempTarget < 1 || autoTempTarget > 50) autoTempTarget = 3;
  } else {
    autoTempTarget = 2;
  }
  
  // Parameter 6: Day
  if (autoParams[5].length() > 0) {
    String dayStr = autoParams[5];
    dayStr.replace("hari", "");
    dayStr.replace(" ", "");
    autoChangeDay = dayStr.toInt();
    if (autoChangeDay < 1 || autoChangeDay > 365) autoChangeDay = 7;
  } else {
    autoChangeDay = 5;
  }
  
  // Simpan parameter auto ke EEPROM
  saveScheduleData();
  
  // Kirim perintah ke Nextion untuk memperbarui tampilan
  Serial2.print("automenu.tSetTempAu.txt=\"");
  Serial2.print(autoTempTarget);
  Serial2.print("\"");
  sendFF();
  
  Serial2.print("automenu.tSetDayAuto.txt=\"");
  Serial2.print(autoChangeDay);
  Serial2.print("\"");
  sendFF();
  
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
  
  if (debug) {
    Serial.print(">> Final Auto Settings - Time: ");
    Serial.print(autoHour1);
    Serial.print(autoHour2);
    Serial.print(":");
    Serial.print(autoMin1);
    Serial.print(autoMin2);
    Serial.print(" | Temp: ");
    Serial.print(autoTempTarget);
    Serial.print("°C | Change every: ");
    Serial.print(autoChangeDay);
    Serial.println(" days");
  }
}

// Di NEXTION.INO - perbarui updateNextionDisplay
void updateNextionDisplay() {
  // ================= TEMPERATURE =================
  Serial2.print("nTemp.val=");
  if (currentTemp == -99.0) {
    Serial2.print("ERR");
  } else {
    Serial2.print((int)currentTemp);
  }
  sendFF();
  
  // ================= TDS =================
  updateTDSToNextion(); // Gunakan fungsi khusus
  
  // ================= FLOW =================
  static unsigned long lastFlowDisplay = 0;
  if (millis() - lastFlowDisplay >= 500) {
    char flowStr[10];
    
    if (currentFlowRate < 0) {
      strcpy(flowStr, "ERR");
    } else {
      dtostrf(currentFlowRate, 5, 2, flowStr);
    }
    
    Serial2.print("manualmenu.tFlow.txt=\"");
    Serial2.print(flowStr);
    Serial2.print("\"");
    sendFF();
    
    lastFlowDisplay = millis();
  }
}

// Fungsi untuk memvalidasi dan memperbaiki status sistem
void validateAndFixSystemStatus() {
  static unsigned long lastValidationTime = 0;
  
  // Jalankan validasi setiap 5 detik
  if (millis() - lastValidationTime < 5000) {
    return;
  }
  
  lastValidationTime = millis();
  
  // Cek konsistensi status
  bool statusChanged = false;
  
  // Jika auto mode aktif, pastikan tombol manual tidak aktif
  if (autoMode) {
    if (fillingActive || drainingActive || coolingActive) {
      if (debug) Serial.println(">> Status conflict: auto mode with manual process");
      fillingActive = false;
      drainingActive = false;
      coolingActive = false;
      statusChanged = true;
    }
  }
  
  // Jika circulation aktif, pastikan tidak ada proses lain
  if (circulationActive) {
    if (fillingActive || drainingActive) {
      if (debug) Serial.println(">> Status conflict: circulation with filling/draining");
      fillingActive = false;
      drainingActive = false;
      statusChanged = true;
    }
  }
  
  // Jika ada perubahan status, kirim feedback ke Nextion
  if (statusChanged) {
    if (debug) Serial.println(">> System status fixed, updating Nextion");
    
    // Kirim status aktual ke Nextion
    if (autoMode) {
      sendNextionFeedback("AUTO_ON");
    } else {
      sendNextionFeedback("AUTO_OFF");
    }
    
    if (circulationActive) {
      sendNextionFeedback("CIRCULATION_ON");
    } else {
      sendNextionFeedback("CIRCULATION_OFF");
    }
  }
}


// --- HANDLER UNTUK BYPASS INLET (VERSI SUPER LANGSUNG) ---
void handleBypassInlet() {  
  bypassInlet = !bypassInlet; // Toggle status

  if (debug) {
    Serial.print(">> Bypass Inlet: ");
    Serial.println(bypassInlet ? "ON" : "OFF");
  }

  // Kontrol langsung tanpa fungsi lain
  if (bypassInlet) {
    digitalWrite(VALVE_INLET_PIN, HIGH);
  } else {
    digitalWrite(VALVE_INLET_PIN, LOW);
  }
}

// --- HANDLER UNTUK BYPASS DRAIN (VERSI SUPER LANGSUNG) ---
void handleBypassDrain() {
  bypassDrain = !bypassDrain; // Toggle status
  
  if (debug) {
    Serial.print(">> Bypass Drain: ");
    Serial.println(bypassDrain ? "ON" : "OFF");
  }
  
  // Kontrol langsung tanpa fungsi lain
  if (bypassDrain) {
    digitalWrite(VALVE_DRAIN_PIN, HIGH);
  } else {
    digitalWrite(VALVE_DRAIN_PIN, LOW);
  }
}

// --- HANDLER UNTUK BYPASS PUMP UV (VERSI SUPER LANGSUNG) ---
void handleBypassPumpUV() {
  bypassPumpUV = !bypassPumpUV; // Toggle status
  
  if (debug) {
    Serial.print(">> Bypass Pump UV: ");
    Serial.println(bypassPumpUV ? "ON" : "OFF");
  }
  
  // Kontrol langsung tanpa fungsi lain
  if (bypassPumpUV) {
    digitalWrite(PUMP_UV_PIN, HIGH);
  } else {
    digitalWrite(PUMP_UV_PIN, LOW);
  }
}

// --- HANDLER UNTUK BYPASS COMPRESSOR (VERSI SUPER LANGSUNG) ---
void handleBypassCompre() {
  bypassCompre = !bypassCompre; // Toggle status
  
  if (debug) {
    Serial.print(">> Bypass Compressor: ");
    Serial.println(bypassCompre ? "ON" : "OFF");
  }
  
  // Kontrol langsung tanpa fungsi lain
  if (bypassCompre) {
    digitalWrite(COMPRESSOR_PIN, HIGH);
  } else {
    digitalWrite(COMPRESSOR_PIN, LOW);
  }
}

// --- HANDLER UNTUK BYPASS OZONE (VERSI SUPER LANGSUNG) ---
void handleBypassOzone() {
  bypassOzone = !bypassOzone; // Toggle status
  
  if (debug) {
    Serial.print(">> Bypass Ozone: ");
    Serial.println(bypassOzone ? "ON" : "OFF");
  }
  
  // Kontrol langsung tanpa fungsi lain
  if (bypassOzone) {
    digitalWrite(OZONE_PIN, HIGH);
  } else {
    digitalWrite(OZONE_PIN, LOW);
  }
}

// Fungsi untuk mengupdate nilai TDS ke layar Nextion
void updateTDSToNextion() {
  Serial2.print("nTDS.val=");
  
  if (tdsValue == -1) {
    Serial2.print("ERR");
  } else {
    // Pastikan nilai adalah integer dan dalam range
    int tdsInt = (int)tdsValue;
    if (tdsInt < 0) tdsInt = 0;
    if (tdsInt > 999) tdsInt = 999; // Batasi maksimal 3 digit
    
    // Kirim sebagai integer
    Serial2.print(tdsInt);
  }
  
  sendFF();
}

// Fungsi untuk mengupdate nDay di semua halaman
void updateNDaysOnAllPages(int days) {
  // Update di halaman utama (home)
  Serial2.print("homemenu.nDay.val=");
  Serial2.print(days);
  sendFF();
  
  // Update di halaman manual
  Serial2.print("manualmenu.nDay.val=");
  Serial2.print(days);
  sendFF();
  
  // Update di halaman auto
  Serial2.print("automenu.nDay.val=");
  Serial2.print(days);
  sendFF();
  
  // Update di halaman setting
  Serial2.print("settingmenu.nDay.val=");
  Serial2.print(days);
  sendFF();

  // Update di halaman bypass
  Serial2.print("bypassmenu.nDay.val=");
  Serial2.print(days);
  sendFF();
  
  if (debug) {
    Serial.print(">> Updated nDay to ");
    Serial.print(days);
    Serial.println(" on all pages");
  }
}

// Tambahkan fungsi untuk memastikan perintah diterima Nextion
void confirmNextionStatus(String component, String expectedValue) {
  // Kirim perintah get untuk memverifikasi status
  Serial2.print("get ");
  Serial2.print(component);
  Serial2.print(".val");
  sendFF();
  
  // Tunggu respons dengan timeout
  unsigned long startTime = millis();
  while (millis() - startTime < 100) { // Timeout 100ms
    if (Serial2.available()) {
      String response = "";
      while (Serial2.available()) {
        char c = Serial2.read();
        if (c == 0xFF) break;
        response += c;
      }
      
      if (response.indexOf(expectedValue) >= 0) {
        if (debug) {
          Serial.print(">> Status confirmed: ");
          Serial.print(component);
          Serial.print(" = ");
          Serial.println(expectedValue);
        }
        return;
      }
    }
  }
  
  // Jika tidak ada konfirmasi, kirim ulang perintah
  if (debug) {
    Serial.print(">> Status not confirmed, resending: ");
    Serial.print(component);
    Serial.print(" = ");
    Serial.println(expectedValue);
  }
  
  Serial2.print(component);
  Serial2.print(".val=");
  Serial2.print(expectedValue);
  sendFF();
}

// --- FUNGSI MASTER STOP UNTUK MENU BYPASS (HANYA PROSES) ---
void handleGotoBypassMenu() {
  if (debug) {
    Serial.println(">> GOTO_BYPASS_MENU received. Stopping all internal processes.");
  }

  // 1. Matikan SEMUA aktuator fisik secara langsung
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(OZONE_PIN, LOW);

  // 2. Reset SEMUA flag proses (Manual & Auto)
  fillingActive = false;
  drainingActive = false;
  coolingActive = false;
  activeProcess = 0;
  autoMode = false; // Keluar dari auto mode untuk keamanan
  circulationActive = false;
  prefillActive = false;
  waterChangeActive = false;

  // 3. Reset SEMUA variabel status internal
  sistemAktif = false;
  statusKompresor = false;
  initialCoolingMode = false;
  targetReached = false;
  fillingStarted = false;
  fillingStage = 0;
  fillingErrorActive = false;
  graceActive = false;
  flowOK = false;
  circulationStarted = false;
  circulationStage = 0;
  circulationErrorActive = false;
  manualCirculationOn = false;
  manualCirculationOff = false;
  circulationStatus = 0;

  // TIDAK ADA PERINTAH KE NEXTION, KARENA NEXTION SUDAH MENANGANNYA
  // Fungsi ini murni untuk menghentikan proses di ESP32

  if (debug) {
    Serial.println(">> All internal processes stopped. System is now idle.");
  }
}

// --- FUNGSI EXIT BYPASS MENU (HANYA PROSES) ---
void handleExitBypassMenu() {
  if (debug) {
    Serial.println(">> EXIT_BYPASS_MENU received. Turning off bypass processes.");
  }

  // 1. Matikan SEMUA aktuator bypass secara langsung
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(OZONE_PIN, LOW);

  // 2. Reset SEMUA flag bypass ke OFF
  bypassInlet = false;
  bypassDrain = false;
  bypassPumpUV = false;
  bypassCompre = false;
  bypassOzone = false;

  // TIDAK ADA PERINTAH KE NEXTION, KARENA ANDA SUDAH MENANGANNYA DI HALAMAN SETTINGMENU

  if (debug) {
    Serial.println(">> All bypass processes turned off. System returned to normal.");
  }
}

// Fungsi khusus untuk membaca durasi pendinginan manual dari Nextion
int bacaDurasiPendinginManual() {
  // Flush buffer untuk memastikan tidak ada sisa data
  while (Serial2.available()) Serial2.read();
  
  // Kirim perintah get ke Nextion
  Serial2.print("get manualmenu.nCoolDurMan.val");
  sendFF();
  
  // Beri jeda untuk Nextion merespons
  delay(200);
  
  int value = manualCoolingDuration; // Gunakan nilai terakhir sebagai default
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (header == 0x71) { // Header untuk respons numerik
      uint8_t lowByte = Serial2.read();
      uint8_t highByte = Serial2.read();
      Serial2.read(); // Baca byte terakhir
      
      // Untuk nilai 8-bit, lowByte adalah nilainya
      int rawValue = lowByte;
      
      // Validasi nilai durasi (misal: antara 1 dan 120 menit)
      if (rawValue >= 1 && rawValue <= 120) {
        value = rawValue;
        if (debug) {
          Serial.print(">> Manual cooling duration read: ");
          Serial.print(value);
          Serial.println(" minutes");
        }
      } else {
        if (debug) {
          Serial.print(">> Invalid duration value: ");
          Serial.print(rawValue);
          Serial.println(", using default");
        }
      }
    }
  } else {
    if (debug) Serial.println(">> No response for nCoolDurMan");
  }
  
  return value;
}