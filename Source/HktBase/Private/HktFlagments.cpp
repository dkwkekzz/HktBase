#include "HktFlagments.h"
#include "HktBehaviorFactory.h"

#define NEW_REGISTRAR(Flagmenet) FBehaviorRegistrar<Flagmenet> Flagmenet##Registrar
NEW_REGISTRAR(FDestroyFlagment);
NEW_REGISTRAR(FSampleFlagment);
NEW_REGISTRAR(FMoveFlagment);
NEW_REGISTRAR(FJumpFlagment);
NEW_REGISTRAR(FAttackFlagment);