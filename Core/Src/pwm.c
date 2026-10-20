/*
 * pwm.c
 *
 *  Created on: Sep 13, 2026
 *      Author: 1002s
 */
#include "stm32g4xx.h"
#include "pwm.h"
/*
 * Center Align PWM (STM32G474RE)
 * TIM1 ch1,2,3 3개 사용, 상보 출력 + 데드타임
 *
 *PHASE1
 * PA7  : TIM1_CH1N / AF6
 * PA8  : TIM1_CH1  / AF6
 *
 *PHASE2
 * PB0  : TIM1_CH2N / AF6
 * PA9  : TIM1_CH2  / AF6
 *
 *PHASE3
 * PB1  : TIM1_CH3N / AF6
 * PA10 : TIM1_CH3  / AF6
 *
 * TIM1CLK : APB2 = 170MHz (APB2 분주 1 -> TIMxCLK = PCLK2)
 * f_pwm   : 20kHz
 * center-aligned에서 1주기 = 2*ARR 카운트 -> ARR = 170MHz / (2*20kHz) = 4250
 */
#define TIM1_CLK_HZ		170000000UL
#define PWM_FREQ_HZ		20000UL
#define PWM_ARR			(TIM1_CLK_HZ / (2 * PWM_FREQ_HZ))	// 4250

/*
 * 데드타임 (BDTR.DTG, tDTS = 1/170MHz = 5.88ns, CKD=00)
 * DTG[7:5]=10x -> DT = (64 + DTG[5:0]) * 2 * tDTS
 * (64 + 21) * 2 * 5.88ns = 1.0us
 * 게이트 드라이버/스위치 사양에 맞게 조정할 것
 */
#define PWM_DTG			(0x80 | 21)

#define AF_TIM1			6U

static void gpio_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af) {
	port->MODER = (port->MODER & ~(3U << (pin * 2))) | (2U << (pin * 2));
	port->OSPEEDR |= (3U << (pin * 2));
	port->AFR[pin >> 3] = (port->AFR[pin >> 3] & ~(0xFU << ((pin & 7) * 4)))
			| (af << ((pin & 7) * 4));
}

void init_pwm(void) {
	// timer1, GPIOA,B 클럭 활성화 (G4는 GPIO가 AHB2)
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
	(void)RCC->AHB2ENR;

	// 다른 핀 설정은 건드리지 않도록 핀 단위로 설정
	gpio_af(GPIOA, 7, AF_TIM1);
	gpio_af(GPIOA, 8, AF_TIM1);
	gpio_af(GPIOA, 9, AF_TIM1);
	gpio_af(GPIOA, 10, AF_TIM1);
	gpio_af(GPIOB, 0, AF_TIM1);
	gpio_af(GPIOB, 1, AF_TIM1);

	// TIM param configuration
	TIM1->CR1 = 0;
	TIM1->PSC = 0;
	TIM1->ARR = PWM_ARR;

	// ch1,2,3 PWM mode 1 (OCxM=0110) + preload
	TIM1->CCMR1 = (6 << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE
			| (6 << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
	TIM1->CCMR2 = (6 << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;

	// 초기 듀티 50% (3상 모두 같으면 상간 전압 0)
	TIM1->CCR1 = PWM_ARR / 2;
	TIM1->CCR2 = PWM_ARR / 2;
	TIM1->CCR3 = PWM_ARR / 2;

	TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC1NE
			| TIM_CCER_CC2E | TIM_CCER_CC2NE
			| TIM_CCER_CC3E | TIM_CCER_CC3NE;

	TIM1->BDTR = PWM_DTG;

	// update interrupt (NVIC TIM1_UP_TIM16_IRQn은 사용할 때 별도로 enable)
	TIM1->DIER = TIM_DIER_UIE;

	// preload 값 반영 후 플래그 정리
	TIM1->EGR = TIM_EGR_UG;
	TIM1->SR = 0;

	// center-aligned mode 3 (CMS=11), ARR preload, URS, 카운터 시작
	TIM1->CR1 = (3 << TIM_CR1_CMS_Pos) | TIM_CR1_ARPE | TIM_CR1_URS | TIM_CR1_CEN;

	// 출력 enable
	TIM1->BDTR |= TIM_BDTR_MOE;
}
