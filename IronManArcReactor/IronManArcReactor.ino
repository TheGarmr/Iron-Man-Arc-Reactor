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
const char *VERSION = "1.2";
const char *AP_SSID = "IRON_MAN_ARC";                       // WiFi network to connect to (change to what you need)
const char *AP_PASSWORD = "";                               // WiFi password (change to what you need)
const char *AP_HOSTNAME = "IRON_MAN_ARC";                   // Device hostname
const char *NTP_POOL_ADDRESS = "ua.pool.ntp.org";           // NTP server address (change to what you need)
const char *OTA_UPDATE_USERNAME = "";                       // To left your OTA page unprotected - leave this blank
const char *OTA_UPDATE_PASSWORD = "";                       // To left your OTA page unprotected - leave this blank
const unsigned long GMT_TIMEZONE_PLUS_HOUR = 2;             // GMT + 2 timezone (Ukrainian Winter time)
const unsigned long NTP_CLIENT_UPDATE_TIME_IN_MS = 3600000; // One hour
const unsigned int WIFI_CONNECTION_TIMEOUT_IN_MS = 10000;   // 10 seconds to connect to wifi
const unsigned int DISPLAY_BACKLIGHT_LEVEL = 2;             // Display brightness level (0-7)
const unsigned int PIXELS_COUNT_ON_RGB_LED = 35;            // Number of NeoPixels
const unsigned int NIGHT_MODE_HOUR_END = 7;                 // Time in hours to enable clock display and increase ring brightness
const unsigned int NIGHT_MODE_HOUR_START = 22;              // Time in hours to shutdown clock display and decrease ring brightness
const unsigned int RED_LED_COLOR_RGB_NUMBER = 0;
const unsigned int GREEN_LED_COLOR_RGB_NUMBER = 0;
const unsigned int BLUE_LED_COLOR_RGB_NUMBER = 255;
//======================================================================================================================================================================

unsigned long colon_previous_millis = 0;
unsigned int LED_RING_BRIGHTNESS = 150;     // LED ring brightness at day(0-255)
unsigned int LED_RING_NIGHT_BRIGHTNESS = 1; // LED ring brightness at night (0-255)
bool auto_night_mode = true;
bool wifi_is_connected = false;
bool show_colon = true;
bool ntp_client_updated_on_startup = false;
bool night_mode_is_enabled = false;
bool disable_clock_in_night_mode = false;

Adafruit_NeoPixel led_ring(PIXELS_COUNT_ON_RGB_LED, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
TM1637Display time_display(DISPLAY_CLK, DISPLAY_DIO);
WiFiUDP wifi_udp_client;
NTPClient ntp_client(wifi_udp_client, NTP_POOL_ADDRESS, 3600 * GMT_TIMEZONE_PLUS_HOUR, NTP_CLIENT_UPDATE_TIME_IN_MS);
AsyncWebServer async_web_server(80);

void setup()
{
  Serial.begin(115200);
  Serial.println("Start of Arc Reactor...");

  turn_off_display();
  pinMode(BLUE_LED_1_PIN, OUTPUT);
  pinMode(BLUE_LED_2_PIN, OUTPUT);
  setupElegantOTA();
  setupWebServer();
  ntp_client.begin();
  led_ring.begin();
  show_white_flash();
  show_blue_light(true);
  setupWiFi();

  if (wifi_is_connected)
  {
    ntp_client_updated_on_startup = ntp_client.update();
  }
  else
  {
    Serial.println("WiFi not connected, turning off display...");
    turn_off_display();
  }
  Serial.println("end of Arc Reactor setup...");
}

void loop()
{
  checkWifiConnectionAndReconnectIfLost();
  int currentHour = ntp_client.getHours();
  night_mode_is_enabled = !(currentHour > NIGHT_MODE_HOUR_END && currentHour < NIGHT_MODE_HOUR_START);

  setLedRingBrightness();
  showTime();
  ElegantOTA.loop();
  delay(1);
}

void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASSWORD);
  WiFi.hostname(AP_HOSTNAME);
  Serial.println("Connecting to wifi...");
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_CONNECTION_TIMEOUT_IN_MS)
  {
    showLoadingOnDigitsDisplay();
  }

  wifi_is_connected = WiFi.status() == WL_CONNECTED;
  if (wifi_is_connected)
  {
    Serial.println("Connected to WiFi...");
    MDNS.begin(AP_HOSTNAME);
  }
  else
  {
    Serial.println("Failed to connect to WiFi... Credentials: " + String(AP_SSID) + " " + String(AP_PASSWORD));
  }
}

void setupWebServer()
{
  async_web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                      { handle_web_root_query(request); });

  async_web_server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request)
                      { request->send(200, "text/plain", String(VERSION)); });

  async_web_server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
                      { handle_reboot_query(request); });

  async_web_server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request)
                      { handle_config_query(request); });

  async_web_server.begin();
  Serial.println("HTTP server started");
}

void setupElegantOTA()
{
  if (isValidString(OTA_UPDATE_USERNAME) && isValidString(OTA_UPDATE_PASSWORD))
  {
    ElegantOTA.setAuth(OTA_UPDATE_USERNAME, OTA_UPDATE_PASSWORD);
  }
  ElegantOTA.begin(&async_web_server);
}

void setLedRingBrightness()
{
  if (night_mode_is_enabled)
  {
    led_ring.setBrightness(LED_RING_NIGHT_BRIGHTNESS);
    led_ring.show();
    if (disable_clock_in_night_mode)
    {
      turn_off_display();
    }
  }
  else
  {
    led_ring.setBrightness(LED_RING_BRIGHTNESS);
    led_ring.show();
    if (!disable_clock_in_night_mode)
    {
      turn_on_display();
    }
  }
}

void checkWifiConnectionAndReconnectIfLost()
{
  wifi_is_connected = WiFi.status() == WL_CONNECTED;

  if (!wifi_is_connected)
  {
    Serial.println("WiFi connection lost, trying to reconnect...");
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.hostname(AP_HOSTNAME);
    WiFi.begin(AP_SSID, AP_PASSWORD);
    unsigned long startAttemptTime = millis();
    const unsigned long connectionTimeout = 10000;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connectionTimeout)
    {
      delay(500);
      Serial.print(".");
    }

    wifi_is_connected = WiFi.status() == WL_CONNECTED;
  }
}

void show_blue_light(bool showSmoothly)
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

void show_white_flash()
{
  led_ring.setBrightness(LED_RING_BRIGHTNESS);
  for (int i = 0; i < PIXELS_COUNT_ON_RGB_LED; i++)
  {
    led_ring.setPixelColor(i, led_ring.Color(250, 250, 250));
  }
  led_ring.show();

  for (int i = LED_RING_BRIGHTNESS; i > 10; i--)
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
  time_display.showNumberDecEx(hour, colonOptions, true, 2, 0);
  time_display.showNumberDecEx(minutes, colonOptions, true, 2, 2);
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
  if (night_mode_is_enabled and disable_clock_in_night_mode)
  {
    turn_off_display();
    return;
  }

  bool ntp_client_updated = false;
  if (wifi_is_connected)
  {
    ntp_client_updated = ntp_client.update();
  }
  if (!ntp_client_updated && !ntp_client_updated_on_startup)
  {
    turn_off_display();
  }
  else
  {
    if (secondChanged())
    {
      show_colon = !show_colon;
    }
    showDigitsOnDisplay(ntp_client.getHours(), ntp_client.getMinutes(), show_colon);
  }
}

void showLoadingOnDigitsDisplay()
{
  turn_on_display();
  uint8_t frames[][4] = {
      {0x01 | 0x02, 0x01 | 0x02, 0x01 | 0x02, 0x01 | 0x02}, // Upper horizontal and right upper
      {0x02 | 0x04, 0x02 | 0x04, 0x02 | 0x04, 0x02 | 0x04}, // Right upper and right lower
      {0x04 | 0x08, 0x04 | 0x08, 0x04 | 0x08, 0x04 | 0x08}, // Right lower and bottom horizontal
      {0x08 | 0x10, 0x08 | 0x10, 0x08 | 0x10, 0x08 | 0x10}, // Bottom horizontal and left lower
      {0x10 | 0x20, 0x10 | 0x20, 0x10 | 0x20, 0x10 | 0x20}, // Left lower and left upper
      {0x20 | 0x01, 0x20 | 0x01, 0x20 | 0x01, 0x20 | 0x01}, // Left upper and upper horizontal
  };

  static int currentFrame = 0; // Current animation frame
  time_display.setSegments(frames[currentFrame]);
  currentFrame = (currentFrame + 1) % 6; // Move to the next frame
  delay(300);
}

void turn_off_display()
{
  time_display.clear();
  time_display.setBrightness(0, false);
}

void turn_on_display()
{
  time_display.setBrightness(DISPLAY_BACKLIGHT_LEVEL, true);
}

bool isValidString(const String &str)
{
  return !str.isEmpty();
}