#include "HktGraph.h"
#include "HktBehavior.h"

struct FTagNode
{
	FHktTag Tag;
	TArray<IHktBehavior*> BehaviorRefs;
};

/** 
* subject������ ������ �����.
* �� ������ Ŭ�� ���� ������ �°� �����Ѵ�.
 */
struct FSubjectNode
{
	TArray<TUniquePtr<IHktBehavior>> Behaviors;
	TMap<FHktTag, FTagNode> TagNodes;
};

struct FHktGraph::FContext
{
	TMap<FHktId, TUniquePtr<IHktBehavior>> Behaviors;
	TMap<FHktId, FSubjectNode> SubjectNodes;
};

FHktGraph::FHktGraph()
	: Context(MakeUnique<FContext>())
{
}

FHktGraph::~FHktGraph()
{
}

IHktBehavior& FHktGraph::AddBehavior(TUniquePtr<IHktBehavior>&& InBehavior)
{
	check(Context);

	FSubjectNode& SubjectNode = Context->SubjectNodes.FindOrAdd(InBehavior->GetSubjectId());
	FHktTagContainer Tags = InBehavior->GetTags();
	for (const FHktTag& Tag : Tags)
	{
		FTagNode* TagNode = SubjectNode.TagNodes.Find(Tag);
		if (TagNode == nullptr)
		{
			TagNode = &SubjectNode.TagNodes.Emplace(Tag);
		}
		TagNode->BehaviorRefs.AddUnique(InBehavior.Get());
	}

	TUniquePtr<IHktBehavior>& Behavior = Context->Behaviors.Emplace(InBehavior->GetBehaviorId(), MoveTemp(InBehavior));
	return *Behavior;
}

const IHktBehavior* FHktGraph::FindBehavior(FHktId InBehaviorId) const
{
	check(Context);

	const TUniquePtr<IHktBehavior>* BehaviorPtr = Context->Behaviors.Find(InBehaviorId);
	if (BehaviorPtr == nullptr)
		return nullptr;

	return BehaviorPtr->Get();
}

void FHktGraph::RemoveBehavior(FHktId InBehaviorId)
{
	check(Context);

	const TUniquePtr<IHktBehavior>* BehaviorPtr = Context->Behaviors.Find(InBehaviorId);
	if (BehaviorPtr == nullptr)
		return;

	RemoveBehavior(**BehaviorPtr);
}

void FHktGraph::RemoveBehavior(const IHktBehavior& InBehavior)
{
	FSubjectNode* SubjectNode = Context->SubjectNodes.Find(InBehavior.GetSubjectId());
	if (SubjectNode == nullptr)
		return;

	bool bExist = false;
	FHktTagContainer Tags = InBehavior.GetTags();
	for (const FHktTag& Tag : Tags)
	{
		FTagNode* TagNode = SubjectNode->TagNodes.Find(Tag);
		if (TagNode)
		{
			TagNode->BehaviorRefs.Remove(const_cast<IHktBehavior*>(&InBehavior));
			bExist = true;
		}
	}

	if (bExist == false)
	{
		Context->SubjectNodes.Remove(InBehavior.GetSubjectId());
	}

	Context->Behaviors.Remove(InBehavior.GetBehaviorId());
}

void FHktGraph::RemoveSubject(FHktId InSubjectId)
{
	Context->SubjectNodes.Remove(InSubjectId);
}