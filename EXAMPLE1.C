
// Example1.c -- Some ATA command examples

#if 0

   Some simple instructions for those of you that would like to
   compile my driver code and try it.

   1) I've always used Borland C (or C++, the driver code is plain C
   code).  One file contains a few lines of embedded ASM code. You will
   need Borland's TASM too.

   2) I've always used small memory model but I think the driver code
   will work with any memory mode. See the MEMMOD variable in the
   make file (EXAMPLE1.MAK).

   3) The EXAMPLE1.EXE program is a DOS real mode program. It will
   not execute in a DOS session (a virtual x86 session) under Windows!
   You should make a DOS boot floppy and boot your system for it and
   execute the EXAMPLE1.EXE program from that floppy.

   4) Here is a very small program that shows how to use the driver
   code to issue some ATA commands.
   This program has one required command line parameter to specify
   the device to run on: P0, P1, S0 or S1.

#endif

//---------- options to include LBA48 and/or PCI Bus Mastering DMA

#define INCL_LBA48      0        // set to 1 to include 48-bit LBA

#define INCL_PCI_DMA    1        // set to 1 to include PCI DMA

//---------- begin

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>

#include "ataio.h"

int pciBus  = -1;
int pciDev  = -1;
int pciFun  = -1;
int priSec  = -1;
int dev     = -1;
int irqNum  = -1;

unsigned int cmdBase  = 0xffff;
unsigned int ctrlBase = 0xffff;
unsigned int bmideBase = 0xffff;

unsigned char * devTypeStr[]
       = { "NO DEVICE", "UNKNOWN TYPE", "ATA", "ATAPI" };

#define BUFFER_SIZE 4096
unsigned char buffer[BUFFER_SIZE];
unsigned char far * bufferPtr;

/*********************************************************/

static void pause( void );

static void pause( void )

{

   // clear any queued up keys
   while ( kbhit() )
   {
      if ( getch() == 0 )
         getch();
   }
   // pause until key hit
   printf( "Press any key to continue...\n" );
   while ( ! kbhit() )
      /* do nothing */ ;
   // clear any queued up keys
   while ( kbhit() )
   {
      if ( getch() == 0 )
         getch();
   }
}

/*********************************************************/

static long get_dnumber( unsigned char * s );

static long get_dnumber( unsigned char * s )

{
   char * cp;
   long val;

   strupr( s );
   if ( strspn( s, "0123456789" ) != strlen( s ) )
      return -1L;
   val = strtol( s, & cp, 0 );
   if ( * cp != 0 )
      return -1L;
   return val;
}

/**********************************************************
**
** get a word of PCI config data from BIOS INT 1AH
**
**********************************************************/

unsigned int GetPciWord( unsigned int busNum, unsigned int devNum,
                         unsigned int funNum, unsigned int regNum );

unsigned int GetPciWord( unsigned int busNum, unsigned int devNum,
                         unsigned int funNum, unsigned int regNum )

{
   union REGS ir, or;

   if ( regNum & 0x0001 )
      return 0xffff;
   ir.x.ax = 0xb109;
   ir.x.bx = ( busNum << 8 ) | ( devNum << 3 ) | funNum;
   ir.x.di = regNum;
   int86( 0x1a, & ir, & or );
   if ( or.x.cflag )
      return 0xffff;
   return or.x.cx;
}

//**********************************************

// a little function to display all the error
// and trace information from the driver

void ShowAll( void );

void ShowAll( void )

{
   int lc = 0;
   unsigned char * cp;

   printf( "ERROR !\n" );

   // display the command error information
   trc_err_dump1();           // start
   while ( 1 )
   {
      cp = trc_err_dump2();   // get and display a line
      if ( cp == NULL )
         break;
      printf( "* %s\n", cp );
   }
   pause();

   // display the command history
   trc_cht_dump1();           // start
   while ( 1 )
   {
      cp = trc_cht_dump2();   // get and display a line
      if ( cp == NULL )
         break;
      printf( "* %s\n", cp );
      lc ++ ;
      if ( ! ( lc & 0x000f ) )
         pause();
   }

   // display the low level trace
   trc_llt_dump1();           // start
   while ( 1 )
   {
      cp = trc_llt_dump2();   // get and display a line
      if ( cp == NULL )
         break;
      printf( "* %s\n", cp );
      lc ++ ;
      if ( ! ( lc & 0x000f ) )
         pause();
   }

   if ( lc & 0x000f )
      pause();
}

//**********************************************

void ClearTrace( void );

void ClearTrace( void )

{

   // clear the command history and low level traces
   trc_cht_dump0();     // zero the command history
   trc_llt_dump0();     // zero the low level trace
}

//**********************************************

int main( int ac, char * av[] )
{
   int numDev;
   int rc;

   printf( "ATADRVR EXAMPLE1 Version " ATA_DRIVER_VERSION ".\n" );

   //---------- initialization and command line option processing

   printf( "Usage: EXAMPLE1 pciBus pciDev pciFun dev irqNum\n" );
                   // av[n]   1      2      3     4     5
   printf( "       If irqNum is zero it will be defaulted.\n" );

   if ( ac != 6 )
   {
      printf( "Missing or extra command line options !\n" );
      return 1;
   }

   pciBus = (int) get_dnumber( av[1] );
   pciDev = (int) get_dnumber( av[2] );
   pciFun = (int) get_dnumber( av[3] );
   if ( ! stricmp( av[4], "P0" ) )
      {  priSec = 0; dev = 0; }
   if ( ! stricmp( av[4], "P1" ) )
      {  priSec = 0; dev = 1; }
   if ( ! stricmp( av[4], "S0" ) )
      {  priSec = 1; dev = 0; }
   if ( ! stricmp( av[4], "S1" ) )
      {  priSec = 1; dev = 1; }
   irqNum = (int) get_dnumber( av[5] );

   printf( "Device %d %d %d %s%d, IRQ %d in not shared mode.\n",
            pciBus, pciDev, pciFun,
            priSec ? "S" : "P", dev,
            irqNum );

   if ( ( pciBus < 0 ) || ( pciDev < 0 ) || ( pciFun < 0 )
        || ( priSec < 0 ) || ( dev < 0 ) || ( irqNum < 0 ) )
   {
      printf( "Invalid command line option(s) !\n" );
      return 1;
   }

   cmdBase  = GetPciWord( pciBus, pciDev, pciFun, 0x10 + ( 8 * priSec ) );
   ctrlBase = GetPciWord( pciBus, pciDev, pciFun, 0x14 + ( 8 * priSec ) );
   bmideBase = GetPciWord( pciBus, pciDev, pciFun, 0x20 );
   if (    ( cmdBase == 0xffff )
        || ( ctrlBase == 0xffff )
        || ( bmideBase == 0xffff ) )
   {
      printf( "PCI information invalid !\n" );
      return 1;
   }
   cmdBase &= 0xfffe;
   ctrlBase &= 0xfffe;
   bmideBase &= 0xfffe;
   if ( cmdBase == 0 )
   {
      cmdBase  = priSec ? 0x170 : 0x1f0;
      ctrlBase = priSec ? 0x370 : 0x3f0;
      irqNum = priSec ? 15 : 14;
   }
   else
   {
      ctrlBase -= 4;
   }
   if ( priSec )
      bmideBase += 8;
   printf( "ATA CMD base %04X, ATA CTRL base %04X, BMIDE base %04X\n",
            cmdBase, ctrlBase, bmideBase );

   if ( ! irqNum )
   {
      irqNum = GetPciWord( pciBus, pciDev, pciFun, 0x3c );
      irqNum &= 0x00ff;
      printf( "IRQ defaulted to %d\n", irqNum );
   }

   //---------- initialization

   // initialize far pointer to the I/O buffer
   bufferPtr = (unsigned char far *) buffer;

   // tell ATADRVR how big the buffer is
   reg_buffer_size = BUFFER_SIZE;

   // set the ATADRVR command timeout (in seconds)
   tmr_time_out = 20;

   //---------- ATADRVR initialization

   // 1) must tell the driver what the I/O port addresses.
   //    note that the driver also supports all the PCMCIA
   //    PC Card modes (I/O and memory).
   pio_set_iobase_addr( cmdBase, ctrlBase, bmideBase );
   printf( "Using I/O base addresses %04X and %04X.\n", cmdBase, ctrlBase );

   // 2) find out what devices are present -- this is the step
   // many driver writers ignore.  You really can't just do
   // resets and commands without first knowing what is out there.
   // Even if you don't care the driver does care.
   numDev = reg_config();
   printf( "Found %d devices, dev 0 is %s, dev 1 is %s.\n",
                  numDev,
                  devTypeStr[ reg_config_info[0] ],
                  devTypeStr[ reg_config_info[1] ] );
   if ( numDev < 1 )
      ShowAll();
   pause();

   //---------- Try some commands in "polling" mode

   printf( "Polling mode...\n" );

   // do an ATA soft reset (SRST) and return the command block
   // regs for device 0 in struct reg_cmd_info
   ClearTrace();
   printf( "Soft Reset...\n" );
   rc = reg_reset( 0, dev );
   if ( rc )
      ShowAll();

   // do an ATA Identify command in LBA28 mode
   ClearTrace();
   printf( "ATA Identify, LBA28, polling...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba28(
               dev, CMD_IDENTIFY_DEVICE,
               0, 0,
               0L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               1L, 0 );
   if ( rc )
      ShowAll();
   else
   {
      // you get to add the code here to display all the ID data
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

   // do a seek command in LBA28 mode to LBA=5025
   ClearTrace();
   printf( "Seek, LBA28, polling...\n" );
   rc = reg_non_data_lba28( dev, CMD_SEEK, 0, 0, 5025L );
   if ( rc )
      ShowAll();
   else
      pause();

   // do an ATA Read Sectors command in LBA28 mode
   // lets read 3 sectors starting at LBA=5
   ClearTrace();
   printf( "ATA Read Sectors, LBA28, polling...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba28(
               dev, CMD_READ_SECTORS,
               0, 3,
               5L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               3L, 0
               );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

#if INCL_LBA48

   // do an ATA Read Sectors command in LBA48 mode
   // lets read 3 sectors starting at LBA=5
   ClearTrace();
   printf( "ATA Read Sectors, LBA48, polling...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba48(
               dev, CMD_READ_SECTORS_EXT,
               0, 3,
               0L, 5L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               3L, 0
               );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

#endif

   //---------- Switch to "interrupt" mode (required for PCI DMA)

   // First, you must enable the appropriate irq
   // HOWEVER, if call this function you MUST make
   // sure your program does not terminate without
   // calling int_disable() !!!

   printf( "Using IRQ %d in not shared mode...\n", irqNum );
   rc = int_enable_irq( 0, irqNum, bmideBase + 2, cmdBase + 7 );
   if ( rc )
   {
      printf( "Unable to set interrupt mode using IRQ %d !\n", irqNum );
      return 1;
   }

   //---------- Try some commands in "interrupt" mode

   // do an ATA Identify command in LBA28 mode
   ClearTrace();
   printf( "ATA Identify, LBA28, interrupt...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba28(
               dev, CMD_IDENTIFY_DEVICE,
               0, 0,
               0L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               1L, 0
               );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

   // do a seek command in LBA28 mode to LBA=5025
   ClearTrace();
   printf( "Seek, LBA28, interrupt...\n" );
   rc = reg_non_data_lba28( dev, CMD_SEEK, 0, 0, 5025L );
   if ( rc )
      ShowAll();
   else
      pause();

   // do an ATA Read Sectors command in LBA28 mode
   // lets read 3 sectors starting at LBA=5
   ClearTrace();
   printf( "ATA Read Sectors, LBA28, interrupt...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba28(
               dev, CMD_READ_SECTORS,
               0, 3,
               5L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               3L, 0
               );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

#if INCL_LBA48

   // do an ATA Read Sectors command in LBA48 mode
   // lets read 3 sectors starting at LBA=5
   ClearTrace();
   printf( "ATA Read Sectors, LBA48, interrupt...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = reg_pio_data_in_lba48(
               dev, CMD_READ_SECTORS_EXT,
               0, 3,
               0L, 5L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               3L, 0
               );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

#endif

#if INCL_PCI_DMA

   // Now do PCI DMA with interrupts.

   // first, tell the driver where the BMIDE registers are located.

   rc = dma_pci_config( bmideBase );
   if ( rc )
   {
      printf( "ERROR !  Call to dma_pci_config() failed,\n" );
      printf( "         dma_pci_config() returned %d!\n", rc );
      return 1;
   }

   reg_incompat_flags |= REG_INCOMPAT_DMA_POLL;

   // do an ATA Read DMA command in LBA28 mode
   // lets read 3 sectors starting at LBA=5
   ClearTrace();
   printf( "ATA Read DMA, LBA28, interrupt...\n" );
   memset( buffer, 0, sizeof( buffer ) );
   rc = dma_pci_lba28(
               dev, CMD_READ_DMA,
               0, 3,
               5L,
               FP_SEG( bufferPtr ), FP_OFF( bufferPtr ),
               3L );
   if ( rc )
      ShowAll();
   else
   {
      // display the first 16 bytes read
      printf( "   data read %02X%02X%02X%02X %02X%02X%02X%02X "
                           "%02X%02X%02X%02X %02X%02X%02X%02X\n",
               buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
               buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
               buffer[ 8], buffer[ 9], buffer[10], buffer[11],
               buffer[12], buffer[13], buffer[14], buffer[15] );
      pause();
   }

#endif

   // disable intrq -- you MUST do this if you
   // called int_enable_irq() !!!
   printf( "Interrupt off...\n" );
   int_disable_irq();

   return 0;
}

// end example1.c
