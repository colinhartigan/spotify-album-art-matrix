char *stack_start; // to check stack size :)
// std libs
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include "SPIFFS.h"

// extras
#include <TJpg_Decoder.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <SpotifyArduino.h>
#include <ArduinoJson.h>
#include <SpotifyArduinoCert.h>

#include <ESP_Color.h>

// custom modules
#include <config.h>
#include <clock.h>
#include <globals.h>
#include <LcdController.h>

// spotify config
#define SPOTIFY_MARKET "US"

// spotify/wifi client setup
WiFiClientSecure client;
SpotifyArduino spotify(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

// timing
unsigned int spotifyRefreshTime = 1000;
unsigned int nextSpotifyRefresh;

unsigned int lcdRefreshTime = 500;
unsigned int nextLcdRefresh;

// -----------------------------------
// spotify
#define ALBUM_ART "/album.jpg"
String lastAlbumUri;
Mode currentMode = SPOTIFY_PLAYING;

// progress
int duration;
int lastProgress;
unsigned long lastProgressTime;

// matrix state
ESP_Color::Color matrixBuffer[16][16];
ESP_Color::Color matrixMirror[16][16];

#define FULL_BRIGHTNESS 75
#define TRANSITION_INTERVAL 2
#define TRANSITION_TIME 300

// tasks
TaskHandle_t lcdTask;

// -----------------------------------
bool updateBuffer(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    for (int i = x; i < x + w; i++)
    {
        for (int j = y; j < y + h; j++)
        {
            uint16_t pixel = bitmap[(i - x) + (j - y) * w];

            ESP_Color::Color color = ESP_Color::Color(pixel);

            matrixBuffer[i][j] = color;
        }
    }

    return 1;
}

float lerp(float a, float b, float t, bool clamp)
{
    if (clamp)
    {
        if (b - a > 0.5)
        {
            a += 1;
        }
        else if (a - b > 0.5)
        {
            b += 1;
        }
    }

    float out = a + (b - a) * t;
    if (clamp)
    {
        out = fmod(out, 1);
    }
    return out;
}

void lerpMatrix()
{
    // get matrix state, convert to hsv, then fade to next color in buffer
    Serial.println("lerping matrix");
    int steps = TRANSITION_TIME / TRANSITION_INTERVAL;
    for (int n = 0; n < steps; n++)
    {
        for (int i = 0; i < 16; i++)
        {
            for (int j = 0; j < 16; j++)
            {
                // get pixel's target color
                ESP_Color::Color target = matrixBuffer[i][j];
                ESP_Color::HSVf targetHsv = target.ToHsv();
                float h2 = targetHsv.H;
                float s2 = targetHsv.S;
                float v2 = targetHsv.V;

                // get initial values
                ESP_Color::Color color = matrixMirror[i][j];
                ESP_Color::HSVf initialHsv = color.ToHsv();
                float h1 = initialHsv.H;
                float s1 = initialHsv.S;
                float v1 = initialHsv.V;

                // lerp
                float stepsFloat = (float)steps;

                float h = lerp(h1, h2, n / stepsFloat, true);
                float s = lerp(s1, s2, n / stepsFloat, false);
                float v = lerp(v1, v2, n / stepsFloat, false);

                ESP_Color::Color rgb = ESP_Color::Color::FromHsv(h, s, v);

                matrix.drawPixel(i, j, rgb.ToRgb565());
            }
        }
        matrix.show();
        delay(TRANSITION_INTERVAL);
    }
}

void drawBuffer()
{
    // finally load in the actual buffer jic stuff was lost in the lerp
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            matrix.drawPixel(i, j, matrixBuffer[i][j].ToRgb565());
            matrixMirror[i][j] = matrixBuffer[i][j];
        }
    }
}

int displayImage(char *albumArtUrl)
{
    uint8_t *imageFile;
    int imageSize;
    bool gotImage = spotify.getImage(albumArtUrl, &imageFile, &imageSize);

    if (gotImage)
    {
        Serial.println("got image");
        delay(1);
        int jpegStatus = TJpgDec.drawJpg(0, 0, imageFile, imageSize);
        free(imageFile);
        lerpMatrix();
        drawBuffer();
        matrix.show();
        return jpegStatus;
    }
    else
    {
        return -2;
    }
}

void setMode(Mode newMode)
{
    if (newMode == currentMode)
        return;

    switch (newMode)
    {
    case SPOTIFY_PLAYING:
        lcd.backlight();
        lcd.clear();
        matrix.setBrightness(FULL_BRIGHTNESS);
        drawBuffer();
        matrix.show();
        break;

    case SPOTIFY_PAUSED:
        matrix.setBrightness(10);
        matrix.show();
        break;

    case CLOCK:
        lcd.noBacklight();
        lcd.clear();
        lastAlbumUri = "";
        break;

    default:
        break;
    }

    currentMode = newMode;
}

void currentlyPlayingCallback(CurrentlyPlaying currentlyPlaying)
{
    if (!currentlyPlaying.isPlaying)
    {
        setMode(SPOTIFY_PAUSED);
        return;
    }
    {
        // make sure mode is spotify
        setMode(SPOTIFY_PLAYING);

        // print some stuff on the lcd
        String artist = String(currentlyPlaying.artists[0].artistName);
        String song = String(currentlyPlaying.trackName);

        // line 1 should be in the format of "Artist - Song"
        String line1 = artist + " - " + song;
        updateLine1((char *)line1.c_str(), true);

        // line2 should show a progress bar
        int newProgress = currentlyPlaying.progressMs / 1000;
        int newDuration = currentlyPlaying.durationMs / 1000;
        if (newProgress != lastProgress)
        {
            lastProgress = newProgress;
            lastProgressTime = millis();
        }
        duration = newDuration;

        // update album art
        SpotifyImage smallestImage = currentlyPlaying.albumImages[2];
        String newAlbum = String(smallestImage.url);
        if (newAlbum != lastAlbumUri)
        {
            Serial.println("updating art");
            int displayImageResult = displayImage((char *)(smallestImage.url));

            if (displayImageResult == 0)
            {
                lastAlbumUri = newAlbum;
            }
            else
            {
                Serial.print("failed to display image: ");
                Serial.println(displayImageResult);
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);

    if (!SPIFFS.begin()) // if not working for some reason, pass true and it will format SPIFFS. after that you can prob remove the true flag
    {
        Serial.println("SPIFFS init failed!");
        while (1)
            yield();
    }
    Serial.println("\r\ninit done.");

    // matrix setup
    matrix.begin();
    matrix.setBrightness(FULL_BRIGHTNESS);
    matrix.fillScreen(0);
    matrix.show();

    // lcd setup
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.noAutoscroll();

    // working with 64x64 images, downscale by 4 = 16x16
    TJpgDec.setJpgScale(4);

    // set callback function for jpeg decoder
    TJpgDec.setCallback(updateBuffer);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WLAN_SSID, WLAN_PASS);

    lcd.print("connecting");
    int col = 3;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        matrix.drawPixel(col, 8, matrix.Color(255, 255, 255));
        matrix.show();
        col += 3;
        if (col > 12)
        {
            col = 3;
            matrix.clear();
        }
    }
    matrix.clear();
    matrix.show();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().c_str());

    // init RTC
    initClock();

    client.setCACert(spotify_server_cert);

    // setup lcd task
    xTaskCreatePinnedToCore(
        lcdLoop,   /* Function to implement the task */
        "lcdLoop", /* Name of the task */
        10000,     /* Stack size in words */
        NULL,      /* Task input parameter */
        1,         /* Priority of the task */
        &lcdTask,  /* Task handle. */
        0);        /* Core where the task should run */
}

void lcdLoop(void *pvParameters)
{
    while (1)
    {
        if (millis() > nextLcdRefresh)
        {
            String line2 = "";

            int elapsed = (millis() - lastProgressTime) / 1000;
            int progress = lastProgress + elapsed;
            if (progress > duration)
                progress = duration;

            line2 += String(progress / 60);
            line2 += ":";
            if (progress % 60 < 10)
                line2 += "0";
            line2 += String(progress % 60);
            line2 += " / ";
            line2 += String(duration / 60);
            line2 += ":";
            if (duration % 60 < 10)
                line2 += "0";
            line2 += String(duration % 60);
            updateLine2((char *)line2.c_str(), false);

            updateLcd();
            nextLcdRefresh = millis() + lcdRefreshTime;
        }
    }
}

void loop()
{
    if (millis() > nextSpotifyRefresh)
    {
        // memory usage for debugging
        Serial.printf("free heap: %d\n", ESP.getFreeHeap());
        char stack;
        Serial.printf("free stack: %d\n", &stack - stack_start);

        // loop things
        Serial.println("getting player state");

        int status = spotify.getCurrentlyPlaying(currentlyPlayingCallback, SPOTIFY_MARKET);
        if (status == 200)
        {
        }
        else
        {
            setMode(CLOCK);
        }

        if (currentMode == CLOCK)
        {
            drawClock();
        }

        nextSpotifyRefresh = millis() + spotifyRefreshTime;
    }
}