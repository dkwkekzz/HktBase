#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Templates/Function.h"
#include "HktTag.h"

// HKT 시스템에서 사용하는 ID 타입과 유효하지 않은 ID 값 정의
using FHktId = int64;
constexpr FHktId InvalidHktId = -1;
