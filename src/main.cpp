
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

#include <LiquidCrystal.h>

// custom modules
#include <config.h>
#include <clock.h>
#include <globals.h>

// spotify config
#define SPOTIFY_MARKET "US"
#include <SpotifyArduinoCert.h>

// spotify/wifi client setup
WiFiClientSecure client;
SpotifyArduino spotify(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

// lcd
LiquidCrystal lcd(23, 22, 5, 18, 19, 21);

// timing
unsigned long delayBetweenRequests = 1000; // = 1 second, fairly responsive
unsigned long requestDueTime;

// -----------------------------------
// globals
#define ALBUM_ART "/album.jpg"
String lastAlbumUri;
CurrentlyPlaying currentlyPlaying;

bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // callback for the jpeg decoder, will render the image in blocks, then when this is done we draw (later in the loop)
    matrix.drawRGBBitmap(x, y, bitmap, w, h);

    return 1;
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
    matrix.setBrightness(150);
    matrix.fillScreen(0);
    matrix.show();

    // lcd setup
    // lcd.begin(16, 2);

    // working with 64x64 images, downscale by 4 = 16x16
    TJpgDec.setJpgScale(4);

    // set callback function for jpeg decoder
    TJpgDec.setCallback(displayOutput);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WLAN_SSID, WLAN_PASS);

    // lcd.setCursor(0, 0);
    // lcd.print("connecting");
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

    Serial.println("");
    Serial.printf("connected to %s\n", WLAN_SSID);
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

    // init RTC
    initClock();

    client.setCACert(spotify_server_cert);
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
    uint8_t *imageFile; // pointer that the library will store the image at (uses malloc)
    int imageSize;      // library will update the size of the image
    bool gotImage = spotify.getImage(albumArtUrl, &imageFile, &imageSize);

    if (gotImage)
    {
        Serial.print("got image");
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

void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    // Use the details in this method or if you want to store them
    // make sure you copy them (using something like strncpy)
    const char *artist = currentlyPlaying.artists[0].artistName;
    const char *song = currentlyPlaying.trackName;

    // lcd.clear();
    // lcd.setCursor(0, 0);
    // lcd.print(artist);
    // lcd.setCursor(0, 1);
    // lcd.print(song);

    // Clear the Text every time a new song is created
    Serial.println("--------- Currently Playing ---------");

    Serial.print("Is Playing: ");
    if (currentlyPlaying.isPlaying)
    {
        Serial.println("Yes");
    }
    else
    {
        Serial.println("No");
    }

    Serial.print("Track: ");
    Serial.println(currentlyPlaying.trackName);
    // Save the song name to a variable
    // songName = const_cast<char *>(currentlyPlaying.trackName);
    Serial.print("Track URI: ");
    Serial.println(currentlyPlaying.trackUri);
    Serial.println();

    Serial.println("Artists: ");
    for (int i = 0; i < currentlyPlaying.numArtists; i++)
    {
        Serial.print("Name: ");
        // Save the song artist name to a variable
        Serial.println(currentlyPlaying.artists[i].artistName);
        // songArtist = const_cast<char *>(currentlyPlaying.artists[0].artistName);
        Serial.print("Artist URI: ");
        Serial.println(currentlyPlaying.artists[i].artistUri);
        Serial.println();
    }

    Serial.print("Album: ");
    Serial.println(currentlyPlaying.albumName);
    Serial.print("Album URI: ");
    Serial.println(currentlyPlaying.albumUri);
    Serial.println();

    long progress = currentlyPlaying.progressMs; // duration passed in the song
    long duration = currentlyPlaying.durationMs; // Length of Song
    Serial.print("Elapsed time of song (ms): ");
    Serial.print(progress);
    Serial.print(" of ");
    Serial.println(duration);
    Serial.println();

    float percentage = ((float)progress / (float)duration) * 100;
    int clampedPercentage = (int)percentage;
    Serial.print("<");
    for (int j = 0; j < 50; j++)
    {
        if (clampedPercentage >= (j * 2))
        {
            Serial.print("=");
        }
        else
        {
            Serial.print("-");
        }
    }
    Serial.println(">");
    Serial.println();

    // will be in order of widest to narrowest
    // currentlyPlaying.numImages is the number of images that
    // are stored

    for (int i = 0; i < currentlyPlaying.numImages; i++)
    {
        // Save the third album image into the smallestImage Variable above.
        // smallestImage = currentlyPlaying.albumImages[2];
        Serial.println("------------------------");
        Serial.printf("Album Image: (%d) ", i);
        Serial.println(currentlyPlaying.albumImages[i].url);
        Serial.print("Dimensions: ");
        Serial.print(currentlyPlaying.albumImages[i].width);
        Serial.print(" x ");
        Serial.print(currentlyPlaying.albumImages[i].height);
        Serial.println();
    }
    Serial.println("------------------------");
}

void currentlyPlayingCallback(CurrentlyPlaying newCurrentlyPlaying)
{
    currentlyPlaying = newCurrentlyPlaying;
}

void loop()
{
    if (millis() > requestDueTime)
    {
        Serial.printf("free heap: %d\n", ESP.getFreeHeap());

        Serial.println("getting player state");

        int status = spotify.getCurrentlyPlaying(currentlyPlayingCallback, SPOTIFY_MARKET);
        if (status == 200)
        {
            Serial.println("got currently playing");
            if (currentlyPlaying.isPlaying)
            {
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
            else
            {
                drawClock();
                lastAlbumUri = "";
            }
        }
        else
        {
            drawClock();
            lastAlbumUri = "";
        }

        requestDueTime = millis() + delayBetweenRequests;
    }
}