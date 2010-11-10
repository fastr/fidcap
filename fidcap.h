#define FSR_DATA_SIZE 4096000 // (128 cols * 16000 rows * 2 bytes)

int fsr_init();
int fsr_buffer_get();
void fsr_buffer_release();
