/*

    TODO: add copyright notice, and source from olimex
    iceprog.c

*/

#include "iceprog.h"
#include "interface.h"

uint8_t rxframe[512], txframe[512], fcs,rfcs;
uint8_t membuf[256];
uint8_t data_buffer[256];
uint16_t txp;
uint32_t maxPage;
bool escaped;

extern device *flash_dev;

//SPIFlash flash(CS);

// ********************************************************************************
// Utility 
// ********************************************************************************
static void addbyte(uint8_t newbyte)
{
    fcs += newbyte;
    if (newbyte == FEND)
    {
        txframe[txp++] = FESC;
        txframe[txp++] = TFEND;
    }
    else if (newbyte == FESC)
    {
        txframe[txp++] = FESC;
        txframe[txp++] = TFESC;
    }
    else
        txframe[txp++] = newbyte;
}

static void sendframe()
{
    fcs = 0xff - fcs;
    addbyte(fcs);
    txframe[txp++] = FEND;
    Serial.write(txframe, txp);
}

static void startframe(uint8_t command)
{
    txframe[0] = FEND;
    txframe[1] = command;
    txp = 2;
    fcs = command;
}

static void decodeFrame(void)
{
    int x, y;
    escaped = false;
    y = 1;
    rfcs = rxframe[1];
    rxframe[0] = rxframe[1];
    for (x = 2; x < 512; x++)
    {
        switch (rxframe[x])
        {

        case FEND:
            x = 513;
            break;

        case FESC:
            escaped = true;
            break;

        case TFEND:
            if (escaped)
            {
                rxframe[y++] = FEND;
                rfcs += FEND;
                escaped = false;
            }
            else
            {
                rxframe[y++] = TFEND;
                rfcs += TFEND;
            }

            break;

        case TFESC:
            if (escaped)
            {
                rxframe[y++] = FESC;
                rfcs += FESC;
                escaped = false;
            }
            else
            {
                rxframe[y++] = TFESC;
                rfcs += TFESC;
            }

            break;

        default:
            escaped = false;
            rxframe[y++] = rxframe[x];
            rfcs += rxframe[x];
            break;
        }
    }
}

// ********************************************************************************
// FLASH WRITE 
// ********************************************************************************
static void secerase(uint32_t sector)
{
    flash.eraseBlock64K(sector << 8);
    startframe(READY);
    sendframe();
}

static void writepage(int pagenr)
{
    int x;
    flash.powerUp();

    for (x = 0; x < 256; x++)
        membuf[x] = rxframe[x + 3];

    flash.writePage(pagenr, membuf);
    flash.readPage(pagenr, data_buffer);
    flash.powerDown();

    for (int a = 0; a < 256; a++)
        if (data_buffer[a] != membuf[a])
            return;

    startframe(READY);
    sendframe();
}

static void flash_bulk_erase(void)
{
    flash.powerUp();
    flash.eraseChip();
    flash.powerDown();
    startframe(READY);
    addbyte(BULK_ERASE);
    sendframe();
}

void SendID(void)
{

    flash.powerUp();
    uint32_t JEDEC = flash.getJEDECID();
    flash.powerDown();
    startframe(READ_ID);
    addbyte(JEDEC >> 16);
    addbyte(JEDEC >> 8);
    addbyte(JEDEC >> 0);
    sendframe();
}

// ********************************************************************************
// FLASH READ 
// ********************************************************************************

//Reads a frame from Serial
static bool readSerialFrame(void) 
{
  Serial.setTimeout(50);
  
  if (!Serial)
  Serial.begin(230400);
  // Serial.begin(1000000);
  while (Serial.available()) {
    Serial.readBytesUntil(FEND,rxframe,512);
    //if (rxframe[0]!=FEND)
                return true;
  }
  return false;
}

static void readpage(uint16_t adr)
{

    bool sendempty = true;
    //delay(5);

    flash.readPage(adr, data_buffer);

    for (int a = 0; a < 256; a++)
    {
        if (data_buffer[a] != 0xff)
        {
            startframe(READ);
            addbyte(adr >> 8);
            addbyte(adr >> 0);
            for (int b = 0; b < 256; b++)
                addbyte(data_buffer[b]);
            sendframe();
            sendempty = false;
            break;
        }
    }
    if (sendempty)
    {
        startframe(EMPTY);
        addbyte(adr >> 8);
        addbyte(adr >> 0);
        sendframe();
    }
}

static void readAllPages(void)
{
    flash.powerUp();
    maxPage = 0x2000;
    delay(10);
    int p;
    for (p = 0; p < maxPage; p++)
        readpage(p);
    startframe(READY);
    sendframe();
    //
    flash.powerDown();
}

// ********************************************************************************

void iceprog_init()
{


}

void iceprog_flash()
{
    // put your main code here, to run repeatedly:

    if (readSerialFrame())
    {
        decodeFrame();
        if (rfcs == 0xff)
        {
            reset_low();

            switch (rxframe[0])
            {

            case FEND:
                break;

            case READ_ID:
                SendID();
                break;

            case BULK_ERASE:
                flash_bulk_erase();
                break;

            case SEC_ERASE:
                flash.powerUp();
                secerase((rxframe[1] << 8) | rxframe[2]);
                flash.powerDown();

                break;

            case READ:
                flash.powerUp();
                readpage((rxframe[1] << 8) | rxframe[2]);
                flash.powerDown();
                break;

            case READ_ALL:
                readAllPages();
                break;

            case PROG:
                writepage((rxframe[1] << 8) | rxframe[2]);
                break;

            default:
                break;

            } //switch
            reset_high();
        }
    }
}