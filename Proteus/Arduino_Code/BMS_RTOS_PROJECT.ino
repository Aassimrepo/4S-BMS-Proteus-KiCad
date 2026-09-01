/*
 ============================================================
        4S BMS - PROTEUS SIMULATION VERSION
        Arduino UNO MAIN CONTROLLER
 ============================================================

 Designed specifically for the Proteus circuit:

 B1 -> 10k / 10k divider -> A0
 B2 -> 20k / 10k divider -> A1
 B3 -> 30k / 10k divider -> A2
 B4 -> 40k / 10k divider -> A3

 IMPORTANT:
 B1, B2, B3 and B4 are treated as INDIVIDUAL CELL VOLTAGES.

 Example:
 B1 = 4.2V
 B2 = 4.2V
 B3 = 4.2V
 B4 = 4.2V

 Arduino calculates:
 Cell 1 = approximately 4.2V
 Cell 2 = approximately 4.2V
 Cell 3 = approximately 4.2V
 Cell 4 = approximately 4.2V

 Pack = approximately 16.8V

 ============================================================
*/


#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <math.h>


// ============================================================
// NOKIA 5110 LCD
// CLK, DIN, DC, CE, RST
// ============================================================

Adafruit_PCD8544 display(13, 11, 7, 6, 5);


// ============================================================
// STATUS LED PINS
// ============================================================

#define CHARGE_LED 2
#define LOAD_LED   3
#define FAULT_LED  4


// ============================================================
// CELL BALANCING LED / MOSFET OUTPUTS
// ============================================================

#define BAL1_PIN 8
#define BAL2_PIN 9
#define BAL3_PIN 10
#define BAL4_PIN 12


// ============================================================
// ANALOG INPUTS
// ============================================================

#define CELL1_PIN A0
#define CELL2_PIN A1
#define CELL3_PIN A2
#define CELL4_PIN A3

#define CURRENT_PIN A4
#define TEMP_PIN    A5


// ============================================================
// ADC CONFIGURATION
// ============================================================

const float ADC_REFERENCE = 5.00;
const float ADC_MAX       = 1023.0;


// ============================================================
// VOLTAGE DIVIDER MULTIPLIERS
//
// B1: 10k / 10k  -> x2
// B2: 20k / 10k  -> x3
// B3: 30k / 10k  -> x4
// B4: 40k / 10k  -> x5
// ============================================================

const float CELL1_MULTIPLIER = 2.0;
const float CELL2_MULTIPLIER = 3.0;
const float CELL3_MULTIPLIER = 4.0;
const float CELL4_MULTIPLIER = 5.0;


// ============================================================
// BMS LIMITS
//
// These are deliberately separated.
//
// 4.20V = normal full cell
// 4.25V = start passive balancing
// 4.30V = overvoltage fault
//
// You can temporarily increase OV_LIMIT for testing.
// ============================================================

const float CELL_OV_LIMIT = 4.30;
const float CELL_UV_LIMIT = 2.00;

const float BALANCE_START = 4.25;

const float CURRENT_LIMIT = 2.00;
const float TEMP_LIMIT    = 60.0;


// ============================================================
// CURRENT SENSOR CONFIGURATION
//
// For Proteus simulation the ACS712 zero-current output
// can vary depending on how the model is configured.
//
// We use a configurable zero voltage.
// ============================================================

const float ACS_ZERO_VOLTAGE = 2.50;
const float ACS_SENSITIVITY  = 0.100;


// ============================================================
// THERMISTOR CONFIGURATION
// ============================================================

const float THERMISTOR_NOMINAL = 10000.0;
const float SERIES_RESISTOR    = 10000.0;
const float BETA_VALUE         = 3950.0;
const float NOMINAL_TEMP_K    = 298.15;


// ============================================================
// CELL VOLTAGE VARIABLES
// ============================================================

float cell1 = 0.0;
float cell2 = 0.0;
float cell3 = 0.0;
float cell4 = 0.0;

float packVoltage = 0.0;


// ============================================================
// OTHER SENSOR VARIABLES
// ============================================================

float currentA = 0.0;
float tempC    = 25.0;
float soc      = 0.0;


// ============================================================
// RAW ADC VOLTAGES
//
// Useful for debugging Proteus.
// ============================================================

float rawA0 = 0.0;
float rawA1 = 0.0;
float rawA2 = 0.0;
float rawA3 = 0.0;


// ============================================================
// FAULT FLAGS
// ============================================================

bool overVoltage  = false;
bool underVoltage = false;
bool overCurrent  = false;
bool overTemp     = false;
bool fault        = false;


// ============================================================
// INDIVIDUAL CELL FAULT FLAGS
// ============================================================

bool ov1 = false;
bool ov2 = false;
bool ov3 = false;
bool ov4 = false;

bool uv1 = false;
bool uv2 = false;
bool uv3 = false;
bool uv4 = false;


// ============================================================
// BALANCING FLAGS
// ============================================================

bool bal1 = false;
bool bal2 = false;
bool bal3 = false;
bool bal4 = false;


// ============================================================
// CODES SENT TO NANO
//
// faultCode:
// 0 = Normal
// 1 = Overvoltage
// 2 = Undervoltage
// 3 = Overcurrent
// 4 = Overtemperature
//
// balCode:
// bit 0 = Cell 1
// bit 1 = Cell 2
// bit 2 = Cell 3
// bit 3 = Cell 4
// ============================================================

byte faultCode = 0;
byte balCode   = 0;


// ============================================================
// LCD
// ============================================================

int page = 0;

unsigned long lastPageChange = 0;
unsigned long lastTelemetry  = 0;
unsigned long lastDebug      = 0;


// ============================================================
// READ ADC VOLTAGE
// ============================================================

float readADCVoltage(int pin)
{
    long total = 0;

    const int samples = 20;

    for (int i = 0; i < samples; i++)
    {
        total += analogRead(pin);
        delayMicroseconds(200);
    }

    float averageADC = total / (float)samples;

    return averageADC * ADC_REFERENCE / ADC_MAX;
}


// ============================================================
// READ CELL VOLTAGE
//
// No aggressive filtering here.
//
// This is intentional.
//
// Your previous filtering could make Proteus testing
// confusing because the displayed value could lag behind
// the actual source voltage.
//
// A small exponential filter is still used to stabilize
// the display.
// ============================================================

float filterVoltage(float previous, float measured)
{
    const float FILTER_ALPHA = 0.35;

    if (previous <= 0.1)
        return measured;

    return previous * (1.0 - FILTER_ALPHA)
           + measured * FILTER_ALPHA;
}


// ============================================================
// READ ALL FOUR CELLS
// ============================================================

void readCells()
{
    // Read actual ADC-side voltages

    rawA0 = readADCVoltage(CELL1_PIN);
    rawA1 = readADCVoltage(CELL2_PIN);
    rawA2 = readADCVoltage(CELL3_PIN);
    rawA3 = readADCVoltage(CELL4_PIN);


    // Convert divider voltage back to cell voltage

    float measuredCell1 = rawA0 * CELL1_MULTIPLIER;
    float measuredCell2 = rawA1 * CELL2_MULTIPLIER;
    float measuredCell3 = rawA2 * CELL3_MULTIPLIER;
    float measuredCell4 = rawA3 * CELL4_MULTIPLIER;


    // Sanity limits
    //
    // These prevent impossible readings caused by
    // floating inputs or ADC noise.

    if (measuredCell1 < 0.0 || measuredCell1 > 10.5)
        measuredCell1 = cell1;

    if (measuredCell2 < 0.0 || measuredCell2 > 10.5)
        measuredCell2 = cell2;

    if (measuredCell3 < 0.0 || measuredCell3 > 10.5)
        measuredCell3 = cell3;

    if (measuredCell4 < 0.0 || measuredCell4 > 10.5)
        measuredCell4 = cell4;


    // Filter

    cell1 = filterVoltage(cell1, measuredCell1);
    cell2 = filterVoltage(cell2, measuredCell2);
    cell3 = filterVoltage(cell3, measuredCell3);
    cell4 = filterVoltage(cell4, measuredCell4);


    // Calculate pack

    packVoltage =
        cell1 +
        cell2 +
        cell3 +
        cell4;
}


// ============================================================
// CALCULATE SOC
//
// 4S Li-ion approximation:
//
// 12.0V = 0%
// 16.8V = 100%
//
// This is a simple simulation SOC model,
// NOT a real battery fuel-gauge algorithm.
// ============================================================

void updateSOC()
{
    soc = ((packVoltage - 12.0) / 4.8) * 100.0;


    if (soc < 0.0)
        soc = 0.0;

    if (soc > 100.0)
        soc = 100.0;
}


// ============================================================
// READ ACS712 CURRENT
// ============================================================

float readCurrent()
{
    float sensorVoltage = readADCVoltage(CURRENT_PIN);

    float current =
        (sensorVoltage - ACS_ZERO_VOLTAGE)
        / ACS_SENSITIVITY;


    // Absolute value for simple Proteus simulation

    if (current < 0)
        current = -current;


    // Remove small simulated noise

    if (current < 0.10)
        current = 0.0;


    return current;
}


// ============================================================
// READ THERMISTOR
// ============================================================

float readTemperature()
{
    int adc = analogRead(TEMP_PIN);


    // If input is essentially disconnected,
    // return room temperature for simulation.

    if (adc <= 2 || adc >= 1021)
        return 25.0;


    float resistance =
        SERIES_RESISTOR *
        ((ADC_MAX / (float)adc) - 1.0);


    if (resistance <= 0)
        return 25.0;


    float temperatureK =
        1.0 /
        (
            (log(resistance / THERMISTOR_NOMINAL)
             / BETA_VALUE)
            +
            (1.0 / NOMINAL_TEMP_K)
        );


    float temperatureC =
        temperatureK - 273.15;


    // Reject completely unrealistic values

    if (temperatureC < -40.0 ||
        temperatureC > 150.0)
    {
        return 25.0;
    }


    return temperatureC;
}


// ============================================================
// FAULT DETECTION
// ============================================================

void updateFaults()
{
    // Individual overvoltage

    ov1 = cell1 > CELL_OV_LIMIT;
    ov2 = cell2 > CELL_OV_LIMIT;
    ov3 = cell3 > CELL_OV_LIMIT;
    ov4 = cell4 > CELL_OV_LIMIT;


    // Individual undervoltage

    uv1 = cell1 < CELL_UV_LIMIT;
    uv2 = cell2 < CELL_UV_LIMIT;
    uv3 = cell3 < CELL_UV_LIMIT;
    uv4 = cell4 < CELL_UV_LIMIT;


    // Combined conditions

    overVoltage =
        ov1 ||
        ov2 ||
        ov3 ||
        ov4;


    underVoltage =
        uv1 ||
        uv2 ||
        uv3 ||
        uv4;


    overCurrent =
        currentA > CURRENT_LIMIT;


    overTemp =
        tempC > TEMP_LIMIT;


    // Overall fault

    fault =
        overVoltage ||
        underVoltage ||
        overCurrent ||
        overTemp;


    // Priority system

    if (overVoltage)
    {
        faultCode = 1;
    }
    else if (underVoltage)
    {
        faultCode = 2;
    }
    else if (overCurrent)
    {
        faultCode = 3;
    }
    else if (overTemp)
    {
        faultCode = 4;
    }
    else
    {
        faultCode = 0;
    }
}


// ============================================================
// PASSIVE BALANCING
//
// IMPORTANT:
//
// Balancing is NOT the same thing as overvoltage protection.
//
// A cell can be at 4.25V and start balancing,
// while the actual overvoltage fault is 4.30V.
//
// This prevents your yellow balancing LEDs from being
// directly tied to the OV fault condition.
// ============================================================

void updateBalancing()
{
    bal1 = cell1 >= BALANCE_START;
    bal2 = cell2 >= BALANCE_START;
    bal3 = cell3 >= BALANCE_START;
    bal4 = cell4 >= BALANCE_START;


    digitalWrite(
        BAL1_PIN,
        bal1 ? HIGH : LOW
    );


    digitalWrite(
        BAL2_PIN,
        bal2 ? HIGH : LOW
    );


    digitalWrite(
        BAL3_PIN,
        bal3 ? HIGH : LOW
    );


    digitalWrite(
        BAL4_PIN,
        bal4 ? HIGH : LOW
    );


    // Generate balancing bit code

    balCode = 0;


    if (bal1)
        balCode |= 1;


    if (bal2)
        balCode |= 2;


    if (bal3)
        balCode |= 4;


    if (bal4)
        balCode |= 8;
}


// ============================================================
// STATUS LED CONTROL
// ============================================================

void updateOutputs()
{
    // CHARGE
    //
    // Charging is disabled for:
    // OV
    // OT

    if (overVoltage || overTemp)
        digitalWrite(CHARGE_LED, LOW);
    else
        digitalWrite(CHARGE_LED, HIGH);


    // LOAD
    //
    // Load disabled for:
    // UV
    // OC
    // OT

    if (underVoltage ||
        overCurrent ||
        overTemp)
    {
        digitalWrite(LOAD_LED, LOW);
    }
    else
    {
        digitalWrite(LOAD_LED, HIGH);
    }


    // Fault LED

    if (fault)
        digitalWrite(FAULT_LED, HIGH);
    else
        digitalWrite(FAULT_LED, LOW);
}


// ============================================================
// SEND TELEMETRY TO NANO
//
// Format:
//
// cell1,
// cell2,
// cell3,
// cell4,
// packVoltage,
// SOC,
// current,
// temperature,
// faultCode,
// balancingCode
// ============================================================

void sendTelemetryToNano()
{
    Serial.print(cell1, 2);
    Serial.print(",");

    Serial.print(cell2, 2);
    Serial.print(",");

    Serial.print(cell3, 2);
    Serial.print(",");

    Serial.print(cell4, 2);
    Serial.print(",");

    Serial.print(packVoltage, 2);
    Serial.print(",");

    Serial.print((int)soc);
    Serial.print(",");

    Serial.print(currentA, 2);
    Serial.print(",");

    Serial.print(tempC, 1);
    Serial.print(",");

    Serial.print(faultCode);
    Serial.print(",");

    Serial.println(balCode);
}


// ============================================================
// SERIAL DEBUG
//
// This is extremely important for Proteus.
//
// It lets us see:
//
// ADC A0-A3
// calculated cell voltages
// pack voltage
// current
// temperature
// fault
// balancing
// ============================================================

void sendDebugData()
{
    Serial.println();
    Serial.println("========== BMS DEBUG ==========");


    Serial.print("ADC A0 = ");
    Serial.print(rawA0, 3);
    Serial.println(" V");


    Serial.print("ADC A1 = ");
    Serial.print(rawA1, 3);
    Serial.println(" V");


    Serial.print("ADC A2 = ");
    Serial.print(rawA2, 3);
    Serial.println(" V");


    Serial.print("ADC A3 = ");
    Serial.print(rawA3, 3);
    Serial.println(" V");


    Serial.println();


    Serial.print("CELL 1 = ");
    Serial.print(cell1, 2);
    Serial.println(" V");


    Serial.print("CELL 2 = ");
    Serial.print(cell2, 2);
    Serial.println(" V");


    Serial.print("CELL 3 = ");
    Serial.print(cell3, 2);
    Serial.println(" V");


    Serial.print("CELL 4 = ");
    Serial.print(cell4, 2);
    Serial.println(" V");


    Serial.print("PACK = ");
    Serial.print(packVoltage, 2);
    Serial.println(" V");


    Serial.print("SOC = ");
    Serial.print(soc, 1);
    Serial.println(" %");


    Serial.print("CURRENT = ");
    Serial.print(currentA, 2);
    Serial.println(" A");


    Serial.print("TEMP = ");
    Serial.print(tempC, 1);
    Serial.println(" C");


    Serial.println();


    Serial.print("OV = ");
    Serial.println(overVoltage ? "YES" : "NO");


    Serial.print("UV = ");
    Serial.println(underVoltage ? "YES" : "NO");


    Serial.print("OC = ");
    Serial.println(overCurrent ? "YES" : "NO");


    Serial.print("OT = ");
    Serial.println(overTemp ? "YES" : "NO");


    Serial.print("FAULT CODE = ");
    Serial.println(faultCode);


    Serial.print("BALANCING CODE = ");
    Serial.println(balCode);


    Serial.println("===============================");
}


// ============================================================
// LCD PAGE 0
// CELL VOLTAGES
// ============================================================

void displayCellPage()
{
    display.setCursor(0, 0);

    display.print("C1:");
    display.print(cell1, 2);


    display.setCursor(42, 0);

    display.print("C2:");
    display.print(cell2, 2);


    display.setCursor(0, 12);

    display.print("C3:");
    display.print(cell3, 2);


    display.setCursor(42, 12);

    display.print("C4:");
    display.print(cell4, 2);


    display.setCursor(0, 28);

    display.print("PACK:");
    display.print(packVoltage, 1);
    display.print("V");


    display.setCursor(0, 42);

    display.print("SOC:");
    display.print((int)soc);
    display.print("%");
}


// ============================================================
// LCD PAGE 1
// CURRENT + TEMPERATURE
// ============================================================

void displaySensorPage()
{
    display.setCursor(0, 0);

    display.print("CURRENT");


    display.setCursor(0, 12);

    display.print(currentA, 2);
    display.print(" A");


    display.setCursor(0, 28);

    display.print("TEMP");


    display.setCursor(0, 40);

    display.print(tempC, 1);
    display.print(" C");
}


// ============================================================
// LCD PAGE 2
// BMS STATUS
// ============================================================

void displayStatusPage()
{
    display.setCursor(0, 0);

    display.print("BMS STATUS");


    display.setCursor(0, 14);

    if (fault)
    {
        display.print("FAULT:");
        display.print(faultCode);
    }
    else
    {
        display.print("NORMAL");
    }


    display.setCursor(0, 28);

    display.print("BAL:");

    if (bal1)
        display.print("1 ");

    if (bal2)
        display.print("2 ");

    if (bal3)
        display.print("3 ");

    if (bal4)
        display.print("4 ");

    if (!bal1 &&
        !bal2 &&
        !bal3 &&
        !bal4)
    {
        display.print("NONE");
    }


    display.setCursor(0, 42);

    display.print("TX -> NANO");
}


// ============================================================
// UPDATE LCD
// ============================================================

void updateLCD()
{
    // Automatically change page every 3 seconds

    if (millis() - lastPageChange >= 3000)
    {
        page++;

        if (page > 2)
            page = 0;

        lastPageChange = millis();
    }


    display.clearDisplay();


    if (page == 0)
    {
        displayCellPage();
    }
    else if (page == 1)
    {
        displaySensorPage();
    }
    else
    {
        displayStatusPage();
    }


    display.display();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // Serial communication
    //
    // Also used to communicate with Nano.

    Serial.begin(9600);


    // Status LEDs

    pinMode(CHARGE_LED, OUTPUT);
    pinMode(LOAD_LED, OUTPUT);
    pinMode(FAULT_LED, OUTPUT);


    // Balancing outputs

    pinMode(BAL1_PIN, OUTPUT);
    pinMode(BAL2_PIN, OUTPUT);
    pinMode(BAL3_PIN, OUTPUT);
    pinMode(BAL4_PIN, OUTPUT);


    // Initial LED states

    digitalWrite(CHARGE_LED, HIGH);
    digitalWrite(LOAD_LED, HIGH);
    digitalWrite(FAULT_LED, LOW);


    digitalWrite(BAL1_PIN, LOW);
    digitalWrite(BAL2_PIN, LOW);
    digitalWrite(BAL3_PIN, LOW);
    digitalWrite(BAL4_PIN, LOW);


    // Nokia LCD

    display.begin();

    display.setContrast(50);

    display.clearDisplay();


    display.setCursor(0, 0);

    display.println("4S BMS");

    display.println("PROTEUS");

    display.println("SIMULATION");


    display.display();


    delay(1500);


    display.clearDisplay();

    display.display();


    lastPageChange = millis();
    lastTelemetry = millis();
    lastDebug     = millis();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // 1. Read cell voltages
    // --------------------------------------------------------

    readCells();


    // --------------------------------------------------------
    // 2. Read current
    // --------------------------------------------------------

    currentA = readCurrent();


    // --------------------------------------------------------
    // 3. Read temperature
    // --------------------------------------------------------

    tempC = readTemperature();


    // --------------------------------------------------------
    // 4. Calculate SOC
    // --------------------------------------------------------

    updateSOC();


    // --------------------------------------------------------
    // 5. Detect faults
    // --------------------------------------------------------

    updateFaults();


    // --------------------------------------------------------
    // 6. Control balancing
    // --------------------------------------------------------

    updateBalancing();


    // --------------------------------------------------------
    // 7. Control status LEDs
    // --------------------------------------------------------

    updateOutputs();


    // --------------------------------------------------------
    // 8. Send telemetry every second
    // --------------------------------------------------------

    if (millis() - lastTelemetry >= 1000)
    {
        sendTelemetryToNano();

        lastTelemetry = millis();
    }


    // --------------------------------------------------------
    // 9. Send detailed debug information every 3 seconds
    // --------------------------------------------------------

    if (millis() - lastDebug >= 3000)
    {
        sendDebugData();

        lastDebug = millis();
    }


    // --------------------------------------------------------
    // 10. Update LCD
    // --------------------------------------------------------

    updateLCD();


    delay(100);
}