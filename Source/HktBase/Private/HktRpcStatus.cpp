#include "HktRpcStatus.h"
#include "HktGrpc.h"

// Convert grpc::StatusCode to EHktRpcStatusCode
EHktRpcStatusCode ConvertStatusCode(grpc::StatusCode Code)
{
    switch (Code)
    {
    case grpc::StatusCode::OK: return EHktRpcStatusCode::OK;
    case grpc::StatusCode::CANCELLED: return EHktRpcStatusCode::CANCELLED;
    case grpc::StatusCode::UNKNOWN: return EHktRpcStatusCode::UNKNOWN;
    case grpc::StatusCode::INVALID_ARGUMENT: return EHktRpcStatusCode::INVALID_ARGUMENT;
    case grpc::StatusCode::DEADLINE_EXCEEDED: return EHktRpcStatusCode::DEADLINE_EXCEEDED;
    case grpc::StatusCode::NOT_FOUND: return EHktRpcStatusCode::NOT_FOUND;
    case grpc::StatusCode::ALREADY_EXISTS: return EHktRpcStatusCode::ALREADY_EXISTS;
    case grpc::StatusCode::PERMISSION_DENIED: return EHktRpcStatusCode::PERMISSION_DENIED;
    case grpc::StatusCode::UNAUTHENTICATED: return EHktRpcStatusCode::UNAUTHENTICATED;
    case grpc::StatusCode::RESOURCE_EXHAUSTED: return EHktRpcStatusCode::RESOURCE_EXHAUSTED;
    case grpc::StatusCode::FAILED_PRECONDITION: return EHktRpcStatusCode::FAILED_PRECONDITION;
    case grpc::StatusCode::ABORTED: return EHktRpcStatusCode::ABORTED;
    case grpc::StatusCode::OUT_OF_RANGE: return EHktRpcStatusCode::OUT_OF_RANGE;
    case grpc::StatusCode::UNIMPLEMENTED: return EHktRpcStatusCode::UNIMPLEMENTED;
    case grpc::StatusCode::INTERNAL: return EHktRpcStatusCode::INTERNAL;
    case grpc::StatusCode::UNAVAILABLE: return EHktRpcStatusCode::UNAVAILABLE;
    case grpc::StatusCode::DATA_LOSS: return EHktRpcStatusCode::DATA_LOSS;
    default: return EHktRpcStatusCode::UNKNOWN;
    }
}

FHktRpcStatus::FHktRpcStatus()
    : Code(EHktRpcStatusCode::UNKNOWN)
    , ErrorMessage(TEXT("Default-constructed status"))
{
}

FHktRpcStatus::FHktRpcStatus(const grpc::Status& Status)
{
    Code = ConvertStatusCode(Status.error_code());
    ErrorMessage = FString(UTF8_TO_TCHAR(Status.error_message().c_str()));
}

bool FHktRpcStatus::IsOk() const
{
    return Code == EHktRpcStatusCode::OK;
}
