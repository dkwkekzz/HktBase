#pragma once

#include "HktDef.h"

class IHktBehavior;

class HKTBASE_API FHktGraph
{
public:
	FHktGraph();
	~FHktGraph();

	IHktBehavior& AddBehavior(TUniquePtr<IHktBehavior>&& InBehavior);
	void RemoveBehavior(FHktId InBehaviorId);

private:
	struct FContext;
	TUniquePtr<FContext> Context;
};