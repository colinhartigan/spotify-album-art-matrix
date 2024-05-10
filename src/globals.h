#include <Adafruit_NeoMatrix.h>

inline Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, 15,
                                                      NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                                                          NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                                                      NEO_GRB + NEO_KHZ800);