/* TSL2591 Auto Adjust
 *  
 * Reads from the TSL2591 light sensor and writes the measurements to serial.
 * Automatically adjusts gain as needed.
 * Communicates using I2C.
 *  
 * Daniel Winker, February 18, 2021
 * Built from Adafruit's TSL2591 example.
 * - I wonder if some of the weird issues could be solved by basing the gain adjustment
 *   on the 'Visible' value instead of Lux? That would remove whatever math-magic is going on.
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
  Serial.println("Configuring!");
  if (choice == 0) {  // Least sensitivity possible
    Serial.println("Setting gain to minimum.");
    tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time, the measurement quality seems to be lower
  } else if (choice == 1) {  
    Serial.println("Setting gain to 1.");
    tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain  
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time
    hysteresis = 0.8;
  } else if (choice == 2) {  
    Serial.println("Setting gain to 25.");
    tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time   
    hysteresis = 0.9;
  } else if (choice == 3) {
    Serial.println("Setting gain to 428.");
    tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time   
    hysteresis = 1.1;
  } else if (choice == 4) {
    Serial.println("Setting gain to 9876. Short integration.");
    tsl.setGain(TSL2591_GAIN_MAX);   // 9876x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // Shortest integration time   
    hysteresis = 1.4;
  } else if (choice == 5) {
    Serial.println("Setting gain to 9876.");
    tsl.setGain(TSL2591_GAIN_MAX);   // 9876x gain
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time   
    hysteresis = 1;
  }
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
      double newLux = -1;
      uint32_t lum;

      // The below do/while loop - 
      // Sometimes, if a shadow passes over the sensor, or if we just switched to high gain, the sensor will
      // return a negative value. Because these issues can be transient, we'll wait momentarily, then try again.
      // If the problem persists, then the sensor is likely saturated and returning an erroneous value, so we'll leave
      // the loop and drop to a lower gain.
      int failCount = 0;
      const int retryLimit = 3;
      do {
        // Take a measurement
        lum = tsl.getFullLuminosity();  // Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum.    
        newLux = tsl.calculateLux((lum & 0xFFFF), (lum >> 16));
        if (newLux < 0) {
          Serial.println("Negative reading. Trying again.");
          failCount++;  // We might need to decrease the gain to get a good reading; this counter prevents an infinite loop.
          delay(500);  // Wait half a second before trying again
        }
      } while (newLux < 0 && failCount < retryLimit);  // Sometimes happens with a sudden shift to darkness (passing shadow), or switch to high gain

      // Interpret the measurement and decide if the gain should be adjusted
      if (newLux < 0 || (newLux < 0.1 && settingsCounter == 4)) {  // This handles two issues that seem to be some kind of saturation
        if (newSetting > 0) {
          newSetting--;
        }
      } else if (newLux == 0 || newLux > 60000) {  // The sensor saturated (leading to a returned zero) or is close
        Serial.println("Sensor is saturated.");
        if (settingsCounter > 0) {
          newSetting = settingsCounter - 1;  // Decrease the gain
        }
      } else if (newLux > 1700 * hysteresis) {
        Serial.println("Using low gain.");
        newSetting = 1;  // Use low gain setting
      } else if (newLux < 0.1) {
        Serial.println("Using highest gain!");
        newSetting = 5;  // Highest gain setting, x9876 (23x more gain than setting 3!)
      } else if (newLux < 2 * hysteresis) {
        Serial.println("Using highest gain. Low integration time.");
        newSetting = 4;  // Highest gain setting, x9876 (23x more gain than setting 3!)
      } else if (newLux < 10 * hysteresis) {
        Serial.println("Using high gain.");
        newSetting = 3;  // Use high gain setting, x428 
      } else {
        Serial.println("Using normal gain.");
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
        Serial.print("Measured "); Serial.print(newLux);
        Serial.print("  Updating setting "); Serial.print(settingsCounter); Serial.print(" to "); Serial.println(newSetting);
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
      Serial.print("Lux: "); Serial.print(lux, 4); Serial.print("  ");  // Print lux to 4 decimal places
      Serial.print("Gain Config: "); Serial.println(settingsCounter);
    }
  }
  goodRead = advancedRead();
  delay(250);
}
