#pragma once

#include "CoreMinimal.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

/**
 * @brief USTRUCT 패킷과 TArray<uint8> 사이의 직렬화/역직렬화를 담당하는 헬퍼 클래스
 */
class FHktStructSerializer
{
public:
    /**
     * @brief UStruct를 TArray<uint8>로 직렬화합니다.
     * @param InStruct 직렬화할 UStruct 객체의 참조
     * @return 직렬화된 데이터가 저장될 배열
     */
    template<typename T>
    static TArray<uint8> SerializeStructToBytes(const T& InStruct)
    {
		TArray<uint8> OutBytes;
        FMemoryWriter Writer(OutBytes, true); // true = bIsPersistent
        FStructOnScope StructOnScope(TBaseStructure<T>::Get());
        *reinterpret_cast<T*>(StructOnScope.GetStructMemory()) = InStruct;

        // UStruct::SerializeTaggedProperties는 리플렉션 기반으로 직렬화 수행
        StructOnScope.GetStruct()->SerializeTaggedProperties(Writer, StructOnScope.GetStructMemory(), nullptr, nullptr);

        return OutBytes;
    }

    /**
     * @brief TArray<uint8>를 UStruct로 역직렬화합니다.
     * @param InBytes 역직렬화할 데이터 배열
     * @param StructDefinition 역직렬화 결과로 만들 UStruct의 타입 정보
     * @return 역직렬화 성공 여부
     */
    template<typename T>
    static bool DeserializeStructFromBytes(const TArray<uint8>& InBytes, T& OutStructData)
    {
        FMemoryReader Reader(InBytes, true);
        FStructOnScope StructOnScope(TBaseStructure<T>::Get());

        StructOnScope.GetStruct()->SerializeTaggedProperties(Reader, StructOnScope.GetStructMemory(), nullptr, nullptr);

        OutStructData = *reinterpret_cast<T*>(StructOnScope.GetStructMemory());

        return true;
    }
};
