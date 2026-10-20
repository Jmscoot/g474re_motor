#include "stm32g4xx.h"
#include "hall.h"

/*
 * hall a: PB1
 * hall b: PB2
 * hall c: PB3
 */

void hall_init(void){
	RCC->AHB2ENR|=(1<<1);	//PORT B EN
	GPIOB->MODER&=~((3<<2)|(3<<4)|(3<<6));	//gpio input

}
