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
bool ILI9341::dma_write_complete = true;

void ILI9341::dma_finished_handler() {
    // static function
    dma_channel_acknowledge_irq0(ILI9341::dma_chan);
    volatile uint8_t dummy = 0;
    for (uint32_t i = 0; i < (2000 * 1000000) / ILI9341::tft_baudrate; i++) { dummy = 123; } // delay a little bit since the IRQ triggers a bit early
    gpio_put(ILI9341::tft_cs, 1);
    spi_set_format(ILI9341::spi_port, 8, (spi_cpol_t) 0, (spi_cpha_t) 0, SPI_MSB_FIRST); // set the SPI back to 8 bit mode
    ILI9341::dma_write_complete = true;
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

void ILI9341::DMAWrite16(const uint16_t* data, uint32_t n, bool increment) {
    while(!ILI9341::dma_write_complete);
    ILI9341::dma_write_complete = false;
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

uint16_t ILI9341::RGBto16bit(uint8_t r, uint8_t g, uint8_t b) {
    r >>= 3;
    g >>= 2;
    b >>= 3;
    return (((uint16_t) b & 31) << 11) | (((uint16_t) g & 63) << 5) | ((uint16_t) r & 31);
}

void ILI9341::FillArea(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye, uint16_t colour) {
    while(!ILI9341::dma_write_complete);
    gpio_put(ILI9341::tft_cs, 0);
    const uint32_t area = (abs(xs - xe) + 1) * (abs(ys - ye) + 1);
    const uint8_t column_address_set = 0x2A;
    const uint8_t page_address_set = 0x2B;
    const uint8_t memory_write = 0x2C;
    ILI9341::pixel_colour = colour;

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

void ILI9341::WriteImage(const uint16_t* img, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    while(!ILI9341::dma_write_complete);
    gpio_put(ILI9341::tft_cs, 0);

    const uint8_t column_address_set = 0x2A;
    const uint8_t page_address_set = 0x2B;
    const uint8_t memory_write = 0x2C;

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &column_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(y);
    this->Write16(y + h - 1);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &page_address_set, 1);
    gpio_put(ILI9341::tft_dc, 1);
    this->Write16(x);
    this->Write16(x + w);

    gpio_put(ILI9341::tft_dc, 0);
    spi_write_blocking(ILI9341::spi_port, &memory_write, 1);
    gpio_put(ILI9341::tft_dc, 1);

    this->DMAWrite16(img, w * h, true);
}

void ILI9341::WriteImage(const uint16_t* img) {
    this->WriteImage(img, 0, 0, this->dx, this->dy);
}

bool ILI9341::ReadTouch(uint16_t* x, uint16_t* y) {
    while(!ILI9341::dma_write_complete); // make sure the display isnt being written to
    spi_set_baudrate(ILI9341::spi_port, ILI9341::xpt_baudrate); // the touch controller is quite slow
    gpio_put(ILI9341::xpt_cs, 0);
    //                             1XXX0001  ->  Start | 12 bit | differential | power down between conversions, IRQ disabled
    const uint8_t cmd_template = 0b10000001;
    const uint8_t n_repeats = 6;
    const float error_max_percent = 0.1;
    uint16_t z1 = 0;
    uint16_t z2 = 0;
    uint8_t rbuf[2];
    const uint8_t z1_control_byte = cmd_template | (3U << 4);
    spi_write_blocking(ILI9341::spi_port, &z1_control_byte, 1); // writes the control byte
    spi_read_blocking(ILI9341::spi_port, 0, rbuf, 2); // read the data
    z1 = (((uint16_t) rbuf[0] << 8) | rbuf[1]) >> 3;
    const uint8_t z2_control_byte = cmd_template | (4U << 4);
    spi_write_blocking(ILI9341::spi_port, &z2_control_byte, 1);
    spi_read_blocking(ILI9341::spi_port, 0, rbuf, 2);
    z2 = (((uint16_t) rbuf[0] << 8) | rbuf[1]) >> 3;
    int32_t z = z1 + 4095 - z2;
    if (z < 400)
        return false;
    uint32_t x_val = 0;
    uint32_t y_val = 0;
    int16_t x_vals_buf[n_repeats];
    int16_t y_vals_buf[n_repeats];
    const uint8_t x_control_byte = cmd_template | (1U << 4);
    const uint8_t y_control_byte = cmd_template | (5U << 4);
    for (uint8_t i = 0; i < n_repeats; i++) {
        spi_write_blocking(ILI9341::spi_port, &x_control_byte, 1);
        spi_read_blocking(ILI9341::spi_port, 0, rbuf, 2);
        x_vals_buf[i] = (((uint16_t) rbuf[0] << 8) | rbuf[1]) >> 3;
        x_val += x_vals_buf[i];
        spi_write_blocking(ILI9341::spi_port, &y_control_byte, 1);
        spi_read_blocking(ILI9341::spi_port, 0, rbuf, 2);
        y_vals_buf[i] = (((uint16_t) rbuf[0] << 8) | rbuf[1]) >> 3;
        y_val += y_vals_buf[i];
    }
    x_val = (x_val / n_repeats);
    y_val = (y_val / n_repeats);
    uint8_t count_x = n_repeats;
    uint8_t count_y = n_repeats;
    // if one value is very different from the average, remove it
    for (uint8_t i = 0; i < n_repeats; i++) {  
        if (abs(((float) x_vals_buf[i] - x_val) / x_val) > error_max_percent) {
            x_vals_buf[i] = -1;
            count_x--;
        }
        if (abs(((float) y_vals_buf[i] - y_val) / y_val) > error_max_percent) {
            y_vals_buf[i] = -1;
            count_y--;
        }
    }
    x_val = 0;
    y_val = 0;
    for (uint8_t i = 0; i < n_repeats; i++) {
        if (x_vals_buf[i] > 0)
            x_val += x_vals_buf[i];
        if (y_vals_buf[i] > 0) 
            y_val += y_vals_buf[i];
    }
    x_val /= count_x;
    y_val /= count_y;
    gpio_put(ILI9341::xpt_cs, 1);
    spi_set_baudrate(ILI9341::spi_port, ILI9341::tft_baudrate);
    *x = this->dx - x_val * this->dx / 4095;
    *y = this->dy - y_val * this->dy / 4095;
    return true;
}

void ILI9341::CorrectValues(uint16_t* x, uint16_t* y, const float coefficients[3][3]) {
    // matrix multiply
    /*  coefficients
            a b c       x   output x
            d e f   *   y = output y
            g h i       1
    */
    uint16_t x_val = *x;
    uint16_t y_val = *y;
    uint16_t output_x = coefficients[0][0] * x_val + coefficients[0][1] * y_val + coefficients[0][2];
    uint16_t output_y = coefficients[1][0] * x_val + coefficients[1][1] * y_val + coefficients[1][2];
    *x = output_x;
    *y = output_y;
}

/*

void ILI9341::CalibrateTouchBlocking() {
    bool done = false;
    this->FillArea(0, this->dx, 0, this->dy, 0xFFFF);
    this->WriteImage(calibration_text, 60, 96, 200, 50);
    const uint16_t x_coords[] = { 20, (uint16_t) (this->dx - (uint16_t) 20), 20, (uint16_t) (this->dx - (uint16_t) 20) };
    const uint16_t y_coords[] = { (uint16_t) (this->dy - (uint16_t) 20), (uint16_t) (this->dy - (uint16_t) 20), 20, 20 };
    uint16_t touch_x[4];
    uint16_t touch_y[4];
    uint32_t previous_time = to_ms_since_boot(get_absolute_time());
    uint32_t current_time = 0;// = to_ms_since_boot(get_absolute_time());
    for (uint8_t i = 0; i < 4; i++) {
        this->WriteImage(calibration_point, x_coords[i] - 12, y_coords[i] - 12, 25, 25);
        while ((!this->ReadTouch(&touch_x[i], &touch_y[i])) || current_time - previous_time < 1000) {
            current_time = to_ms_since_boot(get_absolute_time());
        }
        this->FillArea(x_coords[i] - 12, x_coords[i] + 12, y_coords[i] - 12, y_coords[i] + 12, 0xFFFF);
        previous_time = current_time;
    }
    //  I didn't want to write a program to find the matrix coefficients, so just use python to do it.
    printf("import numpy as np\nimport cv2\nsrc = np.array([[%d, %d], [%d, %d], [%d, %d], [%d, %d]], dtype=np.float32)\ndest = ",
        touch_x[0], touch_y[0],
        touch_x[1], touch_y[1],
        touch_x[2], touch_y[2],
        touch_x[3], touch_y[3]
    );
    printf("np.array([[%d, %d], [%d, %d], [%d, %d], [%d, %d]], dtype=np.float32)\nmatrix = cv2.getPerspectiveTransform(src, dest)\n",
        x_coords[0], y_coords[0],
        x_coords[1], y_coords[1],
        x_coords[2], y_coords[2],
        x_coords[3], y_coords[3]
    );
    printf("print(\"static float calibration_matrix[3][3] = {\\n    {\" + str(matrix[0, 0]) + \", \" + str(matrix[0, 1]) + \", \" +\n");
    printf("    str(matrix[0, 2]) + \"}, \\n    {\" + str(matrix[1, 0]) + \", \" + str(matrix[1, 1]) + \", \" + str(matrix[1, 2]) +\n");
    printf("    \"}, \\n    {\" + str(matrix[2, 0]) + \", \" + str(matrix[2, 1]) + \", \" + str(matrix[2, 2]) + \"}\\n};\")\n");
}

*/
