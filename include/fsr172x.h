// This is also in the fsr branch of the kernel
#define V4L2_PIX_FMT_FSR172X  v4l2_fourcc('F', 'S', 'R', '0') /* 12bit raw fsr172x */

// Set registers on the (OMAP3530-based) fsr172x to capture RAW12 data
void fsr_set_registers();
