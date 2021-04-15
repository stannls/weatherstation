/*
 * This file is part of the weatherstation distribution
 * (https://github.com/ydshz/weatherstation). Copyright (c) 2021 Yannis Storrer.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <BME280I2C.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include <string>
#include <vector>

using namespace std;

const char *ssid = "";
const char *password = "";

HTTPClient http;

int alertBar = 50;

BME280I2C bme;
TFT_eSPI tft = TFT_eSPI();
float pressure;
vector<float> pressure_history;
float temp;
vector<float> temp_history;
float wet;
vector<float> wet_history;
int timer = 0;
bool isAllowedToMail = true;

/*
   Sets the alert bar one unit up if it isn't at the
   maximum.
 */
void ICACHE_RAM_ATTR barUp() {
  if (alertBar < 50) {
    alertBar++;
  }
}

/*
   Sets the alert bar one unit down if it isn't at the
   minimum.
 */
void ICACHE_RAM_ATTR barDown() {
  if (alertBar > 0) {
    alertBar--;
  }
}

/*
   Setup function, which runs when the programm starts.
 */
void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);
  while (!bme.begin()) {
    Serial.println("BME280 nicht gefunden!\n");
    delay(1000);
  }
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextSize(2);
  tft.println("Setting up BME");
  tft.println("Connecting to wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to wifi");
  }
  Serial.println("Connected to wifi");
  tft.println("Connected to wifi");
  Serial.println(WiFi.localIP());
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  tft.println("Attaching Interupts");
  pinMode(D6, INPUT);
  pinMode(D4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(D6), barUp, RISING);
  attachInterrupt(digitalPinToInterrupt(D4), barDown, FALLING);
  delay(5000);
  clearDisplay();
}

/*
   Reads the data from the BME.
 */
void DatenAusgeben()  // Unterprogramm zum Auslesen und Anzeigen der Daten
{
  bme.read(
      pressure, temp,
      wet);  // Werte für Druck, Temperatur und Luftfeuchte werden ausgelesen
  if (timer == 120) {
    if (pressure_history.size() >= 225) {
      pressure_history.erase(pressure_history.begin());
      temp_history.erase(temp_history.begin());
      wet_history.erase(wet_history.begin());
    }
    pressure_history.emplace_back(pressure);
    temp_history.emplace_back(temp);
    wet_history.emplace_back(wet);
    timer = 0;
  } else {
    timer++;
  }
}

/*
   Fills the screen black and resets the cursor.
 */
void clearDisplay() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0, 2);
}

/*
   Prints the current values on the display and draws the graphs.
 */
void visualizeData() {
  tft.drawRect(0, 0, 240, 320, TFT_WHITE);
  tft.setCursor(5, 5, 2);

  tft.print("Temp: ");
  tft.print(temp);
  tft.println("C");
  tft.fillRect(5, 50, 230, 55, TFT_BLACK);
  tft.drawRect(5, 50, 230, 55, TFT_WHITE);
  for (int i = 0; i < temp_history.size(); i++) {
    tft.drawLine(6 + i, 104 - temp_history[i], 6 + i, 104 - temp_history[i],
                 TFT_WHITE);
  }
  tft.drawLine(6, 100 - alertBar, 233, 100 - alertBar, TFT_RED);

  tft.setCursor(5, 105, 2);
  tft.print("Pres:");
  tft.print(pressure);
  tft.println("hP");
  tft.fillRect(5, 150, 230, 55, TFT_BLACK);
  tft.drawRect(5, 150, 230, 55, TFT_WHITE);
  for (int i = 0; i < pressure_history.size(); i++) {
    tft.drawLine(6 + i, 204 - pressure_history[i] / 4000, 6 + i,
                 204 - pressure_history[i] / 4000, TFT_WHITE);
  }

  tft.setCursor(5, 205, 2);
  tft.print("Wet: ");
  tft.print(wet);
  tft.println("%");
  tft.fillRect(5, 250, 230, 55, TFT_BLACK);
  tft.drawRect(5, 250, 230, 55, TFT_WHITE);
  for (int i = 0; i < wet_history.size(); i++) {
    tft.drawLine(6 + i, 304 - wet_history[i] / 2, 6 + i,
                 304 - wet_history[i] / 2, TFT_WHITE);
  }
}

/*
   The alterManager functions checks if a value is too high and sends an email
   if not already send.
 */
void alertManager() {
  if (isAllowedToMail && temp >= alertBar) {
    sendMail("yannis.storrer@web.de", "The station is too hot", temp);
    isAllowedToMail = false;
  }
  if (!isAllowedToMail && temp < alertBar) {
    isAllowedToMail = true;
  }
}

/*
   The loop function which runs in a loop.
 */
void loop() {
  DatenAusgeben();
  visualizeData();
  alertManager();
  delay(500);
}

/*
   Sends a mail to an user with a reason and a value which is too high.

   @param email: The mail adress of the targeted user.
   @param reason: The reason to send the mail.
   @param value: THe value which is too high.
 */
void sendMail(String email, String reason, float value) {
  http.begin("https://thawing-journey-71753.herokuapp.com/alert", true);
  http.addHeader("Content-Type", "application/json");
  String postData = "{\"email\": \"";
  postData += email;
  postData += "\", \"reason\": \"";
  postData += reason;
  postData += "\", \"value\": ";
  postData += String(value);
  postData += "}";
  auto httpCode = http.POST(postData);
  String payload = http.getString();
  http.end();
}
