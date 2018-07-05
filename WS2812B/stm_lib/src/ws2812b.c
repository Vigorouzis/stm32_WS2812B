#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"


#define WS2812_DEADPERIOD 19
uint16_t WS2812_IO_High = 0xFFFF;
uint16_t WS2812_IO_Low = 0x0000;
volatile uint8_t WS2812_TC = 1;
volatile uint8_t TIM4_overflows = 0;
uint16_t WS2812_IO_framedata[6144];  //WS2812 framebuffer    buffersize = (#LEDs / 16) * 24

uint8_t allow_update=0;

void GPIOB_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_NoJTRST, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void TIM4_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    uint16_t PrescalerValue;

    // TIM4 Periph clock enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    PrescalerValue = (uint16_t) (SystemCoreClock / 24000000) - 1;
    /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = 29; // 800kHz
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(TIM4, DISABLE);

    /* Timing Mode configuration: Channel 1 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 8;
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Disable);

    /* Timing Mode configuration: Channel 2 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 17;
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Disable);

    /* configure TIM4 interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}



/* Transmit the framebuffer with buffersize number of bytes to the LEDs
 * buffersize = (#LEDs / 16) * 24 */
void WS2812_sendbuf(uint32_t buffersize)
{
    // transmission complete flag, indicate that transmission is taking place
    WS2812_TC = 0;

    // clear all relevant DMA flags
    DMA_ClearFlag(DMA1_FLAG_TC7 | DMA1_FLAG_HT7 | DMA1_FLAG_GL7 | DMA1_FLAG_TE7);
    DMA_ClearFlag(DMA1_FLAG_TC1 | DMA1_FLAG_HT1 | DMA1_FLAG_GL1 | DMA1_FLAG_TE1);
    DMA_ClearFlag(DMA1_FLAG_HT4 | DMA1_FLAG_GL4 | DMA1_FLAG_TE4);

    // configure the number of bytes to be transferred by the DMA controller
    DMA_SetCurrDataCounter(DMA1_Channel7, buffersize);                              //dma channel 2 --> 7
    DMA_SetCurrDataCounter(DMA1_Channel1, buffersize);                              //dma channel 5 --> 1
    DMA_SetCurrDataCounter(DMA1_Channel4, buffersize);                              //dma channel 7 --> 4

    // clear all TIM4 flags
    TIM4->SR = 0;

    // enable the corresponding DMA channels
    DMA_Cmd(DMA1_Channel7, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);
    DMA_Cmd(DMA1_Channel4, ENABLE);

    // IMPORTANT: enable the TIM4 DMA requests AFTER enabling the DMA channels!
    TIM_DMACmd(TIM4, TIM_DMA_CC1, ENABLE);
    TIM_DMACmd(TIM4, TIM_DMA_CC2, ENABLE);
    TIM_DMACmd(TIM4, TIM_DMA_Update, ENABLE);

    // preload counter with 29 so TIM4 generates UEV directly to start DMA transfer
    TIM_SetCounter(TIM4, 29);

    // start TIM4
    TIM_Cmd(TIM4, ENABLE);
}

/* DMA1 Channel7 Interrupt Handler gets executed once the complete framebuffer has been transmitted to the LEDs */
void DMA1_Channel4_IRQHandler(void)
{
    // clear DMA7 transfer complete interrupt flag
    DMA_ClearITPendingBit(DMA1_IT_TC4);
    // enable TIM4 Update interrupt to append 50us dead period
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    // disable the DMA channels
    DMA_Cmd(DMA1_Channel7, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    // IMPORTANT: disable the DMA requests, too!
    TIM_DMACmd(TIM4, TIM_DMA_CC1, DISABLE);
    TIM_DMACmd(TIM4, TIM_DMA_CC2, DISABLE);
    TIM_DMACmd(TIM4, TIM_DMA_Update, DISABLE);
}

/* TIM4 Interrupt Handler gets executed on every TIM4 Update if enabled */
void TIM4_IRQHandler(void)
{
    // Clear TIM4 Interrupt Flag
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

    /* check if certain number of overflows has occured yet
     * this ISR is used to guarantee a 50us dead time on the data lines
     * before another frame is transmitted */
    if (TIM4_overflows < (uint8_t)WS2812_DEADPERIOD)
    {
        // count the number of occured overflows
        TIM4_overflows++;
    }
    else
    {
        // clear the number of overflows
        TIM4_overflows = 0;
        // stop TIM4 now because dead period has been reached
        TIM_Cmd(TIM4, DISABLE);
        /* disable the TIM4 Update interrupt again
         * so it doesn't occur while transmitting data */
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        // finally indicate that the data frame has been transmitted
        WS2812_TC = 1;
    }
}

/* This function sets the color of a single pixel in the framebuffer
 *
 * Arguments:
 * row = the channel number/LED strip the pixel is in from 0 to 15
 * column = the column/LED position in the LED string from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixel should display
 */
void WS2812_framedata_setPixel(uint8_t row, uint16_t column, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        // clear the data for pixel
        WS2812_IO_framedata[((column*24)+i)] &= ~(0x01<<row);
        WS2812_IO_framedata[((column*24)+8+i)] &= ~(0x01<<row);
        WS2812_IO_framedata[((column*24)+16+i)] &= ~(0x01<<row);
        // write new data for pixel
        WS2812_IO_framedata[((column*24)+i)]    |= ((((green<<i) & 0x80)>>7)<<row);
        WS2812_IO_framedata[((column*24)+8+i)]  |= ((((red<<i)   & 0x80)>>7)<<row);
        WS2812_IO_framedata[((column*24)+16+i)] |= ((((blue<<i)  & 0x80)>>7)<<row);
    }
}

/* This function is a wrapper function to set all LEDs in the complete row to the specified color
 *
 * Arguments:
 * row = the channel number/LED strip to set the color of from 0 to 15
 * columns = the number of LEDs in the strip to set to the color from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixels should display
 */
void WS2812_framedata_setRow(uint8_t row, uint16_t columns, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t i;
    for (i = 0; i < columns; i++)
    {
        WS2812_framedata_setPixel(row, i, red, green, blue);
    }
}

void WS2812_framedata_setChannel(uint8_t Channel, uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t i;
    for (i = 0; i < 256; i++)
    {
        WS2812_framedata_setPixel(Channel, i, red, green, blue);
    }
}

/* This function is a wrapper function to set all the LEDs in the column to the specified color
 *
 * Arguments:
 * rows = the number of channels/LED strips to set the row in from 0 to 15
 * column = the column/LED position in the LED string from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixels should display
 */
void WS2812_framedata_setColumn(uint8_t rows, uint16_t column, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t i;
    for (i = 0; i < rows; i++)
    {
        WS2812_framedata_setPixel(i, column, red, green, blue);
    }
}


void DMA_init(void)   //port b init
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // TIM4 Update event
    /* DMA1 Channel2 configuration ----------------------------------------------*/
    DMA_DeInit(DMA1_Channel7);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&GPIOB->ODR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&WS2812_IO_High;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel7, &DMA_InitStructure);

    // TIM4 CC1 event
    /* DMA1 Channel1 configuration ----------------------------------------------*/
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&GPIOB->ODR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&WS2812_IO_framedata;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    // TIM4 CC2 event
    /* DMA1 Channel7 configuration ----------------------------------------------*/
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&GPIOB->ODR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&WS2812_IO_Low;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    /* configure DMA1 Channel7 interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    /* enable DMA1 Channel7 transfer complete interrupt */
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);
}

uint8_t Get_WS2812_TC()
{
  return  WS2812_TC;
}


void WS2812B_Init()
  {
      GPIOB_init();
      DMA_init();
      TIM4_init();
  }
