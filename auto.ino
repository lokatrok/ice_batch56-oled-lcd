// =====MODE AUTO=====

bool isWaterChangeDay() {

  DateTime now = rtc.now();
  // Logika untuk menentukan apakah hari ini adalah hari pergantian air
  // Misalnya, setiap 7 hari sekali
  TimeSpan diff = now - lastChangeDate;
  return diff.days() >= 7;
}

bool preFillExecutedToday(DateTime now) {
  if (!preFillExecuted) return false;
  
  DateTime lastPreFill = lastPreFillDate;
  return (now.year() == lastPreFill.year() && 
          now.month() == lastPreFill.month() && 
          now.day() == lastPreFill.day());
}

// Setup RTC
void setupRTC() {
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    rtcInitialized = false;
    return;
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  rtcInitialized = true;
  
  if (debug) {
    Serial.println(">> RTC initialized");
  }
}

// ==========MODE AUTO SCHEDULE==========
void AUTOSCHEDULE() {
  if (!rtcInitialized) return;

  // --- CEK COOLDOWN PERIOD ---
  if (millis() - autoOffCooldownTime < 10000) { // Jika kurang dari 10 detik
    if (debug) Serial.println(">> AUTOSCHEDULE skipped: In cooldown period");
    return; // Jangan jalankan apa-apa
  }
  
  DateTime now = rtc.now();
  
  // Hitung waktu target (jam dan menit)
  int targetTime = autoHour1 * 10 + autoHour2;
  int targetMinute = autoMin1 * 10 + autoMin2;
  
  // Validasi target time
  if (targetTime < 0 || targetTime > 23 || targetMinute < 0 || targetMinute > 59) {
    targetTime = 5;
    targetMinute = 30;
  }

  // --- LOGIKA BARU SESUAI ALUR KERJA ---
  
  // 1. CEK JADWAL CIRCULATION UTAMA
  int minutesUntilTarget = getMinutesUntilNextSchedule(targetTime, targetMinute);
  
  if (debug) {
    Serial.print(">> Auto Schedule Check - Current: ");
    Serial.print(now.hour());
    Serial.print(":");
    if (now.minute() < 10) Serial.print("0");
    Serial.print(now.minute());
    Serial.print(" | Target: ");
    Serial.print(targetTime);
    Serial.print(":");
    if (targetMinute < 10) Serial.print("0");
    Serial.print(targetMinute);
    Serial.print(" | Minutes Until: ");
    Serial.println(minutesUntilTarget);
  }

  // 2. CEK HARI PERGANTIAN AIR (Req 3a)
  bool todayIsWaterChangeDay = isWaterChangeDay();
  if (debug) {
    Serial.print(">> Today is water change day: ");
    Serial.println(todayIsWaterChangeDay ? "YES" : "NO");
  }

  if (todayIsWaterChangeDay) {
    // --- HARI PERGANTIAN AIR ---
    int waterChangeMinutes = (targetTime * 60 + targetMinute) - PRE_FILL_MINUTES;
    if (waterChangeMinutes < 0) waterChangeMinutes += 1440;
    
    int minutesUntilWaterChange = getMinutesUntilNextSchedule(waterChangeMinutes / 60, waterChangeMinutes % 60);
    
    if (minutesUntilWaterChange >= 0 && minutesUntilWaterChange <= 5) {
      if (!waterChangeActive && !circulationActive && !prefillActive) {
        if (debug) Serial.println(">> Starting water change process");
        waterChangeActive = true;
        waterChangeStage = 0;
      }
    }
  } else {
    // --- HARI NORMAL ---
    int preFillMinutes = (targetTime * 60 + targetMinute) - PRE_FILL_MINUTES;
    if (preFillMinutes < 0) preFillMinutes += 1440;
    
    int minutesUntilPreFill = getMinutesUntilNextSchedule(preFillMinutes / 60, preFillMinutes % 60);
    
    if (minutesUntilPreFill >= 0 && minutesUntilPreFill <= 5) {
      if (!prefillActive && !waterChangeActive && !circulationActive) {
        if (debug) Serial.println(">> Starting prefill process");
        prefillActive = true;
        prefillStarted = false;
        prefillStage = 0;
      }
    }
  }

  // 3. CEK JADWAL CIRCULATION UTAMA (Req 3c)
  if (minutesUntilTarget >= 0 && minutesUntilTarget <= 5) {
    if (!circulationActive && !prefillActive && !waterChangeActive) {
      if (debug) Serial.println(">> Starting circulation process by schedule");
      circulationActive = true;
      circulationStarted = false;
      circulationStage = 0;
      
      // --- TANDAI BAHWA INI ADALAH PROSES OTOMATIS ---
      circulationStartedAutomatically = true; // <-- KRITIS UNTUK OZONE
    }
  }
}

void STARTWATERCHANGE() {
  unsigned long now = millis();

  if (!waterChangeActive) return;
  
  // Deklarasi variabel di luar switch untuk menghindari error jump to case label
  bool flowSwitchState;
  bool currentFloatState;
  
  switch (waterChangeStage) {
    case 0: // DRAINING STAGE
      if (!graceActive && !flowOK) {
        graceActive = true;
        graceStart = now;

        digitalWrite(VALVE_DRAIN_PIN, HIGH);
        digitalWrite(PUMP_UV_PIN, HIGH);

        pulseCount = 0;
        lastPulseCount = 0;
        lastFlowCalcTime = now;
        lowFlowCount = 0;
        
        // Set tombol stop ke mode water change (pic 9) dan aktifkan blinking
        Serial2.print("cirActive.val=1");
        sendFF();
        Serial2.print("pStopCir.pic=9"); // ON state (biru)
        sendFF();
        Serial2.print("blinkingSC.val=1");
        sendFF();
        Serial2.print("tBlinkSC.en=1");
        sendFF();
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
              waterChangeStage = 2; // Error stage
            }
          }
        } else if (flowOK) {
          if (filteredFlowRate <= 0.1) {
            lowFlowCount++;
            if (lowFlowCount >= 5) {
              
              // Matikan aktuator draining
              digitalWrite(VALVE_DRAIN_PIN, LOW);
              digitalWrite(PUMP_UV_PIN, LOW);
              
              // Reset flag draining
              graceActive = false;
              flowOK = false;
              lowFlowCount = 0;
              
              // Inisialisasi untuk filling stage
              waterChangeStage = 1;
              prefillStarted = now;
              fullDetectedTimeGlobal = 0;
              prefillErrorActive = false;
            }
          } else {
            lowFlowCount = 0;
          }
        }
        lastFlowCalcTime = now;
      }
      break;
      
    case 1: // FILLING STAGE (integrated from prefill)
      // Cek flow switch untuk error
      flowSwitchState = digitalRead(FLOW_SWITCH_PIN);
      
      if (flowSwitchState == HIGH && !prefillErrorActive) {
        prefillErrorActive = true;
        waterChangeStage = 2; // Error stage
        return;
      }
      
      // Recovery dari error
      if (flowSwitchState == LOW && prefillErrorActive) {
        prefillErrorActive = false;
        
        // Lanjutkan pengisian
        digitalWrite(VALVE_INLET_PIN, HIGH);
      }
      
      if (prefillErrorActive) return;
      
      // Cek sensor float
      currentFloatState = readFloatSensor();
      
      if (currentFloatState == LOW) { // Air sudah terdeteksi penuh
        if (fullDetectedTimeGlobal == 0) {
          fullDetectedTimeGlobal = now;
        } else if (now - fullDetectedTimeGlobal >= FLOAT_DEBOUNCE_DELAY) {
          
          // Matikan valve inlet
          digitalWrite(VALVE_INLET_PIN, LOW);
          
          // Proses water change selesai
          waterChangeActive = false;
          waterChangeStage = 0;
          
          // Update last change date
          lastChangeDate = rtc.now();
          
          // Save to EEPROM immediately
          saveScheduleData();
          
          // Reset days counter
          updateNDaysOnAllPages(0);
          
          // Kembalikan tombol stop ke default (pic 8) dan matikan blinking
          Serial2.print("cirActive.val=0");
          sendFF();
          Serial2.print("blinkingSC.val=0");
          sendFF();
          Serial2.print("tBlinkSC.en=0");
          sendFF();
          Serial2.print("pStopCir.pic=8");
          sendFF();
          
          fullDetectedTimeGlobal = 0;
          
          if (debug) {
            Serial.println(">> Water change completed successfully");
          }
          
          return;
        }
      } else {
        fullDetectedTimeGlobal = 0;
      }
      
      // Jika filling belum dimulai, mulai sekarang
      if (digitalRead(VALVE_INLET_PIN) == LOW) {
        // Tunda sedikit sebelum mulai mengisi
        if (now - prefillStarted >= 500) {
          digitalWrite(VALVE_INLET_PIN, HIGH);
        }
      }
      break;
      
    case 2: // ERROR STAGE
      digitalWrite(VALVE_DRAIN_PIN, LOW);
      digitalWrite(VALVE_INLET_PIN, LOW);
      digitalWrite(PUMP_UV_PIN, LOW);
      
      // Set tombol stop ke mode error (pic 17) dan aktifkan blinking error
      Serial2.print("cirActive.val=2");
      sendFF();
      Serial2.print("pStopCir.pic=17"); // ERROR picture (merah)
      sendFF();
      Serial2.print("blinkingEC.val=1");
      sendFF();
      Serial2.print("tBlinkEC.en=1");
      sendFF();
      Serial2.print("blinkingSC.val=0");
      sendFF();
      Serial2.print("tBlinkSC.en=0");
      sendFF();
      
      waterChangeActive = false;
      waterChangeStage = 0;
      break;
  }
}

void STOPWATERCHANGEBYSENSOR() {
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);

  // Reset semua flag draining
  waterChangeActive = false;
  graceActive = false;
  flowOK = false;
  
  // Update Nextion
  Serial2.print("cirActive.val=0");
  sendFF();
  delay(100);
  Serial2.print("blinkingDM.val=0");
  sendFF();
  delay(100);
  Serial2.print("tBlinkDM.en=0");
  sendFF();
  delay(100);
  Serial2.print("pStopCir.pic=8");
  sendFF();
}


// ==========MODE PREFILL==========
void STARTPREFILL() {
  unsigned long now = millis();
  
  if (!prefillActive) {
    if (debug) {
      Serial.println(">> STARTPREFILL skipped - prefill not active");
    }
    return;
  }
  
  if (debug) {
    Serial.println(">> STARTPREFILL called");
    Serial.print(">> Prefill Stage: ");
    Serial.println(prefillStage);
  }
  
  // TAMBAHKAN PENGECEKAN KONFLIK
  if (fillingActive || drainingActive || (circulationActive && circulationStage == 1)) {
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    prefillActive = false;
    prefillStarted = false;
    prefillStage = 0;
    
    // Matikan blinking prefill di Nextion
    Serial2.print("cirActive.val=0");
    sendFF();
    Serial2.print("blinkingSC.val=0");
    sendFF();
    Serial2.print("tBlinkSC.en=0");
    sendFF();
    Serial2.print("pStopCir.pic=8");
    sendFF();
    
    if (debug) {
      Serial.println(">> Prefill stopped - conflict with other process");
    }
    return;
  }
  
  // Update Nextion untuk menunjukkan prefill aktif (HANYA di awal)
  if (prefillStage == 0 && !prefillStarted) {
    Serial2.print("cirActive.val=1"); // Set circulation active (untuk prefill)
    sendFF();
    Serial2.print("pStopCir.pic=9"); // ON state (biru)
    sendFF();
    Serial2.print("blinkingSC.val=1"); // Start blinking
    sendFF();
    Serial2.print("tBlinkSC.en=1"); // Enable blinking
    sendFF();
    
    if (debug) {
      Serial.println(">> Prefill started - Nextion updated");
    }
  }
  
  // Deklarasi variabel di luar switch untuk menghindari error jump to case label
  bool flowSwitchState;
  bool initialFloatState;
  bool currentFloatState;
  
  // Deteksi error setelah drain selesai
  if (prefillStage >= 1) {
    flowSwitchState = digitalRead(FLOW_SWITCH_PIN);
    
    if (flowSwitchState == HIGH && !prefillErrorActive) {
      prefillErrorActive = true;
      
      digitalWrite(VALVE_DRAIN_PIN, LOW);
      digitalWrite(VALVE_INLET_PIN, LOW);
      
      // Tampilkan error di Nextion
      Serial2.print("cirActive.val=2");
      sendFF();
      Serial2.print("pStopCir.pic=17"); // ERROR picture (merah)
      sendFF();
      Serial2.print("blinkingEC.val=1"); // Start error blinking
      sendFF();
      Serial2.print("tBlinkEC.en=1"); // Enable error blinking
      sendFF();
      Serial2.print("blinkingSC.val=0"); // Matikan normal blinking
      sendFF();
      Serial2.print("tBlinkSC.en=0");
      sendFF();
      
      if (debug) {
        Serial.println(">> Prefill error - flow switch problem");
      }
      
      return;
    }
    
    // Recovery error
    if (flowSwitchState == LOW && prefillErrorActive) {
      prefillErrorActive = false;
      
      Serial2.print("blinkingEC.val=0");
      sendFF();
      Serial2.print("tBlinkEC.en=0");
      sendFF();
      
      Serial2.print("cirActive.val=1");
      sendFF();
      Serial2.print("pStopCir.pic=9"); // Kembali ke ON state (biru)
      sendFF();
      Serial2.print("blinkingSC.val=1"); // Start normal blinking
      sendFF();
      Serial2.print("tBlinkSC.en=1");
      sendFF();
      
      // Lanjut ke stage 2
      prefillStage = 2;
      
      digitalWrite(VALVE_DRAIN_PIN, LOW);
      delay(500);
      digitalWrite(VALVE_INLET_PIN, HIGH);
      
      if (debug) {
        Serial.println(">> Prefill error recovered, continuing to stage 2");
      }
      
      return;
    }
  }

  if (prefillErrorActive) return;
  
  // Tambahkan pengecekan sensor float di semua stage
  currentFloatState = readFloatSensor();
  if (currentFloatState == LOW) { // Air sudah penuh, hentikan prefill di semua stage
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    
    prefillActive = false;
    prefillStarted = false;
    prefillStage = 0;
    
    // Matikan blinking prefill di Nextion
    Serial2.print("cirActive.val=0");
    sendFF();
    Serial2.print("blinkingSC.val=0");
    sendFF();
    Serial2.print("tBlinkSC.en=0");
    sendFF();
    Serial2.print("pStopCir.pic=8"); // OFF state
    sendFF();
    
    // Tandai prefill sudah dilakukan
    preFillExecuted = true;
    lastPreFillDate = rtc.now();
    saveScheduleData();
    
    if (debug) {
      Serial.println(">> Prefill stopped - water already full");
    }
    
    return;
  }
  
  switch (prefillStage) {
    case 0:
      initialFloatState = readFloatSensor();
      
      if (initialFloatState) { // Air masih rendah
        digitalWrite(VALVE_DRAIN_PIN, LOW);
        digitalWrite(VALVE_INLET_PIN, LOW);
        delay(100);
        
        digitalWrite(VALVE_DRAIN_PIN, HIGH);
        drainStartTime = now;
        prefillStarted = true;
        prefillStage = 1;
        
        if (debug) {
          Serial.println(">> Prefill stage 1 - draining started");
        }
      }
      break;
      
    case 1:
      if (now - drainStartTime >= 5000) {
        digitalWrite(VALVE_DRAIN_PIN, LOW);
        delay(500);
        
        digitalWrite(VALVE_INLET_PIN, HIGH);
        prefillStage = 2;
        
        if (debug) {
          Serial.println(">> Prefill stage 2 - filling started");
        }
      }
      break;
      
    case 2:
      currentFloatState = readFloatSensor();
      flowSwitchState = digitalRead(FLOW_SWITCH_PIN);

      if (flowSwitchState == HIGH && !prefillErrorActive) {
        prefillErrorActive = true;
    
        digitalWrite(VALVE_INLET_PIN, LOW);
    
        Serial2.print("cirActive.val=2");
        sendFF();
        Serial2.print("pStopCir.pic=17"); // ERROR picture (merah)
        sendFF();
        Serial2.print("blinkingEC.val=1"); // Start error blinking
        sendFF();
        Serial2.print("tBlinkEC.en=1"); // Enable error blinking
        sendFF();
        Serial2.print("blinkingSC.val=0"); // Matikan normal blinking
        sendFF();
        Serial2.print("tBlinkSC.en=0");
        sendFF();
    
        if (debug) {
          Serial.println(">> Prefill error - flow switch problem during filling");
        }
    
        return;
      }

      if (currentFloatState == LOW) { // Air sudah penuh
        // Kurangi debounce delay untuk sensor float
        if (fullDetectedTimeGlobal == 0) {
          fullDetectedTimeGlobal = now;
        } else if (now - fullDetectedTimeGlobal >= 500) {  // Kurangi dari 1000ms menjadi 500ms
          digitalWrite(VALVE_INLET_PIN, LOW);
          
          // Prefill selesai sampai stage 2
          prefillActive = false;
          prefillStarted = false;
          prefillStage = 0;
          
          // Tandai prefill sudah dilakukan
          preFillExecuted = true;
          lastPreFillDate = rtc.now();
          saveScheduleData();

          fullDetectedTimeGlobal = 0;
          
          if (debug) {
            Serial.println(">> Prefill completed successfully");
          }
          
          return;
        }
      } else {
        fullDetectedTimeGlobal = 0;
      }
      break;
  }
}

// ==========MODE CIRCULATION==========
void STARTCIRCULATION () {
    unsigned long now = millis();
    
    if (debug) {
        Serial.println(">> STARTCIRCULATION called");
        Serial.print(">> circulationActive: ");
        Serial.println(circulationActive ? "TRUE" : "FALSE");
        Serial.print(">> circulationStartedAutomatically: ");
        Serial.println(circulationStartedAutomatically ? "TRUE" : "FALSE");
        Serial.print(">> circulationStage: ");
        Serial.println(circulationStage);
    }
    
    if (!circulationActive) {
        if (debug) Serial.println(">> STARTCIRCULATION skipped - circulation not active");
        return;
    }
    
    // Jika prefill sedang aktif, jalankan prefill dulu
    if (prefillActive) {
        if (debug) Serial.println(">> Prefill is active, running prefill process");
        STARTPREFILL();
        
        // Jika prefill sudah selesai, lanjut ke circulation
        if (!prefillActive) {
            if (debug) Serial.println(">> Prefill completed, moving to circulation stage 1");
            circulationStage = 1; // Set stage ke 1 untuk cooling
            sistemAktif = false; // Reset sistem aktif untuk memastikan inisialisasi ulang
        }
        return; // Jangan lanjut ke circulation selama prefill masih aktif
    }
    
    // Cek apakah perlu prefill dulu
    if (circulationStage == 0) {
        bool initialFloatState = readFloatSensor();
        
        if (debug) {
            Serial.print(">> Circulation Stage 0 - Float Sensor State: ");
            Serial.println(initialFloatState ? "LOW WATER" : "HIGH WATER");
        }
        
        if (initialFloatState) { // Air masih rendah
            // Jalankan prefill dulu
            if (debug) Serial.println(">> Water level low, starting prefill");
            
            // Set prefill active
            prefillActive = true;
            prefillStarted = false;
            prefillStage = 0;
            
            // Panggil STARTPREFILL sekali saja
            STARTPREFILL();
            
            // Jangan lanjut ke stage berikutnya, tunggu PREFILL selesai
            return;
        } else {
            // Air sudah penuh, langsung ke stage 1 (cooling)
            if (debug) Serial.println(">> Water level OK, starting cooling");
            
            circulationStage = 1;
            sistemAktif = false; // Reset sistem aktif untuk mempastikan inisialisasi ulang
            
            // Update Nextion untuk menunjukkan circulation aktif
            Serial2.print("cirActive.val=1"); // Set circulation active
            sendFF();
            Serial2.print("pStopCir.pic=9"); // ON state (biru)
            sendFF();
            Serial2.print("blinkingSC.val=1"); // Start blinking
            sendFF();
            Serial2.print("tBlinkSC.en=1"); // Enable blinking
            sendFF();
        }
    }
    
    // Lanjutkan logika circulation untuk stage 1 (cooling)
    if (circulationStage == 1) {
        if (debug) {
            Serial.println(">> Circulation Stage 1 (Cooling) - Checking system status");
            Serial.print(">> sistemAktif: ");
            Serial.println(sistemAktif ? "TRUE" : "FALSE");
        }
        
        // =======================================================
        // === LOGIKA UPDATE DURASI COOLING (TAMBAHKAN INI) ===
        // =======================================================
        // Perbarui durasi setiap 1 detik agar lebih responsif
        static unsigned long lastCoolDurUpdate = 0;
        if (now - lastCoolDurUpdate >= 1000) {
            lastCoolDurUpdate = now;
            
            // Hitung durasi dalam menit
            unsigned long circulationMinutes = (now - circulationStartTime) / 60000;
            
            // Kirim ke Nextion
            Serial2.print("nCoolDurAuto.val=");
            Serial2.print(circulationMinutes);
            sendFF();
        }
        // =======================================================
        
        if (now - waktuTerakhirCek >= intervalCek) {
            waktuTerakhirCek = now;
            
            if (!sistemAktif) {
                if (debug) Serial.println(">> Initializing circulation system...");
                
                // Set flag sistem aktif di awal
                sistemAktif = true;
                
                // --- AKTIVASI BERSAMAAN ---
                // 1. Nyalakan Pompa UV
                digitalWrite(PUMP_UV_PIN, HIGH);
                
                // 2. Nyalakan Ozone HANYA jika mode auto dan belum pernah dihidupkan
                if (circulationStartedAutomatically && !ozoneInitializedForThisCycle) {
                    ozoneActive = true;
                    ozoneStartTime = millis();
                    ozoneInitializedForThisCycle = true;
                    digitalWrite(OZONE_PIN, HIGH);
                    if (debug) Serial.println(">> Ozone turned ON by schedule!");
                }
                
                // 3. Nyalakan Kompresor jika suhu di atas target
                float selisih = currentTemp - autoTempTarget;
                if (selisih > 0) {
                    digitalWrite(COMPRESSOR_PIN, HIGH);
                    statusKompresor = true;
                    lastCompressorChange = now;
                    if (debug) {
                        Serial.println(">> Compressor turned ON immediately");
                    }
                }
                // --- AKHIR AKTIVASI BERSAMAAN ---
                
                // Inisialisasi variabel lainnya
                initialCoolingMode = true;
                targetReached = false;
                circulationStartTime = millis();
                lastUpdateTime = millis();
                
                // Reset variabel stabilisasi
                lastCompressorChange = now;
                lastStableTemp = currentTemp;
                lastTempStableTime = now;
                
                // Reset auto cooling duration
                Serial2.print("nCoolDurAuto.val=0");
                sendFF();
                
                if (debug) {
                    Serial.println(">> Circulation system activated");
                }
            }
            
            // --- LOGIKA PEMATIAN OZONE (DIPERIKSA TERUS MENERUS) ---
            if (ozoneActive && (millis() - ozoneStartTime >= OZONE_DURATION)) {
                digitalWrite(OZONE_PIN, LOW);
                ozoneActive = false;
                if (debug) Serial.println(">> Ozone turned OFF after 15 minutes.");
            }
            
            float selisih = currentTemp - autoTempTarget;
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
                            Serial.println(">> Compressor turned ON");
                        }
                    }
                } else {
                    // MODIFIKASI: Matikan jika bagian integer sudah mencapai target
                    if (statusKompresor && currentTempInt <= autoTempTarget) {
                        digitalWrite(COMPRESSOR_PIN, LOW);
                        statusKompresor = false;
                        lastCompressorChange = now;
                        
                        if (debug) {
                            Serial.print(">> Compressor turned OFF - target reached (");
                            Serial.print(currentTemp);
                            Serial.print("°C, integer part ");
                            Serial.print(currentTempInt);
                            Serial.print("°C <= ");
                            Serial.print(autoTempTarget);
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
                            Serial.print(autoTempTarget + histeresis);
                            Serial.println("°C)");
                        }
                    }
                } 
                // MODIFIKASI: Matikan jika bagian integer sudah mencapai target
                else if (currentTempInt <= autoTempTarget) {
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
                            Serial.print(autoTempTarget);
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
                    Serial.print(autoTempTarget);
                    Serial.print("°C, Difference: ");
                    Serial.print(selisih);
                    Serial.print("°C, Compressor: ");
                    Serial.println(statusKompresor ? "ON" : "OFF");
                }
              }
        }
    }
}

// ==========STOP PREFILL & CIRCULATION==========
void STOPPREFILLCIRCULATION() {
    if (debug) {
        Serial.println(">> STOPPREFILLCIRCULATION called");
    }
    
    // Matikan semua aktuator
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    digitalWrite(OZONE_PIN, LOW);

    // Reset semua variabel prefill
    prefillActive = false;
    prefillStarted = false;
    prefillStage = 0;
    prefillErrorActive = false;
    
    // Reset semua variabel circulation
    circulationActive = false;
    circulationStarted = false;
    circulationStage = 0;
    circulationErrorActive = false;
    sistemAktif = false;
    coolingActive = false;
    manualCirculationOn = false;
    manualCirculationOff = true;
    circulationStatus = 0;
    statusKompresor = false;
    ozoneActive = false;
    ozoneInitializedForThisCycle = false;
    
    // Update Nextion - matikan semua blinking (kirim ke halaman automenu)
    Serial2.print("automenu.cirActive.val=0"); // <-- DIPERBAIKI
    sendFF();
    delay(100);
    Serial2.print("automenu.blinkingSC.val=0"); // <-- DIPERBAIKI
    sendFF();
    delay(100);
    Serial2.print("automenu.tBlinkSC.en=0");    // <-- DIPERBAIKI
    sendFF();
    delay(100);
    Serial2.print("automenu.pStopCir.pic=8");   // <-- DIPERBAIKI
    sendFF();
    
    // Matikan juga error blinking jika aktif
    Serial2.print("automenu.blinkingEC.val=0"); // <-- DIPERBAIKI
    sendFF();
    Serial2.print("automenu.tBlinkEC.en=0");    // <-- DIPERBAIKI
    sendFF();
    
    if (debug) {
        Serial.println(">> Circulation system stopped");
    }
}

void CIRCULATIONSAFETY() {
  // Cek apakah circulation sedang aktif
  if (!circulationActive) return;
  
  // Hitung durasi circulation
  unsigned long circulationDuration = millis() - circulationStartTime;
  
  // Konversi ke jam (3 jam = 10800000 ms)
  if (circulationDuration >= 10800000) { // 3 jam
    
    // Matikan semua perangkat
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    
    // Reset semua status
    circulationActive = false;
    circulationStarted = false;
    circulationStage = 0;
    circulationErrorActive = false;
    sistemAktif = false;
    
    // Update Nextion - tampilkan pic 8 dan matikan blinking
    Serial2.print("cirActive.val=0");
    sendFF();
    delay(100);
    Serial2.print("blinkingSC.val=0");
    sendFF();
    delay(100);
    Serial2.print("tBlinkSC.en=0");
    sendFF();
    delay(100);
    Serial2.print("pStopCir.pic=8");
    sendFF();
  }
}

// Fungsi untuk menghitung menit sampai jadwal berikutnya
int getMinutesUntilNextSchedule(int targetHour, int targetMinute) {
  DateTime now = rtc.now();
  
  // Validasi input
  if (targetHour < 0 || targetHour > 23 || targetMinute < 0 || targetMinute > 59) {
    if (debug) {
      Serial.print(">> Invalid target time: ");
      Serial.print(targetHour);
      Serial.print(":");
      Serial.println(targetMinute);
    }
    return -1; // Return -1 untuk menandakan error
  }
  
  // Waktu target di hari yang sama
  DateTime targetToday = DateTime(now.year(), now.month(), now.day(), targetHour, targetMinute, 0);
  
  // Hitung selisih waktu untuk hari ini
  TimeSpan diffToday = targetToday - now;
  
  // Debug
  if (debug) {
    Serial.print(">> Now: ");
    Serial.print(now.hour());
    Serial.print(":");
    if (now.minute() < 10) Serial.print("0");
    Serial.print(now.minute());
    Serial.print(" | Target: ");
    Serial.print(targetHour);
    Serial.print(":");
    if (targetMinute < 10) Serial.print("0");
    Serial.print(targetMinute);
    Serial.print(" | Diff Today (seconds): ");
    Serial.println(diffToday.totalseconds());
  }
  
  // Jika waktu target hari ini sudah lewat, gunakan hari besok
  if (diffToday.totalseconds() <= 0) {
    DateTime targetTomorrow = targetToday + TimeSpan(1, 0, 0, 0); // Tambah 1 hari
    TimeSpan diffTomorrow = targetTomorrow - now;
    
    if (debug) {
      Serial.print(">> Using tomorrow. Diff Tomorrow (seconds): ");
      Serial.println(diffTomorrow.totalseconds());
    }
    
    return diffTomorrow.totalseconds() / 60;
  } else {
    if (debug) {
      Serial.print(">> Using today. Diff Today (seconds): ");
      Serial.println(diffToday.totalseconds());
    }
    
    return diffToday.totalseconds() / 60;
  }
}