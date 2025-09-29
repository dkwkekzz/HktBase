#pragma once

#include "HktDef.h"
#include "HktBehavior.h"

class FHktGraph
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