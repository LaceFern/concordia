#include "Bitmap.h"

#include <iostream>

int main() {
    BitMap bitmap(128);

    for (int i = 0; i < 128; ++i) {

        assert(bitmap.setZeroPos() == i);
    }
    for (int i = 0; i < 128; ++i) {
        assert(bitmap.get(i) == true);
    }

    for (int i = 0; i < 64; i += 2) {
        bitmap.clear(i);
    }

    for (int i = 0; i < 64; i += 2) {
        assert(bitmap.get(i) == (i % 2 == 1));
    }

    for (int i = 64; i < 128; ++i) {
        assert(bitmap.get(i) == true);
    }

    return 0;
}
