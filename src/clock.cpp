#include <fonts.h>
#include <globals.h>

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

void initClock()
{
    Serial.println("starting clock");
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("failed to obtain time");
        return;
    }
    setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();
}

void drawDigit(int digit, int x, int y, uint16_t color)
{
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (digits[digit][i] & (1 << (2 - j)))
            {
                matrix.drawPixel(x + j, y + i, color);
            }
        }
    }
}

void drawNumber(int number, int x, int y, uint16_t color)
{
    int tens = number / 10;
    int ones = number % 10;
    drawDigit(tens, x, y, color);
    drawDigit(ones, x + 5, y, color);
}

void drawTime(int hours, int minutes, uint16_t color)
{
    drawNumber(hours, 4, 2, color);
    drawNumber(minutes, 4, 9, color);
}

void drawClock()
{
    matrix.clear();
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    uint8_t hours = timeinfo.tm_hour;
    uint8_t minutes = timeinfo.tm_min;
    drawTime(hours, minutes, matrix.Color(255, 255, 255));
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
    matrix.show();
}