#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

#define BLUE_LED_1_PIN D1
#define BLUE_LED_2_PIN D2
#define DISPLAY_DIO D5
#define DISPLAY_CLK D6
#define RGB_LED_PIN D7
//======================================================================================================================================================================

//========================CONFIGURATIONS================================================================================================================================
const char *AP_SSID = "";                  // WiFi network that will appear at first launch or in case of saved network not found
const char *AP_PASSWORD = "";           // Password from the WiFi network
const char *HOSTNAME = "IRON_MAN_ARC";            // Hostname of the device that will be shown
const char *NTP_POOL_ADDRESS = "ua.pool.ntp.org"; // choose at at https://www.ntppool.org/
const long GMT_3_OFFSET_IN_SECONDS = 3600 * 3;    // Offset in seconds for GMT + 3
const int DISPLAY_BACKLIGHT_LEVEL = 2;            // Adjust it 0 to 7
const int LED_RING_BRIGHTNESS = 200;              // Adjust it 0 to 255
const int LED_RING_BRIGHTNESS_FLASH = 250;        // Adjust it 0 to 255
const int PIXELS_COUNT_ON_RGB_LED = 35;           // How many NeoPixels are attached to the Wemos D1 Mini
const int RED_LED_COLOR_RGB_NUMBER = 00;
const int GREEN_LED_COLOR_RGB_NUMBER = 00;
const int BLUE_LED_COLOR_RGB_NUMBER = 255;
//======================================================================================================================================================================

bool wifi_is_connected;
bool show_colon = true;
unsigned long colon_previous_millis = 0;

Adafruit_NeoPixel led_ring(PIXELS_COUNT_ON_RGB_LED, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
TM1637Display digits_display(DISPLAY_CLK, DISPLAY_DIO);
WiFiUDP wifi_udp_client;
NTPClient ntp_client(wifi_udp_client, NTP_POOL_ADDRESS, GMT_3_OFFSET_IN_SECONDS);
AsyncWebServer server(80);

void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASSWORD);
  WiFi.hostname(HOSTNAME);
  Serial.println("Connecting to wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  if (MDNS.begin(HOSTNAME))
  {
    Serial.println("MDNS responder started");
  }
}

void setupWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "I'm online and running"); });

  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
            { 
              request->send(200, "text/plain", "rebooting");
              delay(500);
              ESP.restart(); });

  server.begin();
  Serial.println("HTTP server started");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("\n Starting");
  setupWiFi();
  pinMode(BLUE_LED_1_PIN, OUTPUT);
  pinMode(BLUE_LED_2_PIN, OUTPUT);
  ElegantOTA.begin(&server);

  setupWebServer();

  ntp_client.begin();
  led_ring.begin();
  flash_cuckoo();
  blue_light(true);
  digits_display.setBrightness(DISPLAY_BACKLIGHT_LEVEL);
  showDigitsOnDisplay(88, 88, show_colon);
}

void loop()
{
  wifi_is_connected = WiFi.status() == WL_CONNECTED;
  if (secondChanged())
  {
    show_colon = !show_colon;
  }

  showTime();
  blue_light(false);
  digitalWrite(BLUE_LED_1_PIN, 1);
  digitalWrite(BLUE_LED_2_PIN, 1);

  if (!wifi_is_connected)
  {
    Serial.println("WiFi connection lost, trying to reconnect...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(AP_SSID, AP_PASSWORD);
    WiFi.hostname(HOSTNAME);
    wifi_is_connected = WiFi.status() == WL_CONNECTED;
    if (wifi_is_connected)
    {
      Serial.println("WiFi connection repaired...");
    }
  }

  ElegantOTA.loop();
  delay(1);
}

void blue_light(bool showSmoothly)
{
  led_ring.setBrightness(LED_RING_BRIGHTNESS);
  for (int i = 0; i < PIXELS_COUNT_ON_RGB_LED; i++)
  {
    led_ring.setPixelColor(i, led_ring.Color(RED_LED_COLOR_RGB_NUMBER, GREEN_LED_COLOR_RGB_NUMBER, BLUE_LED_COLOR_RGB_NUMBER));
    if (showSmoothly)
    {
      led_ring.show();
      delay(100);
    }
  }
  if (!showSmoothly)
  {
    led_ring.show();
  }
}

void flash_cuckoo()
{
  led_ring.setBrightness(LED_RING_BRIGHTNESS_FLASH);
  for (int i = 0; i < PIXELS_COUNT_ON_RGB_LED; i++)
  {
    led_ring.setPixelColor(i, led_ring.Color(250, 250, 250));
  }
  led_ring.show();

  for (int i = LED_RING_BRIGHTNESS_FLASH; i > 10; i--)
  {
    led_ring.setBrightness(i);
    led_ring.show();
    delay(7);
  }
  for (int i = 0; i < PIXELS_COUNT_ON_RGB_LED; i++)
  {
    led_ring.setBrightness(0);
  }
  led_ring.show();
}

void showDigitsOnDisplay(int hour, int minutes, bool showColon)
{
  uint8_t colonOptions = showColon ? 0b01000000 : 0b00000000;
  digits_display.showNumberDecEx(hour, colonOptions, true, 2, 0);
  digits_display.showNumberDecEx(minutes, colonOptions, true, 2, 2);
}

bool secondChanged()
{
  unsigned long currentMillis = millis();

  if (currentMillis - colon_previous_millis >= 1000)
  {
    colon_previous_millis = currentMillis;
    return true;
  }

  return false;
}

void showTime()
{
  if (wifi_is_connected)
  {
    ntp_client.update();
  }
  showDigitsOnDisplay(ntp_client.getHours(), ntp_client.getMinutes(), show_colon);
}