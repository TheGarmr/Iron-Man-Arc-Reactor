void handle_web_root_query(AsyncWebServerRequest *request)
{
    String html = indexPageHtml;
    html.replace("%%TOGGLE_STATE%%", is_summer_time ? "checked" : "");

    request->send(200, "text/html", html);
}

void handle_reboot_query(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", "rebooting");
    AsyncWebServerRequest *req = request;
    req->onDisconnect([]()
                      {
                  turn_off_display();
                  delay(500);
                  ESP.restart(); });
}

void handle_config_query(AsyncWebServerRequest *request)
{
    if (request->hasParam("brightness", true))
    {
        int brightness = request->getParam("brightness", true)->value().toInt();
        if (brightness < 0 or brightness > 255)
        {
            request->send(400, "text/plain", "Invalid value for 'brightness'. Use values from 0 to 255.");
        }
        LED_RING_BRIGHTNESS = brightness;
        show_blue_light(false);
    }

    if (request->hasParam("brightnessAtNight", true))
    {
        int brightnessAtNight = request->getParam("brightnessAtNight", true)->value().toInt();
        if (brightnessAtNight < 0 or brightnessAtNight > 255)
        {
            request->send(400, "text/plain", "Invalid value for 'brightnessAtNight'. Use values from 0 to 255.");
        }
        else
        {
            led_ring.show();
        }
        LED_RING_NIGHT_BRIGHTNESS = brightnessAtNight;
        show_blue_light(false);
    }

    if (request->hasParam("autoNightMode", true))
    {
        String autoNightModeParamValue = request->getParam("autoNightMode", true)->value();
        autoNightModeParamValue.toLowerCase();
        if (autoNightModeParamValue == "true")
        {
            auto_night_mode = true;
        }
        else if (autoNightModeParamValue == "false")
        {
            auto_night_mode = false;
        }
        else
        {
            request->send(400, "text/plain", "Invalid value for 'autoNightMode'. Use 'true' or 'false'.");
            return;
        }
    }

    if (request->hasParam("disableClockAtNight", true))
    {
        String disableClockAtNightParamValue = request->getParam("disableClockAtNight", true)->value();
        disableClockAtNightParamValue.toLowerCase();
        if (disableClockAtNightParamValue == "true")
        {
            disable_clock_in_night_mode = true;
        }
        else if (disableClockAtNightParamValue == "false")
        {
            disable_clock_in_night_mode = false;
        }
        else
        {
            request->send(400, "text/plain", "Invalid value for 'disable_clock_in_night_mode'. Use 'true' or 'false'.");
            return;
        }
    }

    request->send(200, "text/plain", "Configuration updated");
}