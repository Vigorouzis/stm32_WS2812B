#include <stm32f10x_conf.h>
#include <WS2812B.h>


uint16_t led=0;
uint16_t ch=0;


int main() {

	WS2812B_Init();
	  while(1){
		  WS2812_framedata_setChannel(1,20,1,2);
	      WS2812_sendbuf(24);
	  }
	}
