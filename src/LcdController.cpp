#include <globals.h>

int line1Offset = 0;
int line2Offset = 0;

bool line1ScrollEnabled = false;
bool line2ScrollEnabled = false;

char line1[128];
char line2[128];

char line1Output[17]; // +1 for null character
char line2Output[17];

void updateLcd()
{
    lcd.clear();

    // scroll the text
    if (line1ScrollEnabled)
    {
        // shift everything back 1
        for (int i = 0; i < 15; i++)
        {
            line1Output[i] = line1Output[i + 1];
        }

        // add next char at the end
        if (line1Offset > strlen(line1) - 1)
            line1Output[15] = ' ';
        else
            line1Output[15] = line1[line1Offset];

        // incriment offset
        if (line1Offset + 1 > strlen(line1) + 4)
            line1Offset = 0;
        else
            line1Offset++;
    }
    else
    {
        strncpy(line1Output, line1, 16);
    }
    line1Output[16] = '\0';
    // Serial.printf("%s (%d, %d)\n", line1Output, line1Offset, strlen(line1));
    lcd.setCursor(0, 0);
    lcd.print(line1Output);

    // line 2
    if (line2ScrollEnabled)
    {
        // shift everything back 1
        for (int i = 0; i < 15; i++)
        {
            line2Output[i] = line2Output[i + 1];
        }

        // add next char at the end
        if (line2Offset > strlen(line2) - 1)
            line2Output[15] = ' ';
        else
            line2Output[15] = line2[line2Offset];

        // incriment offset
        if (line2Offset + 1 > strlen(line2) + 4)
            line2Offset = 0;
        else
            line2Offset++;
    }
    else
    {
        strncpy(line2Output, line2, 16);
    }

    line2Output[16] = '\0';
    lcd.setCursor(0, 1);
    lcd.print(line2Output);
}

void updateLine1(char *text, bool scroll)
{
    if (strcmp(text, line1) == 0)
        return;

    line1Offset = 0;
    memset(line1Output, ' ', sizeof(line1Output));
    strncpy(line1, text, sizeof(line1));
    line1ScrollEnabled = scroll;
}

void updateLine2(char *text, bool scroll)
{
    if (strcmp(text, line2) == 0)
        return;

    line2Offset = 0;
    memset(line2Output, ' ', sizeof(line2Output));
    strncpy(line2, text, sizeof(line2));
    line2ScrollEnabled = scroll;
}