#include "ramhog_mt.h"
#include "ramhog.h"
#include "main.h"
#include "util.h"

#include <boost/bind.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>

CRamhogThreadPool::CRamhogThreadPool(uint32_t Nin, uint32_t Cin, uint32_t Iin,
                                     int numSimultaneousIn, int numWorkersIn) :
    N(Nin), C(Cin), I(Iin),
    numSimultaneous(numSimultaneousIn), numWorkers(numWorkersIn),
    workerWork(workerService), commandWork(commandService),
    waitPads(numSimultaneous)
{
    for (int i=0; i < numWorkers; i++)
    {
        workerPool.create_thread(boost::bind(&boost::asio::io_service::run, &workerService));
    }
    
    printf("Malloc()ing %dx%d scratchpads @ %.2fMB each = %.2fGB RAM...\n",
           N, numSimultaneous, C*8/1024.0/1024.0,
           N*1.0*numSimultaneous*C*8/1024.0/1024.0/1024.0);
    
    scratchpads = (uint64_t ***)malloc(sizeof(uint64_t **) * numSimultaneous);
    fPadUsed = (bool *)malloc(sizeof(bool) * numSimultaneous);
    
    for (int i=0; i < numSimultaneous; i++)
    {
        commandPool.create_thread(boost::bind(&boost::asio::io_service::run, &commandService));
        scratchpads[i] = (uint64_t **)malloc(sizeof(uint64_t *) * N);
        for (int j=0; j < N; j++)
        {
            scratchpads[i][j] = (uint64_t *)malloc(sizeof(uint64_t *) * C);
        }
        fPadUsed[i] = false;
    }
}

CRamhogThreadPool::~CRamhogThreadPool()
{
    for (int i=0; i < numSimultaneous; i++)
    {
        for (int j=0; j < N; j++)
        {
            delete scratchpads[i][j];
        }
        delete scratchpads[i];
    }
    delete scratchpads;
}

static void ramhog_gen_pad_mt(boost::promise<bool> &done,
                              const uint8_t *input, size_t input_size,
                              uint32_t C, uint32_t padIndex,
                              uint64_t *padOut,
                              bool fForMiner, CBlockIndex *pindexForBest)
{
    if (fForMiner && pindexForBest != pindexBest)
    {
        done.set_value(error("ramhog_gen_pad_mt(): Interrupted from new best block"));
        return;
    }
    
    ramhog_gen_pad(input, input_size, C, padIndex, padOut);
    done.set_value(true);
}

typedef struct ramhog_mt_args
{
    boost::asio::io_service &workerService;
    boost::promise<bool> &done;
    const uint8_t *input;
    size_t input_size;
    uint8_t *output;
    size_t output_size;
    uint32_t N, C, I;
    uint64_t **scratchpads;
    bool fForMiner;
    CBlockIndex *pindexForBest;
} ramhog_mt_args;

static void ramhog_mt(ramhog_mt_args &args)
{
    if (args.fForMiner && args.pindexForBest != pindexBest)
    {
        args.done.set_value(error("ramhog_mt(): Interrupted before pad gen from new best block"));
        return;
    }
    
    boost::promise<bool> pad_dones[1024];
    uint32_t padIndex;
    
    for (padIndex=0; padIndex < args.N; padIndex++)
    {
        args.workerService.post(boost::bind(ramhog_gen_pad_mt,
                                            boost::ref(pad_dones[padIndex]),
                                            args.input, args.input_size, args.C, padIndex,
                                            args.scratchpads[padIndex],
                                            args.fForMiner, args.pindexForBest));
    }
    
    bool fSuccess = true;
    for (padIndex=0; padIndex < args.N; padIndex++)
    {
        fSuccess &= pad_dones[padIndex].get_future().get();
    }
    
    if (!fSuccess)
    {
        args.done.set_value(error("ramhog_mt(): Interrupted after pad gen from new best block"));
        return;
    }
    
    ramhog_run_iterations(args.input, args.input_size, args.output, args.output_size,
                          args.N, args.C, args.I, args.scratchpads);
    
    args.done.set_value(true);
}

static bool fNeedCheckBlock = false;
static CCriticalSection cs_needCheckBlock;

void SetNeedCheckBlock(bool fNew)
{
    LOCK(cs_needCheckBlock);

    if (fDebug)
        printf("SetNeedCheckBlock(%s)\n", fNew ? "true" : "false");

    fNeedCheckBlock = fNew;
}

bool CRamhogThreadPool::ramhog(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size,
                               bool fForMiner)
{
    {
        LOCK(cs_needCheckBlock);
        if (fNeedCheckBlock && fForMiner)
            return false;
    }

    CBlockIndex *pindexForBest = pindexBest;

    waitPads.wait();
    
    if (fForMiner && pindexForBest != pindexBest)
    {
        waitPads.post();
        return error("CRamhogThreadPool::ramhog(): Interrupted from new best block");
    }
    
    int whichPad = -1;
    {
        LOCK(cs_pickPad);
        
        int i;
        for (i=0; i < numSimultaneous; i++)
        {
            if (!fPadUsed[i])
            {
                whichPad = i;
                fPadUsed[whichPad] = true;
                break;
            }
        }
        if (whichPad == -1)
        {
            waitPads.post();
            throw new std::runtime_error("waitPads/fPadUsed mismatch");
        }
    }
    
    boost::promise<bool> ramhog_done;
    
    ramhog_mt_args args = {
        workerService, ramhog_done,
        input, input_size, output, output_size,
        N, C, I, scratchpads[whichPad],
        fForMiner, pindexForBest};
    
    commandService.post(boost::bind(ramhog_mt, boost::ref(args)));
    
    bool fSuccess = ramhog_done.get_future().get();
    
    {
        LOCK(cs_pickPad);
        fPadUsed[whichPad] = false;
    }
    
    waitPads.post();
    
    return fSuccess;
}
















