/*
 *  iceprog -- firmware sketch for Arduino based Lattice iCE programmers
 *
 *  Chris B. <chris@protonic.co.uk> @ Olimex Ltd. <c> 2017
 *  Tavish Naruka <tavish@electronut.in> 2019
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Relevant Documents:
 *  -------------------
 *  http://www.latticesemi.com/~/media/Documents/UserManuals/EI/icestickusermanual.pdf
 *  http://www.micron.com/~/media/documents/products/data-sheet/nor-flash/serial-nor/n25q/n25q_32mb_3v_65nm.pdf
 *  http://www.ftdichip.com/Support/Documents/AppNotes/AN_108_Command_Processor_for_MPSSE_and_MCU_Host_Bus_Emulation_Modes.pdf
 *  https://www.olimex.com/Products/FPGA/iCE40/iCE40HX1K-EVB/
 *  https://github.com/Marzogh/SPIFlash  
 */

// Install SPIFlash v2.2.0 Arduino Library; Sketch -> Include Library -> Manage Libraries
// SPIFlash 2.2.0 library for Winbond Flash Memory by Prajwal Bhattaram - Marzogh

//commands list
#define READ_ID   0x9F
#define PWR_UP    0xAB
#define PWR_DOWN  0xB9
#define WR_EN   0x06
#define BULK_ERASE  0xC7
#define SEC_ERASE 0xd8
#define CMD_PROG    0x02
#define CMD_READ    0x03
#define CMD_READ_ALL    0x83
#define CMD_ERROR 0xee
#define CMD_READY   0x44
#define CMD_EMPTY    0x45

#define FEND  0xc0
#define FESC  0xdb
#define TFEND   0xdc
#define TFESC  0xdd

#include "interface.h"

uint8_t rxframe[512], txframe[512], fcs,rfcs;
uint8_t membuf[256];
uint8_t data_buffer[256];
uint16_t txp;
uint32_t maxPage;
bool escaped;

// forward declarations
void decodeFrame(void);
void SendID(void);
void secerase(uint32_t sector);
void readpage(uint16_t adr);
void readAllPages(void);
void writepage(int pagenr);
void startframe(uint8_t command);
void sendframe();
void flash_bulk_erase(void);
void addbyte(uint8_t newbyte);

// FRAME:   <FEND><CMD>{data if any}<FCS(sume of all bytes = 0xFF)><FEND>
// if FEND, FESC in data - <FEND>=<FESC><TFEND>;  <FESC>=<FESC><TFESC>

void setup() {
    // initialize pins
    interface_pins_configure();

    led_off();
    reset_high();
}

void loop()
{
    if (read_serial_frame()) {
        decodeFrame();
        
        if (rfcs==0xff) {
            reset_low();

            switch (rxframe[0]) {
                case FEND:
                    break;

                case READ_ID:
                    SendID();
                    break;

                case BULK_ERASE:
                    flash_bulk_erase();
                    break;

                case SEC_ERASE:
                    flash_power_up();
                    secerase((rxframe[1]<<8) | rxframe[2]);
                    flash_power_down();

                    break;

                case CMD_READ: 
                    flash_power_up();
                    readpage((rxframe[1]<<8) | rxframe[2]);
                    flash_power_down();
                    break;

                case CMD_READ_ALL:
                    readAllPages();
                    break;   

                case CMD_PROG:
                    writepage((rxframe[1]<<8) | rxframe[2]);
                    break;

                default:
                    break;
            }

            reset_high();
        } 
    }
}

void secerase(uint32_t sector)
{
    flash_erase_block64k(sector<<8);

    startframe(CMD_READY);
    sendframe();  
}

void decodeFrame(void)
{
    int x,y;
    escaped = false;
    y = 1;
    rfcs = rxframe[1];
    rxframe[0]=rxframe[1];
    for (x=2;x<512;x++)
    {
        switch (rxframe[x]) {
            case FEND:
                x = 513;
                break;

            case FESC:
                escaped = true;
                break;

            case TFEND:
                if (escaped) {
                    rxframe[y++] = FEND;
                    rfcs+=FEND;
                    escaped = false;
                }
                else {
                    rxframe[y++]=TFEND;
                    rfcs+=TFEND;
                }
                break;

            case TFESC:
                if (escaped) {
                    rxframe[y++]=FESC;
                    rfcs+=FESC;
                    escaped = false;
                }
                else {
                    rxframe[y++]=TFESC;
                    rfcs+=TFESC;
                }
                break;    

            default:
                escaped = false;
                rxframe[y++] = rxframe[x];
                rfcs+=rxframe[x];
                break;
        }
   }
}

void writepage(int pagenr)
{
    int x;
    flash_power_up();

    for (x=0; x<256;x++) {
        membuf[x]=rxframe[x+3];
    }

    flash_write_page(pagenr, membuf);
    flash_read_page(pagenr, data_buffer);
    flash_power_down();

    for (int a = 0; a < 256; a++) {
        if (data_buffer[a] != membuf[a]) {
            return;
        }
    }

    startframe(CMD_READY);
    sendframe();  
}

void flash_bulk_erase(void)
{
    flash_power_up();
    flash_erase_all();
    flash_power_down();
    startframe(CMD_READY);
    addbyte(BULK_ERASE);
    sendframe();
}

void SendID(void)
{
    flash_power_up();
    uint32_t JEDEC = flash_get_JEDEC_ID();
    flash_power_down();
    startframe(READ_ID);
    addbyte(JEDEC >> 16);
    addbyte(JEDEC >> 8);
    addbyte(JEDEC >> 0);
    sendframe();
}

void sendframe() 
{
    fcs = 0xff - fcs;
    addbyte(fcs);
    txframe[txp++] = FEND;
    serial_write(txframe,txp);
}

void startframe(uint8_t command) 
{
  txframe[0]=FEND;
  txframe[1]=command;
  txp = 2;
  fcs = command;
}

void addbyte(uint8_t newbyte) 
{
    fcs+=newbyte;
    if (newbyte == FEND) {
        txframe[txp++] = FESC;
        txframe[txp++] = TFEND;
    }
    else if (newbyte == FESC) {
        txframe[txp++] = FESC;
        txframe[txp++] = TFESC;
    }
    else {
        txframe[txp++] = newbyte;
    }
}

void readAllPages(void) 
{
    flash_power_up();
    maxPage=0x2000;
    delay(10);
    
    for (int p=0;p<maxPage;p++) {
        readpage(p);
    }

    startframe(CMD_READY);
    sendframe();
    
    flash_power_down();
}

void readpage(uint16_t adr)
{
    bool sendempty = true;  

    flash_read_page(adr, data_buffer);

    for (int a = 0; a < 256; a++){
        if (data_buffer[a] != 0xff){
            startframe(CMD_READ);
            addbyte(adr >> 8);
            addbyte(adr >> 0);
            for (int b = 0;b<256;b++) {
                addbyte(data_buffer[b]);
            }
            sendframe();
            sendempty=false;
            break;    
        }
    }

    if (sendempty) {
        startframe(CMD_EMPTY);
        addbyte(adr >> 8);
        addbyte(adr >> 0);  
        sendframe();
    }
}
