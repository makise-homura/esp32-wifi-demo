#include <WiFi.h>
#include <WiFiUdp.h>
#include "src/NTPClient/NTPClient.h"
#include <TimeLib.h>

// USE_SINGLE_UART    "USB CDC on Boot" option     Result
// ---------------------------------------------------------------------------------------------------------------------------------
//    no effect              disabled              all output goes into GPIO UART (pin 20: RX to ESP32-C3, pin 21: TX from ESP32-C3)
//       0                   enabled               messages go into USB UART, time goes into GPIO UART
//       1                   enabled               all output goes into USB UART
#define            USE_SINGLE_UART    0
const char*        wifi_ssid        = "133-2.4G";
const char*        wifi_password    = "f2line..";
const char*        ntp_server       = "0.ru.pool.ntp.org"; // Or "185.211.244.47" if your DNS server is not well reachable
const int          ntp_correction   = +3; // hours
const int          ntp_interval     = 60; // seconds
const int          ntp_retry_time   = 10; // seconds
const int          gpio_led         = 8;
const int          gpio_button      = 9;
const wifi_power_t wifi_power       = WIFI_POWER_8_5dBm;

#if(ARDUINO_USB_CDC_ON_BOOT)
  #if(USE_SINGLE_UART)
    #define UART_Messages HWCDCSerial
    #define UART_Time HWCDCSerial
    #define MSGSTART ""
    #define TIMEEND  "\r\n"
    #define INIT_BOTH_UARTS false
  #else
    #define UART_Messages HWCDCSerial
    #define UART_Time Serial0
    #define MSGSTART ""
    #define TIMEEND  "\r"
    #define INIT_BOTH_UARTS true
  #endif
#else
  #define UART_Messages Serial0
  #define UART_Time Serial0
  #define MSGSTART "\r\n"
  #define TIMEEND  "\r"
  #define INIT_BOTH_UARTS false
#endif

enum {S_INIT, S_CONN, S_TIME} status;

WiFiUDP ntpUDP;
NTPClient *timeClient;

long ntp_started;
bool btn_pressed = false;

void setup()
{
  UART_Messages.begin(115200);
  if (INIT_BOTH_UARTS) UART_Time.begin(115200);
  UART_Messages.println("Initializing...");
  pinMode(gpio_led, OUTPUT);
  pinMode(gpio_button, INPUT_PULLUP);
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
        UART_Messages.printf(MSGSTART "Connected to the WiFi network\r\nIP address: %s\r\n", WiFi.localIP().toString().c_str());
        timeClient->startUpdate();
        ntp_started = millis();
        status = S_CONN;
      }
      break;
    case S_CONN:
      if (timeClient->finishUpdate())
      {
        setTime(timeClient->getEpochTime());
        UART_Messages.printf(MSGSTART "Synchronized system time from %s\r\nCurrent time: %s\r\n", ntp_server, timeClient->getFormattedTime());
        status = S_TIME;
      }
      else if (millis() - ntp_started > ntp_retry_time * 1000)
      {
        UART_Messages.printf(MSGSTART "Syncing system time from %s failed, retyring...\r\n", ntp_server);
        WiFi.reconnect();
        status = S_INIT;
      }
      break;
  }
  switch(digitalRead(gpio_button))
  {
    case HIGH:
      btn_pressed = false;
      break;
    case LOW:
      if(btn_pressed == false)
      {
        UART_Messages.printf(MSGSTART "Button network reset requested, reconnecting...\r\n");
        WiFi.reconnect();
        status = S_INIT;
      }
      btn_pressed = true;
      break;
  }

  time_t t = now();
  int half = millis() % 1000 < 500;
  char s = half ? ':': ' ';
  UART_Time.printf("%02d-%02d-%04d %02d%c%02d%c%02d [%s] [%s] (%ld dBm, channel %d)" TIMEEND, day(t), month(t), year(t), hour(t), s, minute(t), s, second(t), WiFi.status() == WL_CONNECTED ? "CONN" : "OFFL" , status >= S_TIME ? "SYNC" : status >= S_CONN ? "SENT" : "SRCH", WiFi.RSSI(), WiFi.channel());
  digitalWrite(gpio_led, half ? LOW : HIGH);
  delay(100);
}
