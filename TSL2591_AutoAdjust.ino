/* TSL2591 Auto Adjust
 *  
 * Reads from the TSL2591 light sensor and writes the measurements to serial.
 * Automatically adjusts gain as needed.
 * Communicates using I2C.
 *  
 * Daniel Winker, September 22, 2019
 * Built from Adafruit's TSL2591 example.
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); // pass in a number for the sensor identifier (for your use later)
int settingsCounter;
bool garbage;  // The first reading after a settings change should be tossed out
bool gotReading;
uint16_t ir, full, visible;
double lux;
bool goodRead = false;
double hysteresis = 1;

/**************************************************************************/
/*
    Displays some basic information on this sensor from the unified
    sensor API sensor_t type (see Adafruit_Sensor for more information)
*/
/**************************************************************************/
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2561
    Longer integration times seem to give better accuracy (maybe the device is averaging)
*/
/**************************************************************************/
void configureSensor(int choice)
{
  if (choice == 0) {  // Least sensitivity possible
    Serial.println("Setting gain to minimum.");
    tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time
  } else if (choice == 1) {  
    tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain  
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time
    hysteresis = 0.8;
  } else if (choice == 2) {  
    tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time   
    hysteresis = 0.9;
  } else if (choice == 3) {
    tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time   
    hysteresis = 1.1;
  }  // The device's gain can go up to 9876, but measurements with that setting seem unreliable, and there doesn't seem to be a need anyway.
  garbage = true;  // The next measurement will be bad, and should be thrown out.
}

/**************************************************************************/
/*
    Program entry point for the Arduino sketch
*/
/**************************************************************************/
void setup(void) 
{
  Serial.begin(115200);
  Serial.println("Starting Adafruit TSL2591 AutoAdjust Test!");
  if (!tsl.begin()) 
  {
    Serial.println("No sensor found ... check your wiring?");
    while (1);
  }
    
  /* Display some basic information on this sensor */
  displaySensorDetails();
  
  /* Configure the sensor */
  settingsCounter = 2;
  configureSensor(settingsCounter);
  garbage = true;
  gotReading = false;
}

// Reads IR and Full-Spectrum and sets IR, visible, full spectrum, and lux.
// Automatically adjusts gain as needed.
bool advancedRead(void)
{
  while (!gotReading) {
    if (garbage) {  // We just changed the settings, so we need to take a reading then throw it out.
      tsl.getFullLuminosity();  // Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum.
      garbage = false;
    } else {
      int newSetting = settingsCounter;
      // Take a measurement
      uint32_t lum = tsl.getFullLuminosity();  // Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum.    
      double newLux = tsl.calculateLux((lum & 0xFFFF), (lum >> 16));
      
      // Interpret the measurement and decide if the gain should be adjusted
      if (newLux == 0 || newLux > 60000) {  // The sensor saturated or is close
        if (settingsCounter > 0) {
          newSetting = settingsCounter - 1;  // Decrease the gain
        }
      } else if (newLux > 1700 * hysteresis) {
        newSetting = 1;  // Use low gain setting
      } else if (newLux < 100 * hysteresis) {
        newSetting = 3;  // Use high gain setting
      } else {
        newSetting = 2;  // Use normal gain setting
      }

      // Adjust the gain or set the measurements as appropriate
      if (settingsCounter == newSetting) {  // Nothing has changed
        if (newLux < 0) {
          newLux = 0;
        }
        lux = newLux;
        ir = lum >> 16;
        full = lum & 0xFFFF;
        visible = full - ir;
        gotReading = true;
        if (newLux == 0) {
          return false;  // That's as low as it goes; there's nothing else to do.
        } else {
          return true;
        }
      } else {
        Serial.print("Updating setting "); Serial.print(settingsCounter); Serial.print(" to "); Serial.println(newSetting);
        settingsCounter = newSetting;
        configureSensor(settingsCounter);
      }
    }
  }
}

void loop(void) 
{ 
  if (gotReading) {
    gotReading = false;
    if (!goodRead) {
      Serial.println("Too bright!");
    } else {
      Serial.print("[ "); Serial.print(millis()); Serial.print(" ms ] ");
      Serial.print("IR: "); Serial.print(ir);  Serial.print("  ");
      Serial.print("Visible: "); Serial.print(visible); Serial.print("  ");
      Serial.print("Lux: "); Serial.print(lux); Serial.print("  ");
      Serial.print("Gain Config: "); Serial.println(settingsCounter);
    }
  }
  goodRead = advancedRead();
  delay(250);
}
