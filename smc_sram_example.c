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
//int num_samples_to_sample = 65535;
int num_samples_to_sample = 8192;


/** DMA buffer. */
uint32_t* g_dma_buf = (uint32_t*)SRAM_BASE_ADDRESS;
uint16_t* g_sampledpins_buf = (uint32_t*)SRAM_BASE_ADDRESS;
//uint32_t g_dma_buf[DMA_BUF_SIZE];
uint32_t transitionData[DMA_BUF_SIZE];
//! [dmac_define_buffer]

/* DMA transfer done flag. */
volatile uint32_t g_xfer_done = 0;

int state = 0;

void start_sampling(){
	uint32_t i;
	uint32_t cfg;
	dma_transfer_descriptor_t desc;
	pmc_enable_periph_clk(ID_DMAC);
	dmac_init(DMAC);
	dmac_set_priority_mode(DMAC, DMAC_PRIORITY_ROUND_ROBIN);
	dmac_enable(DMAC);
	cfg =	DMAC_CFG_SOD_ENABLE |        /** Enable stop on done */
			DMAC_CFG_AHB_PROT(1) |     /** Set AHB Protection */
			DMAC_CFG_FIFOCFG_ALAP_CFG; /** FIFO Configuration */
	dmac_channel_set_configuration(DMAC, DMA_CH, cfg);
	desc.ul_source_addr = (uint32_t) 0x400E0E00 + 0x003C;
	desc.ul_destination_addr = (uint32_t) g_dma_buf;
	
	desc.ul_ctrlA = DMAC_CTRLA_BTSIZE(num_samples_to_sample) |
	DMAC_CTRLA_SRC_WIDTH_WORD |
	DMAC_CTRLA_DST_WIDTH_WORD;
	
	desc.ul_ctrlB = DMAC_CTRLB_SRC_DSCR_FETCH_DISABLE |
		DMAC_CTRLB_DST_DSCR_FETCH_DISABLE |
		DMAC_CTRLB_FC_MEM2MEM_DMA_FC |
		DMAC_CTRLB_SRC_INCR_FIXED |
		DMAC_CTRLB_DST_INCR_INCREMENTING;
	
	desc.ul_descriptor_addr = 0;
	dmac_channel_single_buf_transfer_init(DMAC, DMA_CH, &desc);
	
	dmac_channel_enable(DMAC, DMA_CH);
	
	while (!dmac_channel_is_transfer_done(DMAC, DMA_CH)) {
	}
	state = 1;
}

uint16_t get_16_pin_inputs(uint32_t sampled_int){
	uint16_t first4 = 0b1111 & (sampled_int >> 3);
	uint16_t second7 = 0b11111110000 & (sampled_int >> 7);
	uint16_t third2 = 0b1100000000000 & (sampled_int >> 10);
	uint16_t fourth2 = 0b110000000000000 & (sampled_int >> 11);
	uint16_t last = 0b1000000000000000 & (sampled_int >> 14);
	return first4|second7|third2|fourth2|last;
}

void print_uint16_binary(uint16_t pinValues){
	uint8_t i;
	for(i = 15; i >= 0; i--){
		printf("%d",(pinValues>>i) & 1);
	}
	printf(" ");
}

void embedded_controller(){
	int i;
	int done = 0;
	while(!done){
		switch(state){
			case 0:
				//clear data
				for(i = 0; i < num_samples_to_sample;i++){
					g_dma_buf[i] = 0;
				}
			
				//start collecting with DMA
				start_sampling();
			case 1:
				//TODO: add compression while data is being sampled
				state = 2;
				break;
			case 2:
			
			
				state = 3;
				break;
			
				for(i = 0; i < num_samples_to_sample;i++){
					printf("%d ", (g_dma_buf[i]>>24) & 1);
				}
			
			
				state = 3;
				break;
			
			
				int transition_index = 0;
				for(i = 1; i < num_samples_to_sample; i++){
					int changed_vals = (g_dma_buf[i - 1] ^ g_dma_buf[i]); // 1 wherever there was a transition
					int j = 0;
					//go through each channel to check if it changed, store new value
					for(; j < 32; j++){
						if((changed_vals>>j) & 1){
							//create the compressed packet using:
							//  channel number = j;
							//  timestamp = i;
							//  new_value = sample_array[i]>>j;
							transitionData[transition_index] = ((g_dma_buf[i]>>j) & 1) | j<<1 | i<<6;
							transition_index ++;
						}
					}
				}
				printf("\n");
				for(i = 0; i < transition_index;i++){
					printf("%d ", transitionData[i]);
				}
				/*
				//Add compressing data step (assuming you have all data already)
				int i = 1;
				int transition_array = SOME_ARRAY_IN_MEMORY;
				int sample_array = SOME_POINTER_IN_MEMORY;
				for(; i < NUM_SAMPLES; i++){
					int changed_vals = (sample_array[i - 1] ^ sample_array[i]); // 1 wherever there was a transition
					int j = 0;
					//go through each channel to check if it changed, store new value
					for(; j < 16; j++){
						if((changed_vals>>j) & 1){
							//create the compressed packet using:
							//  channel number = j;
							//  timestamp = i;
							//  new_value = sample_array[i]>>j;
						}
					}
				}
				*/
				state = 3;
				break;
			case 3:
				//grab relevant numbers from 32 sampled pins
				for(i = 0; i < num_samples_to_sample; i++){
					g_sampledpins_buf[i] = get_16_pin_inputs(g_dma_buf[i])
				}
				for(i = 0; i < num_samples_to_sample; i++){
					print_uint16_binary(g_sampledpins_buf[i]);
				}
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
	
	ioport_set_pin_dir(BUTTON_0_PIN, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(BUTTON_0_PIN, IOPORT_MODE_PULLUP);
	ioport_set_pin_dir(LED_0_PIN, IOPORT_DIR_OUTPUT);

	/* Enable PMC clock for SMC */
	pmc_enable_periph_clk(ID_SMC);

	/* SMC configuration between SRAM and SMC waveforms. */
	configure_sram(SRAM_CHIP_SELECT);
#ifdef SRAM_CHIP_SELECT_2ND
	configure_sram(SRAM_CHIP_SELECT_2ND);
#endif
	
	
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
	dacc_write_conversion_data(DACC_BASE, (uint32_t)(DACC_MAX_DATA * 3.3 / 3.3));	
	
	initialize_pins();
	int prevValue = 0;
	while (1) {
		wait_for_button();
		embedded_controller();
	}
	

#ifdef CONF_KIT_DATA_EXIST
	/* Show the kit data */
	show_kit_data();
#endif

	while (1) {
		/* Infinite loop */
	}

}
