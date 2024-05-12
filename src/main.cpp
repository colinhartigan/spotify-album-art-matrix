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

unsigned int lcdRefreshTime = 400;
unsigned int nextLcdRefresh;

// -----------------------------------
// "globals"
#define ALBUM_ART "/album.jpg"
String lastAlbumUri;
Mode currentMode = SPOTIFY;

// tasks
TaskHandle_t lcdTask;

// -----------------------------------
bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // callback for the jpeg decoder, will render the image in blocks, then when this is done we draw (later in the loop)
    matrix.drawRGBBitmap(x, y, bitmap, w, h);

    return 1;
}

int displayImageUsingFile(char *albumArtUrl)
{
    if (SPIFFS.exists(ALBUM_ART) == true)
    {
        Serial.println("Removing existing image");
        SPIFFS.remove(ALBUM_ART);
    }

    fs::File f = SPIFFS.open(ALBUM_ART, "w+");
    if (!f)
    {
        Serial.println("file open failed");
        return -1;
    }

    // Spotify uses a different cert for the Image server, so we need to swap to that for the call
    client.setCACert(spotify_image_server_cert);
    bool gotImage = spotify.getImage(albumArtUrl, &f);

    // Swapping back to the main spotify cert
    client.setCACert(spotify_server_cert);

    // Make sure to close the file!
    f.close();

    if (gotImage)
    {
        return TJpgDec.drawFsJpg(0, 0, ALBUM_ART);
    }
    else
    {
        return -2;
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
        matrix.show();
        free(imageFile); // Make sure to free the memory!
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
    case SPOTIFY:
        lcd.backlight();
        lcd.clear();
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
        setMode(CLOCK);
        return;
    }
    {
        // make sure mode is spotify
        setMode(SPOTIFY);

        // print some stuff on the lcd
        String artist = String(currentlyPlaying.artists[0].artistName);
        String song = String(currentlyPlaying.trackName);

        // line 1 should be in the format of "Artist - Song"
        String line1 = artist + " - " + song;
        updateLine1((char *)line1.c_str(), true);

        // line2 should show a progress bar
        int progress = currentlyPlaying.progressMs;
        int duration = currentlyPlaying.durationMs;
        int progressLength = (progress * 16) / duration;
        String line2 = "";
        for (int i = 0; i < 16; i++)
        {
            if (i < progressLength)
            {
                line2 += "=";
            }
            else
            {
                line2 += "-";
            }
        }
        updateLine2((char *)line2.c_str(), false);

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

void lcdLoop(void *pvParameters)
{
    while (1)
    {
        if (millis() > nextLcdRefresh)
        {
            updateLcd();
            nextLcdRefresh = millis() + lcdRefreshTime;
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
    matrix.setBrightness(100);
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
    TJpgDec.setCallback(displayOutput);

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