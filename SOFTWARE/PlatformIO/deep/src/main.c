/**
 * Deep badge
 *
 * @authors
 * Copyright (C) 2025 Goran Mahovlić <goran.mahovlic@intergalaktik.eu>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 * Current consumption
 * Raider: 1.5mA
 * Morse: 1.15mA
 * Sleep: 790uA
 * Deep Sleep: 78uA
 */

#include <ch32v00x.h>
#include <ch32v00x_pwr.h>
#include <ch32v00x_rcc.h>
#include <ch32v00x_exti.h>
#include <debug.h>
#include <string.h>
#include <stdbool.h>

void NMI_Handler(void)       __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

//#define POV

#define SYSCLK_FREQ_8MHz_HSI    8000000
//#define PFIC_SLEEPDEEP                          ((uint32_t)0x00000004) /* 1:deep sleep; 0:sleep */

#ifdef POV
	#define ROW_DELAY_MS 30
#endif


#define PROGRAMMING_DELAY 4000

#define RAIDER_DELAY_MS 100

#define NUMBER_OF_CASES 6

// Define timing (adjustable)
#define DOT_DURATION  200
#define DASH_DURATION 600
#define SYMBOL_SPACE  200  // Space between dot/dash
#define LETTER_SPACE  600  // Space between letters
#define WORD_SPACE    1400 // Space between words

uint8_t counter = 0;
uint32_t long_press_counter = 0;
uint8_t interrupt = 0;
uint8_t long_press = 0;
volatile bool timer_wakeup = false;

#ifdef POV
	const uint8_t font_5x5[37][5] = {
		// A-Z
		{0x0E, 0x11, 0x1F, 0x11, 0x11},  // A
		{0x1E, 0x11, 0x1E, 0x11, 0x1E},  // B
		{0x0E, 0x11, 0x10, 0x11, 0x0E},  // C
		{0x1E, 0x11, 0x11, 0x11, 0x1E},  // D
		{0x1F, 0x10, 0x1E, 0x10, 0x1F},  // E
		{0x1F, 0x10, 0x1E, 0x10, 0x10},  // F
		{0x0E, 0x10, 0x13, 0x11, 0x0E},  // G
		{0x11, 0x11, 0x1F, 0x11, 0x11},  // H
		{0x0E, 0x04, 0x04, 0x04, 0x0E},  // I
		{0x01, 0x01, 0x01, 0x11, 0x0E},  // J
		{0x11, 0x12, 0x1C, 0x12, 0x11},  // K
		{0x10, 0x10, 0x10, 0x10, 0x1F},  // L
		{0x11, 0x1B, 0x15, 0x11, 0x11},  // M
		{0x11, 0x19, 0x15, 0x13, 0x11},  // N
		{0x0E, 0x11, 0x11, 0x11, 0x0E},  // O
		{0x1E, 0x11, 0x1E, 0x10, 0x10},  // P
		{0x0E, 0x11, 0x11, 0x13, 0x0F},  // Q
		{0x1E, 0x11, 0x1E, 0x12, 0x11},  // R
		{0x0F, 0x10, 0x0E, 0x01, 0x1E},  // S
		{0x1F, 0x04, 0x04, 0x04, 0x04},  // T
		{0x11, 0x11, 0x11, 0x11, 0x0E},  // U
		{0x11, 0x11, 0x11, 0x0A, 0x04},  // V
		{0x11, 0x11, 0x15, 0x1B, 0x11},  // W
		{0x11, 0x0A, 0x04, 0x0A, 0x11},  // X
		{0x11, 0x0A, 0x04, 0x04, 0x04},  // Y
		{0x1F, 0x02, 0x04, 0x08, 0x1F},  // Z

		// 0-9
		{0x0E, 0x11, 0x11, 0x11, 0x0E},  // 0
		{0x04, 0x0C, 0x04, 0x04, 0x0E},  // 1
		{0x0E, 0x01, 0x0E, 0x10, 0x1F},  // 2
		{0x0E, 0x01, 0x06, 0x01, 0x0E},  // 3
		{0x02, 0x06, 0x0A, 0x1F, 0x02},  // 4
		{0x1F, 0x10, 0x1E, 0x01, 0x1E},  // 5
		{0x0E, 0x10, 0x1E, 0x11, 0x0E},  // 6
		{0x1F, 0x01, 0x02, 0x04, 0x08},  // 7
		{0x0E, 0x11, 0x0E, 0x11, 0x0E},  // 8
		{0x0E, 0x11, 0x0F, 0x01, 0x0E},  // 9

		// Dash (-)
		{0x00, 0x00, 0x1F, 0x00, 0x00}   // -
	};
#endif

#ifdef POV
	void displayCharacter(char c, int scrollSpeed) {
		int index;
		
		if (c >= 'A' && c <= 'Z') {
			index = c - 'A';  // Map 'A' to index 0
		} else if (c >= '0' && c <= '9') {
			index = c - '0' + 26;  // Map '0' to index 26
		} else if (c == '-') {
			index = 36;  // Index for '-'
		} else {
			return;  // Invalid character, do nothing
		}

		for (int col = 0; col < 5; col++) {
			uint8_t columnData = font_5x5[index][col];  // Get column bitmap
			for (int row = 0; row < 5; row++) {
				ledWrite(row, (columnData >> row) & 1);  // Set LEDs
			}
		Delay_Ms(ROW_DELAY_MS);  // Adjust for PoV effect
		}

			ledsOFF();
			Delay_Ms(ROW_DELAY_MS);  // Adjust for PoV effect	
	}
#endif

#ifdef POV
	void scrollText(const char *text, int scrollSpeed) {
		int len = strlen(text);

		for (int i = 0; i < len; i++) {
			displayCharacter(text[i], scrollSpeed);  // Use your existing function
			DelayIfBTN_Ms(scrollSpeed);  // Adjust for PoV effect
			if(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2)){
				break;
			}
		}
	}
#endif

// Morse Code Dictionary (A-Z, 0-9)
const char* MORSE_TABLE[36] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",  // A-J
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",   // K-T
    "..-", "...-", ".--", "-..-", "-.--", "--..",                          // U-Z
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----." // 0-9
};

// Convert character to Morse code
const char* char_to_morse(char c) {
    if (c >= 'A' && c <= 'Z') return MORSE_TABLE[c - 'A'];  // Letters
    if (c >= 'a' && c <= 'z') return MORSE_TABLE[c - 'a'];  // Lowercase
    if (c >= '0' && c <= '9') return MORSE_TABLE[c - '0' + 26]; // Numbers
    return NULL; // Unsupported character
}

volatile uint8_t sleep_mode = 0;

volatile bool timerInterrupt = false;
uint8_t state = 0;

void ledWrite(uint8_t led, uint8_t state){
	if(led>=0 && led<5){ // if LED exists
		if(led==0){GPIO_WriteBit(GPIOD, GPIO_Pin_6, state);} // 1
		else if(led==1){GPIO_WriteBit(GPIOC, GPIO_Pin_4, state);} // 4		
		else if(led==2){GPIO_WriteBit(GPIOC, GPIO_Pin_1, state);} // 2
		else if(led==3){GPIO_WriteBit(GPIOC, GPIO_Pin_2, state);} // 3
		else if(led==4){GPIO_WriteBit(GPIOD, GPIO_Pin_4, state);} // 5
	}
}

// ----------------------------------------------------------------------
// TIM2 interrupt handler (WCH RISC-V specific attribute)
// ----------------------------------------------------------------------
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        TIM_Cmd(TIM2, DISABLE);  // one-shot
        timer_wakeup = true;
    }
}

// ----------------------------------------------------------------------
// Start a one-shot timer for X ms
// ----------------------------------------------------------------------
void start_timer_ms(uint32_t interval_ms)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef t = {0};
    t.TIM_Prescaler = (SystemCoreClock / 1000) - 1; // 1 kHz = 1ms tick
    t.TIM_CounterMode = TIM_CounterMode_Up;
    t.TIM_Period = interval_ms - 1;
    t.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseInit(TIM2, &t);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_EnableIRQ(TIM2_IRQn);

    TIM_Cmd(TIM2, ENABLE);
}

//------------------------------------------------------------
// Stop timer safely (e.g., if another interrupt woke us)
//------------------------------------------------------------
void stop_timer(void)
{
    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    timer_wakeup = false;
}

// ----------------------------------------------------------------------
// Low-power delay function (sleeps until timer fires)
// ----------------------------------------------------------------------
void DelayIfBTN_Ms(uint32_t ms)
{
    timer_wakeup = false;
    start_timer_ms(ms);

    __enable_irq();   // ensure interrupts are allowed
    __WFI();          // sleep until any interrupt (timer)
    // Here, any interrupt could have woken us
    // Check if it was the timer or something else
    if (!timer_wakeup)
    {
        // another interrupt woke us, stop timer
        stop_timer();
    }
}

void ledsOFF(void){
	for (uint8_t i=0;i<5;i++){
		ledWrite(i,0);
	}
}

void ledsON(void){
	for (uint8_t i=0;i<5;i++){
		ledWrite(i,1);
	}
}

void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void){
    __disable_irq();
    if (EXTI->INTFR & (1 << 2)) // Check if PA2 caused the interrupt
    {
        Delay_Ms(10); //HAcY - usually not good idea to put delay here but until we stop timmer button is probabely released and changed stated
        if(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2)){
            interrupt=1;
            if(counter < NUMBER_OF_CASES){ 
                counter++;
            }
            else{
                counter = 0;
            }
        }
    }
	EXTI_ClearITPendingBit(EXTI_Line2);

    __enable_irq();    
}

// Function to blink Morse code
void blink_string(const char* text, uint8_t led) {
	if(led>=0 && led<5){
    while (*text) {
        if (*text == ' ') {
            DelayIfBTN_Ms(WORD_SPACE);  // Space between words
        } else {
            const char* morse = char_to_morse(*text);
            if (morse) {
                while (*morse) {
                    if (*morse == '.') {
                        ledWrite(led,1);
                        DelayIfBTN_Ms(DOT_DURATION);
                    } else if (*morse == '-') {
                        ledWrite(led,1);
                        DelayIfBTN_Ms(DASH_DURATION);
                    }
                    ledWrite(led,0);  // Turn off LED
                    DelayIfBTN_Ms(SYMBOL_SPACE);
                    morse++;
                }
                DelayIfBTN_Ms(LETTER_SPACE); // Space between letters
            }
        }
        text++; // Move to next character
    }
	}
}

void raider(void){
		ledWrite(0,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);		
		ledWrite(0,0);
		ledWrite(1,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		ledWrite(1,0);
		ledWrite(2,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);		
		ledWrite(2,0);
		ledWrite(3,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		ledWrite(3,0);
		ledWrite(4,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);		
		ledWrite(4,0);
		ledWrite(3,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		ledWrite(3,0);
		ledWrite(2,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);
		ledWrite(2,0);
		ledWrite(1,1);
		DelayIfBTN_Ms(RAIDER_DELAY_MS);		
		ledWrite(1,0);
}

void prepare_for_sleep(void)
{
    // Disable peripheral clocks except GPIOA + AFIO
    RCC_APB1PeriphClockCmd(0xFFFF, DISABLE); // all APB1 off
    RCC_APB2PeriphClockCmd(~(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO), DISABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, DISABLE);

    // Optional: set unused GPIOs to analog mode
    GPIO_InitTypeDef analog = {0};
    analog.GPIO_Pin = GPIO_Pin_All;
    analog.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &analog);
    GPIO_Init(GPIOD, &analog);
}

void enter_deep_sleep_event(void)
{
    //RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    //AFIO->PCFR1 |= (0b100 << 24);   // Disable both SWD and JTAG
    //RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, DISABLE);  // Optional power save
	//RCC->APB1PCENR |= RCC_PWREN;          // enable power module
	// select standby on power-down
	PWR->CTLR = PWR_CTLR_PDDS;

	// peripheral interrupt controller send to deep sleep
	PFIC->SCTLR |= (1 << 2);
	//PFIC->SCTLR |= PFIC_SLEEPDEEP;
	// enable power interface module clock
	//RCC->APB1PCENR |= RCC_APB1Periph_PWR;

	// enable low speed oscillator (LSI)
	//RCC->RSTSCKR |= RCC_LSION;
	//while ((RCC->RSTSCKR & RCC_LSIRDY) == 0) {}
	//stop_timer();
    __WFE();
}

/** Enable Interrupt falling edge A2*/
void EXTI0_INT_INIT (void)
{
  AFIO->PCFR1 = AFIO->PCFR1 & 0xFFFF7FFF; //Sets PA1 & PA2 as I/O by clearing bit 15  As we are not using external oscilator

  GPIO_InitTypeDef GPIO_InitStructure = {0};
  EXTI_InitTypeDef EXTI_InitStructure = {0};
  NVIC_InitTypeDef NVIC_InitStructure = {0};

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* GPIOA ----> EXTI_Line2 */
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2);
  EXTI_InitStructure.EXTI_Line = EXTI_Line2;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI->EVENR |= EXTI_Line2;
  EXTI_Init(&EXTI_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn; // external hardware interrupts
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

void init_all(void){
	// GPIO configuration
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);

	// Use the internal HSI oscillator
	RCC->CTLR |= (1 << 0);   // Enable HSI
	while (!(RCC->CTLR & (1 << 1))); // Wait for HSI to be ready

	RCC->CFGR0 &= ~(0x3);    // Select HSI as system clock (SW[1:0] = 00)

	// Disable HSE (external oscillator) completely if not used
	RCC->CTLR &= ~(1 << 16); // HSEON = 0
	RCC->CTLR &= ~(1 << 17); // HSEBYP = 0

	// Enable GPIOD & AFIO clock
	RCC->APB2PCENR |= (1 << 5) | (1 << 0);  // Enable GPIOD & AFIO

	// Ensure AFIO remap is not interfering with PD6
	AFIO->PCFR1 &= ~(1 << 15); // Keep PA1 & PA2 as GPIO
	AFIO->PCFR1 &= ~(1 << 24); // Ensure no remapping affects PD6

	GPIO_InitTypeDef cfgC = {0};
    cfgC.GPIO_Mode = GPIO_Mode_Out_PP;
   	cfgC.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4;
	cfgC.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &cfgC);

	GPIO_InitTypeDef cfgD = {0};
    cfgD.GPIO_Mode = GPIO_Mode_Out_PP;
   	cfgD.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_6;
	cfgD.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOD, &cfgD);

	EXTI0_INT_INIT();
	__enable_irq();

}

int main(void) {
    SystemInit();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	SystemCoreClockUpdate();
    
    Delay_Init();

	Delay_Ms(PROGRAMMING_DELAY);

    init_all();

	while (1) {

		//Debounce
		if (interrupt){
			interrupt=0;
				//Delay_Ms(30);

					while(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2)){
						long_press_counter++;
						if(long_press_counter>=1000000){
							long_press = 1;
							break;	
						}
					}
					long_press_counter = 0;
		}

		if(long_press){
			ledsON();
			Delay_Ms(500);
			for(int i=0;i<5;i++){
				ledWrite(i,0);
				Delay_Ms(500);
			}
			long_press = 0;
			//__WFI();
                        // Now enter deep sleep using WFE (wake on PA2 falling)
            prepare_for_sleep();
            enter_deep_sleep_event();
            // After wake-up, do system reset
            NVIC_SystemReset();
			counter = 0;
		}

		switch (counter) {
			case 6:
				__WFI(); // sleep mode waits for button press interrupt
				break;
			case 0:
				raider();
				break;
			case 1:
				blink_string("A",0);
				break;
			case 2:
				blink_string("B",1);
				break;
			case 3:
				blink_string("C",2);
				break;
			case 4:
				blink_string("D",3);
				break;
			case 5:
				blink_string("E",4);
				break;
			default:
				counter=0;
				break;
			}	
	}
}

void NMI_Handler(void) {
	
}

void HardFault_Handler(void) {
	
	while (1) {

	}
}