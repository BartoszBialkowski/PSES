/**===================================================================================================================*\
  @file LinSM.c

  @brief Biblioteka LinSM - implementacja
  
  Implementacja funkcjonalności 

\*====================================================================================================================*/

/*====================================================================================================================*\
    Załączenie nagłówków
\*====================================================================================================================*/
#include "../include/LinSM.h"

/*====================================================================================================================*\
    Makra lokalne
\*====================================================================================================================*/

/*====================================================================================================================*\
    Typy lokalne
\*====================================================================================================================*/

/*====================================================================================================================*\
    Zmienne globalne
\*====================================================================================================================*/
/* [SWS_LinSM_00175] - Dla każdej sieci powinny istnieć timery. Każda sieć powinna mieć niezależny timer od innej sieci.*/
LinSM_ChannelTimers LinSM_Timer[LINSM_MAX_CHANNELS];
// Zmienne globalne przechowujące aktualny stan główny i substan
/* [SWS_LinSM_00161] - Stan LINSM_UNINIT powinien być ustawiony na starcie*/
LinSM_MainStateType LinSM_MainState = LINSM_UNINIT;
LinSM_ModeType LinSM_Mode = LINSM_NO_COM;
LinSM_SubStateType LinSM_SubState = LINSM_RUN_COMMUNICATION;

/* Global variables to track schedule confirmation and timer */
static boolean LinSM_ScheduleConfirmationPending[LINSM_MAX_CHANNELS] = {FALSE}; /* Confirmation pending flag */

/*====================================================================================================================*\
    Zmienne lokalne (statyczne)
\*====================================================================================================================*/
/* Global variable to track the current schedule index */
static boolean LinSM_ScheduleUpdatePending[LINSM_MAX_CHANNELS] = {FALSE}; /* To track if an update is pending */
static LinIf_SchHandleType LinSM_CurrentScheduleIndex[LINSM_MAX_CHANNELS];


/* Flagi potwierdzenia harmonogramu dla każdego kanału */
static boolean LinSM_GotoSleepConfirmed[LINSM_MAX_CHANNELS] = {FALSE};
static boolean LinSM_ScheduleConfirmed[LINSM_MAX_CHANNELS] = {FALSE};
static boolean LinSM_WakeupConfirmed[LINSM_MAX_CHANNELS] = {FALSE};
static boolean LinSM_TimerActive[LINSM_MAX_CHANNELS] = {FALSE};
static boolean LinSM_ScheduleRequestError[LINSM_MAX_CHANNELS] = {FALSE};

/*====================================================================================================================*\
    Deklaracje funkcji lokalnych
\*====================================================================================================================*/

/*====================================================================================================================*\
    Kod funkcji
\*====================================================================================================================*/

/* [SWS_LinSM_00159] - Wszystkie timery powinny mieć czas podzielny przez LinSMMainProcessingPeriod
		(LinSMMainProcessingPeriod * LinSMConfirmationTimeout) gdzie LinSMConfirmationTimeout jest liczbą całkowitą */
const LinSM_ConfigType LinSM_Config = {
    .LinSMConfigSet = {
        .LinSMModeRequestRepetitionMax = 3, // Maksymalna liczba powtórzeń żądań trybu

        .LinSMChannel = {
            { // Konfiguracja kanału 1
                .LinSMConfirmationTimeout = 5.0f,   // 5 sekund na potwierdzenie
                .LinSMNodeType = 0x01,             // MASTER
                .LinSMSilenceAfterWakeupTimeout = 0.0f, // Nie dotyczy MASTER
                .LinSMTransceiverPassiveMode = FALSE, // Aktywny tryb transceivera
                .LinSMComMNetworkHandleRef = 1,    // Referencja do sieci ComM
                .LinSMSchedule = {
                    { 
                        .LinSMScheduleIndex = 1,   // Indeks harmonogramu 1 dla kanału 1
                        .LinSMScheduleIndexRef = 10 // Referencja harmonogramu 1 w LinIf
                    },
                    { 
                        .LinSMScheduleIndex = 2,   // Indeks harmonogramu 2 dla kanału 1
                        .LinSMScheduleIndexRef = 11 // Referencja harmonogramu 2 w LinIf
                    }
                }
            },
        }
    },

    .LinSMGeneral = {
        .LinSMDevErrorDetect = TRUE,             // Włączone wykrywanie błędów
        .LinSMMainProcessingPeriod = 0.01f,      // Okres 10 ms
        .LinSMVersionInfoApi = TRUE              // Włączone API informacji o wersji
    }
};

const LinSM_ConfigType LinSM_Config_test = {
    .LinSMConfigSet = {
        .LinSMModeRequestRepetitionMax = 3, // Maksymalna liczba powtórzeń żądań trybu

        .LinSMChannel = {

            { // Konfiguracja kanału 2
                .LinSMConfirmationTimeout = 3.0f,   // 3 sekundy na potwierdzenie
                .LinSMNodeType = 0x00,             // SLAVE
                .LinSMSilenceAfterWakeupTimeout = 1.0f, // Czas ciszy po wybudzeniu
                .LinSMTransceiverPassiveMode = TRUE, // Pasywny tryb transceivera
                .LinSMComMNetworkHandleRef = 2,    // Referencja do innej sieci ComM
                .LinSMSchedule = {
                    { 
                        .LinSMScheduleIndex = 3,   // Indeks harmonogramu 3 dla kanału 2
                        .LinSMScheduleIndexRef = 20 // Referencja harmonogramu 3 w LinIf
                    },
                    { 
                        .LinSMScheduleIndex = 4,   // Indeks harmonogramu 4 dla kanału 2
                        .LinSMScheduleIndexRef = 21 // Referencja harmonogramu 4 w LinIf
                    }
                }
            },
        }
    },

    .LinSMGeneral = {
        .LinSMDevErrorDetect = TRUE,             // Włączone wykrywanie błędów
        .LinSMMainProcessingPeriod = 0.01f,      // Okres 10 ms
        .LinSMVersionInfoApi = TRUE              // Włączone API informacji o wersji
    }
};

/* API Implementations */
/**
  @brief 8.3.1 LinSM_Init  [SWS_LinSM_00155]

  Inicjalizacja LinSM.
  Element projektu: [SWS_LinSM_00155] 
*/
/*[SWS_LinSM_00204] - LinIf_SetTrcvMode nie jest wywoływany w funkcji LinSM_Init*/
void LinSM_Init(const LinSM_ConfigType* ConfigPtr) { 
	/*[SWS_LinSM_00043] - reset wszystkich zmiennych globalnych w LinSM_Init
	  [SWS_LinSM_00166] - LinSM_Init nie powiadamia ComM/BswM o zmianie stanu podczas wykonywania się funkcji*/
    if (ConfigPtr == NULL) {
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_INIT_SID, LINSM_E_PARAM_POINTER);

    }
	
    /* Initialize all networks to LINSM_NO_COM */
    for (uint8 channel = 0; channel < sizeof(ConfigPtr->LinSMConfigSet.LinSMChannel); ++channel) {
		//[SWS_LinSM_00216] - LinSM_Init ustawia typ harmonogramu na NULL_SCHEDULE dla kazdego kanału
		LinSM_CurrentScheduleIndex[channel] = NULL_SCHEDULE; 
		LinSM_ScheduleConfirmationPending[channel] = FALSE;
		LinSM_ScheduleUpdatePending[channel] = FALSE;
    }
	/* Zerowanie timerów dla każdej sieci */
    for (uint8 channel = 0; channel < LINSM_MAX_CHANNELS; ++channel) {
		LinSM_Timer[channel].requestTimer = 0;
        LinSM_Timer[channel].timerActive = FALSE;
        LinSM_Timer[channel].retryCount = 0;
    }
}
/**
  @brief 8.3.2 LinSM_ScheduleRequest

  Wyższa warstwa żąda zmiany tabeli harmonogramu w jednej sieci LIN.
  Element projektu: [SWS_LinSM_00113] 
*/
Std_ReturnType LinSM_ScheduleRequest(NetworkHandleType channel, LinIf_SchHandleType schedule) {
	/* Sprawdzenie, czy identyfikator sieci jest w zakresie */
    if (channel >= LINSM_MAX_CHANNELS) {
        /* [SWS_LinSM_00114]: Funkcja musi raportować LINSM_E_NONEXISTENT_NETWORK, jeśli sieć nie istnieje. */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUEST_SID, LINSM_E_NONEXISTENT_NETWORK);
        return E_NOT_OK; 
    }
	/* Sprawdzenie, czy harmonogram jest w zakresie */
    if (schedule >= NUM_SCHEDULES) {
        /* [SWS_LinSM_00115]: Funkcja raportuje LINSM_E_PARAMETER, jeśli identyfikator harmonogramu jest poza zakresem. */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUEST_SID, LINSM_E_PARAMETER);
        return E_NOT_OK; 
    }
	if (LinSM_MainState == LINSM_UNINIT) {
        /* [SWS_LinSM_00116]: Funkcja musi raportować LINSM_E_UNINIT, jeśli moduł nie został zainicjalizowany. */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUEST_SID, LINSM_E_UNINIT);
        return E_NOT_OK; 
    }
	/* Sprawdzenie, czy żądanie dla tej sieci jest w toku */
    if (LinSM_Timer[channel].timerActive) {
        /* [SWS_LinSM_00163]: Funkcja zwraca E_NOT_OK, jeśli inna zmiana harmonogramu jest w toku dla tej sieci. */
        return E_NOT_OK;
    }
	/* Sprawdzenie, czy moduł znajduje się w stanie LINSM_FULL_COM */
    if ((LinSM_Mode != LINSM_FULL_COM) || (LinSM_SubState != LINSM_RUN_COMMUNICATION)) {
        /* [SWS_LinSM_10211]: Funkcja zwraca E_NOT_OK, jeśli LinSM nie jest w stanie LINSM_FULL_COM. */
        return E_NOT_OK;
    }
    /* Sprawdzenie, czy LinSM jest zainicjalizowany */
	if (LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMNodeType != 0x01) {
        /* [SWS_LinSM_00241]: Funkcja LinIf_ScheduleRequest jest dostępna tylko dla trybu MASTER */
        return E_NOT_OK;
    }
	/* [SWS_LinSM_00100] Przed wywołaniem funkcji LinIf_ScheduleRequest LinSM startuje timer */
	LinSM_Timer[channel].timerActive = TRUE; 
    LinSM_Timer[channel].requestTimer = LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMConfirmationTimeout; 
    
    if (LinIf_ScheduleRequest(channel, schedule) == E_NOT_OK) {
		/* [SWS_LinSM_00079] - Wywołanie LinIf_ScheduleRequest natychmiast w trakcie wywołania LinSM_ScheduleRequest */ 
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUEST_SID, LINSM_E_INIT_FAILED);
		/* Zapamiętanie bieżącego harmonogramu */
        LinSM_CurrentScheduleIndex[channel] = LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMSchedule[0].LinSMScheduleIndex;
        LinSM_ScheduleUpdatePending[channel] = TRUE; /* Oznacz aktualizację jako oczekującą */
		LinSM_ScheduleRequestError[channel] = TRUE;
		LinSM_Timer[channel].timerActive = FALSE;
		LinSM_Timer[channel].requestTimer = 0;
		/* [SWS_LinSM_00168] - Wartość zwracana przez LinSM_ScheduleRequest jest taka sama jak przez LinIf_ScheduleRequest*/
        return E_NOT_OK;
    } else {
		LinSM_ScheduleRequestError[channel] = FALSE;
		return E_OK; 
	}
    
}
/**
  @brief 8.3.3 LinSM_GetVersionInfo 

  Pobranie informacji o aktualnej wersji
  Element projektu: [SWS_LinSM_00117]
*/

void LinSM_GetVersionInfo(Std_VersionInfoType* versioninfo) {
    if (versioninfo == NULL) {
		/* [SWS_LinSM_00119] - Jeżeli wskaźnik versioninfo jest NULL to zwróc E_NOT_OK i wyślij kod błędu LINSM_E_PARAM_POINTER*/
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GETVERSIONINFO_SID, LINSM_E_PARAM_POINTER);
        return;
    }
    versioninfo->vendorID = LINSM_VENDOR_ID;
    versioninfo->moduleID = LINSM_MODULE_ID;
    versioninfo->sw_major_version = LINSM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = LINSM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = LINSM_SW_PATCH_VERSION;
}
/**
  @brief 8.3.4 LinSM_GetCurrentComMode

  Funkcja sprawdzająca aktualny tryb komunikacji.
  Element projektu: [SWS_LinSM_00122]
*/
Std_ReturnType LinSM_GetCurrentComMode(NetworkHandleType channel, ComM_ModeType* reqComMode) {
    if (reqComMode == NULL) {
        /* [SWS_LinSM_00124]: Jeśli wskaźnik reqComMode jest NULL, zgłoś błąd LINSM_E_PARAM_POINTER i zwróć E_NOT_OK */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GETCURRENTCOMMODE_SID, LINSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (channel >= LINSM_MAX_CHANNELS) {
        /* [SWS_LinSM_00123]: Jeśli identyfikator sieci jest poza zakresem, zgłoś błąd LINSM_E_NONEXISTENT_NETWORK i zwróć E_NOT_OK */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GETCURRENTCOMMODE_SID, LINSM_E_NONEXISTENT_NETWORK);
        return E_NOT_OK;
    }
    if (LinSM_MainState == LINSM_UNINIT) {
        /* [SWS_LinSM_00125]: Jeśli LinSM nie jest zainicjalizowany, zgłoś błąd LINSM_E_UNINIT i zwróć E_NOT_OK */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GETCURRENTCOMMODE_SID, LINSM_E_UNINIT);
		/* [SWS_LinSM_00182]: Jeśli LinSM nie jest zainicjalizowany, zwróć COMM_NO_COMMUNICATION */
		*reqComMode = COMM_NO_COMMUNICATION;
        return E_NOT_OK;
    }
	
	if (LinSM_Mode == LINSM_NO_COM) {
        /* [SWS_LinSM_00180]: Jeśli aktywny stan to LINSM_NO_COM, zwróć COMM_NO_COMMUNICATION */
        *reqComMode = COMM_NO_COMMUNICATION;
    }
    /* Ustawienie trybu komunikacji w zależności od stanu LinSM */
    if (LinSM_Mode == LINSM_FULL_COM) {
        /* [SWS_LinSM_00181]: Jeśli aktywny stan to LINSM_FULL_COM, zwróć COMM_FULL_COMMUNICATION */
        *reqComMode = COMM_FULL_COMMUNICATION;
    } 
	
    return E_OK;
}
/**
  @brief 8.3.5 LinSM_RequestComMode

  Żądanie trybu komunikacji.
  Przełączenie trybu nie nastąpi natychmiastowo. LinSM powiadomi funkcje wywołującą, kiedy nastąpi przejście trybu.
  
  Element projektu: [SWS_LinSM_00126]
*/
Std_ReturnType LinSM_RequestComMode(NetworkHandleType channel, ComM_ModeType reqComMode) {
    if (channel >= LINSM_MAX_CHANNELS) {
        /* [SWS_LinSM_00127]: Funkcja zgłasza błąd LINSM_E_NONEXISTENT_NETWORK, jeśli identyfikator sieci jest poza zakresem */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_REQUESTCOMMODE_SID, LINSM_E_NONEXISTENT_NETWORK);
        return E_NOT_OK;
    }
    if (LinSM_MainState == LINSM_UNINIT) {
        /* [SWS_LinSM_00128]: Funkcja zgłasza błąd LINSM_E_UNINIT, jeśli moduł LinSM nie został zainicjalizowany */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_REQUESTCOMMODE_SID, LINSM_E_UNINIT);
        return E_NOT_OK;
    }
    if ((reqComMode != COMM_NO_COMMUNICATION) && (reqComMode != COMM_FULL_COMMUNICATION)) {
        /* [SWS_LinSM_00191]: Funkcja zgłasza błąd LINSM_E_PARAMETER, jeśli tryb jest nieprawidłowy
		   [SWS_LinSM_00183]: Zwróć E_NOT_OK jeżeli reqComMode == COMM_SILENT_COMMUNICATION  		*/
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_REQUESTCOMMODE_SID, LINSM_E_PARAMETER);
        return E_NOT_OK;
    }

    /* [SWS_LinSM_00223]: Funkcja zapisuje żądanie zmiany trybu w strukturach wewnętrznych LinSM */
    /* Inicjalizacja zmiany trybu komunikacyjnego */
	/*[SWS_LinSM_00047] - Jeżeli moduł ComM wywołuje LinSM_RequestComMode z reqComMode == COMM_FULL_COMMUNICATION to LinSM bez czekania na wywołanie main funkcji, wywołuje LinIf_WakeUp
	  [SWS_LinSM_00178] - W każdym innym przypadku niż [SWS_LinSM_00047] funkcja LinIf_WakeUp nie powinna być wywoływana*/
	if ((reqComMode == COMM_FULL_COMMUNICATION) && (LinSM_Mode == LINSM_NO_COM) && (LinSM_MainState == LINSM_INIT)) {
        /* Przejście do trybu pełnej komunikacji */
        LinIf_SetTrcvMode(channel, LIN_TRCV_MODE_NORMAL);
		/* [SWS_LinSM_00100] Przed wywołaniem funkcji LinIf_WakeUp LinSM startuje timer */
		LinSM_Timer[channel].timerActive = TRUE; 
		LinSM_Timer[channel].requestTimer = LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMConfirmationTimeout; 
        if (LinIf_WakeUp(channel) != E_OK) {
			Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_REQUESTCOMMODE_SID, LINSM_E_INIT_FAILED);
			LinSM_Mode = LINSM_NO_COM;
			ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
            BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
			LinSM_Timer[channel].timerActive = FALSE;
			LinSM_Timer[channel].requestTimer = 0; 
			/* [SWS_LinSM_00176] - Jeżeli LinIf_Wakeup zwraca E_NOT_OK to LinSM_RequestComMode zwraca również E_NOT_OK*/
			return E_NOT_OK; 
		}else {
			return E_OK;
		}
	/*[SWS_LinSM_00036] - Jeżeli moduł ComM wywołuje LinSM_RequestComMode z reqComMode == COMM_NO_COMMUNICATION to LinSM bez czekania na wywołanie main funkcji, wywołuje LinIf_GotoSleep  
	  [SWS_LinSM_00035] - Moduł LinSM wywołuje LinIf_GotoSleep tylko wtedy gdy LinSM_Mode == LINSM_FULL_COM i LinSM_SubState == LINSM_RUN_COMMUNICATION */
    } else if (reqComMode == COMM_NO_COMMUNICATION && LinSM_Mode == LINSM_FULL_COM && LinSM_MainState == LINSM_INIT && LinSM_SubState == LINSM_RUN_COMMUNICATION) { 
        /* Przejście do trybu braku komunikacji */
		/* [SWS_LinSM_00100] Przed wywołaniem funkcji LinIf_GotoSleep LinSM startuje timer */
		LinSM_Timer[channel].timerActive = TRUE; 
		LinSM_Timer[channel].requestTimer = LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMConfirmationTimeout; 
		/* [SWS_LinSM_10208] W stanie LinSM_Mode == LINSM_FULL_COM i reqComMode == COMM_NO_COMMUNICATION, LinIf_GotoSleep jest wywoływane
		   [SWS_LinSM_10209] W żadnym innym przypadku LinIf_GotoSleep nie powinno być wywoływane*/
        if (LinIf_GotoSleep(channel) == E_OK) { 
			/* [SWS_LinSM_00302] - Jeżeli LinIf_GotoSleep zwraca E_OK to ustaw substan na LINSM_GOTOSLEEP*/
            LinSM_SubState = LINSM_GOTO_SLEEP;
			return E_OK;
        } else {
            Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_REQUESTCOMMODE_SID, LINSM_E_INIT_FAILED);
			LinSM_Timer[channel].timerActive = FALSE;
			LinSM_Timer[channel].requestTimer = 0; /* Reset timera */
			/* Wywołanie ComM i BswM dla bieżącego stanu */
            if (LinSM_Mode == LINSM_FULL_COM) {
                ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION);
                BswM_LinSM_CurrentState(channel, LINSM_FULL_COM);
            }
		}
	}
	/*[SWS_LinSM_00177] Jeżeli LinIf_GotoSleep zwórci E_NOT_OK to LinSM_RequestComMode powinien zwrócić E_NOT_OK*/
	return E_NOT_OK; 
}

/**
  @brief 8.4.1 LinSM_MainFunction

  Funkcja okresowa uruchamiająca timery różnych limitów czasu żądań
  Element projektu: [SWS_LinSM_00156]
*/
	/*[SWS_LinSM_00162] - Obsługa wszystkich timerów realizowana jest w LinSM_MainFunction
	  [SWS_LinSM_00022] - Powinien być stan LINSM_UNINIT
	  [SWS_LinSM_00024] - Powinien być stan LINSM_INIT
	  [SWS_LinSM_00026] - Powinien być stan LINSM_NO_COM
	  [SWS_LinSM_00032] - Powinien być stan LINSM_FULL_COM */
	void LinSM_MainFunction(void) { 
		uint8 channel = 0;	
		switch (LinSM_MainState) {
			case LINSM_UNINIT:
					LinSM_Init(&LinSM_Config);
					/* [SWS_LinSM_00025] - LinSM zmienia stan na LINSM_INIT podczas wywołania funkcji LinSM_Init
					   [SWS_LinSM_00152] - LinSM zmienia substan na LINSM_NO_COM podczas wywołania funkcji LinSM_Init
					   [SWS_LinSM_00160] - Substan LINSM_NO_COM powinien być ustawiony przed wejściem do stanu LINSM_INIT*/					
					LinSM_MainState = LINSM_INIT; 
					LinSM_Mode = LINSM_NO_COM; 
					LinSM_SubState = LINSM_RUN_COMMUNICATION;
				break;

			case LINSM_INIT: 
				switch (LinSM_Mode) {
					/*[SWS_LinSM_00028] - Kiedy LINSM_NO_COM jest aktywny, moduł LinSM powinien nie wydawać poleceń modułowi LinIf w celu komunikacji dla wybranej sieci */
					case LINSM_NO_COM: // 
						/*[SWS_LinSM_00027] - Przy wejściu do LINSM_NO_COM LinSM powiadamia ComM o zmianie stanu za pomocą ComM_BusSM_ModeIndication z parametrem COMM_NO_COMMUNICATION
						  [SWS_LinSM_00193] - Przy wejściu do LINSM_NO_COM LinSM powiadamia BswM o zmianie stanu za pomocą BswM_LinSM_CurrentState z parametrem LINSM_NO_COM						*/
						ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
						BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
						/* [SWS_LinSM_00203] - Po wejściu do LINSM_NO_COM transceiver powinien być ustawiony na STANDBY, jeśli LinSMTransceiverPassiveMode ma wartość true, i SLEEP w przeciwnym razie
											   za pomocą LinIf_SetTrcvMode. Wymóg ten ma zastosowanie tylko wtedy, gdy dla kanału skonfigurowano LinSMTransceiverPassiveMode.*/
						if(LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMTransceiverPassiveMode == TRUE){ 
							LinIf_SetTrcvMode(LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMComMNetworkHandleRef, LIN_TRCV_MODE_STANDBY); 
						}else {
							LinIf_SetTrcvMode(LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMComMNetworkHandleRef, LIN_TRCV_MODE_SLEEP); 
						}
						/* Obsługa timera dla sieci */
						if (LinSM_Timer[channel].timerActive == TRUE) {
							if (LinSM_Timer[channel].requestTimer > 0) {
								LinSM_Timer[channel].requestTimer--;
							}						
							if (LinSM_WakeupConfirmed[channel] == TRUE) {
								/* [SWS_LinSM_00154] - Kiedy moduł LinIf wywołuje potwierdzający callback przed wystąpieniem timeoutu to timer się resetuje */
								LinSM_Timer[channel].requestTimer = 0;  
								LinSM_Timer[channel].timerActive = FALSE;
								LinSM_WakeupConfirmed[channel] = FALSE;
								LinSM_Timer[channel].retryCount = 0;
								/* [SWS_LinSM_00049] - W momencie potwierdzenia prawidłowego wykonania funkcji WakeUp, zmień stan na LINSM_FULL_COM
								   [SWS_LinSM_00202] - We wszystkich innych przypadkach stan nie zmienia się*/
								LinSM_Mode = LINSM_FULL_COM; 
								/* [SWS_LinSM_00301] - Podczas przejścia do LINSM_FULL_COM przejdź także do LINSM_RUN_COMMUNICATION*/
								LinSM_SubState = LINSM_RUN_COMMUNICATION; //
							/* [SWS_LinSM_00101] - Kiedy timer przekroczy czas zapisany w parametrze LinSMConfirmationTimeout, występuje timeout*/
							}else if (LinSM_Timer[channel].requestTimer == 0) {	
								/* [SWS_LinSM_00102] - W momencie wystąpienia timeoutu wysyłany jest kod błędu LINSM_E_CONFIRMATION_TIMEOUT*/
								Det_ReportRuntimeError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_MAIN_FUNCTION_API_ID, LINSM_E_CONFIRMATION_TIMEOUT);
								LinSM_Timer[channel].timerActive = FALSE; /* Reset pending flag */
								LinSM_Timer[channel].retryCount++;
								LinSM_Mode = LINSM_NO_COM; 
								/* [SWS_LinSM_00304] - Jeżeli wystąpi timeout ale maksymalna liczba prób nie została przekroczona to LinSM spróbuje wywołać ponownie LinIf_WakeUp */
								if (LinSM_Timer[channel].retryCount < LinSM_Config.LinSMConfigSet.LinSMModeRequestRepetitionMax) {
									LinSM_Timer[channel].timerActive = TRUE; //[SWS_LinSM_00100]
									LinSM_Timer[channel].requestTimer = LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMConfirmationTimeout; /* Ustawienie timera */
									if (LinIf_WakeUp(channel) != E_OK) { // [SWS_LinSM_00178] [SWS_LinSM_00202]
										LinSM_Mode = LINSM_NO_COM;
										ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
										BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
										LinSM_Timer[channel].timerActive = FALSE;
										LinSM_Timer[channel].requestTimer = 0; /* Reset timera */
									}
								}else{ 
									LinSM_Timer[channel].retryCount = 0; 
									LinSM_Timer[channel].timerActive = FALSE;
									LinSM_Timer[channel].requestTimer = 0;
									/* [SWS_LinSM_00307] 
									   [SWS_LinSM_00170] - W momencie gdy czas timera się skończy i zostania przekroczona maksymalna liczba prób wysłania to LinSm powiadamia ComM o tym samym stanie
									   [SWS_LinSM_00215] - W momencie gdy czas timera się skończy i zostania przekroczona maksymalna liczba prób wysłania to LinSm powiadamia BswM o tym samym stanie*/
									ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
									BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
									break;
								}
							}
						}						
						break;
					case LINSM_FULL_COM: 
						/*[SWS_LinSM_00033] - Przy wejściu do LINSM_FULL_COM LinSM powiadamia ComM o zmianie stanu za pomocą ComM_BusSM_ModeIndication z parametrem COMM_FULL_COMMUNICATION
						  [SWS_LinSM_00192] - Przy wejściu do LINSM_FULL_COM LinSM powiadamia BswM o zmianie stanu za pomocą BswM_LinSM_CurrentState z parametrem LINSM_FULL_COM						*/
						ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION); 
						BswM_LinSM_CurrentState(channel, LINSM_FULL_COM); 
						
						if (LinSM_Config.LinSMConfigSet.LinSMChannel[channel].LinSMTransceiverPassiveMode == TRUE) {
							/*[SWS_LinSM_00205] - Przy wejściu do LINSM_FULL_COM Transceiver powinien być aktywowany za pomocą LinIf_SetTrcvMode */
							LinIf_SetTrcvMode(channel, LIN_TRCV_MODE_NORMAL); 
						}
									
						switch (LinSM_SubState) {
							case LINSM_RUN_COMMUNICATION:
								/* [SWS_LinSM_00213] - Jeśli LinIf_ScheduleRequest zwróci E_NOT_OK to moduł LinSM wywołuje BswM_LinSM_CurrentSchedule ze starym harmonogramem */
								if (LinSM_ScheduleRequestError[channel]) { //[SWS_LinSM_00213]
									BswM_LinSM_CurrentSchedule(channel, LinSM_CurrentScheduleIndex[channel]);
									/* Resetowanie flagi błędu po obsłudze */
									LinSM_ScheduleRequestError[channel] = FALSE;
								}
								/* Obsługa timera dla sieci [SWS_LinSM_00162]*/
								if (LinSM_Timer[channel].timerActive) {
									if (LinSM_Timer[channel].requestTimer > 0) {
										LinSM_Timer[channel].requestTimer--;
									}									
									if (LinSM_ScheduleConfirmed[channel] == TRUE) {
										/* [SWS_LinSM_00154] - Kiedy moduł LinIf wywołuje potwierdzający callback przed wystąpieniem timeoutu to timer się resetuje */
										LinSM_Timer[channel].requestTimer = 0; 
										LinSM_Timer[channel].timerActive = FALSE; /* Potwierdzenie obsłużone */
										LinSM_ScheduleConfirmed[channel] = FALSE;			
									/* [SWS_LinSM_00101] - Kiedy timer przekroczy czas zapisany w parametrze LinSMConfirmationTimeout, występuje timeout*/
									}else if (LinSM_Timer[channel].requestTimer == 0) {	
										/* [SWS_LinSM_00102] - W momencie wystąpienia timeoutu wysyłany jest kod błędu LINSM_E_CONFIRMATION_TIMEOUT*/
										ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION); 
										BswM_LinSM_CurrentState(channel, LINSM_FULL_COM);
										/* [SWS_LinSM_00214] - Jeżeli timer wygaśnie to moduuł LinSM wywołuje BswM_LinSM_CurrentSchedule z niezmienionym harmonogramem */
										BswM_LinSM_CurrentSchedule(channel, LinSM_CurrentScheduleIndex[channel]); 
										Det_ReportRuntimeError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_MAIN_FUNCTION_API_ID, LINSM_E_CONFIRMATION_TIMEOUT);
										LinSM_Timer[channel].timerActive = FALSE; /* Reset pending flag */
									}
								}
								break;
							case LINSM_GOTO_SLEEP: 								
								/* Obsługa timera dla sieci */
								if (LinSM_Timer[channel].timerActive) {
									if (LinSM_Timer[channel].requestTimer > 0) {
									LinSM_Timer[channel].requestTimer--;
									}
									if (LinSM_GotoSleepConfirmed[channel] == TRUE) {
										/* [SWS_LinSM_00154] - Kiedy moduł LinIf wywołuje potwierdzający callback przed wystąpieniem timeoutu to timer się resetuje */
										LinSM_Timer[channel].requestTimer = 0;  
										LinSM_Timer[channel].timerActive = FALSE; 
										LinSM_GotoSleepConfirmed[channel] = FALSE;
										/* [SWS_LinSM_00046] - Kiedy LinSM_GotoSleepConfirmation jest wywołane i LinSM_Mode == LINSM_FULL_COM && LinSM_SubState == LINSM_GOTO_SLEEP to ustaw stan LINSM_NO_COM*/
										LinSM_Mode = LINSM_NO_COM;
										LinSM_SubState = LINSM_RUN_COMMUNICATION;				
									/* [SWS_LinSM_00101] - Kiedy timer przekroczy czas zapisany w parametrze LinSMConfirmationTimeout, występuje timeout*/
									}else if (LinSM_Timer[channel].requestTimer == 0) {	
										/* [SWS_LinSM_00102] - W momencie wystąpienia timeoutu wysyłany jest kod błędu LINSM_E_CONFIRMATION_TIMEOUT*/
										Det_ReportRuntimeError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_MAIN_FUNCTION_API_ID, LINSM_E_CONFIRMATION_TIMEOUT);
										LinSM_Timer[channel].timerActive = FALSE; /* Reset pending flag */
										LinSM_SubState = LINSM_RUN_COMMUNICATION;
									}
								}
							default:
								break;
						}
					default:
						break;
				}
			default:
				break;
		}

	}	
/**
  @brief 8.5.1 LinSM_ScheduleRequestConfirmation

  Funkcja zwrotna wywoływana przez LinIf, gdy nowa tabela harmonogramu jest aktywna
  Element projektu: [SWS_LinSM_00129]
*/

void LinSM_ScheduleRequestConfirmation(NetworkHandleType channel, const LinIf_SchHandleType *schedule) {
    /* Sprawdzenie, czy moduł LinSM jest zainicjalizowany */
    if (LinSM_MainState == LINSM_UNINIT) {
        /* Zgłoszenie błędu: moduł LinSM nie został zainicjalizowany */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUESTCONFIRMATION_API_ID, LINSM_E_UNINIT);
        return;
    }

    /* Sprawdzenie poprawności identyfikatora sieci */
    if (channel >= LINSM_MAX_CHANNELS) {
        /* Zgłoszenie błędu: nieistniejąca sieć */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUESTCONFIRMATION_API_ID, LINSM_E_NONEXISTENT_NETWORK);
        return;
    }

    /* Sprawdzenie wskaźnika na tabelę harmonogramu */
    if (schedule == NULL) {
        /* Zgłoszenie błędu: nieprawidłowy wskaźnik */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_SCHEDULEREQUESTCONFIRMATION_API_ID, LINSM_E_PARAM_POINTER);
        return;
    }

    /* Aktualizacja bieżącego harmonogramu */
    LinSM_CurrentScheduleIndex[channel] = *schedule;
	
	 /* Ustawienie flagi potwierdzenia harmonogramu */
    LinSM_ScheduleConfirmed[channel] = TRUE;

    /*[SWS_LinSM_00206] - Kiedy moduł LinSM dostanie potwierdzenie ustawienia harmonogramu z modułu LinIf to wywołuje BswM_LinSM_CurrentSchedule
	  [SWS_LinSM_00207] - Jeśli LinIf potwierdzi zmianę harmonogramu bez jej wywołania to powinno być wywołane BswM_LinSM_CurrentSchedule*/
    BswM_LinSM_CurrentSchedule(channel, *schedule);
}
/**
  @brief  8.5.3 LinSM_GotoSleepConfirmation

  Funkcja zwrotna wywoływana przez LinIf po wysłaniu polecenia goto sleep
  Element projektu: [SWS_LinSM_00135]
*/

void LinSM_GotoSleepConfirmation(NetworkHandleType channel, boolean success) {
    /* Sprawdzenie, czy moduł LinSM jest zainicjalizowany */
    if (LinSM_MainState == LINSM_UNINIT) {
        /* Zgłoszenie błędu: moduł LinSM nie został zainicjalizowany */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GOTOSLEEPCONFIRMATION_API_ID, LINSM_E_UNINIT);
        return;
    }

    /* Sprawdzenie poprawności identyfikatora sieci */
    if (channel >= LINSM_MAX_CHANNELS) {
        /* Zgłoszenie błędu: nieistniejąca sieć */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_GOTOSLEEPCONFIRMATION_API_ID, LINSM_E_NONEXISTENT_NETWORK);
        return;
    }

    /* Obsługa sukcesu lub niepowodzenia */
    if (success == TRUE && LinSM_Mode == LINSM_FULL_COM && LinSM_SubState == LINSM_GOTO_SLEEP) {

        /* Powiadomienie ComM o stanie NO_COMMUNICATION */
        ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);

        /* Powiadomienie BswM o stanie NO_COM */
        BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
		
		/* Ustawienie flagi potwierdzenia harmonogramu */
		LinSM_GotoSleepConfirmed[channel] = TRUE;
				
    } else {
        /* Operacja nie powiodła się: pozostanie w obecnym stanie */
        /* Powiadomienie modułów o braku zmiany */
        if (LinSM_Mode == LINSM_FULL_COM) {
            ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION); 
            BswM_LinSM_CurrentState(channel, LINSM_FULL_COM);
        } else if (LinSM_Mode == LINSM_NO_COM) {
            ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
            BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
        }
    }
}
/**
  @brief 8.5.4 LinSM_WakeupConfirmation

  Funkcja zwrotna wywoływana przez LinIf po wysłaniu sygnału wakeup
  Element projektu: [SWS_LinSM_00132] 
*/
void LinSM_WakeupConfirmation(NetworkHandleType channel, boolean success) {
    /* Sprawdzenie, czy moduł LinSM jest zainicjalizowany */
    if (LinSM_MainState == LINSM_UNINIT) {
        /* Zgłoszenie błędu: moduł LinSM nie został zainicjalizowany */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_WAKEUPCONFIRMATION_API_ID, LINSM_E_UNINIT);
        return;
    }

    /* Sprawdzenie poprawności identyfikatora sieci */
    if (channel >= LINSM_MAX_CHANNELS) {
        /* Zgłoszenie błędu: nieistniejąca sieć */
        Det_ReportError(MODULE_ID_LINSM, INSTANCE_ID, LINSM_WAKEUPCONFIRMATION_API_ID, LINSM_E_NONEXISTENT_NETWORK);
        return;
    }

    /* Obsługa sukcesu lub niepowodzenia */
    if (success == TRUE && LinSM_Mode == LINSM_NO_COM && LinSM_MainState == LINSM_INIT) {
        /* Powiadomienie ComM o stanie FULL_COMMUNICATION */
        ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION);

        /* Powiadomienie BswM o stanie FULL_COM */
        BswM_LinSM_CurrentState(channel, LINSM_FULL_COM);
		
		 /* Ustawienie flagi potwierdzenia harmonogramu */
		LinSM_WakeupConfirmed[channel] = TRUE;
		
    } else {
        /* Operacja nie powiodła się: pozostanie w obecnym stanie */
        /* Powiadomienie modułów o braku zmiany */
        if (LinSM_Mode == LINSM_NO_COM) {
            ComM_BusSM_ModeIndication(channel, COMM_NO_COMMUNICATION);
            BswM_LinSM_CurrentState(channel, LINSM_NO_COM);
        } else if (LinSM_Mode == LINSM_FULL_COM) {
            ComM_BusSM_ModeIndication(channel, COMM_FULL_COMMUNICATION); 
            BswM_LinSM_CurrentState(channel, LINSM_FULL_COM);
        }
    }
}