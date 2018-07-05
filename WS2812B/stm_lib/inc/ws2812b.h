    void WS2812_sendbuf(uint32_t buffersize);
void WS2812_framedata_setPixel(uint8_t row, uint16_t column, uint8_t red, uint8_t green, uint8_t blue);
void WS2812_framedata_setRow(uint8_t row, uint16_t columns, uint8_t red, uint8_t green, uint8_t blue);
void WS2812_framedata_setColumn(uint8_t rows, uint16_t column, uint8_t red, uint8_t green, uint8_t blue);
void WS2812B_Init();
uint8_t Get_WS2812_TC();
