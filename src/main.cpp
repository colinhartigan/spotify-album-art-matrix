
// ----------------------------
// Standard Libraries
// ----------------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <FS.h>
#include "SPIFFS.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <TJpg_Decoder.h>

#include <SpotifyArduino.h>
#include <ArduinoJson.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <config.h>

// Country code, including this is advisable
#define SPOTIFY_MARKET "US"

// including a "spotify_server_cert" variable
// header is included as part of the SpotifyArduino libary
#include <SpotifyArduinoCert.h>

// led matrix
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, 5,
                                               NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                                                   NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                                               NEO_GRB + NEO_KHZ800);

// file name for where to save the image.
#define ALBUM_ART "/album.jpg"

// so we can compare and not download the same image if we already have it.
String lastAlbumArtUrl;

// Variable to hold image info
SpotifyImage smallestImage;

// so we can store the song name and artist name
char *songName;
char *songArtist;

WiFiClientSecure client;
SpotifyArduino spotify(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

// You might want to make this much smaller, so it will update responsively

unsigned long delayBetweenRequests = 2000; // Time between requests (30 seconds)
unsigned long requestDueTime;              // time when request due

bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    Serial.println("\n=====================");
    Serial.printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

    matrix.drawRGBBitmap(x, y, bitmap, w, h);

    // // Stop further decoding as image is running off bottom of screen
    // if (y >= tft.height())
    //     return 0;

    // tft.pushImage(x, y, w, h, bitmap);

    // // Return 1 to decode next block
    return 1;
}

void setup()
{
    Serial.begin(115200);

    // Initialise SPIFFS, if this fails try .begin(true)
    // NOTE: I believe this formats it though it will erase everything on
    // spiffs already! In this example that is not a problem.
    // I have found once I used the true flag once, I could use it
    // without the true flag after that.

    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS initialisation failed!");
        while (1)
            yield(); // Stay here twiddling thumbs waiting
    }
    Serial.println("\r\nInitialisation done.");

    // Start the tft display and set it to black
    // tft.init();
    // tft.fillScreen(TFT_BLACK);
    matrix.begin();
    matrix.setBrightness(32);
    matrix.fillScreen(0);
    matrix.show();

    // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
    TJpgDec.setJpgScale(4);

    // The decoder must be given the exact name of the rendering function above
    TJpgDec.setCallback(displayOutput);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WLAN_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    client.setCACert(spotify_server_cert);

    // If you want to enable some extra debugging
    // uncomment the "#define SPOTIFY_DEBUG" in SpotifyArduino.h

    Serial.println("Refreshing Access Tokens");
    if (!spotify.refreshAccessToken())
    {
        Serial.println("Failed to get access tokens");
    }
}

int displayImageUsingFile(char *albumArtUrl)
{

    // In this example I reuse the same filename
    // over and over, maybe saving the art using
    // the album URI as the name would be better
    // as you could save having to download them each
    // time, but this seems to work fine.
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
        Serial.print("Got Image");
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
    // const char* artist =

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
    songName = const_cast<char *>(currentlyPlaying.trackName);
    Serial.print("Track URI: ");
    Serial.println(currentlyPlaying.trackUri);
    Serial.println();

    Serial.println("Artists: ");
    for (int i = 0; i < currentlyPlaying.numArtists; i++)
    {
        Serial.print("Name: ");
        // Save the song artist name to a variable
        Serial.println(currentlyPlaying.artists[i].artistName);
        songArtist = const_cast<char *>(currentlyPlaying.artists[0].artistName);
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
        smallestImage = currentlyPlaying.albumImages[2];
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

void loop()
{
    if (millis() > requestDueTime)
    {
        Serial.print("Free Heap: ");
        Serial.println(ESP.getFreeHeap());

        Serial.println("getting currently playing song:");
        // Check if music is playing currently on the account.
        int status = spotify.getCurrentlyPlaying(printCurrentlyPlayingToSerial, SPOTIFY_MARKET);
        if (status == 200)
        {
            Serial.println("Successfully got currently playing");
            String newAlbum = String(smallestImage.url);
            if (newAlbum != lastAlbumArtUrl)
            {
                Serial.println("Updating Art");
                char *my_url = const_cast<char *>(smallestImage.url);
                int displayImageResult = displayImage(my_url);

                if (displayImageResult == 0)
                {
                    lastAlbumArtUrl = newAlbum;
                }
                else
                {
                    Serial.print("failed to display image: ");
                    Serial.println(displayImageResult);
                }
            }
            else if (status == 204)
            {
                Serial.println("Doesn't seem to be anything playing");
            }
            else
            {
                Serial.print("Error: ");
                Serial.println(status);
            }

            requestDueTime = millis() + delayBetweenRequests;
        }
    }
}