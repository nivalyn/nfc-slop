#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/*
  RFID-Based Access Control System
  Board: ESP32 DevKit (PlatformIO environment: esp32dev)
  Reader: RC522 / MFRC522, 13.56 MHz, SPI
  Display: I2C OLED, SSD1306, 128x64 (address 0x3C or 0x3D)
  Indicators: green LED = authorized; red LED = denied
  No buzzer is used.

  IMPORTANT: This prototype checks the card UID only. It does not write
  data to the card and is not suitable as the sole security mechanism for
  a real access-control installation.
*/

// -------------------- Pin assignments --------------------
// RC522 uses the ESP32's VSPI pins.
constexpr uint8_t RFID_SS_PIN   = 5;
constexpr uint8_t RFID_RST_PIN  = 27;  // Avoids conflict with OLED SCL (GPIO 22)
constexpr uint8_t RFID_SCK_PIN  = 18;
constexpr uint8_t RFID_MISO_PIN = 19;
constexpr uint8_t RFID_MOSI_PIN = 23;

constexpr uint8_t GREEN_LED_PIN = 25;
constexpr uint8_t RED_LED_PIN   = 26;

constexpr uint8_t OLED_SDA_PIN  = 21;
constexpr uint8_t OLED_SCL_PIN  = 22;
constexpr uint8_t OLED_WIDTH    = 128;
constexpr uint8_t OLED_HEIGHT   = 64;
constexpr uint8_t OLED_ADDR_1   = 0x3C;
constexpr uint8_t OLED_ADDR_2   = 0x3D;

constexpr unsigned long RESULT_DISPLAY_MS = 2000;

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

bool oledReady = false;
uint8_t oledAddress = 0;

// -------------------- Authorized cards --------------------
// Step 1: Upload the project with this placeholder.
// Step 2: Open the Serial Monitor (115200 baud) and scan your card.
// Step 3: Copy the UID exactly as printed (uppercase, no spaces) into this list.
// Multiple authorized UIDs can be placed on separate lines, comma-separated.
const char* authorizedUIDs[] = {
  "CD16B002"
  // Example only: "04A1B2C3D4"
};
const size_t authorizedCardCount =
    sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);

// -------------------- OLED helpers --------------------
bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void showOLEDMessage(const char* line1, const char* line2 = "") {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.drawLine(0, 11, OLED_WIDTH - 1, 11, SSD1306_WHITE);
  display.setCursor(0, 23);
  display.println(line2);
  display.display();
}

void showHomeScreen() {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("RFID ACCESS CONTROL");
  display.drawLine(0, 11, OLED_WIDTH - 1, 11, SSD1306_WHITE);

  display.setCursor(0, 24);
  display.println("Scan your RFID card");
  display.setCursor(0, 42);
  display.println("Status: READY");
  display.setCursor(0, 54);
  display.println("RC522 / 13.56 MHz");
  display.display();
}

void showCardResult(const String& uid, bool authorized) {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("RFID ACCESS CONTROL");
  display.drawLine(0, 11, OLED_WIDTH - 1, 11, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.println("CARD UID:");
  display.setCursor(2, 27);
  // Standard RC522 UIDs are up to 10 bytes (20 hex characters), which fit
  // on one line at text size 1 on a 128-pixel display.
  display.println(uid);

  display.setCursor(0, 43);
  display.print("STATUS: ");
  display.setCursor(0, 54);
  display.println(authorized ? "Slowly stepbro " : "Fuck You ");
  display.display();
}

// -------------------- RFID helpers --------------------
String readCardUID() {
  String uid;
  uid.reserve(rfid.uid.size * 2);

  for (byte i = 0; i < rfid.uid.size; ++i) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += '0';
    }
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();
  return uid;
}

bool isAuthorized(const String& scannedUID) {
  for (size_t i = 0; i < authorizedCardCount; ++i) {
    if (scannedUID.equalsIgnoreCase(authorizedUIDs[i])) {
      return true;
    }
  }
  return false;
}

void setAccessLEDs(bool authorized) {
  // Ensure the two indicator LEDs are never on at the same time.
  digitalWrite(GREEN_LED_PIN, authorized ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, authorized ? LOW : HIGH);
}

void turnOffLEDs() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  turnOffLEDs();

  Serial.println();
  Serial.println("====================================");
  Serial.println(" RFID-BASED ACCESS CONTROL SYSTEM");
  Serial.println(" ESP32 + RC522 + OLED + 2 LEDs");
  Serial.println("====================================");

  // Start I2C using the chosen ESP32 pins, then check for a common OLED address.
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (i2cDevicePresent(OLED_ADDR_1)) {
    oledAddress = OLED_ADDR_1;
  } else if (i2cDevicePresent(OLED_ADDR_2)) {
    oledAddress = OLED_ADDR_2;
  }

  if (oledAddress != 0 &&
      display.begin(SSD1306_SWITCHCAPVCC, oledAddress, true, false)) {
    oledReady = true;
    Serial.print("OLED detected at I2C address 0x");
    Serial.println(oledAddress, HEX);
  } else {
    oledReady = false;
    Serial.println("WARNING: OLED not detected at 0x3C or 0x3D.");
    Serial.println("RFID and LEDs will continue; check OLED wiring/address.");
  }

  if (oledReady) {
    showOLEDMessage("Starting RFID...", "Please wait");
  }

  // Start SPI with explicit pins and initialize the RC522.
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();
  delay(50);

  Serial.println("RC522 version information:");
  rfid.PCD_DumpVersionToSerial();
  Serial.println();
  Serial.println("Ready. Scan a 13.56 MHz compatible card/tag.");
  Serial.println("A card is denied until its UID is added to authorizedUIDs.");

  showHomeScreen();
}

void loop() {
  // Wait for a new card; the user may need to remove and present the card again.
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(30);
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    delay(30);
    return;
  }

  const String scannedUID = readCardUID();
  const bool authorized = isAuthorized(scannedUID);

  Serial.println("------------------------------------");
  Serial.println("Card detected");
  Serial.print("UID: ");
  Serial.println(scannedUID);
  Serial.println(authorized ? "ACCESS GRANTED" : "ACCESS DENIED");
  Serial.println("------------------------------------");

  setAccessLEDs(authorized);
  showCardResult(scannedUID, authorized);

  delay(RESULT_DISPLAY_MS);

  turnOffLEDs();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  showHomeScreen();
  delay(250);
}
