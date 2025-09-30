#pragma once

#include "HktDef.h"

class IHktAttribute
{
public:
	virtual ~IHktAttribute() {}
	virtual uint32 GetTypeId() const = 0;
	virtual FHktTagContainer GetTags() const = 0;
};