// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#pragma push_macro("verify")
#pragma push_macro("check")
#pragma push_macro("ensure")

#ifdef verify
#undef verify
#endif
#ifdef check
#undef check
#endif
#ifdef ensure
#undef ensure
#endif

THIRD_PARTY_INCLUDES_START
#include <grpcpp/grpcpp.h>
#include "google/protobuf/empty.pb.h"
#include "HktGrpcHeaders.generated.h"
THIRD_PARTY_INCLUDES_END

#pragma pop_macro("ensure")
#pragma pop_macro("check")
#pragma pop_macro("verify")
