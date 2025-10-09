#pragma once

#include "HktDef.h"

class IHktBehavior;

class HKTBASE_API FHktGraph
{
public:
	FHktGraph();
	~FHktGraph();

	IHktBehavior& AddBehavior(TUniquePtr<IHktBehavior>&& InBehavior);
	const IHktBehavior* FindBehavior(FHktId InBehaviorId) const;
	void RemoveBehavior(FHktId InBehaviorId);
	void RemoveBehavior(const IHktBehavior& InBehavior);
	void RemoveSubject(FHktId InSubjectId);

private:
	struct FContext;
	TUniquePtr<FContext> Context;
};