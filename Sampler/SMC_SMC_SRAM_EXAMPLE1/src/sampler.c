/**
 * \file
 *
 * \brief SAM EDBG TWI Information Interface Example
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

/**
 * \mainpage SAM EDBG TWI Information Interface Example
 *
 * \section Purpose
 *
 * The example show the feature of the EDBG TWI information interface.
 *
 * \section Requirements
 *
 * This package can be used with SAM4E Xplained Pro.
 *
 * \section Description
 *
 * The program demo how to read the extension boards information and kit data
 * by the EDBG TWI information interface.
 *
 * \section Usage
 *
 * -# Build the program and download it inside the evaluation board.
 * -# On the computer, open and configure a terminal application
 *    (e.g. HyperTerminal on Microsoft Windows) with these settings:
 *   - 115200 bauds
 *   - 8 bits of data
 *   - No parity
 *   - 1 stop bit
 *   - No flow control
 * -# Start the application.
 * -# In the terminal window, the following text should appear (values
 *    depend on the board and chip used):
 *    \code
 *     -- EDBG TWI Information Interface Example --
 *     -- xxxxxx-xx
 *     -- Compiled: xxx xx xxxx xx:xx:xx --
 *    \endcode
 * -# Extension boards information and kit data will be show in the terminal.
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include <asf.h>
#include <string.h>
#include "conf_board.h"
#include "ioport.h"
#include "dmac.h"
#include "smc.h"
#include "pio.h"
#include <unistd.h>

//dacc constants
//#define DACC_CHANNEL        0 // (PB13)
#define DACC_CHANNEL        1 // (PB14)

//! DAC register base for test
#define DACC_BASE           DACC
//! DAC ID for test
#define DACC_ID             ID_DACC

#define DACC_ANALOG_CONTROL (DACC_ACR_IBCTLCH0(0x02) \
| DACC_ACR_IBCTLCH1(0x02) \
| DACC_ACR_IBCTLDACCORE(0x01))

/**
 *  \brief Configure the Console UART.
 */
static void configure_console(void)
{
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.paritytype = CONF_UART_PARITY
	};

	/* Configure console UART. */
	sysclk_enable_peripheral_clock(CONSOLE_UART_ID);
	stdio_serial_init(CONF_UART, &uart_serial_options);
}

//! [dmac_define_channel]
/** DMA channel used in this example. */
#define DMA_CH 0
//! [dmac_define_channel]

//! [dmac_define_buffer]
/** DMA buffer size. */
#define DMA_BUF_SIZE    8192
int num_samples_to_sample = 1024;
volatile uint32_t* waiting_buffers[2];
dma_transfer_descriptor_t descriptor_pointers[128];


/** DMA buffer. */
uint32_t* g_dma_buf = (uint32_t*)SRAM_BASE_ADDRESS;
uint16_t* g_sampledpins_buf = (uint16_t*)SRAM_BASE_ADDRESS;
//uint32_t g_dma_buf[DMA_BUF_SIZE];
uint8_t* transitionData = ((uint32_t*)SRAM_BASE_ADDRESS_2ND);
//! [dmac_define_buffer]

/* DMA transfer done flag. */
volatile uint32_t g_xfer_done = 0;

volatile int32_t num_times_in_handler;

volatile uint32_t buffer_number;

volatile uint16_t trigger_values[16];

volatile int output_voltage;

uint32_t transition_index;

int state = 0;

int swapped;


inline uint16_t get_16_pin_inputs(uint32_t sampled_int){
	uint16_t first4 = 0b1111 & (sampled_int >> 3);
	uint16_t second7 = 0b11111110000 & (sampled_int >> 7);
	uint16_t third2 = 0b1100000000000 & (sampled_int >> 10);
	uint16_t fourth2 = 0b110000000000000 & (sampled_int >> 11);
	uint16_t last = 0b1000000000000000 & (sampled_int >> 14);
	return first4|second7|third2|fourth2|last;
}

dma_transfer_descriptor_t desc1,desc2;

void start_sampling(){
	
	
	// DMA config
	uint32_t cfg;
	pmc_enable_periph_clk(ID_DMAC);
	dmac_init(DMAC);
	dmac_set_priority_mode(DMAC, DMAC_PRIORITY_ROUND_ROBIN);
	dmac_enable(DMAC);
	cfg =	DMAC_CFG_SOD_DISABLE |     /** Disable stop on done */
	DMAC_CFG_AHB_PROT(1) |     /** Set AHB Protection */
	DMAC_CFG_FIFOCFG_ALAP_CFG; /** FIFO Configuration */
	dmac_channel_set_configuration(DMAC, DMA_CH, cfg);
	uint32_t i;
	
	uint32_t ul_ctrlA = DMAC_CTRLA_BTSIZE(num_samples_to_sample) |
		DMAC_CTRLA_SRC_WIDTH_WORD |
		DMAC_CTRLA_DST_WIDTH_WORD;
	
	uint32_t ul_ctrlB = DMAC_CTRLB_SRC_DSCR_FETCH_FROM_MEM |
		DMAC_CTRLB_DST_DSCR_FETCH_FROM_MEM |
		DMAC_CTRLB_FC_MEM2MEM_DMA_FC |
		DMAC_CTRLB_SRC_INCR_FIXED |
		DMAC_CTRLB_DST_INCR_INCREMENTING;
	
	
	desc1.ul_source_addr = (uint32_t) 0x400E0E00 + 0x003C;
	desc1.ul_destination_addr = (uint32_t) waiting_buffers[0];
	
	desc1.ul_ctrlA = ul_ctrlA;
	
	desc1.ul_ctrlB = ul_ctrlB;
	
	desc1.ul_descriptor_addr = &desc2;
		
		
	desc2.ul_source_addr = (uint32_t) 0x400E0E00 + 0x003C;
	desc2.ul_destination_addr = (uint32_t) waiting_buffers[1];
	
	desc2.ul_ctrlA = ul_ctrlA;
		
	desc2.ul_ctrlB = ul_ctrlB;
	
	desc2.ul_descriptor_addr = &desc1;
		
	
	ul_ctrlA = DMAC_CTRLA_BTSIZE(num_samples_to_sample) |
		DMAC_CTRLA_SRC_WIDTH_WORD |
		DMAC_CTRLA_DST_WIDTH_WORD;
		
	for(i = 2; i < 128; i++){
		descriptor_pointers[i].ul_ctrlA = ul_ctrlA;
		descriptor_pointers[i].ul_ctrlB = ul_ctrlB;
		descriptor_pointers[i].ul_source_addr = (uint32_t) 0x400E0E00 + 0x003C;
		descriptor_pointers[i].ul_destination_addr = (uint32_t) (g_dma_buf + i * num_samples_to_sample);
		descriptor_pointers[i].ul_descriptor_addr = descriptor_pointers + i + 1;
	}
	//set last descriptor to point to null to signal done
	descriptor_pointers[127].ul_descriptor_addr = 0;
		
	dmac_channel_multi_buf_transfer_init(DMAC, DMA_CH, &desc1);
	NVIC_EnableIRQ(DMAC_IRQn);
	dmac_enable_interrupt(DMAC, (DMAC_EBCIER_CBTC0 << DMA_CH | DMAC_EBCIDR_BTC0  << DMA_CH));

	g_xfer_done = 0;
	dmac_channel_enable(DMAC, DMA_CH);
	while (!g_xfer_done) {
	}
	state = 1;
}

int8_t check_trigger_behavior(uint16_t first_pins, uint16_t last_pins){
	int i;
	for(i = 0; i < 16; i++){
		if(trigger_values[i] != 2){
			if(((last_pins>>i) & 1) != trigger_values[i] || ((last_pins>>i) & 1) == ((first_pins>>i) & 1)){
				return 0;
			}
		}
	}
	return 1;
}


void DMAC_Handler(void)
{
	if(!swapped){
		buffer_number = !buffer_number;
	}
	
	if(swapped == 0 && check_trigger_behavior(get_16_pin_inputs(waiting_buffers[buffer_number][0]),get_16_pin_inputs(waiting_buffers[buffer_number][num_samples_to_sample-1]))){
		dmac_channel_set_descriptor_addr(DMAC,DMA_CH,descriptor_pointers+2);
		swapped = 1;
	}
	
	
	uint32_t dma_status;

	dma_status = dmac_get_status(DMAC);

	if (dma_status & (DMAC_EBCIER_CBTC0 << DMA_CH)) {
		g_xfer_done = 1;
	}
}


void print_uint16_binary(uint16_t pinValues){
	int8_t i;
	for(i = 0; i <16; i++){
		printf("%d",~(pinValues>>i) & 1);
	}
	return;
}


volatile uint32_t g_ul_ms_ticks = 0;

static void mdelay(uint32_t ul_dly_ticks)
{
	uint32_t ul_cur_ticks;
	ul_cur_ticks = g_ul_ms_ticks;
	while ((g_ul_ms_ticks
	- ul_cur_ticks) < ul_dly_ticks);
}

void SysTick_Handler(void)
{
	g_ul_ms_ticks++;
}

//write check_trigger_behavior function

void embedded_controller(){
	state = 0;
	int i;
	int done = 0;
	int start = 0;
	while(!done){
		switch(state){
			case 0:
				//clear data
				for(i = 0; i < num_samples_to_sample * 128;i++){
					g_dma_buf[i] = 0;
				}
				for(i = 0; i < num_samples_to_sample * 128 * 4;i++){
					transitionData[i] = 0;
				}
				
				/*
				for(i = 0; i < 16; i++){
					trigger_values[i] = 2;
				}
				trigger_values[0] = 1;
				*/
				
				buffer_number = 1;
				transition_index = 0;
				num_times_in_handler = -1;
				swapped = 0;
				
				waiting_buffers[0] = g_dma_buf;
				waiting_buffers[1] = g_dma_buf + num_samples_to_sample;
				
			
				//start collecting with DMA
				start_sampling();
			case 1:
				//grab relevant numbers from 32 sampled pins
				//if buffer_number == 0, everything is fine
				//if buffer_number == 1, swap first 1024 and second 1024 first, then proceed
				if(buffer_number){
					for(i = 0; i < num_samples_to_sample; i++){
						uint32_t temp = waiting_buffers[buffer_number][i];
						waiting_buffers[1][i] = waiting_buffers[0][i];
						waiting_buffers[0][i] = temp;
					}
				}
				for(i = 0; i < num_samples_to_sample*128; i++){
					g_sampledpins_buf[i] = get_16_pin_inputs(g_dma_buf[i]);
				}
				
		
				
			
				int transition_index = 0;
				/*
				for(i = 1; i < num_samples_to_sample*128 + 8192*2; i++){
					if (g_sampledpins_buf[i] != g_sampledpins_buf[i-1]){
						int changed_vals = (g_sampledpins_buf[i - 1] ^ g_sampledpins_buf[i]); // 1 wherever there was a transition
						int j = 0;
						//go through each channel to check if it changed, store new value
						for(; j < 16; j++){
							if((changed_vals>>j) & 1){
								//create the compressed packet using:
								//  channel number = j;
								//  timestamp = i;
								//  new_value = sample_array[i]>>j;
								uint32_t transition_point = ((g_sampledpins_buf[i]>>j) & 1) | j<<1 | i<<6;
								uint8_t first_byte = (uint8_t)(transition_point>>16);
								uint8_t second_byte = (uint8_t)(transition_point>>8);
								uint8_t third_byte = (uint8_t)(transition_point>>0);
								transitionData[3*transition_index] = first_byte;
								transitionData[3*transition_index+1] = second_byte;
								transitionData[3*transition_index+2] = third_byte;
								
								transition_index ++;
							}
						}
						
					}
				}
				*/
				
				
				
				
				state = 2;
				break;
				
			
			case 2:
			/*
				for(i = 0; i < num_samples_to_sample; i++){
					printf("%d ", (g_dma_buf[i]>>11) & 1);
					//print_uint16_binary(g_sampledpins_buf[i]);
				}
				*/
				//grab relevant numbers from 32 sampled pins
				/*
				for(i = 0; i < num_samples_to_sample*4; i++){
					g_sampledpins_buf[i] = get_16_pin_inputs(g_dma_buf[i]);
				}
				for(i = 0; i < num_samples_to_sample*5; i++){
					//printf("%d", ~(g_sampledpins_buf[i]>>4) & 1);
					//printf("%d", ~(g_sampledpins_buf[i]>>5) & 1);
					//printf("%d", ~(g_sampledpins_buf[i]>>6) & 1);
					//printf("%d", ~(g_sampledpins_buf[i]>>7) & 1);
					print_uint16_binary(g_sampledpins_buf[i]);
					printf("\r\n");
				}
				*/
				
				//printf("transition index: %d\r\n", transition_index);
				/*
				for(i = 0; i < num_samples_to_sample*128; i+= 64){
					//printf("%d ", (g_dma_buf[1023*num_samples_to_sample + i]>>8) & 1);
					//printf("%d\t",i);
					print_uint16_binary(g_sampledpins_buf[i]);
					printf("\r\n");
				}
				*/
				while(!check_trigger_behavior(g_sampledpins_buf[0],g_sampledpins_buf[start])){
					start += 1;
				}
				
				for(i = start; i < num_samples_to_sample*128; i+= 16){
				//for(i = 0; i < num_samples_to_sample; i+= 64){
					char a = ~((char)g_sampledpins_buf[i]);
					char b = ~((char)(g_sampledpins_buf[i]>>8));
					printf("%c%c",b,a);
				}
				
				
				printf("no more data || no more data || no more data");
				NVIC_EnableIRQ(SysTick_IRQn);
				volatile int error = SysTick_Config(sysclk_get_cpu_hz()/1000);
				mdelay(1000);
				done = 1;
				break;
			
		}
	}
}


void wait_for_button(){
	while (1) {
		/* Is button pressed? */
		if (ioport_get_pin_level(BUTTON_0_PIN) == BUTTON_0_ACTIVE) {
			/* Yes, so turn LED on. */
			ioport_set_pin_level(LED_0_PIN, LED_0_ACTIVE);
			break;
			} else {
			/* No, so turn LED off. */
			ioport_set_pin_level(LED_0_PIN, !LED_0_ACTIVE);
		}
	}
}

void initialize_pins(){
	
	ioport_set_pin_dir(PIO_PA3_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA3_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA4_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA4_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA5_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA5_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA6_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA6_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA11_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA11_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA12_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA12_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA13_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA13_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA14_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA14_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA15_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA15_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA16_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA16_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA17_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA17_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA21_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA21_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA22_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA22_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA24_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA24_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA25_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA25_IDX, IOPORT_MODE_PULLDOWN);
	ioport_set_pin_dir(PIO_PA29_IDX, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(PIO_PA29_IDX, IOPORT_MODE_PULLDOWN);
}


static void configure_sram(uint32_t cs)
{
	smc_set_setup_timing(SMC, cs, SMC_SETUP_NWE_SETUP(1)
	| SMC_SETUP_NCS_WR_SETUP(1)
	| SMC_SETUP_NRD_SETUP(1)
	| SMC_SETUP_NCS_RD_SETUP(1));
	smc_set_pulse_timing(SMC, cs, SMC_PULSE_NWE_PULSE(6)
	| SMC_PULSE_NCS_WR_PULSE(6)
	| SMC_PULSE_NRD_PULSE(6)
	| SMC_PULSE_NCS_RD_PULSE(6));
	smc_set_cycle_timing(SMC, cs, SMC_CYCLE_NWE_CYCLE(7)
	| SMC_CYCLE_NRD_CYCLE(7));
	smc_set_mode(SMC, cs, SMC_MODE_READ_MODE | SMC_MODE_WRITE_MODE);
}

/**
 * \brief Main application
 */
int main(void)
{
	/* Initialize the SAM system */
	sysclk_init();

	/* Initialize the board */
	board_init();

	/*Configure UART console.*/	

	configure_console();
	
	char uc_key[200];
	
	
	initialize_pins();
	int prevValue = 0;
	
	int i;
	for(i = 0; i < 16; i++){
		trigger_values[i] = 2;
	}
	for(i = 0; i < 200;i++){
		uc_key[i] = 0;
	}
	volatile int numchars = 0;
	uc_key[0] = '0';
	while(numchars == 0 || uc_key[numchars-1] != ';'){
		usart_serial_getchar((Usart *)CONSOLE_UART, uc_key + numchars);
		//printf("%c",uc_key[numchars]);
		if(uc_key[numchars] != 0){
			numchars ++;
		}
	}
	uc_key[numchars] = '\0';
	
	i = 0;
	output_voltage = atoi(uc_key);
	while(uc_key[i] != '\n'){
		i++;
	}
	i++;
	int j;
	for(j = 0; j < 16; j++){
		trigger_values[j] = atoi(uc_key+i);
		while(uc_key[i] != ',' && uc_key[i] != ';'){
			i++;
		}
		i++;
	}
	
	ioport_set_pin_dir(BUTTON_0_PIN, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(BUTTON_0_PIN, IOPORT_MODE_PULLUP);
	ioport_set_pin_dir(LED_0_PIN, IOPORT_DIR_OUTPUT);

	/* Enable PMC clock for SMC */
	pmc_enable_periph_clk(ID_SMC);

	/* SMC configuration between SRAM and SMC waveforms. */
	configure_sram(SRAM_CHIP_SELECT);
	configure_sram(SRAM_CHIP_SELECT_2ND);
	
	
	// DAC stuff
		/* Enable clock for DACC */
	sysclk_enable_peripheral_clock(DACC_ID);
	/* Reset DACC registers */
	dacc_reset(DACC_BASE);
	/* Half word transfer mode */
	dacc_set_transfer_mode(DACC_BASE, 0);
	dacc_set_timing(DACC_BASE,0, 0x10);
	/* Disable TAG and select output channel DACC_CHANNEL */
	dacc_set_channel_selection(DACC_BASE, DACC_CHANNEL);
	/* Enable output channel DACC_CHANNEL */
	dacc_enable_channel(DACC_BASE, DACC_CHANNEL);

	/* Set up analog current */
	dacc_set_analog_control(DACC_BASE, DACC_ANALOG_CONTROL);
	
	
	
	// max val is DACC_MAX_DATA
	// set the 3.3 here to change output voltage
	dacc_write_conversion_data(DACC_BASE, (uint32_t)(DACC_MAX_DATA * 1.9 / 3.3));
	
	
	

	
	//printf("%s",uc_key);
	//printf("no more data || no more data || no more data");
	//while(1);
	ioport_set_pin_level(LED_0_PIN, LED_0_ACTIVE);
		
	embedded_controller();
	ioport_set_pin_level(LED_0_PIN, !LED_0_ACTIVE);
		
	rstc_enable_user_reset((RSTC));
	rstc_start_software_reset(RSTC);
	
	

#ifdef CONF_KIT_DATA_EXIST
	/* Show the kit data */
	show_kit_data();
#endif

	while (1) {
		/* Infinite loop */
	}

}
