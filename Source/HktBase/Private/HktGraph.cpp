#include "HktGraph.h"
#include "HktBehavior.h"

struct FTagNode
{
	FHktTag Tag;
	TArray<IHktBehavior*> BehaviorRefs;
};

struct FSubjectNode
{
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

void FHktGraph::RemoveBehavior(FHktId InBehaviorId)
{
	check(Context);

	TUniquePtr<IHktBehavior>* BehaviorPtr = Context->Behaviors.Find(InBehaviorId);
	if (BehaviorPtr == nullptr)
		return;

	TUniquePtr<IHktBehavior>& Behavior = *BehaviorPtr;
	FSubjectNode* SubjectNode = Context->SubjectNodes.Find(Behavior->GetSubjectId());
	if (SubjectNode == nullptr)
		return;

	bool bExist = false;
	FHktTagContainer Tags = Behavior->GetTags();
	for (const FHktTag& Tag : Tags)
	{
		FTagNode* TagNode = SubjectNode->TagNodes.Find(Tag);
		if (TagNode)
		{
			TagNode->BehaviorRefs.Remove(Behavior.Get());
			bExist = true;
		}
	}

	if (bExist == false)
	{
		Context->SubjectNodes.Remove(Behavior->GetSubjectId());
	}

	Context->Behaviors.Remove(InBehaviorId);

}
