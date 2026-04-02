#include <WiFi.h>
#include <WiFiUdp.h>
#include "src/NTPClient/NTPClient.h"
#include <TimeLib.h>

const char*        wifi_ssid        = "133-2.4G";
const char*        wifi_password    = "f2line..";
const char*        ntp_server       = "0.ru.pool.ntp.org"; // Or "185.211.244.47" if your DNS server is not well reachable
const int          ntp_correction   = +3; // hours
const int          ntp_interval     = 60; // seconds
const int          ntp_retry_time   = 10; // seconds
const int          gpio_led         = 8;
const wifi_power_t wifi_power       = WIFI_POWER_8_5dBm;

enum {S_INIT, S_CONN, S_TIME} status;

WiFiUDP ntpUDP;
NTPClient *timeClient;

long ntp_started;

void setup()
{
  // Don't forget to turn on "USB CDC on Boot" option, or you won't see any serial output.
  Serial.begin(115200);
  Serial.println("Initializing..");
  pinMode(gpio_led, OUTPUT);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setTxPower(wifi_power);
  timeClient = new NTPClient(ntpUDP, ntp_server, ntp_correction * 3600, ntp_interval * 1000); 
  timeClient->begin();
  status = S_INIT;
}

void loop() {
  switch(status)
  {
    case S_INIT:
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.printf("\r\nConnected to the WiFi network\r\nIP address: %s\r\n", WiFi.localIP().toString().c_str());
        timeClient->startUpdate();
        ntp_started = millis();
        status = S_CONN;
      }
      break;
    case S_CONN:
      if (timeClient->finishUpdate())
      {
        setTime(timeClient->getEpochTime());
        Serial.printf("\r\nSynchronized system time from %s\r\nCurrent time: %s\r\n", ntp_server, timeClient->getFormattedTime());
        status = S_TIME;
      }
      else if (millis() - ntp_started > ntp_retry_time * 1000)
      {
        Serial.printf("\r\nSyncing system time from %s failed, retyring...\r\n", ntp_server);
        WiFi.reconnect();
        status = S_INIT;
      }
      break;
  }

  time_t t = now();
  int half = millis() % 1000 < 500;
  char s = half ? ':': ' ';
  Serial.printf("%02d%c%02d%c%02d %c\r", hour(t), s, minute(t), s, second(t), status >= S_TIME ? '+' : status >= S_CONN ? '*' : '?');
  digitalWrite(gpio_led, half ? LOW : HIGH);
  delay(100);
}
