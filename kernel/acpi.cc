
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/acpi.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
extern "C" {
#include "common.h"
#include "mach_dep.h"


static uint32_t *SMI_CMD;
static uint8_t ACPI_ENABLE;
static uint8_t ACPI_DISABLE;
static uint32_t *PM1a_CNT;
static uint32_t *PM1b_CNT;
static uint16_t SLP_TYPa;
static uint16_t SLP_TYPb;
static uint16_t SLP_EN;
static uint16_t SCI_EN;
static uint8_t PM1_CNT_LEN;

struct RSDPtr
{
   uint8_t Signature[8];
   uint8_t CheckSum;
   uint8_t OemID[6];
   uint8_t Revision;
   uint32_t *RsdtAddress;
};


struct FACP
{
   uint8_t Signature[4];
   uint32_t Length;
   uint8_t unneded1[40 - 8];
   uint32_t DSDT;
   uint8_t unneded2[48 - 44];
   uint32_t SMI_CMD;
   uint8_t ACPI_ENABLE;
   uint8_t ACPI_DISABLE;
   uint8_t unneded3[64 - 54];
   uint32_t PM1a_CNT_BLK;
   uint32_t PM1b_CNT_BLK;
   uint8_t unneded4[89 - 72];
   uint8_t PM1_CNT_LEN;
};



// check if the given address has a valid header
static unsigned int *acpiCheckRSDPtr(unsigned int *ptr)
{
   char *sig = "RSD PTR ";
   struct RSDPtr *rsdp = (struct RSDPtr *) ptr;
   uint8_t *bptr;
   uint8_t check = 0;
   int i;

   if (ut_memcmp(sig, (unsigned char *)rsdp, 8) == 0)
   {
      // check checksum rsdpd
      bptr = (uint8_t *) ptr;
      for (i=0; i<sizeof(struct RSDPtr); i++)
      {
         check += *bptr;
         bptr++;
      }

      // found valid rsdpd
      if (check == 0) {
#if 1
          if (rsdp->Revision == 0)
            ut_printf("		acpi 1\n");
         else
            ut_printf("		acpi 2\n");
#endif
    	  ut_printf("	found the acpi address: %x\n",rsdp);
         return (unsigned int *) rsdp->RsdtAddress;
      }
   }

   return NULL;
}



// finds the acpi header and returns the address of the rsdt
static  unsigned int *acpiGetRSDPtr(void)
{
   unsigned int *addr;
   unsigned int *rsdp;

   // search below the 1mb mark for RSDP signature
   for (addr = (unsigned int *) 0x400E0000; (int) addr<0x40100000; addr += 0x10/sizeof(addr)) // JANA changed
   {
      rsdp = acpiCheckRSDPtr(addr);
      if (rsdp != NULL)
         return rsdp;
   }


   // at address 0x40:0x0E is the RM segment of the ebda
   int ebda = *((short *) 0x40E);   // get pointer
   ebda = ebda*0x10 &0x000FFFFF;   // transform segment into linear address

   // search Extended BIOS Data Area for the Root System Description Pointer signature
   for (addr = (unsigned int *) ebda; (int) addr<ebda+1024; addr+= 0x10/sizeof(addr))
   {
      rsdp = acpiCheckRSDPtr(addr);
      if (rsdp != NULL)
         return rsdp;
   }

   return NULL;
}



// checks for a given header and validates checksum
static  int acpiCheckHeader(unsigned int *ptr, char *sig)
{
	unsigned char *p=(unsigned char *)ptr;
	if (p <KADDRSPACE_START){
		p=__va(p);
		ptr=(unsigned int *)p;
	}
   if (ut_memcmp((unsigned char *)ptr, sig, 4) == 0)
   {
      char *checkPtr = (char *) ptr;
      int len = *(ptr + 1);
      char check = 0;
      while (0<len--)
      {
         check += *checkPtr;
         checkPtr++;
      }
      if (check == 0)
         return 0;
   }
   return -1;
}



static  int acpiEnable(void)
{
   // check if acpi is enabled
   if ( (inw((unsigned int) PM1a_CNT) &SCI_EN) == 0 )
   {
      // check if acpi can be enabled
      if (SMI_CMD != 0 && ACPI_ENABLE != 0)
      {
         outb((unsigned int) SMI_CMD, ACPI_ENABLE); // send acpi enable command
         // give 3 seconds time to enable acpi
         int i;
         for (i=0; i<300; i++ )
         {
            if ( (inw((unsigned int) PM1a_CNT) &SCI_EN) == 1 )
               break;
            sc_sleep(10);
         }
         if (PM1b_CNT != 0)
            for (; i<300; i++ )
            {
               if ( (inw((unsigned int) PM1b_CNT) &SCI_EN) == 1 )
                  break;
               sc_sleep(10);
            }
         if (i<300) {
            return 0;
         } else {
            ut_printf("couldn't enable acpi.\n");
            return -1;
         }
      } else {
         ut_printf("no known way to enable acpi.\n");
         return -1;
      }
   } else {
      //ut_printf("acpi was already enabled.\n");
      return 0;
   }
}



//
// uint8_tcode of the \_S5 object
// -----------------------------------------
//        | (optional) |    |    |    |
// NameOP | \          | _  | S  | 5  | _
// 08     | 5A         | 5F | 53 | 35 | 5F
//
// -----------------------------------------------------------------------------------------------------------
//           |           |              | ( SLP_TYPa   ) | ( SLP_TYPb   ) | ( Reserved   ) | (Reserved    )
// PackageOP | PkgLength | NumElements  | uint8_tprefix Num | uint8_tprefix Num | uint8_tprefix Num | uint8_tprefix Num
// 12        | 0A        | 04           | 0A         05  | 0A          05 | 0A         05  | 0A         05
//
//----this-structure-was-also-seen----------------------
// PackageOP | PkgLength | NumElements |
// 12        | 06        | 04          | 00 00 00 00
//
// (Pkglength bit 6-7 encode additional PkgLength uint8_ts [shouldn't be the case here])
//
int init_acpi(unsigned long unused_arg1)
{
	//return -1;
   unsigned int *ptr = acpiGetRSDPtr();

   ut_printf(" first : phy %x \n",ptr);
   unsigned char *p=__va(ptr);
   ptr=(unsigned int *)p;
#ifdef DEBUG
   ut_printf(" first :virt  %x \n",ptr);
#endif
   // check if address is correct  ( if acpi is available on this pc )
   if (ptr != NULL && acpiCheckHeader(ptr, "RSDT") == 0)
   {

      // the RSDT contains an unknown number of pointers to acpi tables
      int entrys = *(ptr + 1);
      entrys = (entrys-36) /4;
      ptr += 36/4;   // skip header information

      while (0<entrys--)
      {
         // check if the desired table is reached
         if (acpiCheckHeader((unsigned int *) *ptr, "FACP") == 0)
         {
            entrys = -2;
            ut_printf("   Found FACP: %x\n",ptr);
            struct FACP *facp = (struct FACP *) __va(*ptr);
            if (acpiCheckHeader((unsigned int *) __va(facp->DSDT), "DSDT") == 0)
            {
               // search the \_S5 package in the DSDT
               char *S5Addr = (char *)( facp->DSDT +36); // skip header
               ut_printf("   s5addr: %x\n",S5Addr);
               S5Addr=__va(S5Addr);
              // int dsdtLength = *(facp->DSDT+1) -36;
               int *p = __va(facp->DSDT+1);
               int dsdtLength = *(p)-36;
               int loop;
               ut_printf("   lenght: %x  addr:%x\n",dsdtLength,p);
               //while (0 < dsdtLength--)
               loop=0;
            	   while(loop<0x1400)
               {
                  if ( ut_memcmp(S5Addr, "_S5_", 4) == 0){
                	  ut_printf("	 found S5 in memcmp\n");
                     break;
                  }
                  S5Addr++;
                  loop++;
               }
               ut_printf("  	 after lenght: %x s5addr:%x\n",dsdtLength,S5Addr);
               // check if \_S5 was found
             //  if (dsdtLength > 0)
               if (1)
               {
                  // check for valid AML structure
            	   ut_printf("	  check  s5addr: %x\n",S5Addr);
                  if ( ( *(S5Addr-1) == 0x08 || ( *(S5Addr-2) == 0x08 && *(S5Addr-1) == '\\') ) && *(S5Addr+4) == 0x12 )
                  {
                     S5Addr += 5;
                     S5Addr += ((*S5Addr &0xC0)>>6) +2;   // calculate PkgLength size

                     if (*S5Addr == 0x0A)
                        S5Addr++;   // skip uint8_tprefix
                     SLP_TYPa = *(S5Addr)<<10;
                     S5Addr++;

                     if (*S5Addr == 0x0A)
                        S5Addr++;   // skip uint8_tprefix
                     SLP_TYPb = *(S5Addr)<<10;

                     SMI_CMD = facp->SMI_CMD;

                     ACPI_ENABLE = facp->ACPI_ENABLE;
                     ACPI_DISABLE = facp->ACPI_DISABLE;

                     PM1a_CNT = facp->PM1a_CNT_BLK;
                     PM1b_CNT = facp->PM1b_CNT_BLK;

                     PM1_CNT_LEN = facp->PM1_CNT_LEN;

                     SLP_EN = 1<<13;
                     SCI_EN = 1;
                     ut_log("	ACPI sucessfully Initialized\n");
                     return JSUCCESS;
                  } else {
                     ut_printf("\\_S5 parse error.\n");
                  }
               } else {
                  ut_printf("\\_S5 not present.\n");
               }
            } else {
               ut_printf("DSDT invalid.\n");
            }
         }
         ptr++;
      }
      ut_printf("no valid FACP present.\n");
   } else {
      ut_printf("no acpi.\n");
   }

   return JFAIL;
}


void acpi_shutdown(void)
{
   // SCI_EN is set to 1 if acpi shutdown is possible
   if (SCI_EN == 0)
      return;

   acpiEnable();

   ut_printf(" sending the shutdown command \n");

   // send the shutdown command
   outw((unsigned int) PM1a_CNT, SLP_TYPa | SLP_EN );
   if ( PM1b_CNT != 0 )
      outw((unsigned int) PM1b_CNT, SLP_TYPb | SLP_EN );

   ut_printf("acpi poweroff failed.\n");
}
}
