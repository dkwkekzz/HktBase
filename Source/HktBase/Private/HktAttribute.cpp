#include "HktAttribute.h"

// --- ���� ���� ---
class IHktAttribute;

// --- �ڵ� ID ������ ���� ���� ---

/**
 * @brief Behavior Ÿ�Ժ��� ���� ID�� �����ϴ� Ŭ����
 */
class FAttributeTypeIdGenerator
{
public:
    static uint32 GetNextId()
    {
        // 0�� ��ȿ���� ���� ������ ���� �� �����Ƿ� 1���� �����մϴ�.
        static uint32 Counter = 1;
        return Counter++;
    }
};

/**
 * @brief ���ø��� ����Ͽ� �� Behavior ������ Ÿ�Կ� ���� ���� ID�� ������ Ÿ�ӿ� �Ҵ��մϴ�.
 * @tparam T USTRUCT�� ���ǵ� ������ Ÿ��
 * @return �ش� Ÿ���� ���� ID
 */
template<typename T>
uint32 GetAttributeTypeId()
{
    static const uint32 Id = FAttributeTypeIdGenerator::GetNextId();
    return Id;
}

// ���丮�� �̸� �����մϴ�.
class FHktAttributeFactory;

/**
 * @brief ������(USTRUCT)�� ����(Execute)�� �����ϴ� CRTP ��� ���ø� Ŭ�����Դϴ�.
 * @tparam Derived �Ļ��� Behavior Ŭ���� Ÿ�� (��: FHktPlayerBehavior)
 * @tparam DataType �� Behavior�� ����� USTRUCT ������ Ÿ�� (��: FHktPlayerData)
 */
template<typename Derived, typename DataType>
class THktAttribute
{
public:
    // ���� �� ������ ����ü�� ������ ����(move)�� ���� �޾� ����� �����մϴ�.
    explicit THktAttribute(DataType&& InData) : Data(MoveTemp(InData)) {}
    virtual ~THktAttribute() override = default;

    // ���ø� ��� ID �����⸦ ����Ͽ� ���� ID�� �ڵ����� �ο��մϴ�.
    virtual uint32 GetTypeId() const
    {
        return GetAttributeTypeId<DataType>();
    }

private:
    /**
     * @brief �ڵ� ����� ���� ���� ����ü�Դϴ�.
     * �� ����ü�� static �ν��Ͻ��� ������ �� �����ڰ� ȣ��Ǿ� ���丮�� Behavior�� �ڵ����� ����մϴ�.
     */
    struct FAutoRegisterer
    {
        FAutoRegisterer()
        {
            FHktAttributeFactory::Register(GetAttributeTypeId<DataType>(), []() -> TUniquePtr<IHktAttribute>
                {
                    return MakeUnique<Derived>();
                });
        }
    };

    // static �ν��Ͻ��� �����Ͽ� FAutoRegisterer�� �����ڸ� ȣ���ϵ��� �մϴ�.
    static FAutoRegisterer AutoRegisterer;

protected:
    // �Ļ� Ŭ������ �����Ϳ� ������ �� �ֵ��� protected ����� �����մϴ�.
    DataType Data;
};

// static ��� ������ Ŭ���� �ܺο��� �����մϴ�.
template<typename Derived, typename DataType>
typename THktAttribute<Derived, DataType>::FAutoRegisterer THktAttribute<Derived, DataType>::AutoRegisterer;

/**
 * @brief ���ŵ� �����͸� ������� Behavior �ν��Ͻ��� �����ϴ� ���丮�Դϴ�.
 */
class FHktAttributeFactory
{
public:
    // ������ �Լ� Ÿ�� ���� (USTRUCT �����͸� �޾� Behavior�� ����)
    using FAttributeCreator = TFunction<TUniquePtr<IHktAttribute>()>;

    static void Register(uint32 InTypeId, FAttributeCreator InCreator)
    {
        GetRegistry().Add(InTypeId, InCreator);
    }

    static TUniquePtr<IHktAttribute> Create(uint32 InTypeId)
    {
        const FAttributeCreator* Creator = GetRegistry().Find(InTypeId);
        if (!Creator)
        {
            return nullptr;
        }

        return (*Creator)(); // ��ϵ� ������ �Լ��� ȣ���Ͽ� Behavior ����
    }

private:
    static TMap<uint32, FAttributeCreator>& GetRegistry()
    {
        static TMap<uint32, FAttributeCreator> Registry;
        return Registry;
    }
};
