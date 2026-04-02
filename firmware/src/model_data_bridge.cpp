#include <stdint.h>

#include "../model/model_data.cc"

// Return pointer to 4-byte aligned TFLite flatbuffer
const unsigned char* getModelData() {
    return MODEL_DATA;
}

unsigned int getModelDataLen() {
    return MODEL_DATA_LEN;
}
