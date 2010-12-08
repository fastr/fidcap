#define FSR_DATA_SIZE 524288 // (128 cols * 2048 rows * 2 bytes)
int fsr_init();
int fsr_buffer_get();
void fsr_buffer_release();
