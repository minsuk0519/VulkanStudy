#pragma once

namespace GUI_ENUM
{
	enum DEFERRED_TYPE
	{
		DEFERRED_POSITION = 0,
		DEFERRED_NORMAL = 1,
		DEFERRED_LIGHT = 2,
	};

	enum LIGHT_COMPUTATION_TYPE
	{
		LIGHT_COMPUTE_PBR = 0,
		LIGHT_COMPUTE_BASIC,
		LIGHT_COMPUTE_MAX,
	};
}