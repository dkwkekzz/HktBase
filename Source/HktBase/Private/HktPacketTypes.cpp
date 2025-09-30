#include "HktPacketTypes.h"
#include "HktBehaviorFactory.h"

FBehaviorRegistrar<FMovePacket> MoveRegistrar;
FBehaviorRegistrar<FJumpPacket> JumpRegistrar;
FBehaviorRegistrar<FAttackPacket> AttackRegistrar;
FBehaviorRegistrar<FDestroyPacket> DestroyRegistrar;