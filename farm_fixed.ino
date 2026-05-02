#include <DHT.h>
#include <SoftwareSerial.h>

SoftwareSerial btSerial(9, 10);
#define DHTPIN   2
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

const int mq2Pin  = A1;
const int soilPin = A0;
const int fanPin  = 4;
const int pumpPin = 3;  

String btCommand  = "";
bool   btDataReady = false;
unsigned long lastSensorRead = 0; 
unsigned long lastReport     = 0;
const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long REPORT_INTERVAL = 5000;
float lastTemp = 0, lastHum = 0;

// Added flags to prevent auto-logic from overriding manual Bluetooth commands
bool manualFan = false;
bool manualPump = false;

// ════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] Starting SMART FARMER System..."));
  
  pinMode(fanPin,  OUTPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(fanPin,  LOW);
  digitalWrite(pumpPin, LOW);
  
  pinMode(mq2Pin, INPUT);
  pinMode(soilPin, INPUT);

  dht.begin();
  btSerial.begin(9600);
  
  Serial.println(F("[BOOT] DHT + BT ready"));
  Serial.println(F("[BOOT] Waiting for commands..."));
}

void readBluetooth() {
  while (btSerial.available()) {
    char c = btSerial.read();
    
    if (c != '\n' && c != '\r') {
      if (btCommand.length() < 32) {
        btCommand += c;
      }
    }

   
    if (btCommand == "F1" || btCommand == "F0" || btCommand == "P1" || btCommand == "P0" || btCommand == "STATUS") {
      btDataReady = true;
    }
  }
}

void processCommand() {
  if (!btDataReady) return;
  
  btDataReady = false;
  btCommand.trim();
  btCommand.toUpperCase();

  Serial.print(F("[CMD] Got: "));
  Serial.println(btCommand);

  if (btCommand == "F1") {
    digitalWrite(fanPin, HIGH);
    manualFan = true; 
    btSerial.println(F("OK:F1"));
    Serial.println(F("[CMD] Fan ON (Manual)"));

  } else if (btCommand == "F0") {
    digitalWrite(fanPin, LOW);
    manualFan = false; 
    btSerial.println(F("OK:F0"));
    Serial.println(F("[CMD] Fan OFF (Auto restored)"));

  } else if (btCommand == "P1") {
    digitalWrite(pumpPin, HIGH);
    manualPump = true; 
    btSerial.println(F("OK:P1"));
    Serial.println(F("[CMD] Pump ON (Manual)"));

  } else if (btCommand == "P0") {
    digitalWrite(pumpPin, LOW);
    manualPump = false;
    btSerial.println(F("OK:P0"));
    Serial.println(F("[CMD] Pump OFF (Auto restored)"));

  } else if (btCommand == "STATUS") {
    int gas  = analogRead(mq2Pin);
    int soil = analogRead(soilPin);
    bool fanOn  = digitalRead(fanPin);
    bool pumpOn = digitalRead(pumpPin);

    String report = String(lastTemp, 1) + "," +
                    String(lastHum,  1) + "," +
                    String(gas)         + "," +
                    String(soil)        + "," +
                    (fanOn  ? "1" : "0") + "," +
                    (pumpOn ? "1" : "0");

    btSerial.println(report);
    Serial.print(F("[CMD] STATUS sent: "));
    Serial.println(report);

  } else {
    btSerial.println(F("ERR:UNKNOWN"));
    Serial.print(F("[CMD] Unknown: "));
    Serial.println(btCommand);
  }

  btCommand = ""; 
}

void runFarmLogic() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) lastTemp = t;
  if (!isnan(h)) lastHum  = h;

  int gas  = analogRead(mq2Pin);
  int soil = analogRead(soilPin);

  if (!manualFan) {
    if (lastTemp > 30 || lastHum > 60 || gas > 400) {
      digitalWrite(fanPin, HIGH);
    } else {
      digitalWrite(fanPin, LOW);
    }
  }

  if (!manualPump) {
    if (soil < 400) {
      digitalWrite(pumpPin, HIGH);
    } else {
      digitalWrite(pumpPin, LOW);
    }
  }

  Serial.print(F("[DATA] Soil: "));
  Serial.print(soil);
  Serial.print(F(" | Gas: "));
  Serial.println(gas);

  unsigned long now = millis();
  if (now - lastReport >= REPORT_INTERVAL) {
    bool fanOn  = digitalRead(fanPin);
    bool pumpOn = digitalRead(pumpPin);

    String report = String(lastTemp, 1) + "," +
                    String(lastHum,  1) + "," +
                    String(gas)         + "," +
                    String(soil)        + "," +
                    (fanOn  ? "1" : "0") + "," +
                    (pumpOn ? "1" : "0");

    btSerial.println(report);
    Serial.print(F("[AUTO] Sent: "));
    Serial.println(report);

    lastReport = now;
  }
}

void loop() {
  readBluetooth();
  processCommand();
  
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    runFarmLogic();
    lastSensorRead = now;
  }
}