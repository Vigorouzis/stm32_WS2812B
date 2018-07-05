#include <stm32f10x_conf.h>
#include <WS2812B.h>


uint16_t led=0;
uint16_t ch=0;


int main() {

	WS2812B_Init();

	WS2812_framedata_setRow(0,12,10,0,10);
	      WS2812_sendbuf(312);
	      while(1);
	}
