#pragma once

#include <RE/Fallout.h>

namespace SmokingGuns::SmokeFollowTest
{
    void OnPlayerUpdate();

    bool Start(RE::TESObjectREFR* a_hostRef);
    bool Update();
    bool Stop();
    bool IsRunning();

    void DumpNodeTree(RE::NiAVObject* a_root);
}
