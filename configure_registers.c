#include <stdlib.h>
#include <stdio.h> // used for printf()
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>


#define ISP_BASE_REG_ADDR    ((void*)0x480BC000) // Must be mapped to a page boundry so we map starting from the ISP base
#define CCDC_BASE_REG_OFF    0x00000600
#define CCDC_REG_LENGTH      0x00000A00          // All the way up to the last register in FSR

// Contains the ISP registers offsets from the base register address
#define ISP_CTRL        (0x00000040)

// Contain the offsets from the Base Register Address
#define CCDC_PID         (CCDC_BASE_REG_OFF+0x00000000)
#define CCDC_PCR          (CCDC_BASE_REG_OFF+0x00000004)
#define CCDC_SYN_MODE       (CCDC_BASE_REG_OFF+0x00000008)
#define CCDC_HD_VD_WID       (CCDC_BASE_REG_OFF+0x0000000C)
#define CCDC_PIX_LINES      (CCDC_BASE_REG_OFF+0x00000010)
#define CCDC_HORZ_INFO      (CCDC_BASE_REG_OFF+0x00000014) 
#define CCDC_VERT_START      (CCDC_BASE_REG_OFF+0x00000018)
#define CCDC_VERT_LINES      (CCDC_BASE_REG_OFF+0x0000001C)
#define CCDC_CULLING        (CCDC_BASE_REG_OFF+0x00000020)
#define CCDC_HSIZE_OFF      (CCDC_BASE_REG_OFF+0x00000024)
#define CCDC_SDOFST        (CCDC_BASE_REG_OFF+0x00000028) 
#define CCDC_SDR_ADDR        (CCDC_BASE_REG_OFF+0x0000002C) 
#define CCDC_CLAMP        (CCDC_BASE_REG_OFF+0x00000030) 
#define CCDC_DCSUB        (CCDC_BASE_REG_OFF+0x00000034) 
#define CCDC_COLPTN        (CCDC_BASE_REG_OFF+0x00000038)
#define CCDC_BLKCMP        (CCDC_BASE_REG_OFF+0x0000003C)
#define CCDC_FPC          (CCDC_BASE_REG_OFF+0x00000040) 
#define CCDC_FPC_ADDR        (CCDC_BASE_REG_OFF+0x00000044)
#define CCDC_VDINT        (CCDC_BASE_REG_OFF+0x00000048)
#define CCDC_REC656IF      (CCDC_BASE_REG_OFF+0x00000050)
#define CCDC_CFG         (CCDC_BASE_REG_OFF+0x00000054)
#define CCDC_FMTCFG        (CCDC_BASE_REG_OFF+0x00000058)

static int RW_DDR(void *base, unsigned long length, int offset, int dataSize, int flag, unsigned char *value);

/******************************************************************************
 * RW_DDR : Read/Write data from memory (registers)
 ******************************************************************************/
/* base     : start of address to be mapped
   length   : length of address to be mapped
   offset   : start of address to read/write
   dataSize : size of data to read/write
   flag     : 0 (write) or 1 (read)
   value    : value to write (only) to register, ignore for read
*/
int RW_DDR(void *base, unsigned long length, int offset, int dataSize, int flag, unsigned char *value)
{

  int fd;
  int k;
  unsigned char *src, *src1;

  // open memory device
  if ((fd=open("/dev/mem", O_RDWR|O_SYNC))==-1)
  {
    printf("\n\n Failure to open /dev/mem \n\n");
    exit(1);
  }

  // map memory location to src
  printf("base = %p, length = %lu, file = %d\n",base, length, fd);
  src = (unsigned char*)mmap((unsigned char *)base, length, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, fd, (off_t)base);

  printf ("\n \t FSR is mapped to 0x%08x \n", (unsigned int)src);

  if (src != (unsigned char *)base)
  {
    printf("\n \t Failure to map memory error equals %s\n",strerror(errno));
    exit(1);
  }

  // map memory offset locations
  src1 = src + offset;

  printf("\n \t Register is mapped to 0x%08x \n", (unsigned int)src1);

  for (k=0; k < dataSize; k++)
  {
    if(flag == 0)
    {
      // flag = 0; write data (value) to memory
      *(src1 + k) = value[k];
      printf(" \n \t Register location : 0x%08x Value : 0x%02x", (unsigned int)(src1 + k), *(src1 + k) );
    }
    else if(flag == 1)
    {
      // flag = 1; read data from memory
      printf(" \n \t Register location : 0x%08x Value : 0x%02x", (unsigned int)(src1 + k), *(src1 + k) );
    }
  }
  
  printf("\n\n");
  
  // close the memory map
  munmap(src, length);

  // close the file desciptor
  close(fd);

  return 0;
}
/**
*  
*  @brief Goes through and configures all the FSR registers to their proper settings.
*
*/
void ConfigureAllRegisters()
{
  int fd;
  int k;
  unsigned char *src;
  unsigned int *word_register;
  unsigned int register_value;
  unsigned char value[4];

  // open memory device
  if ((fd=open("/dev/mem", O_RDWR|O_SYNC))==-1)
  {
    printf("\n\n Failure to open /dev/mem \n\n");
    exit(1);
  }

  src = (unsigned char*)mmap((unsigned char *)ISP_BASE_REG_ADDR, CCDC_REG_LENGTH, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, fd, (off_t)ISP_BASE_REG_ADDR);
  
  if (src != ISP_BASE_REG_ADDR)
  {
    printf("\n \t Failure to map memory error equals %s\n", strerror(errno));
    exit(1);
  }


  /////////////////////////////////////////////
  // ISP_CTRL  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+ISP_CTRL);
  register_value = 0x00C12900;
  *word_register = register_value;
  

  /////////////////////////////////////////////
  // CCDC_SYNC_MODE  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_SYN_MODE);
  register_value = 0x00030401;
  *word_register = register_value;
  //printf("My reading = 0x%08X\n",*word_register);
  

  /////////////////////////////////////////////
  // CCDC_PIX_LINES  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_PIX_LINES);
  register_value = 0x00807D01;
  *word_register = register_value;


  /////////////////////////////////////////////
  // CCDC_HORZ_INFO  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_HORZ_INFO);
  register_value = 0x0001007F;
  *word_register = register_value;


  /////////////////////////////////////////////
  // CCDC_VERT_START  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_VERT_START);
  register_value = 0x00010001;
  *word_register = register_value;


  /////////////////////////////////////////////
  // CCDC_HSIZE_OFF  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_HSIZE_OFF);
  register_value = 0x00000100;
  *word_register = register_value;
  

  /////////////////////////////////////////////
  // CCDC_VDINT  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_VDINT);
  register_value = 0x00011F41;
  *word_register = register_value;
  

  /////////////////////////////////////////////
  // CCDC_FMTCFG  
  /////////////////////////////////////////////
  word_register = (unsigned int *)(src+CCDC_FMTCFG);
  register_value = 0x00000000;
  *word_register = register_value;
  
    
  // close the memory map
  munmap(src, CCDC_REG_LENGTH);
}
