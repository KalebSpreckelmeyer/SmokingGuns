#pragma once

#include <RE/Fallout.h>

// CommonLibF4 exposes the runtime RTTI for Fallout 4 connect-point data, but
// this snapshot does not expose the concrete class layout.  This is the layout
// used by F4SE when reading the "CPA" NiExtraData block.
namespace RE::BSConnectPoint
{
	class Parents :
		public NiExtraData
	{
	public:
		static constexpr auto Ni_RTTI{
			RE::Ni_RTTI::BSConnectPoint__Parents
		};

		class ConnectPoint :
			public BSIntrusiveRefCounted
		{
		public:
			BSFixedString parent;    // 08
			BSFixedString name;      // 10
			NiQuaternion rotation;   // 18
			NiPoint3 position;       // 28
			float scale{ 1.0F };     // 34
		};

		BSTArray<ConnectPoint*> points;  // 18
	};

	static_assert(sizeof(Parents::ConnectPoint) == 0x38);
	static_assert(sizeof(Parents) == 0x30);
}
