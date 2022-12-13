// #include "stdio.h" // for printf
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "ILI9341Driver.hpp"
#include "images.hpp"


ILI9341 dp;

int main() {
    // stdio_init_all(); // for printf

    dp.Init();
    dp.WriteImage((uint16_t*) image);

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    while(1) { // rekt noob timeam
        gpio_put(25, 1);
        sleep_ms(200);
        gpio_put(25, 0);
        sleep_ms(200);
    }
}