#include <Arduino.h>  // Core Arduino library (automatically included in Arduino IDE)
#include <Wire.h>  // I2C communication (for OLED)
#include <Adafruit_GFX.h>  // Graphics library for OLED
#include <Adafruit_SSD1306.h> // SSD1306 OLED driver
#include <DHTesp.h> // DHT22 temperature/humidity sensor
#include <WiFi.h>  // ESP32 WiFi connectivity

// OLED Display Configuration
#define SCREEN_WIDTH  128 // OLED width in pixels
#define SCREEN_HEIGHT 64 // OLED height in pixels
#define OLED_RESET -1// Reset pin (-1 = no reset pin)
#define SCREEN_ADDRESS 0x3C // I2C address of OLED (usually 0x3C or 0x3D)

// Pin Definitions
#define BUZZER 18
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 35
#define PB_DOWN 33
#define DHTPIN 14

// NTP Configuration
#define NTP_SERVER     "pool.ntp.org"  // NTP server for time sync
int UTC_OFFSET = 19800; // Timezone offset (e.g., 19800 seconds = 5.5 hours for IST).

// Initialize Components
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Global Time Variables
int days, hours, minutes, seconds; // Time variable
bool alarm_enabled = true;  // Master alarm toggle
int n_alarms = 2; // Number of alarms
int alarm_hours[] = {0, 1}; // Default alarm hours (e.g., 12:00 AM, 1:00 AM)
int alarm_minutes[] = {1, 10}; // Default alarm minutes
bool alarm_triggered[] = {false, false};

// Music Notes for Alarm
int notes[] = {262, 294, 330, 349, 392, 440, 494, 523};

// Menu System
int current_mode = 0; //Tracks selected menu item (0-4)
int max_modes = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm", "3 - View Alarms", "4 - Delete Alarm", "5 - Set Time Zone"};

void setup() {
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_CANCEL, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_UP, INPUT);
    pinMode(PB_DOWN, INPUT);
    dhtSensor.setup(DHTPIN, DHTesp::DHT22); // Initializes the DHT22 sensor

    Serial.begin(9600);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // Initializes OLED 
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // If OLED fails, print error and halt (for(;;) infinite loop).
    }
    display.display(); //Wakes up the OLED.
    delay(2000); //Pauses for 2 seconds (allows OLED to stabilize).

    WiFi.begin("Wokwi-GUEST", "", 6); //Connects ESP32 to WiFi.
    while (WiFi.status() != WL_CONNECTED) {
        delay(250); //Short pause between retries
        display.clearDisplay();
        print_line("Connecting to WIFI", 0, 0, 2);
    }

    display.clearDisplay();
    print_line("Connected to WIFI", 0, 0, 2);
    configTime(UTC_OFFSET, 0, NTP_SERVER); //Configures NTP time sync

    display.clearDisplay();
    print_line("Welcome to Medibox", 10, 20, 2); //Displays "Welcome to Medibox" at position (10, 20) with text size 2.
    display.clearDisplay();
}

void loop() {
    update_time_with_check_alarm(); // Update time and check alarms

    if (digitalRead(PB_OK) == LOW) { // If "OK" button pressed
        delay(200); 
        go_to_menu(); // Enter menu system
        update_time();
    }

    check_temp(); // Monitor temperature/humidity
}

void print_line(String text, int column, int row, int text_size) {
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column, row);
    display.println(text);
    display.display();
/* Workflow:
Set text properties.
Position cursor.
Print text.
Force OLED update with display.display().*/
    
}

void update_time() { // Check this using previous code
    struct tm timeinfo; // Holds time data
    getLocalTime(&timeinfo); // Fetch time from NTP

    hours = timeinfo.tm_hour; //Stores the current hour in 24-hour format.
    minutes = timeinfo.tm_min;//Stores the current minute.
    seconds = timeinfo.tm_sec;//Stores the current second.
    days = timeinfo.tm_mday; //The timeinfo struct is populated by getLocalTime(&timeinfo), which fetches time from the NTP server (adjusted by UTC_OFFSET).

}

void update_time_with_check_alarm() {
  update_time();// Sync time
  print_time_now();// Display time on OLED

  if (alarm_enabled) {
      for (int i = 0; i < n_alarms; i++) {
          // Make sure the alarm is checked for every second
          if (alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
              // Check if current time matches any alarm
              if (!alarm_triggered[i]) {
                //If current time matches an alarm and it hasn’t been triggered
                  ring_alarm();
                  alarm_triggered[i] = true;
              }
          } 
          else {
              alarm_triggered[i] = false; // Reset trigger when time moves past alarm
          }
      }
  }
}


void print_time_now() {
    display.clearDisplay(); // Clear OLED
    print_line(String(hours) + ":" + String(minutes) + ":" + String(seconds), 30, 0, 2);
    // Displays current time in HH:MM:SS format.
}


void ring_alarm() { /*Activates visual (LED) and audible (buzzer) alarms when triggered,
  with options to cancel or snooze.*/
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;//ensures the alarm loops until user intervenes

  while (!break_happened) {
      for (int i : notes) {// Iterate through musical notes
          if (digitalRead(PB_CANCEL) == LOW) {// Cancel alarm
              delay(200);
              Serial.println("Alarm canceled.");
              break_happened = true;
              break;// Exit note loop
          }

          if (digitalRead(PB_OK) == LOW) { // Snooze Functionality
              delay(200);
              for (int j = 0; j < n_alarms; j++) {
                  if (alarm_hours[j] == hours && alarm_minutes[j] == minutes) {
                      alarm_minutes[j] += 5; // Add 5 minutes
                      if (alarm_minutes[j] >= 60) { 
                          alarm_minutes[j] -= 60; // Adjust minutes
                          alarm_hours[j] = (alarm_hours[j] + 1) % 24; // Increment hour safely
                      }
                      
                      alarm_triggered[j] = false; // Reset so it rings again
                      Serial.println("Snooze activated. New alarm at " + 
                                      String(alarm_hours[j]) + ":" + String(alarm_minutes[j]));
                  }
              }
              break_happened = true;
              break;
          }

          tone(BUZZER, i);// Play note
          delay(500); // Note duration
          noTone(BUZZER);// Stop buzzer
      }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}




int wait_for_button_press() {// Blocks until a button is pressed, returning the button ID.
  while(true)//loop halts all other operations until a button is pressed.
  {
    if(digitalRead(PB_UP)==LOW){// Up button
      delay(200);
      return PB_UP;
    }
    if(digitalRead(PB_DOWN)==LOW){// Down button
    
      delay(200);
      return PB_DOWN;
    }
    if(digitalRead(PB_OK)==LOW)// OK button
    {
      delay(200);
      return PB_OK;
    }
    if(digitalRead(PB_CANCEL)==LOW)// Cancel button
    {
      delay(200);
      return PB_CANCEL;
    }
    update_time();// Keep time updated while waiting
  }
}

void go_to_menu() {
  unsigned long lastTimeUpdate = millis();  // Store the last update time
  
  while (digitalRead(PB_CANCEL) == HIGH) {// Loop until "CANCEL" is pressed
      display.clearDisplay();
      print_line(modes[current_mode], 0, 0, 2);// Show current menu item


      // Update time every 1 second while in the menu
      if (millis() - lastTimeUpdate >= 1000) {
          update_time();
          lastTimeUpdate = millis();  // Reset time update reference
      }

      int pressed = wait_for_button_press();  // Wait for button press

      if (pressed == PB_UP) {// Scroll up
          delay(200);
          current_mode = (current_mode + 1) % max_modes;
      }
      else if (pressed == PB_DOWN) {// Scroll down
          delay(200);
          current_mode = (current_mode - 1 + max_modes) % max_modes;
      }  
      else if (pressed == PB_OK) { // Select mode
          run_mode(current_mode); // Execute selected mode
      } 
      else if (pressed == PB_CANCEL) {
          break;
      } 
  }

  // Force an NTP resync to correct any delay
  configTime(UTC_OFFSET, 0, NTP_SERVER);
  delay(500);  // Allow time to sync
  update_time();

  // Immediately display the latest time
  display.clearDisplay();
  print_time_now();
}
void run_mode(int mode) {/* Acts as a "router" that executes
   the appropriate function based on the selected menu option.*/
    if (mode == 0) {
      set_time();
    }
    else if (mode == 1) {
      set_alarm();
    }
    else if (mode == 2) {
      view_alarms();
    }
    else if (mode == 3) {
      delete_alarm();
    }
    else if (mode == 4) {
      set_time_zone();
    }
}

void set_time() {  
    display.clearDisplay();
    print_line("Time Set", 0, 0, 2);
    delay(1000);
}

void set_alarm() {//Guides the user through setting a new alarm time via button interactions.
    int alarm_index = 0;// Tracks which alarm is being configured

    // Select which alarm to set (Alarm 1 or Alarm 2)
    while (true) {
        display.clearDisplay();
        print_line("Select Alarm (1/2): " + String(alarm_index + 1), 0, 0, 2);

        int pressed = wait_for_button_press();

        if (pressed == PB_UP) {
            alarm_index = (alarm_index + 1) % n_alarms; // Cycle forward
        } 
        else if (pressed == PB_DOWN) {
            alarm_index = (alarm_index - 1 + n_alarms) % n_alarms;// Cycle backward
        }
        else if (pressed == PB_OK) {
            break;// Confirm selection
        }
        else if (pressed == PB_CANCEL) {
            return;  // Exit without setting an alarm
        }
    }

    int temp_hour = alarm_hours[alarm_index];
    int temp_minute = alarm_minutes[alarm_index];

    // Set Alarm Hour
    while (true) {
        display.clearDisplay();
        print_line("Set Hour: " + String(temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();

        if (pressed == PB_UP) temp_hour = (temp_hour + 1) % 24;//Increment hour (0→23→0)
        else if (pressed == PB_DOWN) temp_hour = (temp_hour - 1 + 24) % 24;//Decrement hour (23→22→...→0→23)
        else if (pressed == PB_OK) break;//Confirm hour
        else if (pressed == PB_CANCEL) return;//Returns to menu
    }

    // Set Alarm Minute
    while (true) {
        display.clearDisplay();
        print_line("Set Minute: " + String(temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();

        if (pressed == PB_UP) temp_minute = (temp_minute + 1) % 60;
        else if (pressed == PB_DOWN) temp_minute = (temp_minute - 1 + 60) % 60;
        else if (pressed == PB_OK) break;//Confirm minute
        else if (pressed == PB_CANCEL) return;//Returns to menu
    }

    // Save the alarm
    alarm_hours[alarm_index] = temp_hour;
    alarm_minutes[alarm_index] = temp_minute;
    alarm_triggered[alarm_index] = false;

    display.clearDisplay();
    print_line("Alarm " + String(alarm_index + 1) + " Set!", 0, 0, 2);
    delay(1000);
}

void view_alarms() {
    display.clearDisplay();
    for (int i = 0; i < n_alarms; i++) {
        if (alarm_hours[i] >= 0) {  // Checks if alarm is set (hours >= 0).
            print_line("Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 0, i * 10, 1);
        } else {
            print_line("Alarm " + String(i + 1) + ": Not Set", 0, i * 10, 1);
        }
    }
    delay(2000);
}

void delete_alarm() {
    display.clearDisplay();
    print_line("Select Alarm to Delete", 0, 0, 2);
    delay(1000);

    int selected_alarm = 0;
    while (true) {
        display.clearDisplay();
        print_line("Delete Alarm " + String(selected_alarm + 1) + "?", 0, 0, 2);

        int pressed = wait_for_button_press();

        if (pressed == PB_UP) {
            selected_alarm = (selected_alarm + 1) % n_alarms;
        } else if (pressed == PB_DOWN) {
            selected_alarm = (selected_alarm - 1 + n_alarms) % n_alarms;
        } else if (pressed == PB_OK) {
            alarm_hours[selected_alarm] = -1;  //  Remove the alarm
            alarm_minutes[selected_alarm] = -1;
            alarm_triggered[selected_alarm] = false;
            
            display.clearDisplay();
            print_line("Alarm Deleted", 0, 0, 2);
            delay(1000);
            break;
        } else if (pressed == PB_CANCEL) {
            break;
        }
    }
}
void set_time_zone() {
    float temp_offset = UTC_OFFSET / 3600.0;  // Convert to hours (can hold decimals)
    while (true) {
        display.clearDisplay();
        print_line("UTC Offset: " + String(temp_offset, 1), 0, 0, 2);  // Show 1 decimal place

        int pressed = wait_for_button_press();

        if (pressed == PB_UP) {
          temp_offset += 0.5;  // Increase by 30 min
        }  
        else if (pressed == PB_DOWN) {
          temp_offset -= 0.5;  // Decrease by 30 min
        }
        else if (pressed == PB_OK) {
          UTC_OFFSET = temp_offset * 3600;  // Convert back to seconds
          configTime(UTC_OFFSET, 0, NTP_SERVER);
          break;
        } 
        else if (pressed == PB_CANCEL) {
          break;
        }
    }
}


void check_temp() {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    if (data.temperature < 24 || data.temperature > 32) print_line("TEMP WARNING!", 0, 30, 1);
    if (data.humidity < 65 || data.humidity > 80) print_line("HUMIDITY WARNING!", 0, 40, 1);
}
