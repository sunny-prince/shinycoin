#ifndef HASHBLOCK_H
#define HASHBLOCK_H

#include <inttypes.h>

const uint32_t MAIN_SHINY_PADS = 16;
const uint32_t MAIN_SHINY_CHUNKS = 125829120;
const uint32_t MAIN_SHINY_ITERS = 8388608;

const uint32_t TEST_SHINY_PADS = 16;
const uint32_t TEST_SHINY_CHUNKS = 8388608;
const uint32_t TEST_SHINY_ITERS = 1048576;

extern uint32_t nShinyScratchpads;
extern uint32_t nShinyHashChunks;
extern uint32_t nShinyHashIterations;

#endif // HASHBLOCK_H
