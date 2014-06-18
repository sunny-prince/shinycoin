#ifndef RAMHOG_MT_H
#define RAMHOG_MT_H

#include "util.h"

#include <boost/asio/io_service.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

class CRamhogThreadPool
{
private:
    uint32_t N, C, I;
    int numSimultaneous, numWorkers;
    
    boost::asio::io_service workerService;
    boost::thread_group workerPool;
    boost::asio::io_service::work workerWork;
    
    boost::asio::io_service commandService;
    boost::thread_group commandPool;
    boost::asio::io_service::work commandWork;
    
    uint64_t ***scratchpads;
    
    CSemaphore waitPads;
    CCriticalSection cs_pickPad;
    bool *fPadUsed;
    
public:
    CRamhogThreadPool(uint32_t N, uint32_t C, uint32_t I,
                      int numSimultaneous, int numWorkers);
    ~CRamhogThreadPool();
    
    bool ramhog(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size,
                bool fForMiner);
};

void SetNeedCheckBlock(bool fNew);

#endif
