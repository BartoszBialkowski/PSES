#ifndef LINSM_H
#define LINSM_H

#include "Std_Types.h"

/* Identyfikatory API */
#define INSTANCE_ID                                     0
#define MODULE_ID_LINSM                                 0x35
#define LINSM_INIT_SID                                  0x01
#define LINSM_SCHEDULEREQUEST_SID                       0x10
#define LINSM_GETVERSIONINFO_SID                        0x02
#define LINSM_GETCURRENTCOMMODE_SID                     0x11
#define LINSM_REQUESTCOMMODE_SID                        0x12
#define LINSM_MAIN_FUNCTION_API_ID                      0x30
#define LINSM_SCHEDULEREQUESTCONFIRMATION_API_ID        0x20
#define LINSM_GOTOSLEEPCONFIRMATION_API_ID              0x22
#define LINSM_WAKEUPCONFIRMATION_API_ID                 0x21

#define NULL_SCHEDULE 			 		0x00

/* [SWS_LinSM_00053] Kody błędów */
#define LINSM_E_UNINIT           		0x00  /* Moduł nie został zainicjalizowany */
#define LINSM_E_NONEXISTENT_NETWORK 	0x20
#define LINSM_E_PARAM_POINTER    		0x40  /* Wskaźnik na parametr jest NULL */
#define LINSM_E_PARAMETER        		0x30  /* Nieprawidłowy parametr */
#define LINSM_E_INIT_FAILED     		0x50  /* Żądanie trybu jest nieprawidłowe */

//[SWS_LinSM_00224] Runtime error
#define LINSM_E_CONFIRMATION_TIMEOUT	0x00

/* Maksymalna liczba kanałów i harmonogramów */
#define LINSM_MAX_CHANNELS                              1
#define NUM_SCHEDULES                                   2

// Vendor ID
#define LINSM_VENDOR_ID                 1234    // Przykładowy ID dostawcy

// Module ID
#define LINSM_MODULE_ID                 5678    // Przykładowy ID modułu

// Software versioning
#define LINSM_SW_MAJOR_VERSION          1       // Główna wersja
#define LINSM_SW_MINOR_VERSION          0       // Pomniejsza wersja
#define LINSM_SW_PATCH_VERSION          2       // Wersja poprawkowa

// Tryb czuwania (standby)
#define LIN_TRCV_MODE_STANDBY   0x00  // Symboliczna wartość dla trybu STANDBY

// Tryb uśpienia (sleep)
#define LIN_TRCV_MODE_SLEEP     0x01  // Symboliczna wartość dla trybu SLEEP

// Tryb normalny (normal)
#define LIN_TRCV_MODE_NORMAL    0x02  // Symboliczna wartość dla trybu NORMAL

/* Typy stanów */
// Typ enumeracyjny dla stanów głównych maszyny stanów
typedef enum {
    LINSM_UNINIT, // Stan początkowy, niezainicjalizowany
    LINSM_INIT    // Stan zainicjalizowany
} LinSM_MainStateType;

// Typ enumeracyjny dla substanów w stanie LINSM_FULL_COM
typedef enum {
    LINSM_RUN_COMMUNICATION, // Stan działania komunikacji
    LINSM_GOTO_SLEEP         // Stan przejścia w tryb uśpienia
} LinSM_SubStateType;

typedef struct {
    uint32 requestTimer;       /* Timer dla bieżącego żądania */
    boolean timerActive;         /* Flaga aktywności timera */
    uint8 retryCount;          /* Licznik ponownych prób */
} LinSM_ChannelTimers; /* Timer for schedule confirmation [SWS_LinSM_00175]*/

/* Standard types for API */
// [SWS_LinSM_00219]
typedef enum {
	COMM_NO_COMMUNICATION = 0,
	COMM_SILENT_COMMUNICATION = 1,
	COMM_FULL_COMMUNICATION = 2
} ComM_ModeType; 

typedef uint8 NetworkHandleType; 
typedef uint8 LinIf_SchHandleType;

/* Type definitions for API */
//[SWS_LinSM_00220]
typedef enum {
    LINSM_NO_COM, // Stan bez komunikacji
    LINSM_FULL_COM // Stan pełnej komunikacji
} LinSM_ModeType;

//[SWS_LinSM_00221]
typedef struct {
    struct {
        uint8 LinSMModeRequestRepetitionMax;
        struct {
            float32 LinSMConfirmationTimeout;
            uint8 LinSMNodeType;
            float32 LinSMSilenceAfterWakeupTimeout;
            boolean LinSMTransceiverPassiveMode;
            uint8 LinSMComMNetworkHandleRef;
            struct {
                uint8 LinSMScheduleIndex;
                uint8 LinSMScheduleIndexRef;
            } LinSMSchedule[NUM_SCHEDULES];
        } LinSMChannel[LINSM_MAX_CHANNELS];
    } LinSMConfigSet;

    struct {
        boolean LinSMDevErrorDetect;
        float32 LinSMMainProcessingPeriod;
        boolean LinSMVersionInfoApi;
    } LinSMGeneral;
} LinSM_ConfigType;

/* API functions*/
void LinSM_Init(const LinSM_ConfigType* ConfigPtr);
Std_ReturnType LinSM_ScheduleRequest(NetworkHandleType channel, LinIf_SchHandleType schedule);
void LinSM_GetVersionInfo(Std_VersionInfoType* versioninfo);
Std_ReturnType LinSM_GetCurrentComMode(NetworkHandleType channel, ComM_ModeType* reqComMode);
Std_ReturnType LinSM_RequestComMode(NetworkHandleType channel, ComM_ModeType reqComMode);
void LinSM_MainFunction(void);

/* API callback functions*/
void LinSM_ScheduleRequestConfirmation(NetworkHandleType channel, const LinIf_SchHandleType* schedule);
void LinSM_GotoSleepConfirmation(NetworkHandleType channel, boolean success);
void LinSM_WakeupConfirmation(NetworkHandleType channel, boolean success);

/* [SWS_LinSM_00229]API mandatory interfaces*/
void BswM_LinSM_CurrentSchedule(NetworkHandleType channel, LinIf_SchHandleType schedule);
void BswM_LinSM_CurrentState(NetworkHandleType channel, LinSM_ModeType state);
void ComM_BusSM_ModeIndication(NetworkHandleType channel, ComM_ModeType mode);
Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);
Std_ReturnType LinIf_GotoSleep(NetworkHandleType channel);
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType channel, LinIf_SchHandleType schedule);
Std_ReturnType LinIf_WakeUp(NetworkHandleType channel);

/* [SWS_LinSM_00138]API optional interfaces*/
void Det_ReportError(uint16 moduleId, uint8 instanceId, uint8 apiId, uint8 errorId);
Std_ReturnType LinIf_SetTrcvMode(NetworkHandleType channel, uint8 mode);

extern const LinSM_ConfigType LinSM_Config;

#endif /* LINSM_H */
