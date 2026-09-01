/*
NANO AUXILIARY LOGGER / ALARM / IOT GATEWAY
Board: Arduino Nano

Receives BMS telemetry from Uno through RX D0.
Controls buzzer on D8.
Logs faults to 24LC256 EEPROM using A4/A5 I2C.
Reads auxiliary thermistor on A0.
Outputs IoT-ready text through SoftwareSerial D2/D3.

UNO D1/TX -> Nano D0/RX
Common GND required.
*/
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 display(13, 11, 7, 6, 5);
#include <Wire.h>
#include <SoftwareSerial.h>
#include <math.h>

SoftwareSerial espSerial(2, 3); // RX, TX

#define BUZZER_PIN 8
#define AUX_TEMP_PIN A0

#define EEPROM_ADDR 0x50

#define ADDR_FAULT_COUNT 0
#define ADDR_LAST_FAULT  10
#define ADDR_LAST_SOC    20
#define ADDR_LAST_TEMP   30
#define ADDR_OV_COUNT    40
#define ADDR_UV_COUNT    50
#define ADDR_OC_COUNT    60
#define ADDR_OT_COUNT    70

float c1 = 0, c2 = 0, c3 = 0, c4 = 0;
float packV = 0;
int soc = 0;
float currentA = 0;
float mainTempC = 25;
float auxTempC = 25;

byte faultCode = 0;
byte balCode = 0;

String rxLine = "";
byte lastLoggedFault = 0;

unsigned long lastLogTime = 0;
unsigned long lastBuzzerToggle = 0;
unsigned long lastIoTPrint = 0;

bool buzzerState = false;

float readAuxTemperature()
{
  int adc = analogRead(AUX_TEMP_PIN);

  if (adc <= 5 || adc >= 1018)
    return 25.0;

  float resistance = 10000.0 * ((1023.0 / adc) - 1.0);

  if (resistance <= 0)
    return 25.0;

  float tempK = 1.0 / (
    (log(resistance / 10000.0) / 3950.0) +
    (1.0 / 298.15)
  );

  float t = tempK - 273.15;

  if (t < -20 || t > 120)
    return 25.0;

  return t;
}

void eepromWriteByte(unsigned int memAddress, byte data)
{
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(memAddress >> 8));
  Wire.write((int)(memAddress & 0xFF));
  Wire.write(data);
  Wire.endTransmission();
  delay(5);
}

byte eepromReadByte(unsigned int memAddress)
{
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(memAddress >> 8));
  Wire.write((int)(memAddress & 0xFF));
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, 1);

  if (Wire.available())
    return Wire.read();

  return 0;
}

void incrementEEPROMCounter(unsigned int addr)
{
  byte value = eepromReadByte(addr);
  value++;
  eepromWriteByte(addr, value);
}

void logFault(byte code)
{
  if (code == 0)
    return;

  incrementEEPROMCounter(ADDR_FAULT_COUNT);
  eepromWriteByte(ADDR_LAST_FAULT, code);
  eepromWriteByte(ADDR_LAST_SOC, (byte)soc);
  eepromWriteByte(ADDR_LAST_TEMP, (byte)mainTempC);

  if (code == 1) incrementEEPROMCounter(ADDR_OV_COUNT);
  else if (code == 2) incrementEEPROMCounter(ADDR_UV_COUNT);
  else if (code == 3) incrementEEPROMCounter(ADDR_OC_COUNT);
  else if (code == 4) incrementEEPROMCounter(ADDR_OT_COUNT);
}

String faultText(byte code)
{
  if (code == 1) return "OV";
  if (code == 2) return "UV";
  if (code == 3) return "OC";
  if (code == 4) return "OT";
  return "NONE";
}

String balanceText(byte code)
{
  String b = "";

  if (code & 1) b += "C1 ";
  if (code & 2) b += "C2 ";
  if (code & 4) b += "C3 ";
  if (code & 8) b += "C4 ";

  if (b.length() == 0)
    b = "NONE";

  return b;
}

void updateBuzzer()
{
  byte activeFault = faultCode;

  if (auxTempC > 60.0 && activeFault == 0)
    activeFault = 4;

  if (activeFault == 0)
  {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    return;
  }

  unsigned long interval = 500;

  if (activeFault == 1) interval = 250;
  else if (activeFault == 2) interval = 700;
  else if (activeFault == 3) interval = 150;
  else if (activeFault == 4) interval = 1000;

  if (millis() - lastBuzzerToggle >= interval)
  {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    lastBuzzerToggle = millis();
  }
}

// Parse:
// c1,c2,c3,c4,packV,soc,current,temp,faultCode,balCode
bool parseLine(String line)
{
  line.trim();

  if (line.length() < 10)
    return false;

  float values[8];
  int startIndex = 0;

  for (int i = 0; i < 8; i++)
  {
    int commaIndex = line.indexOf(',', startIndex);
    if (commaIndex == -1)
      return false;

    String token = line.substring(startIndex, commaIndex);
    values[i] = token.toFloat();

    startIndex = commaIndex + 1;
  }

  int commaIndex = line.indexOf(',', startIndex);
  if (commaIndex == -1)
    return false;

  faultCode = (byte)line.substring(startIndex, commaIndex).toInt();
  balCode = (byte)line.substring(commaIndex + 1).toInt();

  c1 = values[0];
  c2 = values[1];
  c3 = values[2];
  c4 = values[3];
  packV = values[4];
  soc = (int)values[5];
  currentA = values[6];
  mainTempC = values[7];

  return true;
}

void readFromUno()
{
  while (Serial.available())
  {
    char ch = Serial.read();

    if (ch == '\n')
    {
      if (parseLine(rxLine))
      {
        if (faultCode != 0)
        {
          if (faultCode != lastLoggedFault || millis() - lastLogTime > 5000)
          {
            logFault(faultCode);
            lastLoggedFault = faultCode;
            lastLogTime = millis();
          }
        }
        else
        {
          lastLoggedFault = 0;
        }
      }

      rxLine = "";
    }
    else
    {
      rxLine += ch;

      if (rxLine.length() > 100)
        rxLine = "";
    }
  }
}

void sendIoTTelemetry()
{
  espSerial.print("C1=");
  espSerial.print(c1, 2);
  espSerial.print(",C2=");
  espSerial.print(c2, 2);
  espSerial.print(",C3=");
  espSerial.print(c3, 2);
  espSerial.print(",C4=");
  espSerial.print(c4, 2);
  espSerial.print(",PACK=");
  espSerial.print(packV, 2);
  espSerial.print(",SOC=");
  espSerial.print(soc);
  espSerial.print(",I=");
  espSerial.print(currentA, 2);
  espSerial.print(",TMAIN=");
  espSerial.print(mainTempC, 1);
  espSerial.print(",TAUX=");
  espSerial.print(auxTempC, 1);
  espSerial.print(",FAULT=");
  espSerial.print(faultText(faultCode));
  espSerial.print(",BAL=");
  espSerial.println(balanceText(balCode));

  espSerial.print("EEPROM_FAULT_COUNT=");
  espSerial.print(eepromReadByte(ADDR_FAULT_COUNT));
  espSerial.print(",LAST_FAULT=");
  espSerial.println(faultText(eepromReadByte(ADDR_LAST_FAULT)));
}

void setup()
{
  Serial.begin(9600);
  espSerial.begin(9600);

  Wire.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  espSerial.println("NANO LOGGER READY");
  espSerial.println("Waiting for Uno telemetry...");
  display.begin();
display.setContrast(50);
display.clearDisplay();
display.setCursor(0, 0);
display.println("NANO LOG");
display.println("READY");
display.display();
delay(1000);
}

void updateNanoLCD()
{
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("FAULT:");
  display.print(faultText(faultCode));

  display.setCursor(0, 10);
  display.print("BAL:");
  display.print(balanceText(balCode));

  display.setCursor(0, 22);
  display.print("FCNT:");
  display.print(eepromReadByte(ADDR_FAULT_COUNT));

  display.setCursor(0, 34);
  display.print("LAST:");
  display.print(faultText(eepromReadByte(ADDR_LAST_FAULT)));

  display.setCursor(0, 44);
  display.print("BUZZ:");
  display.print(faultCode ? "ON" : "OFF");

  display.display();
}

void loop()
{
  readFromUno();

  auxTempC = readAuxTemperature();

  updateBuzzer();

  if (millis() - lastIoTPrint > 1000)
  {
    sendIoTTelemetry();
    lastIoTPrint = millis();
  }
  updateNanoLCD();
}
