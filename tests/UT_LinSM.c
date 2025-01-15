//#include "LinSM.h"
#include "../include/fff.h"
#include "../include/acutest.h"
#include "../include/Std_Types.h"
#include "../src/LinSM.c"
#include "mocks/mocks.c"

/*
Std_ReturnType LinIf_ScheduleRequest_custom(NetworkHandleType channel, LinIf_SchHandleType schedule) {
	uint8 call_count = 0;
    call_count++;
    if (call_count == 1) {
        return E_OK; // Pierwsze wywołanie zwraca E_OK
    } else {
        return E_NOT_OK; // Drugie (i kolejne) wywołania zwracają E_NOT_OK
    }
}*/
/**
  @brief Test funkcji LinSM_Init

  Funkcja testująca różne ustawienia inicjujące.
*/
void test_LinSM_Init(void) {
    ResetAllMocks();
	LinSM_Timer[0].timerActive = TRUE;
	LinSM_Timer[0].requestTimer = 2;
    /* Wywołanie funkcji z NULL jako parametrem */
    LinSM_Init(NULL);

    /* Sprawdzenie, czy zgłoszono błąd przez Det_ReportError */
    TEST_CHECK(Det_ReportError_fake.call_count == 1);
    TEST_CHECK(Det_ReportError_fake.arg0_val == MODULE_ID_LINSM);
    TEST_CHECK(Det_ReportError_fake.arg1_val == INSTANCE_ID);
    TEST_CHECK(Det_ReportError_fake.arg2_val == LINSM_INIT_SID);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_PARAM_POINTER);

    /* Wywołanie funkcji z poprawnym parametrem */
    LinSM_Init(&LinSM_Config);

    /* Sprawdzenie, czy moduł został poprawnie zainicjalizowany */
	TEST_CHECK(LinSM_Timer[0].timerActive == FALSE);  
    TEST_CHECK(LinSM_Timer[0].requestTimer == 0);
	TEST_CHECK(LinSM_Timer[0].retryCount == 0);
    TEST_MSG("LinSM_MainState powinien być ustawiony na LINSM_INIT.");
}
/**
  @brief Test funkcji LinSM_ScheduleRequest

  Funkcja testująca różne ustawienia harmonogramów w LinSM
*/
void test_LinSM_ScheduleRequest(void) {
    ResetAllMocks();
	Std_ReturnType result;
    /* Test z niezainicjalizowanym modułem */
	
    result = LinSM_ScheduleRequest(0, 0);
    TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.call_count == 1);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_UNINIT);
	
	LinSM_MainFunction();
	
    /* Test z nieprawidłowym kanałem */
    result = LinSM_ScheduleRequest(LINSM_MAX_CHANNELS + 1, 0);
    TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.call_count == 2);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_NONEXISTENT_NETWORK);

    /* Test z nieprawidłowym harmonogramem */
    result = LinSM_ScheduleRequest(0, NUM_SCHEDULES + 1);
    TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.call_count == 3);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_PARAMETER);

	LinSM_Timer[0].timerActive = TRUE;
	result = LinSM_ScheduleRequest(0, 0);
	TEST_CHECK(result == E_NOT_OK);
	
	LinSM_Timer[0].timerActive = FALSE;
	LinSM_Mode = LINSM_NO_COM;
	result = LinSM_ScheduleRequest(0, 0);
	TEST_CHECK(result == E_NOT_OK);
	
	LinSM_Init(&LinSM_Config);
	LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_RUN_COMMUNICATION;
	
	LinIf_ScheduleRequest_fake.return_val = E_OK;
	result = LinSM_ScheduleRequest(0, 0);
    TEST_CHECK(result == E_OK);
	
	//LinSM_Init(&LinSM_Config_test);
	//result = LinSM_ScheduleRequest(0, 0);
	//TEST_CHECK(result == E_NOT_OK);
	

}
/**
  @brief Test braku ustawienia harmonogramu dla funkcji LinSM_ScheduleRequest 

  Funkcja testująca negarywną odpowiedź wykonania ustawienia harmonogramu
*/
void test_LinSM_ScheduleRequest_error(void) {
    ResetAllMocks();
	Std_ReturnType result;

	LinSM_MainFunction();

	LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_RUN_COMMUNICATION;
	
	LinIf_ScheduleRequest_fake.return_val = E_NOT_OK;
	result = LinSM_ScheduleRequest(0, 0);
    TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_INIT_FAILED);
	

}
/**
  @brief Test LinSM_GetVersionInfo

  Funkcja sprawdzająca aktualną wersje.
*/
void test_LinSM_GetVersionInfo(void) {
    ResetAllMocks();

    Std_VersionInfoType versionInfo;
    LinSM_GetVersionInfo(&versionInfo);

    TEST_CHECK(versionInfo.vendorID == LINSM_VENDOR_ID);
    TEST_CHECK(versionInfo.moduleID == LINSM_MODULE_ID);
    TEST_CHECK(versionInfo.sw_major_version == LINSM_SW_MAJOR_VERSION);
    TEST_CHECK(versionInfo.sw_minor_version == LINSM_SW_MINOR_VERSION);
    TEST_CHECK(versionInfo.sw_patch_version == LINSM_SW_PATCH_VERSION);
	
	LinSM_GetVersionInfo(NULL);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_PARAM_POINTER);
}
/**
  @brief Test funkcji LinSM_GetCurrentComMode

  Funkcja sptawdzająca czy dobrze jest pobierany aktualny tryb pracy.
*/
void test_LinSM_GetCurrentComMode(void) {
    ResetAllMocks();
	ComM_ModeType mode;
	Std_ReturnType result;
	

	result = LinSM_GetCurrentComMode(0, &mode);
	TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_UNINIT);
	
	LinSM_MainFunction();	

	result = LinSM_GetCurrentComMode(LINSM_MAX_CHANNELS + 1, &mode);
	TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_NONEXISTENT_NETWORK);
	
    result = LinSM_GetCurrentComMode(0, &mode);
    TEST_CHECK(result == E_OK);
    TEST_CHECK(mode == COMM_NO_COMMUNICATION);
	
	LinSM_Mode = LINSM_FULL_COM;
	result = LinSM_GetCurrentComMode(0, &mode);
	TEST_CHECK(result == E_OK);
    TEST_CHECK(mode == COMM_FULL_COMMUNICATION);
	
	result = LinSM_GetCurrentComMode(0, NULL);
	TEST_CHECK(result == E_NOT_OK);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_PARAM_POINTER);
}
/**
  @brief Test negatywnej odpowiedzi na ustawienie trybu pracy w funkcji LinSM_RequestComMode

  Funkcja sprawdzająca działanie LinSM_RequestComMode w przypadku negatywnej odpowiedzi przez moduł LinIf.
*/
void test_LinSM_RequestComMode_error(void) {
    ResetAllMocks();
	Std_ReturnType result;
	LinIf_WakeUp_fake.return_val = E_NOT_OK;
	LinIf_GotoSleep_fake.return_val = E_NOT_OK;
	
	result = LinSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 1);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_UNINIT);
	
	LinSM_MainFunction();
	
	LinSM_Mode = LINSM_NO_COM;
    result = LinSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
	TEST_CHECK(LinSM_Mode == LINSM_NO_COM);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 2);
	TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_INIT_FAILED);
	
	LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_RUN_COMMUNICATION;
	result = LinSM_RequestComMode(0, COMM_NO_COMMUNICATION);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 3);
	TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_INIT_FAILED);
}
/**
  @brief Test funkcji LinSM_RequestComMode

  Funkcja sprawdzająca ustawienie się danego trybu pracy.
*/
void test_LinSM_RequestComMode(void) {
    ResetAllMocks();
	Std_ReturnType result;
	
	result = LinSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 1);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_UNINIT);
	
	LinSM_MainFunction();
	
	result = LinSM_RequestComMode(LINSM_MAX_CHANNELS + 1, COMM_FULL_COMMUNICATION);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 2);
    TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_NONEXISTENT_NETWORK);
	
	result = LinSM_RequestComMode(0, COMM_SILENT_COMMUNICATION);
	TEST_CHECK(result == E_NOT_OK);
	TEST_CHECK(Det_ReportError_fake.call_count == 3);
	TEST_CHECK(Det_ReportError_fake.arg3_val == LINSM_E_PARAMETER);
	
	ResetAllMocks();
	LinSM_MainFunction();
	
	LinSM_Mode = LINSM_NO_COM;
    result = LinSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
    TEST_CHECK(result == E_OK);
	
	LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_RUN_COMMUNICATION;
	result = LinSM_RequestComMode(0, COMM_NO_COMMUNICATION);
	TEST_CHECK(result == E_OK);
}
/**
  @brief Test inicjalizacji funkcji LinSM_MainFunction

  Funkcja sprawdzająca poprawną inicjalizacje maszyny stanów.
*/
void test_LinSM_MainFunction_Uninit(void) {
    ResetAllMocks();

    LinSM_MainFunction();

    TEST_CHECK(LinSM_MainState == LINSM_INIT); // Powinno przejść do stanu INIT
	
    TEST_CHECK(LinIf_SetTrcvMode_fake.arg1_val == LIN_TRCV_MODE_STANDBY); // Nie powinno być wywołań LinIf_SetTrcvMode
}
/**
  @brief Test prawidłowego ustawienia się trybu trcv

  Funkcja sprawdzająca czy status Trcv poprawnie ustawia się dla trybu pasywnego.
*/
void test_LinSM_MainFunction_PassiveTrcvMode(void) {
    ResetAllMocks();
	
	LinSM_MainState = LINSM_INIT;
	LinSM_Init(&LinSM_Config_test);
    LinSM_MainFunction();

	TEST_CHECK(LinIf_SetTrcvMode_fake.arg1_val == LIN_TRCV_MODE_SLEEP);

}
/**
  @brief Test trybu LINSM_NO_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie trybu LINSM_NO_COM w stanie LINSM_INIT
*/
void test_LinSM_MainFunction_NO_COM(void) {
    ResetAllMocks();

    LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_NO_COM;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 10;
    LinSM_WakeupConfirmed[0] = TRUE;

    LinSM_MainFunction();

	TEST_CHECK(LinSM_Timer[0].requestTimer == 0); 
    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
    TEST_CHECK(LinSM_Mode == LINSM_FULL_COM); // Powinno przejść do pełnej komunikacji
    TEST_CHECK(LinSM_SubState == LINSM_RUN_COMMUNICATION); // Powinno przejść do RUN_COMMUNICATION
}
/**
  @brief Test przekroczenia timeoutu LINSM_NO_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie gdy timeout jest wywołany.
*/
void test_LinSM_MainFunction_NO_COM_timeout(void) {
    ResetAllMocks();

    LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_NO_COM;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 1;
    LinSM_WakeupConfirmed[0] = FALSE;

    LinSM_MainFunction();

    TEST_CHECK(LinSM_Timer[0].timerActive == TRUE); // Timer powinien zostać wyłączony
    TEST_CHECK(LinSM_Mode == LINSM_NO_COM); // Powinno przejść do pełnej komunikacji
    TEST_CHECK(LinSM_Timer[0].retryCount == 1); // Powinno przejść do RUN_COMMUNICATION
	TEST_CHECK(Det_ReportRuntimeError_fake.arg3_val == LINSM_E_CONFIRMATION_TIMEOUT);
}
/**
  @brief Test ponownej próby wybudzenia po przekroczeniu timeoutu LINSM_NO_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie gdy timeout jest wywołany i układ próbuje się ponownie wybudzić.
*/
void test_LinSM_MainFunction_NO_COM_WakeUp_after_timeout(void) {
    ResetAllMocks();
	LinIf_WakeUp_fake.return_val = E_NOT_OK;
    LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_NO_COM;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 1;
    LinSM_WakeupConfirmed[0] = FALSE;

    LinSM_MainFunction();

    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); // Timer powinien zostać wyłączony
    TEST_CHECK(LinSM_Mode == LINSM_NO_COM); // Powinno przejść do pełnej komunikacji
    TEST_CHECK(LinSM_Timer[0].requestTimer == 0); // Powinno przejść do RUN_COMMUNICATION
	TEST_CHECK(Det_ReportRuntimeError_fake.arg3_val == LINSM_E_CONFIRMATION_TIMEOUT);
}
/**
  @brief Test przekroczenia maskymalnej liczby ponownych próby wybudzania po przekroczeniu timeoutu LINSM_NO_COM w maszynie stanów.

  Funkcja sprawdzająca zachowanie układu po przekroczeniu maksymalnej liczby prób jego wybudzania.
*/
void test_LinSM_MainFunction_NO_COM_RetryCount(void) {
    ResetAllMocks();
	//LinIf_WakeUp_fake.return_val = E_OK;
    LinSM_MainState = LINSM_INIT;
	LinSM_Mode = LINSM_NO_COM;
	LinSM_Timer[0].timerActive = TRUE;
	LinSM_Timer[0].requestTimer = 0;
	LinSM_Timer[0].retryCount = 5;
    LinSM_WakeupConfirmed[0] = FALSE;

    LinSM_MainFunction();

    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
    TEST_CHECK(LinSM_Mode == LINSM_NO_COM); 
    TEST_CHECK(LinSM_Timer[0].requestTimer == 0);
	TEST_CHECK(LinSM_Timer[0].retryCount == 0);
	TEST_CHECK(ComM_BusSM_ModeIndication_fake.arg1_val == COMM_NO_COMMUNICATION);
	TEST_CHECK(BswM_LinSM_CurrentState_fake.arg1_val == LINSM_NO_COM);
}
/**
  @brief Test trybu LINSM_FULL_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie trybu LINSM_FULL_COM w stanie LINSM_INIT
*/
void test_LinSM_MainFunction_FULL_COM(void) {
    ResetAllMocks();
	
	LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_FULL_COM;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 10;
    LinSM_ScheduleConfirmed[0] = TRUE;

    LinSM_MainFunction();

	TEST_CHECK(LinSM_Timer[0].requestTimer == 0); 
    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
	TEST_CHECK(ComM_BusSM_ModeIndication_fake.arg1_val == COMM_FULL_COMMUNICATION);
	TEST_CHECK(BswM_LinSM_CurrentState_fake.arg1_val == LINSM_FULL_COM);

}
/**
  @brief Test przekroczenia timeoutu LINSM_FULL_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie gdy timeout jest wywołany.
*/
void test_LinSM_MainFunction_FULL_COM_timeout(void) {
    ResetAllMocks();

    LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_FULL_COM;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 1;
    LinSM_ScheduleConfirmed[0] = FALSE;

    LinSM_MainFunction();

    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
    TEST_CHECK(LinSM_Mode == LINSM_FULL_COM); 
	TEST_CHECK(Det_ReportRuntimeError_fake.arg3_val == LINSM_E_CONFIRMATION_TIMEOUT);
	TEST_CHECK(ComM_BusSM_ModeIndication_fake.arg1_val == COMM_FULL_COMMUNICATION);
	TEST_CHECK(BswM_LinSM_CurrentState_fake.arg1_val == LINSM_FULL_COM);
}
/**
  @brief Test błędnego wywołania harmonogramu w trybie LINSM_FULL_COM w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie gdy zgłoszona jest flaga błędu podczas próby zapisania harmonogramu.
*/
void test_LinSM_MainFunction_FULL_COM_sch_req_err(void) {
    ResetAllMocks();

    LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_FULL_COM;
	LinSM_ScheduleRequestError[0] = TRUE;

    LinSM_MainFunction();

    TEST_CHECK(LinSM_ScheduleRequestError[0] == FALSE); 

}
/**
  @brief Test trybu LINSM_GOTO_SLEEP w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie substanu LINSM_GOTO_SLEEP w trybie LINSM_FULL_COM w stanie LINSM_INIT
*/
void test_LinSM_MainFunction_GO_TO_SLEEP(void) {
    ResetAllMocks();
	
	LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_GOTO_SLEEP;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 10;
    LinSM_GotoSleepConfirmed[0] = TRUE;

    LinSM_MainFunction();

	TEST_CHECK(LinSM_Timer[0].requestTimer == 0); 
    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
	TEST_CHECK(LinSM_GotoSleepConfirmed[0] == FALSE);
	TEST_CHECK(LinSM_Mode == LINSM_NO_COM);
	TEST_CHECK(LinSM_SubState == LINSM_RUN_COMMUNICATION);
}
/**
  @brief Test przekroczenia timeoutu LINSM_GOTO_SLEEP w maszynie stanów.

  Funkcja sprawdzająca poprawne działanie gdy timeout jest wywołany.
*/
void test_LinSM_MainFunction_GO_TO_SLEEP_timeout(void) {
    ResetAllMocks();
	
	LinSM_MainState = LINSM_INIT;
    LinSM_Mode = LINSM_FULL_COM;
	LinSM_SubState = LINSM_GOTO_SLEEP;
    LinSM_Timer[0].timerActive = TRUE;
    LinSM_Timer[0].requestTimer = 0;
    LinSM_GotoSleepConfirmed[0] = FALSE;

    LinSM_MainFunction();

	TEST_CHECK(LinSM_Timer[0].requestTimer == 0); 
    TEST_CHECK(LinSM_Timer[0].timerActive == FALSE); 
	TEST_CHECK(LinSM_Mode == LINSM_FULL_COM);
	TEST_CHECK(LinSM_SubState == LINSM_RUN_COMMUNICATION);
	TEST_CHECK(Det_ReportRuntimeError_fake.arg3_val == LINSM_E_CONFIRMATION_TIMEOUT);
}
/*
  Lista testów - wpisz tutaj wszystkie funkcje które mają być wykonane jako testy.
*/
TEST_LIST = {
	{ "Test of LinSM_Init", test_LinSM_Init },
	{ "Test of LinSM_ScheduleRequest", test_LinSM_ScheduleRequest },
	{ "Test of LinSM_ScheduleRequest_E_NOT_OK", test_LinSM_ScheduleRequest_error },
	{ "Test of LinSM_GetVersionInfo", test_LinSM_GetVersionInfo },
    { "Test of LinSM_GetCurrentComMode", test_LinSM_GetCurrentComMode },   /* Format to { "nazwa testu", nazwa_funkcji } */
	{ "Test of LinSM_RequestComMode", test_LinSM_RequestComMode },
	{ "Test of LinSM_RequestComMode_E_NOT_OK", test_LinSM_RequestComMode_error },
	{ "Test of LinSM_MainFunction_Uninit", test_LinSM_MainFunction_Uninit },
	{ "Test of LinSM_MainFunction_NO_COM", test_LinSM_MainFunction_NO_COM },
	{ "Test of LinSM_MainFunction_NO_COM_timeout", test_LinSM_MainFunction_NO_COM_timeout },
	{ "Test of LinSM_MainFunction_NO_COM_WakeUp_after_timeout", test_LinSM_MainFunction_NO_COM_WakeUp_after_timeout },
	{ "Test of LinSM_MainFunction_NO_COM_RetryCount", test_LinSM_MainFunction_NO_COM_RetryCount },
	{ "Test of LinSM_MainFunction_FULL_COM", test_LinSM_MainFunction_FULL_COM },
	{ "Test of LinSM_MainFunction_FULL_COM_timeout", test_LinSM_MainFunction_FULL_COM_timeout },
	{ "Test of LinSM_MainFunction_FULL_COM_sch_req_err", test_LinSM_MainFunction_FULL_COM_sch_req_err },
	{ "Test of LinSM_MainFunction_GO_TO_SLEEP", test_LinSM_MainFunction_GO_TO_SLEEP },
	{ "Test of LinSM_MainFunction_GO_TO_SLEEP_timeout", test_LinSM_MainFunction_GO_TO_SLEEP_timeout },
    { NULL, NULL }                                        /* To musi być na końcu */
};


