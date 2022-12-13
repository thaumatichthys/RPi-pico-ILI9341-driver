#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "math.h"


// default values
#define MISO    16
#define CS      15
#define SCLK    18
#define MOSI    19
#define TFT_RST 20
#define TFT_DC  21
#define SPI_PORT spi0
#define SPI_BITRATE 40000000 // adjust to your needs

// these were obtained from here https://github.com/adafruit/Adafruit_ILI9341
const uint8_t startup_cmds[] = {
    0xEF, 3, 0x03, 0x80, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xF7, 1, 0x20,
    0xEA, 2, 0x00, 0x00,
    0xC0, 1, 0x23, // "power control 1"
    0xC1, 1, 0x10, // "power control 2"
    0xC5, 2, 0x3E, 0x28, // "vcm control"
    0xC7, 1, 0x86, // "vcm control 2"
    0x36, 1, 0x48, // "memory access control"
    0x37, 1, 0x00, // "vertical scroll zero"
    0x3A, 1, 0x55, // "pixel format set"
    0xB1, 2, 0x00, 0x18, // "frame rate control"
    0xB6, 3, 0x08, 0x82, 0x27, // "display function control"
    0xF2, 1, 0x00, // "3gamma function disable"
    0x26, 1, 0x01, // "gamma curve selected"
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x11, 0x80, // "exit sleep" (the 0x80 is not supposed to actually be sent)
    0x29, 0x80, // "display on"
    0x00
};

class ILI9341 {
    const uint16_t dx = 320;
    const uint16_t dy = 240;

    // this needs to be here since local values get deleted after the function returns, so the DMA can't read those
    static uint16_t pixel_colour; 

    static uint8_t miso;
    static uint8_t cs;
    static uint8_t sclk;
    static uint8_t mosi;
    static uint8_t tft_rst;
    static uint8_t tft_dc;
    static spi_inst_t* spi_port;
    static void dma_finished_handler();
    static uint dma_chan;
    static dma_channel_config dma_cfg;
    void Write16(uint16_t value);
    void DMAWrite16(uint16_t* data, uint32_t n, bool increment);
    void InitDMA();
    public:
    void Init();
    void FillArea(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint8_t r, uint8_t g, uint8_t b);
    void WriteImage(uint16_t* img);
};