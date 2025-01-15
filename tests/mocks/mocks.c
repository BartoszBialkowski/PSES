#include "../../include/fff.h"

/* Definiowanie globalnych zmiennych FFF */
DEFINE_FFF_GLOBALS;

/* Mockowanie funkcji z LinIf */
FAKE_VALUE_FUNC(Std_ReturnType, LinIf_ScheduleRequest, NetworkHandleType, LinIf_SchHandleType);
FAKE_VALUE_FUNC(Std_ReturnType, LinIf_SetTrcvMode, NetworkHandleType, uint8);
FAKE_VALUE_FUNC(Std_ReturnType, LinIf_WakeUp, NetworkHandleType);
FAKE_VALUE_FUNC(Std_ReturnType, LinIf_GotoSleep, NetworkHandleType);

/* Mockowanie funkcji z Det */
FAKE_VOID_FUNC(Det_ReportError, uint16, uint8, uint8, uint8);

/* Mockowanie funkcji z ComM */
FAKE_VOID_FUNC(ComM_BusSM_ModeIndication, NetworkHandleType, ComM_ModeType);

/* Mockowanie funkcji z BswM */
FAKE_VOID_FUNC(BswM_LinSM_CurrentState, NetworkHandleType, LinSM_ModeType);
FAKE_VOID_FUNC(BswM_LinSM_CurrentSchedule, NetworkHandleType, LinIf_SchHandleType);
FAKE_VOID_FUNC(BswM_LinSM_CurrentComMode, NetworkHandleType, ComM_ModeType);
FAKE_VALUE_FUNC(Std_ReturnType, Det_ReportRuntimeError, uint16, uint8, uint8, uint8);

// Resetowanie wszystkich mocków
void ResetAllMocks(void) {
    RESET_FAKE(Det_ReportError);
    RESET_FAKE(LinIf_SetTrcvMode);
    RESET_FAKE(LinIf_WakeUp);
	RESET_FAKE(LinIf_GotoSleep);
	RESET_FAKE(ComM_BusSM_ModeIndication);
	RESET_FAKE(BswM_LinSM_CurrentState);
	RESET_FAKE(BswM_LinSM_CurrentSchedule);
	RESET_FAKE(BswM_LinSM_CurrentComMode);
	RESET_FAKE(Det_ReportRuntimeError);
	FFF_RESET_HISTORY();
}