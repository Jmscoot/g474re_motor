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

	// interrupt EN
	// 1. 주변장치 interrupt EN(EXTI)
	RCC->APB2ENR|=RCC_APB2ENR_SYSCFGEN;	//EXTI의 clk공급

	SYSCFG->EXTICR[0]&=~((1<<4)|(1<<8)|(1<<12));	//EXTI를 사용할 pin 매핑
	SYSCFG->EXTICR[0]|=(1<<4)|(1<<8)|(1<<12);	//EXTI를 사용할 pin 매핑


	EXTI->IMR1|=(1<<1)|(1<<2)|(1<<3);	//EXTI1,2,3의 마스크 해제
	EXTI->RTSR1|=(1<<1)|(1<<2)|(1<<3);
	EXTI->FTSR1



	// 2. NVIC interrupt setting
	// 3. CPU interrupt setting


}
