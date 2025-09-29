/**
 * @file HktEventBus.h
 * @brief NodeIndex와 GameplayTag를 Key로, TPayload 타입별 이벤트를 관리하는 라우터 객체입니다.
 * @tparam TPayload 이벤트 발생 시 전달할 인자의 타입입니다.
 */

#pragma once

#include "HktDef.h"

 /**
  * @brief NodeIndex와 GameplayTag를 Key로, 특정 데이터 타입(TPayload)의 이벤트를 라우팅하는 싱글톤 클래스입니다.
  * GameplayTag의 계층 구조를 활용하여 다양한 방식의 이벤트 전파가 가능합니다.
  * @tparam TPayload 이벤트 페이로드로 사용할 데이터 타입입니다.
  */
template<typename TPayload>
struct THktEventBus
{
public:
    /**
     * @brief TPayload 타입의 인자 하나를 받는 멀티캐스트 델리게이트입니다.
     * const&로 인자를 전달하여 불필요한 복사를 방지합니다.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FBehaviorEvent, const TPayload& /* Payload */);

    /**
     * @brief 특정 노드에 속한 이벤트들을 GameplayTag를 키로 하여 관리하는 라우터입니다.
     * TArray 대신 TMap을 사용하여 특정 태그에 대한 이벤트 접근을 O(1)에 가깝게 최적화합니다.
     */
    struct FEventRouter
    {
        TMap<FGameplayTag, FBehaviorEvent> Events;
    };

    /**
     * @brief EventBus의 싱글톤 인스턴스를 반환합니다.
     * @return THktEventBus의 싱글톤 인스턴스
     */
    static THktEventBus& Get()
    {
        static THktEventBus Instance;
        return Instance;
    }

    /**
     * @brief 특정 태그에 대한 이벤트 델리게이트를 가져옵니다. 이벤트 바인딩(Bind) 시 사용됩니다.
     * 해당 태그의 이벤트가 존재하지 않으면 새로 생성하여 반환합니다.
     * @param NodeIndex 이벤트를 식별하는 노드의 인덱스입니다.
     * @param Tag 바인딩할 이벤트의 정확한 GameplayTag입니다.
     * @return 해당 태그에 대한 FBehaviorEvent 델리게이트의 레퍼런스입니다.
     */
    FBehaviorEvent& GetEvent(FHktId NodeIndex, const FGameplayTag& Tag)
    {
        // FindOrAdd를 사용하여 노드에 해당하는 라우터를 찾거나 없으면 추가합니다.
        FEventRouter& Router = EventRouterMap.FindOrAdd(NodeIndex);
        // 라우터 내에서 특정 태그에 해당하는 이벤트를 찾거나 없으면 추가합니다.
        return Router.Events.FindOrAdd(Tag);
    }

    FORCEINLINE FBehaviorEvent& GetEvent(FHktId NodeIndex)
    {
        return GetEvent(NodeIndex, FGameplayTag());
    }

    /**
     * @brief 지정된 태그와 '정확히' 일치하는 리스너에게만 이벤트를 전파(Broadcast)합니다.
     * @param NodeIndex 이벤트가 발생한 노드의 인덱스입니다.
     * @param Tag 전파할 이벤트의 GameplayTag입니다.
     * @param Payload 이벤트와 함께 전달할 데이터입니다.
     */
    void Broadcast(FHktId NodeIndex, const FGameplayTag& Tag, const TPayload& Payload)
    {
        // 해당 노드에 라우터가 존재하는지 확인합니다.
        if (FEventRouter* Router = EventRouterMap.Find(NodeIndex))
        {
            // 라우터 내에서 정확히 일치하는 태그의 이벤트를 찾습니다.
            if (FBehaviorEvent* Event = Router->Events.Find(Tag))
            {
                // 델리게이트에 바인딩된 함수가 있는지 확인 후 이벤트를 전파합니다.
                if (Event->IsBound())
                {
                    Event->Broadcast(Payload);
                }
            }
        }
    }

private:
    // private 생성자로 싱글톤 패턴을 강제합니다.
    THktEventBus() = default;
    ~THktEventBus() = default;
    THktEventBus(const THktEventBus&) = delete;
    THktEventBus& operator=(const THktEventBus&) = delete;

    /**
     * @brief NodeIndex를 키로, FEventRouter를 값으로 가지는 맵입니다.
     * 각 노드(예: 특정 캐릭터나 액터)별로 독립적인 이벤트 라우터를 관리합니다.
     */
    TMap<FHktId, FEventRouter> EventRouterMap;
};
