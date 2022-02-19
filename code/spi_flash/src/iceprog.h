#ifndef ICEPROG_H
#define ICEPROG_H

#define LED 17
#define CDONE 3
#define RESET 2
#define UEXT_POWER 8
#define CS  13

//commands list
#define READ_ID   0x9F
#define PWR_UP    0xAB
#define PWR_DOWN  0xB9
#define WR_EN   0x06
#define BULK_ERASE  0xC7
#define SEC_ERASE 0xd8
#define PROG    0x02
#define READ    0x03
#define READ_ALL    0x83
#define CMD_ERROR 0xee
#define READY   0x44
#define EMPTY    0x45

#define FEND  0xc0
#define FESC  0xdb
#define TFEND   0xdc
#define TFESC  0xdd


void iceprog_init();
void iceprog_flash();


#endif // ICEPROG_H