/*
 * LoRa Sensor — Arduino Pro Mini (led och regulator borttagen) + RFM95W + DS18B20 — low power version
 *
 * Skickar temperatur vid uppstart och sedan var ~15:e minut.
* Anpassad för hård sleep:
 *   - DS18B20 power på D7
 *   - DS18B20 DQ på D4
 *   - 4.7k pullup mellan D4 och D7
 *   - D4 hålls LOW i sleep
 *   - D7 hålls LOW i sleep
 */

#include <SPI.h>
#include <LoRa.h>
#include <sha256.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LowPower.h>
#include <avr/power.h>

// ===== App / auth =====

const char* SHARED_SECRET = <SECRET_NETW_KEY>;
const char* MY_ID  = "sensor1";
const char* HUB_ID = "hub";

// ===== Pins =====

#define LORA_CS       10
#define LORA_RST       9
#define LORA_DIO0      2

#define ONEWIRE_PIN    4
#define DS18B20_PWR    7

// Sätts LOW i sleep om du tidigare använt D6 för sensorpower.
// Skadar inte om D6 är okopplad.
#define OLD_DS18B20_PWR 6

// ===== Sleep timing =====

// 113 × 8 s ≈ 904 s ≈ 15 minuter
const uint16_t SLEEP_CYCLES_8S = 113;

// ===== LoRa config =====

const long LORA_FREQUENCY = 868E6;
const int  LORA_SF = 7;
const long LORA_BW = 125E3;
const int  LORA_CR = 5;
const uint8_t LORA_SYNC_WORD = 0xA5;
const int  LORA_TX_POWER = 17;

// ===== EEPROM nonce =====

const int ADDR_TX_NONCE = 0;
const uint32_t NONCE_SAVE_INTERVAL = 100;

uint32_t txNonce = 1;

// ===== DS18B20 =====

OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// ===== EEPROM =====

uint32_t readNonceFromEeprom() {
  uint32_t n = 0;

  for (int i = 0; i < 4; i++) {
    n |= ((uint32_t)EEPROM.read(ADDR_TX_NONCE + i)) << (i * 8);
  }

  if (n == 0xFFFFFFFF) {
    n = 0;
  }

  return n;
}

void writeNonceToEeprom(uint32_t n) {
  for (int i = 0; i < 4; i++) {
    EEPROM.update(ADDR_TX_NONCE + i, (n >> (i * 8)) & 0xFF);
  }
}

void nonceSetup() {
  uint32_t saved = readNonceFromEeprom();

  // Hoppa fram för att undvika nonce-reuse efter reset.
  txNonce = saved + NONCE_SAVE_INTERVAL;

  writeNonceToEeprom(txNonce);
}

// ===== Crypto =====

String computeHmac(const String& data) {
  Sha256.initHmac((const uint8_t*)SHARED_SECRET, strlen(SHARED_SECRET));
  Sha256.print(data);

  uint8_t* hash = Sha256.resultHmac();

  String hex = "";
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 0x10) {
      hex += "0";
    }
    hex += String(hash[i], HEX);
  }

  return hex;
}

// ===== Low-power pin states =====

void sensorSleepPinsHard() {
  /*
   * Sleep-state för DS18B20.
   *
   * Detta är säkert när 4.7k pullup sitter mellan D4 och D7.
   * Använd INTE detta om pullupen sitter mellan D4 och permanent VCC,
   * eftersom D4 då skulle dra ström genom pullupen.
   */

  digitalWrite(ONEWIRE_PIN, LOW);
  pinMode(ONEWIRE_PIN, OUTPUT);     // DQ hårt LOW i sleep

  digitalWrite(DS18B20_PWR, LOW);
  pinMode(DS18B20_PWR, OUTPUT);     // sensor VDD LOW

  digitalWrite(OLD_DS18B20_PWR, LOW);
  pinMode(OLD_DS18B20_PWR, OUTPUT); // gammal D6 också LOW
}

void sensorWakePins() {
  /*
   * Gör DS18B20 redo.
   * D7 HIGH gör både VDD och externa pullupen HIGH.
   * D4 ska inte ha intern pullup.
   */

  digitalWrite(ONEWIRE_PIN, LOW);
  pinMode(ONEWIRE_PIN, INPUT);      // släpp DQ, ingen intern pullup

  pinMode(DS18B20_PWR, OUTPUT);
  digitalWrite(DS18B20_PWR, HIGH);

  delay(10);
}

void radioSleep() {
  LoRa.sleep();
  delay(10);

  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);

  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);

  // Lämna SPI-pinnar i kända lägen
  pinMode(MOSI, OUTPUT);
  digitalWrite(MOSI, LOW);

  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);

  digitalWrite(MISO, LOW);
  pinMode(MISO, INPUT);

  digitalWrite(LORA_DIO0, LOW);
  pinMode(LORA_DIO0, INPUT);
}

void radioWake() {
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);

  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);

  pinMode(MOSI, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  LoRa.idle();
}

void avrSleepPrepare() {
  // ADC av
  ADCSRA &= ~_BV(ADEN);
  power_adc_disable();

  // TWI/I2C av
  power_twi_disable();

  // UART av
  UCSR0B = 0;
  power_usart0_disable();

  // RX/TX high-Z
  digitalWrite(0, LOW);
  pinMode(0, INPUT);

  digitalWrite(1, LOW);
  pinMode(1, INPUT);

  // Analog comparator av
  ACSR |= _BV(ACD);
}

void prepareForSleep() {
  sensorSleepPinsHard();
  radioSleep();
  avrSleepPrepare();
}

// ===== Sensor =====

void initSensor() {
  sensorWakePins();

  sensors.begin();
  sensors.setResolution(10);   // 10-bit ≈ 187 ms conversion

  sensorSleepPinsHard();
}

float readTemperature() {
  sensorWakePins();

  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  sensorSleepPinsHard();

  return temp;
}

// ===== LoRa =====

void initLoRa() {
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    // Om radion inte startar, gå inte vidare.
    // Ingen Serial här, så bara häng.
    while (true) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.enableCrc();

  radioSleep();
}

void sendData(float temperature) {
  uint32_t n = txNonce++;

  if (txNonce % NONCE_SAVE_INTERVAL == 0) {
    writeNonceToEeprom(txNonce);
  }

  char tempStr[8];
  dtostrf(temperature, 0, 1, tempStr);

  String body = String(MY_ID) + "|" + HUB_ID + "|" + String(n) +
                "|DATA|" + String(tempStr);

  String sig = computeHmac(body);
  String packet = body + "|" + sig;

  radioWake();

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();

  radioSleep();
}

// ===== Main work =====

void measureAndSend() {
  float temp = readTemperature();

  if (temp == DEVICE_DISCONNECTED_C || temp < -50.0 || temp > 125.0) {
    // Ingen Serial i low-power-versionen.
    // Vid sensorfel skickar vi inget.
  } else {
    sendData(temp);
  }

  prepareForSleep();
}

void sleepMinutes() {
  for (uint16_t i = 0; i < SLEEP_CYCLES_8S; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

// ===== setup / loop =====

void setup() {
  avrSleepPrepare();
  sensorSleepPinsHard();

  nonceSetup();
  initSensor();
  initLoRa();

  // Skicka en gång direkt vid boot
  measureAndSend();
}

void loop() {
  sleepMinutes();
  measureAndSend();
}
