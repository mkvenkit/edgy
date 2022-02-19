#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <zephyr.h>
#include <gpio.h>
#include <flash.h>
#include <device.h>
#include <uart.h>

// from main.c
extern struct device *cdcacm_dev;
extern struct k_pipe rx_pipe;

// from iceprog_fw.c
extern uint8_t rxframe[];

// 64K
#define FLASH_SECTOR_SIZE        0x10000
#define FLASH_PAGE_SIZE          256

#define GPIO_RESET_PIN  22
#define GPIO_CDONE_PIN  23
#define GPIO_LED_PIN    DT_GPIO_LEDS_LED_0_GPIO_PIN

#define GPIO_RESET_GPIODEV  "GPIO_0"
#define GPIO_CDONE_GPIODEV  "GPIO_0"
#define GPIO_LED_GPIODEV    DT_GPIO_LEDS_LED_0_GPIO_CONTROLLER

// we'll use these for quick access
static struct device *gpio_reset_dev;
static struct device *gpio_cdone_dev;
static struct device *gpio_led_dev;
static struct device *flash_dev;

static void led_off()
{
    gpio_pin_write(gpio_led_dev, GPIO_LED_PIN, 1);
}

static void led_on()
{
    gpio_pin_write(gpio_led_dev, GPIO_LED_PIN, 0);
}

static void reset_high()
{
    gpio_pin_write(gpio_reset_dev, GPIO_RESET_PIN, 1);
}

static void reset_low()
{
    gpio_pin_write(gpio_reset_dev, GPIO_RESET_PIN, 0);
}

static u32_t cdone_read()
{
    u32_t val;
    gpio_pin_read(gpio_cdone_dev, GPIO_CDONE_PIN, &val);

    return val;
}

static void flash_erase_block64k(u32_t address)
{
    // flash_write_protection_set(flash_dev, false);
	if (flash_erase(flash_dev,
			address,
			FLASH_SECTOR_SIZE) != 0) {
        // error
	}
}

static void flash_write_page(u32_t page_num, u8_t *buf)
{
    // flash_write_protection_set(flash_dev, false);
    if (flash_write(flash_dev, page_num<<8, buf,
	    FLASH_PAGE_SIZE) != 0) {
		// error
	}
}

static void flash_read_page(u32_t page_num, u8_t *buf)
{
    if (flash_read(flash_dev, page_num<<8, buf,
	    FLASH_PAGE_SIZE) != 0) {
        // error
	}
}

static void flash_power_up()
{
    flash_write_protection_set(flash_dev, false);
}

static void flash_power_down()
{
    // do nothing?
}

static void flash_erase_all()
{
    size_t num_pages = flash_get_page_count(flash_dev);

    if (flash_erase(flash_dev,
			0,
			num_pages<<8) != 0) {
        // error
	}
}

static u32_t flash_get_JEDEC_ID()
{
#warning JEDEC ID is hardcoded
    return 0xEF164017;
}

void serial_write(u8_t *buf, u32_t len)
{
    for (int i=0; i<len; i++) {
        uart_poll_out(cdcacm_dev, buf[i]);
    }
}

static void delay(u32_t ms)
{
    k_sleep(ms);
}

static void interface_pins_configure()
{
    gpio_reset_dev = device_get_binding(GPIO_RESET_GPIODEV);
    gpio_cdone_dev = device_get_binding(GPIO_CDONE_GPIODEV);
    gpio_led_dev = device_get_binding(GPIO_LED_GPIODEV);

    // CDONE, RESET, and LED
    gpio_pin_configure(gpio_cdone_dev, GPIO_CDONE_PIN, (GPIO_DIR_IN));
    gpio_pin_configure(gpio_reset_dev, GPIO_RESET_PIN, (GPIO_DIR_OUT));
    gpio_pin_configure(gpio_led_dev, GPIO_LED_PIN, (GPIO_DIR_OUT));

    flash_dev = device_get_binding(CONFIG_SPI_FLASH_W25QXXDV_DRV_NAME);
}

// serial stuff
// Reads a frame from Serial
bool read_serial_frame(void) 
{
    size_t bytes_read;
    uint8_t byte;

    int index = 0;
    while(index < 512) {
        k_pipe_get(&rx_pipe, &byte, 1, &bytes_read, 1, K_FOREVER);
        if(byte == FEND) {
            break;
        }
        rxframe[index] = byte;
        index++;
    }

    return index > 0;
}

#endif // __INTERFACE_H__