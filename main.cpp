//#include "stdio.h" // for printf
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "ILI9341Driver.hpp"
#include "images.hpp"


ILI9341 dp;

int main() {
    stdio_init_all(); // for printf

    dp.Init();

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    sleep_ms(100);
    dp.WriteImage(image);

    while(1) {
        gpio_put(25, 1);
        sleep_ms(20);
        gpio_put(25, 0);
        sleep_ms(2);
        uint16_t x;
        uint16_t y;
        if (dp.ReadTouch(&x, &y)) {
            dp.CorrectValues(&x, &y, calibration_matrix);
            dp.FillArea(x - 2, x + 2, y - 2, y + 2, 0);
        }
    }
}