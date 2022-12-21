#include "ILI9341Driver.hpp"


// I should probably add a setter for all these values
uint16_t ILI9341::pixel_colour;
uint32_t ILI9341::tft_baudrate = TFT_BITRATE;
uint32_t ILI9341::xpt_baudrate = XPT_BITRATE;
uint8_t ILI9341::miso = MISO;
uint8_t ILI9341::tft_cs = TFT_CS;
uint8_t ILI9341::xpt_cs = XPT_CS;
uint8_t ILI9341::sclk = SCLK;
uint8_t ILI9341::mosi = MOSI;
uint8_t ILI9341::tft_rst = TFT_RST;
uint8_t ILI9341::tft_dc = TFT_DC;
spi_inst_t* ILI9341::spi_port = SPI_PORT;
uint ILI9341::dma_chan;
dma_channel_config ILI9341::dma_cfg;

void ILI9341::dma_finished_handler() {
    // static function
    dma_channel_acknowledge_irq0(ILI9341::dma_chan);
    volatile uint8_t dummy = 0;
    for (uint32_t i = 0; i < (2000 * 1000000) / ILI9341::tft_baudrate; i++) { dummy = 123; } // delay a little bit since the IRQ triggers a bit early
    gpio_put(ILI9341::tft_cs, 1);
    spi_set_format(ILI9341::spi_port, 8, (spi_cpol_t) 0, (spi_cpha_t) 0, SPI_MSB_FIRST); // set the SPI back to 8 bit mode
}

void ILI9341::Write16(uint16_t value) {
    uint8_t arr[2] = {(uint8_t) (value >> 8), (uint8_t) value};
    spi_write_blocking(ILI9341::spi_port, arr, 2);
}

void ILI9341::InitDMA() {
    ILI9341::dma_chan = dma_claim_unused_channel(true);
    ILI9341::dma_cfg = dma_channel_get_default_config(ILI9341::dma_chan);
    channel_config_set_transfer_data_size(&ILI9341::dma_cfg, DMA_SIZE_16);
    channel_config_set_write_increment(&ILI9341::dma_cfg, false);
    channel_config_set_dreq(&ILI9341::dma_cfg, spi_get_dreq(ILI9341::spi_port, true));

    dma_channel_set_irq0_enabled(ILI9341::dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, ILI9341::dma_finished_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void ILI9341::DMAWrite16(uint16_t* data, uint32_t n, bool increment) {
    dma_channel_wait_for_finish_blocking(ILI9341::dma_chan);
    spi_set_format(ILI9341::spi_port, 16, (spi_cpol_t) 0, (spi_cpha_t) 0, SPI_MSB_FIRST); // set the hardware SPI unit to 16 bit mode
    channel_config_set_read_increment(&ILI9341::dma_cfg, increment);
    dma_channel_configure (
        ILI9341::dma_chan,
        &ILI9341::dma_cfg,
        &spi_get_hw(ILI9341::spi_port)->dr,
        data,
        n,
        true
    );
}

void ILI9341::Init() {
    spi_init(this->spi_port, ILI9341::tft_baudrate);
    gpio_set_function(ILI9341::miso, GPIO_FUNC_SPI);
    gpio_set_function(ILI9341::sclk, GPIO_FUNC_SPI);
    gpio_set_function(ILI9341::mosi, GPIO_FUNC_SPI);
    this->InitDMA();
    gpio_init(ILI9341::tft_cs);
    gpio_set_dir(ILI9341::tft_cs, GPIO_OUT);
    gpio_put(ILI9341::tft_cs, 1); // CS is active low
    gpio_init(ILI9341::xpt_cs);
    gpio_set_dir(ILI9341::xpt_cs, GPIO_OUT);
    gpio_put(ILI9341::xpt_cs, 1);
    gpio_init(ILI9341::tft_rst);
    gpio_init(ILI9341::tft_dc);
    gpio_set_dir(ILI9341::tft_rst, GPIO_OUT);
    gpio_set_dir(ILI9341::tft_dc, GPIO_OUT);
    gpio_put(ILI9341::tft_rst, 1);
    int iterations = 2;
    size_t startup_cmds_size = sizeof(startup_cmds);
    sleep_ms(100);
    gpio_put(ILI9341::tft_cs, 0);
    for (int i = 0; i < iterations; i++) {
        if (i + 1 == iterations) {
            if (startup_cmds_size < i) {
                return;
            }
            uint8_t n = startup_cmds[i];

            uint8_t command = startup_cmds[i - 1];
            gpio_put(ILI9341::tft_dc, 0);
            spi_write_blocking(ILI9341::spi_port, &command, 1);
            gpio_put(ILI9341::tft_dc, 1);
            if (n == 0x80) {
                sleep_ms(150);
                iterations += 2;
            }
            else {
                for (int o = 0; o < n; o++) {
                    spi_write_blocking(ILI9341::spi_port, &startup_cmds[o + i + 1], 1);
                }
                iterations += n + 2;
            }
        }
    }
    gpio_put(ILI9341::tft_cs, 1);
}

void ILI9341::FillArea(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(ILI9341::tft_cs, 0);
    const uint32_t area = (abs(xs - xe) + 1) * (abs(ys - ye) + 1);
    const uint8_t column_address_set = 0x2A;
    const uint8_t page_address_set = 0x2B;
    const uint8_t memory_write = 0x2C;
    r >>= 3;
    g >>= 2;
    b >>= 3;
    ILI9341::pixel_colour = (((uint16_t) b & 31) << 11) | (((uint16_t) g & 63) << 5) | ((uint16_t) r & 31);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &page_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(xs);
    this->Write16(xe);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &column_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(ys);
    this->Write16(ye);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &memory_write, 1);
    gpio_put(ILI9341::tft_dc, 1);

    this->DMAWrite16(&ILI9341::pixel_colour, area, false);
}

void ILI9341::WriteImage(uint16_t* img) {
    gpio_put(ILI9341::tft_cs, 0);

    const uint8_t column_address_set = 0x2A;
    const uint8_t page_address_set = 0x2B;
    const uint8_t memory_write = 0x2C;

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &column_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(0);
    this->Write16(this->dy);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &page_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(0);
    this->Write16(this->dx);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &memory_write, 1);
    gpio_put(ILI9341::tft_dc, 1);

    this->DMAWrite16(img, this->dx * this->dy, true);
}

bool ILI9341::ReadTouch(uint8_t* x, uint8_t* y) { // this isnt finished
    spi_set_baudrate(ILI9341::spi_port, ILI9341::xpt_baudrate);
    gpio_put(ILI9341::xpt_cs, 0);
    //                             1XXX0001  ->  Start | 12 bit | differential | power down between conversions, IRQ disabled
    const uint8_t cmd_template = 0b10000001;
    uint16_t x_val = 0;
    uint16_t y_val = 0;
    
    uint8_t x_control_byte = cmd_template | (1U << 4);
    spi_write_blocking(ILI9341::spi_port, &x_control_byte, 1); // writes the control byte
    spi_read16_blocking(ILI9341::spi_port, 0, &x_val, 1);

    uint8_t y_control_byte = cmd_template | (5U << 4);
    spi_write_blocking(ILI9341::spi_port, &y_control_byte, 1); // writes the control byte
    spi_read16_blocking(ILI9341::spi_port, 0, &y_val, 1);

    // UNFINISHED

    printf("Data: X: %d, y: %d\n", x_val, y_val);

    gpio_put(ILI9341::xpt_cs, 1);
    spi_set_baudrate(ILI9341::spi_port, ILI9341::tft_baudrate);
    return false;
}
