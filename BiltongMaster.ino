/*
 * BILTONG MASTER
 * BILTONG BOX CONTROL CENTER
 * BY YOKOMAFOODS.COM
 * MAKE YOUR OWN BILTONG
 * 
 * MADE FOR Diymore ESP32 TFT
 * 
 * AUTO/MANUAL MODE
 * 
 * HAFE FUN
 * 
 * CHEERS
 * JOHANNES from YOKOMA FOODS
 */
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <DHT.h>

TFT_eSPI tft = TFT_eSPI();

// Touch screen pins (Diymore ESP32 TFT)
#define TOUCH_CS 15
#define TOUCH_IRQ 36

// Initialize touch screen
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Sensor and actuator pins
#define DHTPIN 23          // Pin for DHT22 sensor
#define DHTTYPE DHT22     // Type of DHT sensor
#define FAN_PWM_PIN 15     // PWM pin for fan control
#define FAN_TACH_PIN 16   // Tachometer pin for fan speed measurement
#define RELAY_PIN 18      // Pin for relay (heating)

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Display dimensions
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Colors
#define BACKGROUND_COLOR TFT_BLACK
#define BUTTON_COLOR TFT_BLUE
#define TEXT_COLOR TFT_WHITE

// Button positions and sizes
#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 40
#define BUTTON_SPACING 20

#define TEMP_UP_BUTTON_X 20
#define TEMP_UP_BUTTON_Y 50
#define TEMP_DOWN_BUTTON_X 140
#define TEMP_DOWN_BUTTON_Y 50

#define HUMIDITY_UP_BUTTON_X 20
#define HUMIDITY_UP_BUTTON_Y 120
#define HUMIDITY_DOWN_BUTTON_X 140
#define HUMIDITY_DOWN_BUTTON_Y 120

#define MODE_BUTTON_X 20
#define MODE_BUTTON_Y 190

// Variables
float manualTemperature = 20.0;
float manualHumidity = 50.0;
bool manualMode = true;
float currentTemperature = 0;
float currentHumidity = 0;
int fanSpeed = 0;
bool relayState = false;
volatile unsigned long tachCount = 0; // Counter for fan tachometer pulses
unsigned long lastTachTime = 0;      // Time of last tachometer reading
int rpm = 0;                         // Fan speed in RPM

// Touch calibration (adjust according to your display)
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 880
#define TS_MAXY 940

// Interrupt service routine for tachometer
void IRAM_ATTR tachometerISR() {
    tachCount++;
}

void drawButtons() {
    // Temperature buttons
    tft.fillRect(TEMP_UP_BUTTON_X, TEMP_UP_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_COLOR);
    tft.fillRect(TEMP_DOWN_BUTTON_X, TEMP_DOWN_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_COLOR);

    tft.setTextColor(TEXT_COLOR, BUTTON_COLOR);
    tft.setTextSize(2);
    tft.setCursor(TEMP_UP_BUTTON_X + 10, TEMP_UP_BUTTON_Y + 10);
    tft.print("Temp +");
    tft.setCursor(TEMP_DOWN_BUTTON_X + 10, TEMP_DOWN_BUTTON_Y + 10);
    tft.print("Temp -");

    // Humidity buttons
    tft.fillRect(HUMIDITY_UP_BUTTON_X, HUMIDITY_UP_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_COLOR);
    tft.fillRect(HUMIDITY_DOWN_BUTTON_X, HUMIDITY_DOWN_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_COLOR);

    tft.setCursor(HUMIDITY_UP_BUTTON_X + 10, HUMIDITY_UP_BUTTON_Y + 10);
    tft.print("Hum +");
    tft.setCursor(HUMIDITY_DOWN_BUTTON_X + 10, HUMIDITY_DOWN_BUTTON_Y + 10);
    tft.print("Hum -");

    // Mode button
    tft.fillRect(MODE_BUTTON_X, MODE_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, manualMode ? TFT_GREEN : TFT_RED);
    tft.setTextColor(TEXT_COLOR, manualMode ? TFT_GREEN : TFT_RED);
    tft.setCursor(MODE_BUTTON_X + 10, MODE_BUTTON_Y + 10);
    tft.print(manualMode ? "Manual" : "Auto");
}

void updateDisplay(float temperature, float humidity) {
    tft.fillScreen(BACKGROUND_COLOR); // Clear screen

    drawButtons();

    // Display current values
    tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(2);
    tft.setCursor(20, 200);
    tft.printf("Temp: %.1f C", temperature);
    tft.setCursor(20, 230);
    tft.printf("Hum: %.1f %%", humidity);
    tft.setCursor(20, 260);
    tft.printf("Fan: %d %%", fanSpeed);
    tft.setCursor(20, 290);
    tft.printf("RPM: %d", rpm);
    tft.setCursor(20, 320);
    tft.printf("Heat: %s", relayState ? "ON" : "OFF");
}

bool isButtonPressed(int x, int y, int buttonX, int buttonY, int buttonWidth, int buttonHeight) {
    return (x >= buttonX && x <= buttonX + buttonWidth && y >= buttonY && y <= buttonY + buttonHeight);
}

void handleTouch() {
    if (ts.touched()) {
        TS_Point touch = ts.getPoint();

        // Map touch coordinates to screen coordinates
        int touchX = map(touch.x, TS_MINX, TS_MAXX, 0, TFT_WIDTH);
        int touchY = map(touch.y, TS_MINY, TS_MAXY, 0, TFT_HEIGHT);

        if (isButtonPressed(touchX, touchY, TEMP_UP_BUTTON_X, TEMP_UP_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
            manualTemperature += 0.5;
        } else if (isButtonPressed(touchX, touchY, TEMP_DOWN_BUTTON_X, TEMP_DOWN_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
            manualTemperature -= 0.5;
        } else if (isButtonPressed(touchX, touchY, HUMIDITY_UP_BUTTON_X, HUMIDITY_UP_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
            manualHumidity += 1.0;
        } else if (isButtonPressed(touchX, touchY, HUMIDITY_DOWN_BUTTON_X, HUMIDITY_DOWN_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
            manualHumidity -= 1.0;
        } else if (isButtonPressed(touchX, touchY, MODE_BUTTON_X, MODE_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
            manualMode = !manualMode; // Toggle manual/auto mode
        }
    }
}

void controlFanAndHeater(float temperature, float humidity) {
    // Control heater (relay)
    if (temperature < manualTemperature) {
        relayState = true;
    } else if (temperature > manualTemperature + 1) {
        relayState = false;
    }
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

    // Control fan speed based on humidity
    fanSpeed = map(humidity, 30, 80, 0, 255); // Adjust mapping as needed
    fanSpeed = constrain(fanSpeed, 0, 255);
    analogWrite(FAN_PWM_PIN, fanSpeed);
}

void calculateRPM() {
    unsigned long currentTime = millis();
    if (currentTime - lastTachTime >= 1000) { // Calculate RPM every second
        noInterrupts(); // Disable interrupts to read tachCount safely
        unsigned long count = tachCount;
        tachCount = 0;
        interrupts(); // Re-enable interrupts

        // Calculate RPM (assuming 2 pulses per revolution)
        rpm = (count / 2) * 60; // Convert pulses per second to RPM
        lastTachTime = currentTime;
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize DHT sensor
    dht.begin();

    // Initialize pins
    pinMode(FAN_PWM_PIN, OUTPUT);
    pinMode(FAN_TACH_PIN, INPUT_PULLUP);
    pinMode(RELAY_PIN, OUTPUT);
    analogWrite(FAN_PWM_PIN, 0); // Start with fan off
    digitalWrite(RELAY_PIN, LOW); // Start with heater off

    // Attach interrupt for tachometer
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), tachometerISR, FALLING);

    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(BACKGROUND_COLOR);

    // Initialize touch screen
    ts.begin();
    ts.setRotation(1);
}

void loop() {
    // Read sensor data
    currentTemperature = dht.readTemperature();
    currentHumidity = dht.readHumidity();

    if (isnan(currentTemperature) || isnan(currentHumidity)) {
        Serial.println("Error reading sensor");
        return;
    }

    // Handle touch input
    handleTouch();

    // Control fan and heater
    if (manualMode) {
        controlFanAndHeater(currentTemperature, currentHumidity);
        updateDisplay(manualTemperature, manualHumidity);
    } else {
        controlFanAndHeater(currentTemperature, currentHumidity);
        updateDisplay(currentTemperature, currentHumidity);
    }

    // Calculate and display fan RPM
    calculateRPM();

    delay(100); // Small delay to avoid flickering
}
