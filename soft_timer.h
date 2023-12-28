#pragma once

#ifndef _SOFT_TIMER_LIB_
#define _SOFT_TIMER_LIB_

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <string.h>



#ifndef STIMER_DYNAMIC_SIZE

#define STIMER_STATIC_SIZE

#ifndef STIMER_MAX_SIZE
#define STIMER_MAX_SIZE 64
#endif // STIMER_MAX_SIZE
#pragma message "Default: static size = #STIMER_MAX_SIZE"
#endif // STIMER_DYNAMIC_SIZE

#ifndef _TRAZA_
#define USERLOG_STIMER(prio, fmt, args...)
#else // _TRAZA_ 

#include "userlog.h"
// #define TRAZA_STIMER
#ifndef TRAZA_STIMER
#define USERLOG_STIMER(prio, fmt, args...)

#else // TRAZA_STIMER

#ifndef STIMER_VERBOSE
#define STIMER_VERBOSE LOG_DEBUG
#endif // STIMER_VERBOSE

#define USERLOG_STIMER(prio, fmt, args...) \
    ({                                     \
        if (prio <= STIMER_VERBOSE)        \
        {                                  \
            char usrStr[255];              \
            memset(usrStr, '\0', 255);     \
            strcat(usrStr, __FILE__ " ");  \
            strcat(usrStr, __func__);      \
            strcat(usrStr, "() ");         \
            strcat(usrStr, fmt);           \
            USERLOG(prio, usrStr, ##args); \
        }                                  \
    })

#endif // TRAZA_STIMER
#endif //_TRAZA_

typedef uint32_t stimer_t;

typedef enum BOOLSTIMER
{
    falseST = 0,
    trueST = 1,
    FALSEST = falseST,
    TRUEST

} boolSTimer_t;
// alias of callback functions
// doble pointer para poder modificar
typedef void (*pHandler_t)(void *pArg, int size);

typedef struct
{
    stimer_t id;
    unsigned int periodTime;
    unsigned int currentTime;
    boolSTimer_t inhibitedCB;
    boolSTimer_t paused;
    int numRep;
    pHandler_t handler;
    boolSTimer_t autoRemove;
    void *pArg;
    int argSize;

} stimer_s;

///@brief Assigns handler to signal and sets trigger interval. Must be used once before any softTimer. 
///@param base_ms base_time*1ms for trigger interval
///@return 1 on success 0 on failure
int initSTimer(unsigned int base_ms);

/// @brief Disarm, delete timer and free array of timers
/// @return 1 on succes 0 on failure
int endSTimer();

/// @brief Create a timer and set params and handle function
/// @param id Id
/// @param period Period of time
/// @param repetitions Repetitions
/// @param handlerCBfunc Handler callback function will be triggered when the timer is over each repetition
/// @param pArgs Pointer to Arguments that will be passaed to callback function 
/// @param sizeArg Size in bytes of pArgs
/// @return 1 on success 0 on failure
int createSTimerCB(stimer_t *id, int period, int repetitions, pHandler_t handlerCBfunc, void *pArgs, int sizeArgs);

/// @brief Create a timer without a callback funcion and set params 
/// @param id Id
/// @param period Period of time
/// @param repetitions Repetitions
/// @return 1 on success 0 on failure
int createSTimer(stimer_t *id, int period, int repetitions);

/// @brief Set params and handle to the specific timer
/// @param id Id
/// @param period Period of time
/// @param repetitions Repetitions
/// @param handlerCBfunc Handler callback function will be triggered when the timer is over each repetition
/// @param pArgs Pointer to Arguments that will be passaed to callback function 
/// @param sizeArg Size in bytes of pArgs
/// @return 1 on success 0 on failure
int setSTimer(stimer_t *id, int period, int repetitions, pHandler_t handlerCBfunc, void *pArgs, int sizeArgs);

/// @brief Remove a timer
/// @param id Timer id stimer_t
void removeSTimer(stimer_t *id);

/// @brief Inhibid callbackfuncion
/// @param id idTimer
void inhibitSTimer(stimer_t *id);

/// @brief Re-activate callbackfuncion
/// @param id idTimer
void activateSTimer(stimer_t *id);

/// @brief Pause a timer
/// @param id idTimer
void pauseSTimer(stimer_t *id);

/// @brief Resume a timer
/// @param id idTimer
void resumeSTimer(stimer_t *id);

void printfSTimer(stimer_t *tId);
unsigned int getTime(stimer_t *id);
unsigned int getRepets(stimer_t *id);

unsigned int time_ms(uint32_t ms);

#endif // _SOFT_TIMER_LIB_