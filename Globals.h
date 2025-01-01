#pragma once
#include "Utils.h"
#include "game/SDK/Marvel_classes.hpp"

struct CPattern {
  const char *signature;
  const char *mask;
};

namespace G {
inline bool bInitialized = false;
inline SDK::UEngine *pEngine = nullptr;
inline SDK::UWorld *pWorld = nullptr;
inline SDK::APlayerController *pLocalPlayer = nullptr;
inline SDK::AMarvelBaseCharacter *pLocalMarvel = nullptr;
inline  SDK::TArray<SDK::AActor *> pActors{};
inline SDK::ULevel *pLevel = nullptr;
inline SDK::AMarvelPlayerState *pLocalState = nullptr;
std::int32_t localTeam = 0;

namespace Offsets {

inline std::int32_t PR_INDEX = 118;
};
}; // namespace G
