// =====MODE MANUAL=====

// Di MANUAL.INO - perbaiki STARTFILLING
void STARTFILLING() {
  unsigned long now = millis();
  
  if (!fillingActive) return;
  
  // Deklarasi variabel di luar switch untuk menghindari error jump to case label
  bool flowSwitchState;
  bool initialFloatState;
  bool currentFloatState;
  
  // Recovery otomatis ketika error hilang (dijalankan di semua stage jika error aktif)
  if (fillingErrorActive) {
    flowSwitchState = digitalRead(FLOW_SWITCH_PIN);
    
    if (flowSwitchState == LOW) { // Aliran kembali normal
      fillingErrorActive = false;
      
      // Matikan Blink Merah
      Serial2.print("blinkingEF.val=0");
      sendFF();
      Serial2.print("tBlinkEF.en=0");
      sendFF();
      
      // Filling On
      Serial2.print("activeProcess.val=2");
      sendFF();
      Serial2.print("pFillingMan.pic=7");   
      sendFF();
      Serial2.print("blinkingFM.val=1");    
      sendFF();
      Serial2.print("tBlinkFM.en=1");
      sendFF();
      
      // Lanjut proses Filling Stage 2 tanpa Draining
      fillingStage = 2;
      
      // Aktifkan inlet valve untuk melanjutkan pengisian
      digitalWrite(VALVE_DRAIN_PIN, LOW);
      delay(500);
      digitalWrite(VALVE_INLET_PIN, HIGH);
      
      return;
    }
  }

  if (fillingErrorActive) return; // Mencegah sistem berjalan saat tidak aman
  
  switch (fillingStage) {
    case 0:
      initialFloatState = readFloatSensor(); // Air Rendah/ Air Penuh
      
      //Float Sensor LOW (Air Penuh), Hentikan Proses Filling
      if (initialFloatState == LOW) {
        digitalWrite(VALVE_DRAIN_PIN, LOW);
        digitalWrite(VALVE_INLET_PIN, LOW);
  
        fillingActive = false;
        fillingStarted = false;
        fillingStage = 0;
  
      // --- PERBAIKI DI SINI ---
      Serial2.print("manualmenu.activeProcess.val=0");
      sendFF();
      Serial2.print("automenu.activeProcess.val=0");
      sendFF();
      delay(50);
  
      Serial2.print("blinkingFM.val=0");
      sendFF();
      delay(50);
      Serial2.print("tBlinkFM.en=0");
      sendFF();
      delay(50);
      Serial2.print("pFillingMan.pic=6");
      sendFF();

      return;
    }
      
      //Flow Switch YES, Float Sensor HIGH (Air Rendah), Jalankan Proses Filling Stage 1
      if (initialFloatState) {
        digitalWrite(VALVE_DRAIN_PIN, LOW);
        digitalWrite(VALVE_INLET_PIN, LOW);
        delay(100);
        
        digitalWrite(VALVE_DRAIN_PIN, HIGH);
        drainStartTime = now;
        fillingStarted = true;
        fillingStage = 1;
      }
      break;

    //Stage 1: Draining 5 detik  
    case 1:
      if (now - drainStartTime >= 5000) {
        
        digitalWrite(VALVE_DRAIN_PIN, LOW);
        delay(500);
        
        digitalWrite(VALVE_INLET_PIN, HIGH);
        fillingStage = 2;
      }
      break;
      
    //Stage 2: Filling On  
    case 2:
      currentFloatState = readFloatSensor();
      flowSwitchState = digitalRead(FLOW_SWITCH_PIN);

      if (flowSwitchState == HIGH && !fillingErrorActive) {
        fillingErrorActive = true;
    
        digitalWrite(VALVE_INLET_PIN, LOW);
    
        // Tampilkan error di Nextion
        Serial2.print("activeProcess.val=4");
        sendFF();
        Serial2.print("pFillingMan.pic=16");
        sendFF();
        Serial2.print("blinkingEF.val=1");
        sendFF();
        Serial2.print("tBlinkEF.en=1");
        sendFF();
        Serial2.print("blinkingFM.val=0");
        sendFF();
        Serial2.print("tBlinkFM.en=0");
        sendFF();
    
        return;
      }
      
      if (currentFloatState == LOW) { // Air sudah penuh
        if (fullDetectedTimeGlobal == 0) {
          fullDetectedTimeGlobal = now;
        } else if (now - fullDetectedTimeGlobal >= FLOAT_DEBOUNCE_DELAY) {
          digitalWrite(VALVE_INLET_PIN, LOW);
          fillingActive = false;
          fillingStarted = false;
          fillingStage = 0;

          // --- PERBAIKI DI SINI ---
          Serial2.print("manualmenu.activeProcess.val=0");
          sendFF();
          Serial2.print("automenu.activeProcess.val=0");
          sendFF();
          delay(50);
    
          Serial2.print("blinkingFM.val=0");
          sendFF();
          delay(50);
          Serial2.print("tBlinkFM.en=0");
          sendFF();
          delay(50);
          Serial2.print("pFillingMan.pic=6");
          sendFF();
    
          fullDetectedTimeGlobal = 0;

          return;
        }
      } else {
        fullDetectedTimeGlobal = 0;
      }
      break;
  }
}

void STOPFILLINGBYSENSOR() {
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  
  fillingActive = false;
  fillingStarted = false;
  fillingStage = 0;
  fillingErrorActive = false;
  fillingStoppedBySensor = true;

  // Update activeProcess di kedua halaman
  Serial2.print("manualmenu.activeProcess.val=0");
  sendFF();
  Serial2.print("automenu.activeProcess.val=0");
  sendFF();
  delay(100);
  
  // Update tombol di halaman manual
  Serial2.print("manualmenu.blinkingFM.val=0");
  sendFF();
  Serial2.print("manualmenu.tBlinkFM.en=0");
  sendFF();
  Serial2.print("manualmenu.pFillingMan.pic=6");
  sendFF();
}

void STOPFILLINGBYMANUAL() {
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);

  fillingActive = false;
  fillingStarted = false;
  fillingStage = 0;
  fillingErrorActive = false;
  fillingStoppedBySensor = false;
  
  Serial2.print("activeProcess.val=0");
  sendFF();
  delay(100);
  Serial2.print("blinkingFM.val=0");
  sendFF();
  delay(100);
  Serial2.print("tBlinkFM.en=0");
  sendFF();
  delay(100);
  Serial2.print("pFillingMan.pic=6");
  sendFF();
}

void STARTDRAINING() {
  unsigned long now = millis();

  if (!drainingActive) return;
  
  // TAMBAHKAN PENGECEKAN KONFLIK FILLING
  if (fillingActive) {
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    drainingActive = false;
    graceActive = false;
    flowOK = false;
    
    Serial2.print("activeProcess.val=0");
    sendFF();
    delay(150);
    Serial2.print("blinkingDM.val=0");
    sendFF();
    delay(150);
    Serial2.print("tBlinkDM.en=0");
    sendFF();
    delay(150);
    Serial2.print("pDrainingMan.pic=10");
    sendFF();
    
    return;
  }
  
  // TAMBAHKAN PENGECEKAN KONFLIK COOLING
  if (coolingActive) {
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    drainingActive = false;
    graceActive = false;
    flowOK = false;
    
    Serial2.print("activeProcess.val=0");
    sendFF();
    delay(150);
    Serial2.print("blinkingDM.val=0");
    sendFF();
    delay(150);
    Serial2.print("tBlinkDM.en=0");
    sendFF();
    delay(150);
    Serial2.print("pDrainingMan.pic=10");
    sendFF();
    
    return;
  }
  
  if (!graceActive && !flowOK) {
    graceActive = true;
    graceStart = now;

    digitalWrite(VALVE_DRAIN_PIN, HIGH);
    digitalWrite(PUMP_UV_PIN, HIGH);

    pulseCount = 0;
    lastPulseCount = 0;
    lastFlowCalcTime = now;
    lowFlowCount = 0;
  }
  
  if (millis() - lastFlowCalcTime >= 1000) {
    noInterrupts();
    unsigned long pulses = pulseCount - lastPulseCount;
    lastPulseCount = pulseCount;
    interrupts();
    
    float rawFlowRate = (pulses * 60.0) / FS300A_CALIBRATION;
    float filteredFlowRate = getAverageFlowRate(rawFlowRate);
    
    updateFlowRateToNextion(filteredFlowRate);
    
    if (graceActive && !flowOK) {
      if (now - graceStart >= 5000) {
        if (filteredFlowRate > 0.1) {
          flowOK = true;
          graceActive = false;
          lowFlowCount = 0;
        } else {
          STOPDRAININGBYSENSOR();
        }
      }
    } else if (flowOK) {
      if (filteredFlowRate <= 0.1) {
        lowFlowCount++;
        if (lowFlowCount >= 5) {
          STOPDRAININGBYSENSOR();
        }
      } else {
        lowFlowCount = 0;
      }
    }
    lastFlowCalcTime = now;
  }
}

void STOPDRAININGBYSENSOR() {
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);

  drainingActive = false;
  graceActive = false;
  flowOK = false;
  
  // Update activeProcess di kedua halaman
  Serial2.print("manualmenu.activeProcess.val=0");
  sendFF();
  Serial2.print("automenu.activeProcess.val=0");
  sendFF();
  delay(150);
  
  // Update tombol di halaman manual
  Serial2.print("manualmenu.blinkingDM.val=0");
  sendFF();
  Serial2.print("manualmenu.tBlinkDM.en=0");
  sendFF();
  Serial2.print("manualmenu.pDrainingMan.pic=10");
  sendFF();
}

void STOPDRAININGBYMANUAL() {
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);

  drainingActive = false;
  graceActive = false;
  flowOK = false;
  
  Serial2.print("activeProcess.val=0");
  sendFF();
  delay(150);
  Serial2.print("blinkingDM.val=0");
  sendFF();
  delay(150);
  Serial2.print("tBlinkDM.en=0");
  sendFF();
  delay(150);
  Serial2.print("pDrainingMan.pic=10");
  sendFF();
}

void STARTCOOLING () {
  unsigned long now = millis();

  if (!coolingActive) return;
  
  if (now - waktuTerakhirCek >= intervalCek) {
    waktuTerakhirCek = now;
    
    if (activeProcess == 1) {
      // TAMBAHKAN PENGECEKAN KONFLIK FILLING
      if (fillingActive) {
        sistemAktif = false;
        digitalWrite(COMPRESSOR_PIN, LOW);
        digitalWrite(PUMP_UV_PIN, LOW);
        statusKompresor = false;
        initialCoolingMode = false;
        targetReached = false;
        coolingActive = false;

        // --- PERBAIKI DI SINI ---
        Serial2.print("manualmenu.activeProcess.val=0");
        sendFF();
        Serial2.print("automenu.activeProcess.val=0");
        sendFF();
        
        Serial2.print("blinkingCM.val=0");
        sendFF();
        Serial2.print("tBlinkCM.en=0");
        sendFF();
        Serial2.print("pCoolingMan.pic=8");
        sendFF();
        
        return;
      }
      
      // TAMBAHKAN PENGECEKAN KONFLIK DRAINING
      if (drainingActive) {
        sistemAktif = false;
        digitalWrite(COMPRESSOR_PIN, LOW);
        digitalWrite(PUMP_UV_PIN, LOW);
        statusKompresor = false;
        initialCoolingMode = false;
        targetReached = false;
        coolingActive = false;

        // --- PERBAIKI DI SINI ---
        Serial2.print("manualmenu.activeProcess.val=0");
        sendFF();
        Serial2.print("automenu.activeProcess.val=0");
        sendFF();
        
        Serial2.print("blinkingCM.val=0");
        sendFF();
        Serial2.print("tBlinkCM.en=0");
        sendFF();
        Serial2.print("pCoolingMan.pic=8");
        sendFF();
        
        return;
      }
      
      if (!sistemAktif) {
        sistemAktif = true;
        digitalWrite(PUMP_UV_PIN, HIGH);
        initialCoolingMode = true;
        targetReached = false;
        coolingStartTime = millis(); // Pastikan start time di-set
        lastUpdateTime = millis();
        
        // Reset variabel stabilisasi
        lastCompressorChange = now;
        lastStableTemp = currentTemp;
        lastTempStableTime = now;
        
        Serial2.print("nCoolDurMan.val=0");
        sendFF();
      }
      
      float selisih = currentTemp - suhuTarget;
      int currentTempInt = (int)currentTemp; // Ambil bagian bilangan bulat
      
      // Cek apakah suhu stabil
      bool tempStable = false;
      if (abs(currentTemp - lastStableTemp) <= TEMP_STABILITY_THRESHOLD) {
        if (now - lastTempStableTime >= TEMP_STABILITY_TIME) {
          tempStable = true;
        }
      } else {
        lastStableTemp = currentTemp;
        lastTempStableTime = now;
      }
      
      if (initialCoolingMode) {
        if (selisih > 0) {
          if (!statusKompresor) {
            digitalWrite(COMPRESSOR_PIN, HIGH);
            statusKompresor = true;
            lastCompressorChange = now;
            
            if (debug) {
              Serial.println(">> Compressor turned ON - initial cooling mode");
            }
          }
        } else {
          // MODIFIKASI: Matikan jika bagian integer sudah mencapai target
          if (statusKompresor && currentTempInt <= suhuTarget) {
            digitalWrite(COMPRESSOR_PIN, LOW);
            statusKompresor = false;
            lastCompressorChange = now;
            
            if (debug) {
              Serial.print(">> Compressor turned OFF - target reached (");
              Serial.print(currentTemp);
              Serial.print("°C, integer part ");
              Serial.print(currentTempInt);
              Serial.print("°C <= ");
              Serial.print(suhuTarget);
              Serial.println("°C)");
            }
          }
          initialCoolingMode = false;
          targetReached = true;
        }
      } else {
        // LOGIKA HISTERESIS YANG DIPERBAIKI
        // Hanya gunakan stabilisasi untuk MENYALAKAN kompresor
        bool canTurnOnCompressor = (now - lastCompressorChange >= COMPRESSOR_STABILIZE_TIME);
        
        // MODIFIKASI: Hidupkan jika suhu aktual melebihi target + histeresis
        if (selisih > histeresis) {
          if (!statusKompresor && canTurnOnCompressor) {
            digitalWrite(COMPRESSOR_PIN, HIGH);
            statusKompresor = true;
            lastCompressorChange = now;
            
            if (debug) {
              Serial.print(">> Compressor turned ON - hysteresis threshold exceeded (");
              Serial.print(currentTemp);
              Serial.print("°C > ");
              Serial.print(suhuTarget + histeresis);
              Serial.println("°C)");
            }
          }
        } 
        // MODIFIKASI: Matikan jika bagian integer sudah mencapai target
        else if (currentTempInt <= suhuTarget) {
          if (statusKompresor) {
            digitalWrite(COMPRESSOR_PIN, LOW);
            statusKompresor = false;
            lastCompressorChange = now;
            
            if (debug) {
              Serial.print(">> Compressor turned OFF - target reached (");
              Serial.print(currentTemp);
              Serial.print("°C, integer part ");
              Serial.print(currentTempInt);
              Serial.print("°C <= ");
              Serial.print(suhuTarget);
              Serial.println("°C)");
            }
          }
        }
        // Jika target integer belum tercapai dan belum melebihi histeresis, biarkan status tidak berubah
        
        // Debug untuk histeresis
        if (debug && tempStable) {
          Serial.print(">> Temperature stable: ");
          Serial.print(currentTemp);
          Serial.print("°C (int: ");
          Serial.print(currentTempInt);
          Serial.print("°C), Target: ");
          Serial.print(suhuTarget);
          Serial.print("°C, Difference: ");
          Serial.print(selisih);
          Serial.print("°C, Compressor: ");
          Serial.println(statusKompresor ? "ON" : "OFF");
        }
      }
      unsigned long elapsedTimeMinutes = (millis() - coolingStartTime) / 60000;

      // Update tampilan durasi di Nextion
      Serial2.print("nCoolDurMan.val=");
      Serial2.print(elapsedTimeMinutes);
      sendFF();
    } else {
      if (sistemAktif) {
        sistemAktif = false;
        digitalWrite(COMPRESSOR_PIN, LOW);
        digitalWrite(PUMP_UV_PIN, LOW);
        statusKompresor = false;
        initialCoolingMode = false;
        targetReached = false;
        coolingActive = false;
      }
    }
  }

  // =======================================================
  // === LOGIKA DURASI (DI LUAR INTERVAL CEK) ===
  // =======================================================
  // Cek durasi secara terus-menerus agar lebih responsif
  /*if (coolingActive && activeProcess == 1) {
    // Hitung waktu yang telah berlalu dalam menit
    unsigned long elapsedTimeMinutes = (millis() - coolingStartTime) / 60000;
  
    // Update tampilan durasi di Nextion setiap detik
    static unsigned long lastDurDisplay = 0;
    if (millis() - lastDurDisplay >= 1000) {
      lastDurDisplay = millis();
      Serial2.print("nCoolDurMan.val=");
      Serial2.print(elapsedTimeMinutes);
      sendFF();
    }
  
    // Cek apakah durasi sudah terlampaui
    if (elapsedTimeMinutes >= manualCoolingDuration) {
      if (debug) {
        Serial.print(">> Manual cooling duration (");
        Serial.print(manualCoolingDuration);
        Serial.println(" min) reached. Stopping cooling.");
      }
  
      // Matikan cooling
      STOPCOOLING();
    }
  }*/
}

void STOPCOOLING() {
  if (sistemAktif) {
    sistemAktif = false;
    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    statusKompresor = false;
    initialCoolingMode = false;
    targetReached = false;
    coolingActive = false;
    activeProcess = 0;
    
    // Update activeProcess di kedua halaman
    Serial2.print("manualmenu.activeProcess.val=0");
    sendFF();
    Serial2.print("automenu.activeProcess.val=0");
    sendFF();
    
    // Update tombol di halaman manual
    Serial2.print("manualmenu.blinkingCM.val=0");
    sendFF();
    Serial2.print("manualmenu.tBlinkCM.en=0");
    sendFF();
    Serial2.print("manualmenu.pCoolingMan.pic=8");
    sendFF();
  }
}