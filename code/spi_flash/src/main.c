/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <uart.h>
#include <stdio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(ice_edge, LOG_LEVEL_DBG);

/* CDC ACM device */
struct device *cdcacm_dev;

/* pipe for rx from serial */
unsigned char __aligned(4) rx_ring_buffer[100];
struct k_pipe rx_pipe;

/* exported functions from iceprog_fw.c */
void setup();
void loop();

/*
	uart data from CDC ACM port is received via interrupt, and to send you can
	use uart_poll_out(...), or uart_fifo_fill(...)
*/

static void cdc_acm_interrupt_handler(struct device *dev)
{
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {

		if (uart_irq_rx_ready(dev)) {
			unsigned char byte;

			LOG_DBG("RX ready interrupt");

			while (uart_fifo_read(dev, &byte, sizeof(byte))) {
				// process byte, if full packet is recieved, then for e.g. put 
				// in a fifo for some other thread to process 

				/* send data to the consumers */
				size_t bytes_written;
				k_pipe_put(&rx_pipe, &byte, 1, &bytes_written,
					1, K_NO_WAIT);
			}
		}
	}
}

void main(void)
{
	cdcacm_dev = device_get_binding(DT_NORDIC_NRF_USBD_VIRTUALCOM_LABEL);
	if (!cdcacm_dev) {
		LOG_ERR("CDC ACM device not found");
		return;
	}

	LOG_INF("USB serial initialized");

	uart_irq_callback_set(cdcacm_dev, cdc_acm_interrupt_handler);

	/* initiailize rx pipe */
	k_pipe_init(&rx_pipe, rx_ring_buffer, sizeof(rx_ring_buffer));

	/* Enable rx interrupts */
	uart_irq_rx_enable(cdcacm_dev);

	while(1) {
		loop();
	}
}
