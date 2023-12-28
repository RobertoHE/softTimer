#include "soft_timer.h"

timer_t _timerIdSTimer;
struct itimerspec _timerConfig;
#if defined(STIMER_STATIC_SIZE)
stimer_s _arraySTimer[STIMER_MAX_SIZE];
#elif defined(STIMER_DYNAMIC_SIZE)
stimer_s *_arraySTimer;
#else
#error NOT DIFINED STIMER_DYNAMIC_SIZE NEITHER STIMER_STATIC_SIZE
#endif


// Private Variables
volatile uint32_t _lengthSTimer = 0;
volatile stimer_t _counterIdSTimer = 0;

uint8_t _signalRTSTimer;
boolSTimer_t _configuredSTimer = FALSEST;
boolSTimer_t _configuredSigRT = FALSEST;
volatile boolSTimer_t _criticPause = FALSEST;
volatile boolSTimer_t _handleSTRunning = FALSEST;

uint32_t SOFT_TIMER_BASETIME_MS=1;

// Private
stimer_t _addSTimer(stimer_t *timerId);
inline int _checkNullSTimer();
inline int _checkIdSTimer(stimer_t *id);
int _refeshElapseTimeSTimer(unsigned int posi);
void _handleSTimer();
void _setHandlerSTimer(int posi, pHandler_t handlerCBfunc);
void _autoRemoveSTimer(uint64_t posi);
void _pauseInteruptTimer();
void _resumeInteruptTimer();

// Public functions
int initSTimer(unsigned int base_ms)
{
    USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    _criticPause = TRUEST;

    // Se asocia manejador a senal
    // comprobar que SIGRTMIN+N esta libre.
    // Asi no sobreescribimos la funcion handler que hubiera
    if (!_configuredSigRT)
    {
        _signalRTSTimer = SIGRTMIN;
        struct sigaction saPrev;
        saPrev.sa_flags = SA_SIGINFO | SA_RESTART;
        saPrev.sa_sigaction = NULL;
        do
        {
            sigemptyset(&saPrev.sa_mask);
            if (sigaction(_signalRTSTimer, NULL, &saPrev) == -1)
            {
                USERLOG_STIMER(LOG_ERR, "%s()Error sigaction 1\n\r");
                return 0;
            }

            // comprobar que no hay handle ya asignado
            if (saPrev.sa_sigaction != NULL)
            {
                if (sigaction(_signalRTSTimer++, &saPrev, NULL) == -1)
                {
                    USERLOG_STIMER(LOG_ERR, "Error sigaction 2\n\r");
                    return 0;
                }

                // errExit("sigaction2");
                if (_signalRTSTimer > SIGRTMAX)
                {
                    USERLOG_STIMER(LOG_ERR, "Error al iniciar el timer \n\r");
                    return 0;
                }
            }
            else
            {
                break;
            }
        } while (_signalRTSTimer <= SIGRTMAX);

        USERLOG_STIMER(LOG_DEBUG, "\tSTIMER SIGNAL RT: %u\n\r", _signalRTSTimer);
        struct sigaction saTimer;
        saTimer.sa_flags = SA_SIGINFO;
        saTimer.sa_sigaction = _handleSTimer;
        sigemptyset(&saTimer.sa_mask);
        if (sigaction(_signalRTSTimer, &saTimer, NULL) == -1)
        {
            USERLOG_STIMER(LOG_ERR, "Error sigaction 3\n\r");
            return 0;
        }
        _configuredSigRT = TRUEST;
    }

    // errExit("sigaction3");

    // Evento
    struct sigevent evento;
    evento.sigev_signo = _signalRTSTimer;
    evento.sigev_notify = SIGEV_SIGNAL;

    // Creamos temporizador
    timer_create(1, &evento, &_timerIdSTimer);
    USERLOG_STIMER(LOG_DEBUG, "\tTIMER CREATED ID: %u\n\r", _timerIdSTimer);

    SOFT_TIMER_BASETIME_MS = base_ms;
    if (base_ms <= 0)
        base_ms = 1;

    uint32_t base_s = 0;
    while (base_ms >= 1000L)
    {
        base_s++;
        base_ms -= 1000L;
    }

    // configuramos intervalo del timer en ms
    struct itimerspec tempo_spec;
    tempo_spec.it_value.tv_sec = base_s;
    tempo_spec.it_value.tv_nsec = base_ms * 1000000L;
    tempo_spec.it_interval.tv_sec = tempo_spec.it_value.tv_sec;
    tempo_spec.it_interval.tv_nsec = tempo_spec.it_value.tv_nsec;

    // Se inicia el Timer
    timer_settime(_timerIdSTimer, 0, &tempo_spec, NULL);
    timer_gettime(_timerIdSTimer, &_timerConfig);
    //_criticPause = FALSEST;
    _configuredSTimer = TRUEST;
    _lengthSTimer = 0;

    USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
    return 1;
}

int endSTimer()
{
    USERLOG_STIMER(LOG_NOTICE, "begin \n\r");
    // desarma el el timer
    timer_delete(_timerIdSTimer);
#ifdef STIMER_DYNAMIC_SIZE
    if (_checkNullSTimer() == 0)
    {
        free(_arraySTimer);
        _arraySTimer = NULL;
    }
#endif
    _lengthSTimer = 0;
    _configuredSTimer = FALSEST;
    _criticPause = FALSEST;
    USERLOG_STIMER(LOG_NOTICE, "end success \n\r");
    return 1;
}

void printfSTimer(stimer_t *tId)
{
#if (STIMER_VERBOSE >= LOG_INFO)
    uint16_t tout = 0U;
    tout = ~tout;
    while (_criticPause == TRUEST && tout)
    {
        if (tout--)
        {
            USERLOG_STIMER(LOG_ERR, "NOT available due critic running.\n");
            return;
        }
    }
    int pos = -1;
    pos = _checkIdSTimer(tId);
    if (pos == -1)
    {
        USERLOG_STIMER(LOG_ERR, "STIMER No existe\n");
        return;
    }
    USERLOG_STIMER(LOG_DEBUG, "ID: %d, Pos: %d /, getTime: %d Nrep: %d\n",
                   (stimer_t)*tId,
                   pos,
                   getTime(tId),
                   getRepets(tId));
#endif
}

/// In case id is not found we add one new item to the array and return the id
int setSTimer(stimer_t *id, int period, int repetitions, pHandler_t handlerCBfunc, void *pArgs, int sizeArgs)
{

    uint8_t flg = 1;
    while (_criticPause && _lengthSTimer != 0)
    {

        if (flg--)
        {
            USERLOG_STIMER(LOG_NOTICE, " begin ERROR critic. Paused\n\r");
        }
        // return;
    }

    _pauseInteruptTimer();

    USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    int16_t posi = -1;
    // Si no se encuentra id lo agregamos y actualizamos el valor de id
    if ((posi = _checkIdSTimer(id)) == -1)
    {
        if (_addSTimer(id) == 0)
        {
            USERLOG_STIMER(LOG_ERR, "Error - Addtimer \n\r");
            _resumeInteruptTimer();
            return 0;
        }
        if ((posi = _checkIdSTimer(id)) == -1)
        {
            USERLOG_STIMER(LOG_ERR, "Error - CheckID \n\r");
            _resumeInteruptTimer();
            return 0;
        }
    }

    USERLOG_STIMER(LOG_DEBUG, "\tID: %d POSICION: %d \n\r", (int)*id, posi);
    _arraySTimer[posi].inhibitedCB = TRUEST;
    _arraySTimer[posi].paused = FALSEST;
    _arraySTimer[posi].periodTime = time_ms(period);
    _arraySTimer[posi].currentTime = time_ms(period);
    _arraySTimer[posi].numRep = repetitions;
    _arraySTimer[posi].handler = handlerCBfunc;
    _arraySTimer[posi].pArg = pArgs;
    _arraySTimer[posi].argSize = sizeArgs;

    // printfSTimer(id);
    //  printfSTimer(_arraySTimer[posi].id);
    _arraySTimer[posi].inhibitedCB = FALSEST;
    USERLOG_STIMER(LOG_DEBUG, "end success \n\r");

    _resumeInteruptTimer();
    return 1;
}

#warning createSTimer de tipo enum ERROR o SUCCESS
int createSTimer(stimer_t *id, int period, int repetitions)
{
    return setSTimer(id, period, repetitions, NULL, NULL, 0);
}

int createSTimerCB(stimer_t *id, int period, int repetitions, pHandler_t handlerCBfunc, void *pArgs, int sizeArgs)
{
    return setSTimer(id, period, repetitions, handlerCBfunc, pArgs, sizeArgs);
}

void removeSTimer(stimer_t *id)
{

    USERLOG_STIMER(LOG_DEBUG, "begin id: %d \n\r", *id);

    inhibitSTimer(id);
    pauseSTimer(id);

    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
    {
        USERLOG_STIMER(LOG_DEBUG, "\tNot found.\n\r");
        //_resumeInteruptTimer();
        return;
    }

    *id = 0;
    _autoRemoveSTimer(posi);

    USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
    _resumeInteruptTimer();
    return;
}


void inhibitSTimer(stimer_t *id)
{

    // USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return;
    USERLOG_STIMER(LOG_DEBUG, "\tinhibitSTimer id: %d\n\r", *id);
    _arraySTimer[posi].inhibitedCB = TRUEST;
    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
}

void activateSTimer(stimer_t *id)
{
    // USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return;
    USERLOG_STIMER(LOG_DEBUG, "\tactivateSTimer id: %d\n\r", id);
    _arraySTimer[posi].inhibitedCB = FALSEST;

    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
}

void pauseSTimer(stimer_t *id)
{
    // USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return;
    USERLOG_STIMER(LOG_DEBUG, "\tpausedSTimer id: %d\n\r", *id);
    _arraySTimer[posi].paused = TRUEST;
    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
}

void resumeSTimer(stimer_t *id)
{

    // USERLOG_STIMER(LOG_DEBUG, "begin\n\r");
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return;
    USERLOG_STIMER(LOG_DEBUG, "\tresumeSTimer id: %d\n\r", *id);
    _arraySTimer[posi].paused = FALSEST;
    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
}


inline unsigned int getTime(stimer_t *id)
{
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return 0;

    return _arraySTimer[posi].currentTime;
}

inline unsigned int getRepets(stimer_t *id)
{
    int posi;
    if ((posi = _checkIdSTimer(id)) == -1)
        return 0;
    return _arraySTimer[posi].numRep;
}

unsigned int time_ms(uint32_t ms){
    if(ms%SOFT_TIMER_BASETIME_MS >= (float)SOFT_TIMER_BASETIME_MS/2.0)
        return (ms/SOFT_TIMER_BASETIME_MS + 1);
    return (ms/SOFT_TIMER_BASETIME_MS);
}

// Private

/// @brief check if array of timers is null
/// @return 1 if null 0 if not null
inline int _checkNullSTimer()
{
#ifdef STIMER_DYNAMIC_SIZE
    return (_arraySTimer == NULL);
#else
    return 0;
#endif
}

/// @brief Retrieves the position of the timer id
/// @param id Timer id
/// @return Position inside the array on succes, -1 not found
inline int _checkIdSTimer(stimer_t *id)
{
    if (*id == 0 || id == NULL)
    {
        // USERLOG_STIMER(LOG_ERR, "id 0 o null \n\r");
        return -1;
    }

    if (_checkNullSTimer())
        return -1;

    int16_t i, posi = -1;
    for (i = 0; i < _lengthSTimer; i++)
    {

        if (*id == _arraySTimer[i].id)
        {
            posi = i;
            break;
        }
    }
    // USERLOG_STIMER(LOG_DEBUG, "id: %d length: %d \n\r", *id, _lengthSTimer);
    if (posi == -1)
    {
        *id = 0;
        USERLOG_STIMER(LOG_ERR, "Error ID not found\n\r");
    }
    return posi;
}

/// @brief Updates time passed, checks if timeout happens and the repetitions left
/// @param posi position of the timer inside the array
/// @return Time left to timeout
int _refeshElapseTimeSTimer(unsigned int posi)
{
    // USERLOG_STIMER(LOG_DEBUG, "begin. Posi:%d\n\r", posi);
    //  USERLOG_STIMER(LOG_DEBUG,"Refresh timer inicio ID: %d\n\r",_pArraySTimer[posi].id);
    if (_checkNullSTimer())
        return -1;

    // eliminar zombis

    if (posi >= _lengthSTimer)
    {
        USERLOG_STIMER(LOG_ERR, "posicion %d mayor que tamanio %d\n\r", posi, _lengthSTimer);
        return -1;
    }

    if (_arraySTimer[posi].id == 0)
    {
        if (FALSEST == _criticPause)
            _autoRemoveSTimer(posi);
        return -1;
    }

    // USERLOG_STIMER(LOG_INFO, "id 0 \n");

    if (_arraySTimer[posi].paused == TRUEST)
        return -3;

    // USERLOG_STIMER(LOG_DEBUG,"ID %d y pos: %u \n\r", _arraySTimer[posi].id, posi);
    // printfSTimer(_arraySTimer[posi].id);

    // no quedan repeticiones ni tiempo
    if (_arraySTimer[posi].numRep == 0 && _arraySTimer[posi].currentTime == 0)
    {
        // USERLOG_STIMER(LOG_DEBUG, "Refresh timer no repeticiones pos: %d ID: %d\n\r", posi, _arraySTimer[posi].id);
        if (FALSEST == _criticPause)
            _autoRemoveSTimer(posi);
        return 0;
    } // no 1 porque significa que es la ultima que no se ejecuto por inhibicion

    if (_arraySTimer[posi].numRep != 0 && _arraySTimer[posi].currentTime == 0)
    {
        _arraySTimer[posi].currentTime = _arraySTimer[posi].periodTime;
    }

    if (_arraySTimer[posi].currentTime > 0)
    {
        --_arraySTimer[posi].currentTime;
    }
    if (_arraySTimer[posi].currentTime != 0)
    {
        return _arraySTimer[posi].currentTime;
    }

    // ejectar CB y restar repeticiones

    // USERLOG_STIMER(LOG_INFO, "tiempo %d rep %d \n\r", _arraySTimer[posi].currentTime, _arraySTimer[posi].numRep);
    if (_criticPause == TRUEST)
    {
        // si hay hay usa critica, ponemos el tiempo a 1 para que se intente ejecutar en el siguiente ciclo
        return _arraySTimer[posi].currentTime++;
    }
    if (_arraySTimer[posi].handler != NULL)
    {
        _arraySTimer[posi].handler(_arraySTimer[posi].pArg, _arraySTimer[posi].argSize);
    }

    if (_arraySTimer[posi].numRep == -1)
    {
        //_arraySTimer[posi].currentTime = _arraySTimer[posi].periodTime;
        return 0;
    }

    --_arraySTimer[posi].numRep;

    USERLOG_STIMER(LOG_ERR, "reps restantes %d \n\r", _arraySTimer[posi].numRep);
    //_arraySTimer[posi].currentTime = _arraySTimer[posi].periodTime;
    return 0;

    // USERLOG_STIMER(LOG_DEBUG,"Refresh timer return currentTime %d , reps: %d \n\r", _arraySTimer[posi].currentTime, _arraySTimer[posi].numRep);
    // USERLOG_STIMER(LOG_DEBUG,"end success \n\r",__func__);
}

/// @brief Check every timer and executes handler of that timer on timeout
void _handleSTimer()
{

    if (_handleSTRunning == TRUEST)
    {
        return;
    }
    _handleSTRunning = TRUEST;

    if (_criticPause)
    {
        _handleSTRunning = FALSEST;
        return;
    }

    int32_t i = -1;

    while (++i < _lengthSTimer)
    {
        if (_criticPause == TRUEST)
        {
            continue;
        }
        _refeshElapseTimeSTimer(i);
    }
    _handleSTRunning = FALSEST;
    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");

    return;
}

void _autoRemoveSTimer(uint64_t posi)
{
    while (_criticPause)
    {
        uint8_t flg = 1;
        if (flg--)
        {
            USERLOG_STIMER(LOG_NOTICE, "begin ERROR critic. Paused\n\r");
        }
        // return;
    }
    _pauseInteruptTimer();

    // USERLOG_STIMER(LOG_DEBUG, "\t%s(), remove POSICION: %d \n\r", posi);
    _arraySTimer[posi].id = 0;
    _arraySTimer[posi].paused = TRUEST;
    _arraySTimer[posi].inhibitedCB = TRUEST;
    _arraySTimer[posi].periodTime = 0;
    _arraySTimer[posi].currentTime = 0;
    _arraySTimer[posi].numRep = 0;
    _arraySTimer[posi].handler = NULL;
    _arraySTimer[posi].pArg = NULL;
    _arraySTimer[posi].argSize = 0;

    uint64_t i = 0;
    for (i = posi; i < (_lengthSTimer - 1); ++i)
    {
        // USERLOG_STIMER(LOG_DEBUG, "\t%s()sustituye la posicion %d por la %d \n\r", i + 1, i);
        // USERLOG_STIMER(LOG_DEBUG, "\t\t%s()los valores eran ID \n\r");
        // printfSTimer(&_arraySTimer[i].id);
        memcpy(&_arraySTimer[i], &_arraySTimer[i + 1], sizeof(stimer_s));
        // USERLOG_STIMER(LOG_DEBUG, "\t\t%s()los valores nuevos son: \n\r");
        // printfSTimer(&_arraySTimer[i].id);
    }

#ifdef STIMER_DYNAMIC_SIZE

    // USERLOG_STIMER(LOG_DEBUG, "\tPREV _lengthSTimer: %d \n\r", _lengthSTimer);
    if (_lengthSTimer > 1)
    { // Actualizamos el tamaño alocado
        USERLOG_STIMER(LOG_DEBUG, "\trealloc to size: %d \n\r", _lengthSTimer - 1);
        _arraySTimer = (stimer_s *)realloc(_arraySTimer, sizeof(*_arraySTimer) * (_lengthSTimer - 1));
    }
    else if (_lengthSTimer <= 1)
    {
        if (_checkNullSTimer() == 0)
            // free(_arraySTimer);
            ;
    }

#endif // STIMER_DYNAMIC_SIZE
    if (_lengthSTimer > 0)
        --_lengthSTimer;

    // USERLOG_STIMER(LOG_DEBUG, "\t%s()_lengthSTimer: %d \n\r", _lengthSTimer);

    // free memory allocated to the array if we have 0 elements left
    if (_lengthSTimer == 0 || _checkNullSTimer())
    {
        // USERLOG_STIMER(LOG_DEBUG, "\t%s()realease empty array\n\r");
        endSTimer();
    }

    _resumeInteruptTimer();
    // USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
    return;
}


void _pauseInteruptTimer()
{
    if (!_configuredSTimer)
        return;
    _criticPause = TRUEST;

    // Guardamos la configuracion previa del timer
    timer_gettime(_timerIdSTimer, &_timerConfig);
    // configuramos intervalo del timer a 0 para deshabilitarlo
    struct itimerspec tempo_spec;
    tempo_spec.it_value.tv_sec = 0;
    tempo_spec.it_value.tv_nsec = 0;
    tempo_spec.it_interval.tv_sec = tempo_spec.it_value.tv_sec;
    tempo_spec.it_interval.tv_nsec = tempo_spec.it_value.tv_nsec;

    // Se desarma el Timer
    timer_settime(_timerIdSTimer, 0, &tempo_spec, NULL);
    return;
}

void _resumeInteruptTimer()
{
    if (!_configuredSTimer)
        return;

    // rearmamos el timer
    timer_settime(_timerIdSTimer, 0, &_timerConfig, NULL);
    _criticPause = FALSEST;
    return;
}


stimer_t _addSTimer(stimer_t *timerId)
{
    USERLOG_STIMER(LOG_DEBUG, "begin\n\r");

    if (_configuredSTimer == FALSEST)
    {
        if (initSTimer(SOFT_TIMER_BASETIME_MS) == 0)
        {
            USERLOG_STIMER(LOG_ERR, "Error init timer \n\r");
            //_criticPause = FALSEST;
            return 0;
        }
        USERLOG_STIMER(LOG_DEBUG, "\tInit timers success \n\r");
    }
#ifdef STIMER_STATIC_SIZE
    if (_lengthSTimer >= STIMER_MAX_SIZE)
    {
        USERLOG_STIMER(LOG_ERR, "Error MAX SIZE REARCH \n\r");
        return 0;
    }
#endif // STIMER_STATIC_SIZE
#ifdef STIMER_DYNAMIC_SIZE
    // Asignar un puntero al array dinamico si no esta asiganado previamente
    if (_checkNullSTimer() == 1)
    {
        USERLOG_STIMER(LOG_DEBUG, "\tmalloc \n\r");
        _arraySTimer = malloc(sizeof(*_arraySTimer));
    }
    else
    {
        USERLOG_STIMER(LOG_DEBUG, "\trealloc resize array \n\r");
        _arraySTimer = realloc(_arraySTimer, (_lengthSTimer + 1) * sizeof(*_arraySTimer));
    }

    // fallo al alojar memoria dinámica
    if (_checkNullSTimer() == 1)
    {
        //_criticPause = FALSEST;
        endSTimer();
        USERLOG_STIMER(LOG_ERR, "Error array null timer \n\r");
        return 0;
    }
#endif // STIMER_DYNAMIC_SIZE

    // overflow protecction
    if (++_counterIdSTimer == 0)
        _counterIdSTimer++;

    // duplication protection
    stimer_t it = 0;
    while (it < _lengthSTimer)
    {
        if (_counterIdSTimer == _arraySTimer[it].id)
        {
            _counterIdSTimer++;
            it = 0;
        }
        else
        {
            it++;
        }
    }

    // asignament
    _arraySTimer[_lengthSTimer].id = _counterIdSTimer;
    *timerId = (stimer_t)_arraySTimer[_lengthSTimer].id;
    _lengthSTimer++;

    //_criticPause = FALSEST;
    USERLOG_STIMER(LOG_INFO, "\tSTimer array new size: %d, element id added: %d id saved: %d \n\r",
                    _lengthSTimer, _arraySTimer[(_lengthSTimer - 1)].id, (unsigned long int)*timerId);
    USERLOG_STIMER(LOG_DEBUG, "end success \n\r");
    return *timerId;
}
