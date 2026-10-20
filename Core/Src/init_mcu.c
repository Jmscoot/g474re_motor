// clk 활성화 및 configuration (STM32G474RE, RM0440 기준)

#include "stm32g4xx.h"
#include "init_mcu.h"

/*
 * HSI16 : 16MHz (내부 RC)
 * PLL   : HSI16 / M(4) = 4MHz(VCO in) * N(85) = 340MHz(VCO out) / R(2) = 170MHz
 * SYSCLK = HCLK = PCLK1 = PCLK2 = 170MHz
 * TIMxCLK = 170MHz (APB 분주비가 1이면 TIMxCLK = PCLK)
 *
 * 1로 바꾸면 PA8로 MCO(PLL/16 = 10.625MHz) 출력.
 * 단, PA8은 pwm.c에서 TIM1_CH1로 쓰므로 확인할 때만 켤 것.
 */
#define USE_MCO_PA8	1

static void delay_cycles(volatile uint32_t n) {
	while (n--)
		;
}

void init_mcu(void) {
	/* 1. ====================== 전압 레귤레이터: Range 1 boost ======================
	 * G4는 F4의 overdrive가 없고, 대신 Range 1 boost 모드(R1MODE=0)를 쓴다.
	 * Range 1 normal : 최대 150MHz / Range 1 boost : 최대 170MHz
	 */
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	(void)RCC->APB1ENR1; // 클럭 인가 후 레지스터 접근 전 지연

	// VOS = Range 1 (리셋 기본값이지만 명시)
	PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | PWR_CR1_VOS_0;
	while (PWR->SR2 & PWR_SR2_VOSF)
		;

	/* 2. ====================== HSI16 ON ====================== */
	RCC->CR |= RCC_CR_HSION;
	while ((RCC->CR & RCC_CR_HSIRDY) == 0)
		;

	/* 3. ====================== PLL 설정 ======================
	 * VCO input : 2.66 ~ 16MHz,  VCO output : 96 ~ 344MHz
	 * PLLM 필드 = M-1, PLLR 필드 00 = /2
	 */
	RCC->CR &= ~RCC_CR_PLLON;
	while (RCC->CR & RCC_CR_PLLRDY)
		;

	RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSI
			| ((4 - 1) << RCC_PLLCFGR_PLLM_Pos)
			| (85 << RCC_PLLCFGR_PLLN_Pos)
			| (0 << RCC_PLLCFGR_PLLR_Pos)
			| RCC_PLLCFGR_PLLREN; // PLLCLK(R 출력) -> SYSCLK

	RCC->CR |= RCC_CR_PLLON;
	while ((RCC->CR & RCC_CR_PLLRDY) == 0)
		;

	/* 4. ====================== boost 전환 + 클럭 전환 ======================
	 * RM0440 Range 1 boost 진입 순서:
	 *  (1) AHB 프리스케일러 /2
	 *  (2) R1MODE = 0 (boost)
	 *  (3) Flash wait state 조정 (170MHz, boost -> 4WS)
	 *  (4) SYSCLK을 PLL로 전환
	 *  (5) 1us 이상 대기 후 AHB /1
	 */
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV2;

	PWR->CR5 &= ~PWR_CR5_R1MODE;

	FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY)
			| FLASH_ACR_LATENCY_4WS
			| FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS)
		;

	// APB1, APB2 = /1 (G4는 APB도 170MHz까지 가능)
	RCC->CFGR &= ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
		;

//	delay_cycles(200); // HCLK 85MHz에서 1us 이상

	RCC->CFGR &= ~RCC_CFGR_HPRE; // AHB /1 -> HCLK 170MHz

//	// HAL_Delay/SysTick 등이 쓰는 SystemCoreClock 갱신
//	SystemCoreClockUpdate();

#if USE_MCO_PA8
	/* 5. ====================== MCO 출력 (PA8, AF0) ======================
	 * MCOSEL 0101 = PLLCLK, MCOPRE 100 = /16 -> 10.625MHz
	 */
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE8) | (2 << GPIO_MODER_MODE8_Pos);
	GPIOA->OSPEEDR |= (3 << GPIO_OSPEEDR_OSPEED8_Pos);
	GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8; // AF0 = MCO

	RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCOSEL | RCC_CFGR_MCOPRE))
			| (5 << RCC_CFGR_MCOSEL_Pos)
			| (4 << RCC_CFGR_MCOPRE_Pos);
#endif
}
