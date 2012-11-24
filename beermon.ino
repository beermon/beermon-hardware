#include <DallasTemperature.h>
#include <OneWire.h>
#include <SoftwareSerial.h>
#include <Time.h>
#include <stdio.h>

enum State
{
  ReadingTemperature,
  ReadingRightFlow,
  ReadingLeftFlow,
  ReadingBluetooth,
  WritingBluetooth
};

SoftwareSerial bluetooth(2, 3);
const int bluetoothInterrupt = 0;
volatile boolean bluetoothInterruption = false;
volatile unsigned long bluetoothInterruptedAt;
const int bluetoothInterruptionWindow = 500;

void bluetoothInterrupted()
{
  bluetoothInterruption = true;
  bluetoothInterruptedAt = millis();
}

void detachBluetoothInterupt()
{
  detachInterrupt(bluetoothInterrupt);
}

void resetBluetoothInterrupt()
{
  attachInterrupt(bluetoothInterrupt, bluetoothInterrupted, CHANGE);
  bluetoothInterruption = false;
  bluetoothInterruptedAt = 0;
}

void setupBluetooth()
{
  bluetooth.begin(115200);
  bluetooth.print("$$$");
  delay(100);
  bluetooth.println("U,9600,N");
  bluetooth.begin(9600);
  attachInterrupt(bluetoothInterrupt, bluetoothInterrupted, CHANGE);
}

SoftwareSerial rightFlow(5, 4);
String rightFlowInput;
String rightFlowOutput;

void setupRightFlow()
{
  rightFlow.begin(38400);
  rightFlow.print("T1\rO\rX\rC\r");
  rightFlowInput.reserve(5);
  rightFlowOutput.reserve(30);
}

SoftwareSerial leftFlow(7, 6);
String leftFlowInput;
String leftFlowOutput;

void setupLeftFlow()
{
  leftFlow.begin(38400);
  leftFlow.print("T1\rO\rX\rC\r");
  leftFlowInput.reserve(5);
  leftFlowOutput.reserve(30);
}

OneWire sensorsWire(8);
DallasTemperature sensors(&sensorsWire);
DeviceAddress thermometer = { 0x28, 0xAE, 0xD5, 0x00, 0x04, 0x00, 0x00, 0x43 };

void setupSensors()
{
  sensors.begin();
  sensors.setResolution(thermometer, 8);
}

void setup()
{
  Serial.begin(115200);
  setupBluetooth();
  setupRightFlow();
  delay(100);
  setupLeftFlow();
  setupSensors();
}

boolean readTemperature(float * temperature)
{
  sensors.requestTemperaturesByAddress(thermometer);
  *temperature = sensors.getTempC(thermometer);
  return true;
}

String flowBuffer;

boolean readFlow(SoftwareSerial * flow, float * volume)
{
  while ((*flow).available())
  {
    char b = (*flow).read();
    flowBuffer += b;
    if (b == '\r')
    {
      char line[25];
      flowBuffer.toCharArray(line, sizeof(line), 0);
      *volume = atof(strtok(line, ","));
      flowBuffer = "";
      return true;
    }
  }
  return false;
}

String bluetoothBuffer;

boolean readBluetooth(SoftwareSerial * bluetooth)
{
  while ((*bluetooth).available())
  {
    char b = (*bluetooth).read();
    bluetoothBuffer += b;
    if (b == '\n')
    {
      if (bluetoothBuffer == "RESET RIGHT\n") rightFlow.print("X\r");
      else if (bluetoothBuffer == "RESET LEFT\n") leftFlow.print("X\r");
      bluetoothBuffer = "";
      return true;
    }
  }
  if (millis() >= bluetoothInterruptedAt + bluetoothInterruptionWindow)
  {
    bluetoothBuffer = "";
    return true;
  }
  return false;
}

boolean writeBluetooth(SoftwareSerial * bluetooth, float temperature, float rightFlowVolume, float leftFlowVolume)
{
  (*bluetooth).print("READING ");
  (*bluetooth).print(temperature);
  (*bluetooth).print(" ");
  (*bluetooth).print(rightFlowVolume);
  (*bluetooth).print(" ");
  (*bluetooth).print(leftFlowVolume);
  (*bluetooth).println("");
}

State state, interruptedState;
float temperature;
float rightFlowVolume;
float leftFlowVolume;

void transition()
{
  if (bluetoothInterruption)
  {
    interruptedState = state;
    state = ReadingBluetooth;
    detachBluetoothInterupt();
    bluetooth.listen();
    return;
  }

  switch(state)
  {
    case ReadingTemperature:
      state = ReadingRightFlow;
      rightFlow.listen();
      break;
    case ReadingRightFlow:
      state = ReadingLeftFlow;
      leftFlow.listen();
      break;
    case ReadingLeftFlow:
      state = WritingBluetooth;
      break;
    case ReadingBluetooth:
      state = interruptedState;
      switch (state)
      {
        case ReadingRightFlow:
          rightFlow.listen();
          break;
        case ReadingLeftFlow:
          leftFlow.listen();
          break;
      }
      break;
    case WritingBluetooth:
      state = ReadingTemperature;
      break;
  }
}

void loop()
{
  switch(state)
  {
    case ReadingTemperature:
      if (readTemperature(&temperature)) transition();
      break;
    case ReadingRightFlow:
      if (readFlow(&rightFlow, &rightFlowVolume)) transition();
      break;
    case ReadingLeftFlow:
      if (readFlow(&leftFlow, &leftFlowVolume)) transition();
      break;
    case ReadingBluetooth:
      if (readBluetooth(&bluetooth))
      {
        resetBluetoothInterrupt();
        transition();
      }
      break;
    case WritingBluetooth:
      if (writeBluetooth(&bluetooth, temperature, rightFlowVolume, leftFlowVolume)) transition();
      break;
    default:
      state = ReadingTemperature;
      break;
  }
}
