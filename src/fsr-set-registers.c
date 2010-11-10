#include <stdlib.h> // exit
#include <stdio.h> // printf
#include <errno.h> // errno
#include <string.h> // strerror
#include <fcntl.h> // open
#include <sys/mman.h> // mmap

// Must be mmap'ed to a page boundry use this rather than starting at CCDC_BASE_REG
#define ISP_BASE_REG_ADDR ((void*) 0x480BC000)
#define CCDC_BASE_REG_OFF 0x00000600
#define CCDC_REG_LENGTH   0x00000A00

// Contains the ISP registers offsets from the base register address
#define ISP_CTRL        (0x00000040)

// Contains the offsets from the Base Register Address
//
// Note: Although all register offsets are listed here,
//       it is only necessary to uncomment those offsets
//       being used in the fsr_set_registers() function.
//
// #define CCDC_PID         (CCDC_BASE_REG_OFF+0x00000000)
// #define CCDC_PCR          (CCDC_BASE_REG_OFF+0x00000004)
#define CCDC_SYN_MODE       (CCDC_BASE_REG_OFF+0x00000008)
// #define CCDC_HD_VD_WID       (CCDC_BASE_REG_OFF+0x0000000C)
#define CCDC_PIX_LINES      (CCDC_BASE_REG_OFF+0x00000010)
#define CCDC_HORZ_INFO      (CCDC_BASE_REG_OFF+0x00000014) 
#define CCDC_VERT_START      (CCDC_BASE_REG_OFF+0x00000018)
#define CCDC_VERT_LINES      (CCDC_BASE_REG_OFF+0x0000001C)
// #define CCDC_CULLING        (CCDC_BASE_REG_OFF+0x00000020)
#define CCDC_HSIZE_OFF      (CCDC_BASE_REG_OFF+0x00000024)
// #define CCDC_SDOFST        (CCDC_BASE_REG_OFF+0x00000028) 
// #define CCDC_SDR_ADDR        (CCDC_BASE_REG_OFF+0x0000002C) 
// #define CCDC_CLAMP        (CCDC_BASE_REG_OFF+0x00000030) 
// #define CCDC_DCSUB        (CCDC_BASE_REG_OFF+0x00000034) 
// #define CCDC_COLPTN        (CCDC_BASE_REG_OFF+0x00000038)
// #define CCDC_BLKCMP        (CCDC_BASE_REG_OFF+0x0000003C)
// #define CCDC_FPC          (CCDC_BASE_REG_OFF+0x00000040) 
// #define CCDC_FPC_ADDR        (CCDC_BASE_REG_OFF+0x00000044)
#define CCDC_VDINT        (CCDC_BASE_REG_OFF+0x00000048)
// #define CCDC_REC656IF      (CCDC_BASE_REG_OFF+0x00000050)
// #define CCDC_CFG         (CCDC_BASE_REG_OFF+0x00000054)
#define CCDC_FMTCFG        (CCDC_BASE_REG_OFF+0x00000058)

/**
*  
*  @brief Goes through and configures all the ISP/CCDC registers to the proper FSR settings.
*
*/
void fsr_set_registers()
{
  int fd;
  unsigned char* src;
  unsigned int* word_register;
  unsigned int register_value;

  // open memory device
  if ((fd=open("/dev/mem", O_RDWR|O_SYNC))==-1)
  {
    printf("\n\n Failure to open /dev/mem \n\n");
    exit(1);
  }

  src = (unsigned char*) mmap(
    (unsigned char*) ISP_BASE_REG_ADDR,
    CCDC_REG_LENGTH,
    PROT_READ | PROT_WRITE | PROT_EXEC, 
    MAP_SHARED, 
    fd, 
    (off_t) ISP_BASE_REG_ADDR
  );
  
  if (ISP_BASE_REG_ADDR != src)
  {
    perror("mmap isp_base_reg");
    exit(EXIT_FAILURE);
  }


  // ISP_CTRL  
  word_register = (unsigned int *)(src+ISP_CTRL);
  register_value = 0x00C12900;
  *word_register = register_value;


  // CCDC_SYNC_MODE  
  word_register = (unsigned int *)(src+CCDC_SYN_MODE);
  register_value = 0x00030401;
  *word_register = register_value;


  // CCDC_PIX_LINES  
  word_register = (unsigned int *)(src+CCDC_PIX_LINES);
  register_value = 0x00807D01;
  *word_register = register_value;


  // CCDC_HORZ_INFO  
  word_register = (unsigned int *)(src+CCDC_HORZ_INFO);
  register_value = 0x0001007F;
  *word_register = register_value;


  // CCDC_VERT_START  
  word_register = (unsigned int *)(src+CCDC_VERT_START);
  register_value = 0x00010001;
  *word_register = register_value;


  // CCDC_HSIZE_OFF  
  word_register = (unsigned int *)(src+CCDC_HSIZE_OFF);
  register_value = 0x00000100;
  *word_register = register_value;
  

  // CCDC_VDINT  
  word_register = (unsigned int *)(src+CCDC_VDINT);
  register_value = 0x00011F41;
  *word_register = register_value;
  

  // CCDC_FMTCFG  
  word_register = (unsigned int *)(src+CCDC_FMTCFG);
  register_value = 0x00000000;
  *word_register = register_value;
  

  // CCDC_VERT_LINES  
  word_register = (unsigned int *)(src+CCDC_VERT_LINES);
  register_value = 0x000037FF;
  *word_register = register_value;

  // Previously used for the DaVinci-based FSR 

  // FMT_HORZ  
  // word_register = (unsigned int *)(src+FMT_HORZ);
  // register_value = 0x00010080;
  // *word_register = register_value;


  // FMT_VERT  
  // word_register = (unsigned int *)(src+FMT_VERT);
  // register_value = 0x00011E80;
  // *word_register = register_value;

    
  // close the memory map
  munmap(src, CCDC_REG_LENGTH);
}
