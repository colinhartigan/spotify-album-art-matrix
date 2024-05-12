#include <Adafruit_NeoMatrix.h>
#include <LiquidCrystal_I2C.h>

inline Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, 15,
                                                      NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                                                          NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                                                      NEO_GRB + NEO_KHZ800);
inline LiquidCrystal_I2C lcd(0x27, 16, 2);

enum Mode
{
    SPOTIFY,
    CLOCK
};