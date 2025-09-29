#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Delegates/Delegate.h"
#include "Templates/Function.h"

// HKT 시스템에서 사용하는 ID 타입과 유효하지 않은 ID 상수 정의
using FHktId = int64;
constexpr FHktId InvalidHktId = -1;
