//=====SENSORS=====

unsigned long lastFlowRead = 0;
bool timeSettingMode = false;
int timeSettingStep = 0; // 0=waiting, 1=setting hour, 2=setting minute
int tempHour = 0;
int tempMinute = 0;

// Setup sensors
void setupSensors() {
  // Setup sensor pins
  pinMode(FLOAT_SENSOR_PIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(FLOW_SWITCH_PIN, INPUT_PULLUP);  // Tambahkan flow switch
  pinMode(TDS_SENSOR_PIN, INPUT);
  
  // Setup actuators
  pinMode(VALVE_DRAIN_PIN, OUTPUT);
  pinMode(VALVE_INLET_PIN, OUTPUT);
  pinMode(COMPRESSOR_PIN, OUTPUT);
  pinMode(PUMP_UV_PIN, OUTPUT);
  
  // Setup flow sensor interrupt
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, RISING);
  
  // Setup temperature sensor
  sensors.begin();
  
  // Inisialisasi array filter flow rate
  for (int i = 0; i < FLOW_SAMPLES; i++) {
    flowReadings[i] = 0;
  }
  
  // Set all outputs to LOW
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
  
  // Reset flow sensor variables
  pulseCount = 0;
  lastPulseCount = 0;
  lastPulseCountAuto = 0;
  lastFlowCalcTime = 0;
  lastFlowCalcTimeAuto = 0;
}

// Interrupt service routine for flow sensor
void IRAM_ATTR flowISR() {
  unsigned long now = micros();
  if (now - lastInterruptTime > DEBOUNCE_TIME * 1000) {
    pulseCount++;
    lastInterruptTime = now;
  }
}

// Handle time setting via Serial Monitor
void handleTimeSetting() {
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    
    if (input.equalsIgnoreCase("SETTIME")) {
      timeSettingMode = true;
      timeSettingStep = 1;
      Serial.println(">> TIME SETTING MODE ACTIVATED");
      Serial.println(">> Enter hour (0-23):");
      return;
    }
    
    if (timeSettingMode) {
      int value = input.toInt();
      
      switch (timeSettingStep) {
        case 1: // Setting hour
          if (value >= 0 && value <= 23) {
            tempHour = value;
            timeSettingStep = 2;
            Serial.println(">> Hour set to: " + String(tempHour));
            Serial.println(">> Enter minute (0-59):");
          } else {
            Serial.println(">> Invalid hour! Enter hour (0-23):");
          }
          break;
          
        case 2: // Setting minute
          if (value >= 0 && value <= 59) {
            tempMinute = value;
            timeSettingStep = 3;
            Serial.println(">> Minute set to: " + String(tempMinute));
            Serial.println(">> Confirm time " + String(tempHour) + ":" + 
                         (tempMinute < 10 ? "0" : "") + String(tempMinute) + "? (Y/N)");
          } else {
            Serial.println(">> Invalid minute! Enter minute (0-59):");
          }
          break;
          
        case 3: // Confirmation
          if (input.equalsIgnoreCase("Y") || input.equalsIgnoreCase("YES")) {
            // Set RTC time
            DateTime now = rtc.now();
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), 
                              tempHour, tempMinute, 0));
            Serial.println(">> Time successfully set to: " + 
                         String(tempHour) + ":" + 
                         (tempMinute < 10 ? "0" : "") + String(tempMinute));
            timeSettingMode = false;
            timeSettingStep = 0;
          } else if (input.equalsIgnoreCase("N") || input.equalsIgnoreCase("NO")) {
            Serial.println(">> Time setting cancelled");
            timeSettingMode = false;
            timeSettingStep = 0;
          } else {
            Serial.println(">> Please enter Y/YES or N/NO");
          }
          break;
      }
    }
  }
}

// Read sensors
void readSensors() {
  // ================= TEMPERATURE SENSOR =================
  sensors.requestTemperatures();
  currentTemp = sensors.getTempCByIndex(0);
  
  // Handle DS18B20 error codes
  if (currentTemp == -127.00) {
    currentTemp = -99.0; // Error indicator
  } 
  else if (currentTemp == 85.00) {
    currentTemp = -99.0; // Error indicator
  }
  
  // ================= FLOW SENSOR (FS300A) =================
  // Kurangi interval pembacaan menjadi 500ms untuk update lebih cepat
  static unsigned long lastFlowRead = 0;
  if (millis() - lastFlowRead >= 500) {  // Ubah dari 1000 menjadi 500
    noInterrupts();
    unsigned long pulses = pulseCount - lastPulseCount;
    lastPulseCount = pulseCount;
    interrupts();
    
    // Calculate flow rate untuk FS300A
    if (pulses == 0) {
      currentFlowRate = 0.0; // No flow
    } else {
      // Rumus: (pulses * 60) / kalibrasi
      currentFlowRate = (pulses * 60.0) / FS300A_CALIBRATION;
      
      // Batasi nilai maksimal (sesuai spesifikasi FS300A)
      if (currentFlowRate > FLOW_MAX_RATE) currentFlowRate = FLOW_MAX_RATE;
    }
    
    lastFlowRead = millis();
  }
  
  // ================= TDS SENSOR (DENGAN KOMPENSASI SUHU) =================
  int raw = analogRead(TDS_SENSOR_PIN);
  float voltage = raw * (3.3 / 4095.0);

  // Validasi TDS sensor
  if (raw < 100 || raw > 4000) { // Sensor disconnected or short circuit
    tdsValue = -1; // Error indicator
  }
  else if (voltage < 0.1 || voltage > 3.2) { // Voltage out of range
    tdsValue = -1; // Error indicator
  }
  else {
    // --- KALKULASI DENGAN KOMPENSASI SUHU ---
  
    // 1. Hitung EC pada suhu saat ini
    float ec = (133.42 * voltage * voltage * voltage
             - 255.86 * voltage * voltage
             + 857.39 * voltage);

    // 2. Kompensasi ke suhu referensi (25°C)
    float referenceTemp = 25.0; // Suhu referensi standar
    float temperatureCoefficient = 0.02; // Koefisien suhu standar (2% per °C)
  
    // Pastikan currentTemp valid
    if (currentTemp > -50.0 && currentTemp < 80.0) {
      float compensatedEC = ec / (1.0 + temperatureCoefficient * (currentTemp - referenceTemp));
      tdsValue = compensatedEC * 0.5; // Konversi ke PPM
    } else {
      // Jika suhu error, gunakan EC tanpa kompensasi
      tdsValue = ec * 0.5;
    }
  
    // Validasi nilai akhir
    if (tdsValue < 0) tdsValue = 0;
    if (tdsValue > 9999) tdsValue = 9999; // Maksimal 4 digit untuk display
  }
}

// Fungsi untuk membaca float sensor - LOGIKA SUDAH DIBALIK
bool readFloatSensor() {
  // Baca nilai aktual dari pin
  bool sensorState = digitalRead(FLOAT_SENSOR_PIN);
  
  // Debug
  if (debug) {
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug >= 2000) {
      Serial.print(">> Float Sensor Raw Pin: ");
      Serial.print(sensorState ? "HIGH" : "LOW");
      Serial.print(" | Interpretation (NEW): ");
      // Interpretasi baru setelah logika dibalik
      Serial.println(sensorState ? "HIGH WATER" : "LOW WATER");
      lastDebug = millis();
    }
  }
  
  // KEMBALIKAN NILAI YANG SUDAH DIBALIK
  // Jika pin HIGH (mengambang), kita anggap sebagai AIR PENUH (true)
  // Jika pin LOW (tenggelam), kita anggap sebagai AIR RENDAH (false)
  return sensorState; // <-- INI ADALAH PERUBAHAN UTAMA
}

// Fungsi untuk membaca flow switch
bool readFlowSwitch() {
  return digitalRead(FLOW_SWITCH_PIN);
}

// Debug float sensor
void debugFloatSensor() {
  bool floatState = readFloatSensor();
  
  if (debug) {
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug >= 2000) {
      Serial.print(">> Float Sensor: ");
      Serial.print(floatState ? "HIGH (Low Water)" : "LOW (High Water)");
      Serial.print(" | Raw Pin: ");
      Serial.println(digitalRead(FLOAT_SENSOR_PIN));
      lastDebug = millis();
    }
  }
  
  // Detect if sensor hasn't changed for a long time
  static bool lastFloatState = HIGH;
  static unsigned long lastFloatChange = 0;
  
  if (floatState != lastFloatState) {
    lastFloatState = floatState;
    lastFloatChange = millis();
  } else if (millis() - lastFloatChange > 30000 && debug) {
    Serial.println(">> WARNING: Float sensor unchanged for 30 seconds!");
  }
}

// Setup temperature sensor
void setupTemperatureSensor() {
  sensors.begin();
  
  // Check if sensor is detected
  if (!sensors.getAddress(tempDeviceAddress, 0)) {
    Serial.println(">> ERROR: Cannot find DS18B20 temperature sensor address!");
  } else {
    Serial.print(">> DS18B20 sensor detected at address: ");
    for (uint8_t i = 0; i < 8; i++) {
      if (tempDeviceAddress[i] < 16) Serial.print("0");
      Serial.print(tempDeviceAddress[i], HEX);
    }
    Serial.println();
    
    // Set sensor resolution to 12-bit for maximum accuracy
    sensors.setResolution(tempDeviceAddress, 12);
    
    // Read initial temperature for testing
    sensors.requestTemperatures();
    float testTemp = sensors.getTempCByIndex(0);
    Serial.print(">> Initial temperature: ");
    Serial.print(testTemp);
    Serial.println("°C");
  }
}

// Test sensors
void testSensors() {
  Serial.println("=== TESTING SENSORS ===");
  
  // Test flow sensor
  Serial.print("FS300A Flow Sensor: ");
  Serial.print("Pulses: ");
  Serial.print(pulseCount);
  Serial.print(" | Expected Calibration: ");
  Serial.println(FS300A_CALIBRATION);
  
  // Test flow switch
  bool flowSwitchState = readFlowSwitch();
  Serial.print("Flow Switch: ");
  Serial.println(flowSwitchState ? "HIGH (Water/Pressure Detected)" : "LOW (No Water/Pressure)");
  
  // Test float sensor
  bool floatState = readFloatSensor();
  Serial.print("Float Sensor: ");
  Serial.println(floatState ? "HIGH (Low Water)" : "LOW (High Water)");
  
  // Test temperature sensor
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  Serial.print("Temperature Sensor: ");
  if (temp == -127.00) {
    Serial.println("NOT CONNECTED");
  } else if (temp == 85.00) {
    Serial.println("ERROR");
  } else {
    Serial.print(temp);
    Serial.println("°C");
  }
  
  // Test TDS sensor
  int tdsRaw = analogRead(TDS_SENSOR_PIN);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  Serial.print("TDS Sensor: ");
  Serial.print(tdsRaw);
  Serial.print(" (");
  Serial.print(tdsVoltage);
  Serial.println("V)");
  
  Serial.println("=====================");
}

// Perbaikan getAverageFlowRate untuk FS300A
float getAverageFlowRate(float newReading) {
  // Validasi pembacaan
  if (newReading < 0) newReading = 0;
  if (newReading > FLOW_MAX_RATE) newReading = FLOW_MAX_RATE; // Gunakan konstanta
  
  // Simpan ke array
  flowReadings[flowIndex] = newReading;
  flowIndex = (flowIndex + 1) % FLOW_SAMPLES;
  
  // Hitung rata-rata
  float sum = 0;
  int count = 0;
  for (int i = 0; i < FLOW_SAMPLES; i++) {
    if (flowReadings[i] > 0) {
      sum += flowReadings[i];
      count++;
    }
  }
  
  return (count > 0) ? sum / count : 0;
}