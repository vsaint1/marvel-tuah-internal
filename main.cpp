/////////////////////
// D3D12 HOOK ImGui//
/////////////////////

#include "main.h"
#include "Globals.h"

int countnum = -1;
bool nopants_enabled = true;

//=========================================================================================================================//

typedef HRESULT(APIENTRY *Present12)(IDXGISwapChain *pSwapChain,
                                     UINT SyncInterval, UINT Flags);
Present12 oPresent = NULL;

typedef void(APIENTRY *DrawInstanced)(ID3D12GraphicsCommandList *dCommandList,
                                      UINT VertexCountPerInstance,
                                      UINT InstanceCount,
                                      UINT StartVertexLocation,
                                      UINT StartInstanceLocation);
DrawInstanced oDrawInstanced = NULL;

typedef void(APIENTRY *DrawIndexedInstanced)(
    ID3D12GraphicsCommandList *dCommandList, UINT IndexCountPerInstance,
    UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation,
    UINT StartInstanceLocation);
DrawIndexedInstanced oDrawIndexedInstanced = NULL;

typedef void(APIENTRY *ExecuteCommandLists)(ID3D12CommandQueue *queue,
                                            UINT NumCommandLists,
                                            ID3D12CommandList *ppCommandLists);
ExecuteCommandLists oExecuteCommandLists = NULL;

//=========================================================================================================================//

bool ShowMenu = false;
bool ImGui_Initialised = false;

namespace Process {
DWORD ID;
HANDLE Handle;
HWND Hwnd;
HMODULE Module;
WNDPROC WndProc;
int WindowWidth;
int WindowHeight;
LPCSTR Title;
LPCSTR ClassName;
LPCSTR Path;
} // namespace Process

namespace DirectX12Interface {
ID3D12Device *Device = nullptr;
ID3D12DescriptorHeap *DescriptorHeapBackBuffers;
ID3D12DescriptorHeap *DescriptorHeapImGuiRender;
ID3D12GraphicsCommandList *CommandList;
ID3D12CommandQueue *CommandQueue;

struct _FrameContext {
  ID3D12CommandAllocator *CommandAllocator;
  ID3D12Resource *Resource;
  D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
};

uintx_t BuffersCounts = -1;
_FrameContext *FrameContext;
} // namespace DirectX12Interface

namespace Chams {

inline SDK::UMaterial *material = nullptr;

inline SDK::UMaterialInstanceDynamic *chamsMaterial = nullptr;
inline SDK::UMaterialInstanceDynamic *chamsVisibleMaterial = nullptr;
inline SDK::UMaterialInstanceDynamic *chamsOccludedMaterial = nullptr;

void CreateMaterial(SDK::UWorld *world, const char *materialName);

void ApplyChams(SDK::USkeletalMeshComponent *mesh,
                SDK::UMaterialInstanceDynamic *materialDyn,
                SDK::FLinearColor visibleColor, SDK::FLinearColor occludedColor,
                bool visibility);

} // namespace Chams

namespace Chams {

void CreateMaterial(SDK::UWorld *world, const char *materialName) {

  Chams::material = SDK::UObject::FindObject<SDK::UMaterial>(materialName);

 

  if (Chams::material) {
    Chams::material->bDisableDepthTest = 1;
    Chams::material->Wireframe = 1;
    Chams::material->BlendMode = SDK::EBlendMode::BLEND_Translucent;
    Chams::material->MaterialDomain = SDK::EMaterialDomain::MD_Surface;
    Chams::material->AllowTranslucentCustomDepthWrites = 1;
    Chams::material->bIsBlendable = 1;
    Chams::material->LightmassSettings.EmissiveBoost = 2.0f;
    Chams::material->LightmassSettings.DiffuseBoost = 1.0f;
  }

  Chams::chamsMaterial =
      SDK::UKismetMaterialLibrary::CreateDynamicMaterialInstance(
          world, Chams::material, Utils::ToFName(L"ChamsMaterial"),
          SDK::EMIDCreationFlags::Transient);

  Chams::chamsVisibleMaterial =
      SDK::UKismetMaterialLibrary::CreateDynamicMaterialInstance(
          world, Chams::material, Utils::ToFName(L"ChamsVisibleMaterial"),
          SDK::EMIDCreationFlags::Transient);

  Chams::chamsOccludedMaterial =
      SDK::UKismetMaterialLibrary::CreateDynamicMaterialInstance(
          world, Chams::material, Utils::ToFName(L"ChamsOccludedMaterial"),
          SDK::EMIDCreationFlags::Transient);
}

void ApplyChams(SDK::USkeletalMeshComponent *mesh,
                SDK::UMaterialInstanceDynamic *materialDyn,
                SDK::FLinearColor visibleColor, SDK::FLinearColor occludedColor,
                bool visibility) {

  if (mesh == nullptr)
    return;

  if (Chams::material == nullptr)
    return;

  if (Chams::chamsVisibleMaterial == nullptr)
    return;

  if (Chams::chamsOccludedMaterial == nullptr)
    return;

  auto mats = mesh->GetMaterials();

  mesh->bRenderCustomDepth = true;
  mesh->CustomDepthStencilValue = 1;

  if (visibility) {

    chamsVisibleMaterial->SetVectorParameterValue(Utils::ToFName(L"Color"),
                                                  visibleColor);

    chamsVisibleMaterial->SetScalarParameterValue(
        Utils::ToFName(L"OutlineThickness"), 2.0f);

    chamsVisibleMaterial->SetVectorParameterValue(
        Utils::ToFName(L"OutlineColor"), {1.f, 1.0f, 1.0f, 1.0f});

    for (int i = 0; i < mats.Num(); i++) {
 

      if (mats[i] && mats[i] != chamsVisibleMaterial) {
        mesh->SetMaterial(i, chamsVisibleMaterial);
      }
    }

  } else {

    chamsOccludedMaterial->SetVectorParameterValue(Utils::ToFName(L"Color"),
                                                   occludedColor);

    chamsVisibleMaterial->SetScalarParameterValue(
        Utils::ToFName(L"OutlineThickness"), 2.0f);

    chamsVisibleMaterial->SetVectorParameterValue(
        Utils::ToFName(L"OutlineColor"), {1.f, 1.0f, 1.0f, 1.0f});

    for (int i = 0; i < mats.Num(); i++) {

      if (mats[i] && mats[i] != chamsOccludedMaterial) {
        mesh->SetMaterial(i, chamsOccludedMaterial);
      }
    }
  }
} // namespace Chams
} // namespace Chams

constexpr bool IsValidPtr(const void *ptr) {
  return ptr != nullptr && reinterpret_cast<uintptr_t>(ptr) >= 0x44000000;
}

void DrawDistance(SDK::FVector2D Location, float Distance) {
  if (!Settings::ESP::bDistance)
    return;

  char dist[64];
  sprintf_s(dist, "[%.fm]", Distance);

  ImVec2 TextSize = ImGui::CalcTextSize(dist);
  ImGui::GetForegroundDrawList()->AddText(
      ImVec2(Location.X - TextSize.x / 2 + 35,
             Location.Y - TextSize.y / 2 + 30),
      ImGui::GetColorU32({255, 155, 0, 255}), dist);
}


std::unordered_map<std::int32_t, SDK::FName> boneMap;

void DrawSkeleton(SDK::AMarvelBaseCharacter *marvelChar, ImColor color) {

  if (!marvelChar)
    return;

  // can crash here
  SDK::USkeletalMeshComponent *mesh = marvelChar->GetMesh();

  if (!mesh)
    return;

  const int32_t numBones = mesh->GetNumBones();

  for (int32_t boneIndex = 2; boneIndex < numBones; ++boneIndex) {

    if (boneIndex >= 255)
      continue;

    SDK::FName boneName = mesh->GetBoneName(boneIndex);

    SDK::FVector boneWorldPos = mesh->GetSocketLocation(boneName);

    SDK::FVector2D boneScreenPos;

    char boneStr[64];
    sprintf_s(boneStr, "%d", boneIndex);

    ImVec2 TextSize = ImGui::CalcTextSize(boneStr);

    if (G::pLocalPlayer->ProjectWorldLocationToScreen(boneWorldPos,
                                                      &boneScreenPos, false)) {

      SDK::FName parentBoneName = mesh->GetParentBone(boneName);

      SDK::FVector parentBoneWorldPos = mesh->GetSocketLocation(parentBoneName);
      SDK::FVector2D parentBoneScreenPos;

      if (G::pLocalPlayer->ProjectWorldLocationToScreen(
              parentBoneWorldPos, &parentBoneScreenPos, false)) {

        // ImGui::GetBackgroundDrawList()->AddText(
        //     {static_cast<float>(boneScreenPos.X),
        //      static_cast<float>(boneScreenPos.Y)},
        //     IM_COL32_WHITE, boneStr);

        ImGui::GetBackgroundDrawList()->AddLine(
            {static_cast<float>(boneScreenPos.X),
             static_cast<float>(boneScreenPos.Y)},
            {static_cast<float>(parentBoneScreenPos.X),
             static_cast<float>(parentBoneScreenPos.Y)},
            color);
      }
    }

    if (boneMap.find(boneIndex) == boneMap.end()) {
      boneMap.insert({boneIndex, boneName});
    }
  }
}

static bool showMaterials = false;
static bool showFonts = false;

void Menu() {

  ImGui::SetNextWindowSize({600, 400});
  ImGui::Begin(_X("MARVEL TUAH by thugzin3"));

  if (ImGui::BeginTabBar(_X("MenuTabs"))) {

    if (ImGui::BeginTabItem(_X("Aimbot"))) {
      ImGui::Checkbox(_X("Enabled"), &Settings::Aimbot::bEnabled);
      ImGui::Checkbox(_X("Rage"), &Settings::Aimbot::bRage);
      ImGui::Checkbox(_X("Prediction"), &Settings::Aimbot::bPrediction);
      ImGui::Checkbox(_X("Filter by Lower Health"),
                      &Settings::Aimbot::bFocusSquish);
      ImGui::Checkbox(_X("Visible Check"), &Settings::Aimbot::bVisibleCheck);
      HotKeyBinding();

      if (Settings::Aimbot::bRage) {
        ImGui::SliderFloat(_X("Rage Factor"), &Settings::Aimbot::RageFactor,
                           1.0f, 50.f, "%.f", 1.0f);
        ImGui::Checkbox(_X("Spin"), &Settings::Aimbot::bSpin);
        if (Settings::Aimbot::bSpin) {
          ImGui::SliderFloat(_X("Yaw"), &Settings::Aimbot::yaw, -180.0f, 180.0f,
                             "%.f", 1.0f);

          ImGui::SliderFloat(_X("Pitch"), &Settings::Aimbot::pitch, -90.f, 90.f,
                             "%.f", 1.0f);
          ImGui::SliderFloat(_X("Roll"), &Settings::Aimbot::roll, 0.0f, 50.f,
                             "%.f", 1.0f);
        }
      }

      ImGui::SliderFloat(_X("FOV"), &Settings::Aimbot::FOV, 1.0f, 360.f, "%.f",
                         2.0f);

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(_X("ESP"))) {
      ImGui::Checkbox(_X("Skeleton"), &Settings::ESP::bSkeleton);
      ImGui::Checkbox(_X("Healthbar"), &Settings::ESP::bHealthBar);
      ImGui::Checkbox(_X("SnapLines"), &Settings::ESP::bSnapLines);
      ImGui::Checkbox(_X("Box"), &Settings::ESP::bBox);
      ImGui::Checkbox(_X("Distance"), &Settings::ESP::bDistance);
      ImGui::Checkbox(_X("Name"), &Settings::ESP::bName);
      ImGui::Checkbox(_X("Chams"), &Settings::ESP::bChamsEnabled);
      if (Settings::ESP::bChamsEnabled) {

        ImGui::Checkbox(_X("Enemies"), &Settings::ESP::bChamsEnemies);
        ImGui::Checkbox(_X("LocalPlayer"), &Settings::ESP::bChamsLocal);
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(_X("Misc"))) {
      ImGui::Text(_X("Skin Changer"));

      static const char *items[]{"0", "1"};
      static int selectedItem = 0;

      auto heroId = G::pLocalMarvel == nullptr ? 0 : G::pLocalMarvel->HeroID;

      char hero[64];
      sprintf_s(hero, _X("[HeroID: %d]"), heroId);

      ImGui::Text(hero);

      if (ImGui::Combo("Skins", &selectedItem, items, IM_ARRAYSIZE(items))) {
      }

      ImGui::Text(_X("PRICE [7.000.000] VBUCKS"));
      ImGui::Button(_X("BUY"));
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(_X("Exploits"))) {

      ImGui::Text(_X("PRICE [18.000.000] VBUCKS"));
      ImGui::Button(_X("BUY"));
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(_X("Dev"))) {

      if (ImGui::Button(_X("Materials"))) {
        showMaterials = !showMaterials;
      }

      if (ImGui::Button(_X("Fonts"))) {
        showFonts = !showFonts;
      }


      if (showFonts) {
        std::ofstream logFile;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
          SDK::UObject *obj = SDK::UObject::GObjects->GetByIndex(i);
          if (obj == nullptr)
            continue;

          if (obj && obj->IsA(SDK::UFont::StaticClass())) {
            std::string documentsPath = Utils::GetDocumentsPath();

            if (!documentsPath.empty()) {

              logFile.open(documentsPath + "\\" + "fonts_dump.txt",
                           std::ios::app);

              if (logFile.is_open()) {
                logFile << "Font index(" << i << ")"
                        << "  " << "name (" << obj->GetFullName().c_str()
                        << ") \r\n";

                logFile.close();
              }
            }
            ImGui::Text(_X("Font (%d,%s)"), i, obj->GetFullName().c_str());
          }
        }
      }

      if (showMaterials) {
        std::ofstream logFile;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
          SDK::UObject *obj = SDK::UObject::GObjects->GetByIndex(i);
          if (obj == nullptr)
            continue;

          if (obj && obj->IsA(SDK::UMaterial::StaticClass())) {
            std::string documentsPath = Utils::GetDocumentsPath();

            if (!documentsPath.empty()) {

              logFile.open(documentsPath + "\\" + "materials_dump.txt",
                           std::ios::app);

              if (logFile.is_open()) {
                logFile << "Material index(" << i << ")"
                        << "  " << "name (" << obj->GetFullName().c_str()
                        << ") \r\n";

                logFile.close();
              }
            }
            ImGui::Text(_X("Material (%d,%s)"), i, obj->GetFullName().c_str());
          }
        }
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }


  ImGui::End();
}

void Aim(SDK::FRotator currentRotation, SDK::FRotator targetRotation,
         float rageFactor) {
  const float deltaTime = 1.0f / 60.0f;
  SDK::FRotator rotation = SDK::UKismetMathLibrary::RInterpTo(
      currentRotation, targetRotation, deltaTime, rageFactor);
  if (G::pLocalPlayer == nullptr)
    return;

  G::pLocalPlayer->SetControlRotation(rotation);
}


auto sSize = Utils::GetScreenSize();

void Shitbot() {

  SDK::UWorld *gWorld = SDK::UWorld::GetWorld();

  if (!IsValidPtr(gWorld))
    return;

  auto engine = SDK::UEngine::GetEngine();

  if (!engine)
    return;

  G::pEngine = engine;

  auto owningGameInstance = gWorld->OwningGameInstance;
  if (!IsValidPtr(owningGameInstance))
    return;

  auto localPlayer = owningGameInstance->LocalPlayers[0];
  if (!IsValidPtr(localPlayer))
    return;

  auto viewportClient = localPlayer->ViewportClient;
  if (!IsValidPtr(viewportClient))
    return;

  auto playerController = localPlayer->PlayerController;
  if (!IsValidPtr(playerController))
    return;

  G::pLocalPlayer = playerController;

  SDK::APawn *localPawn = nullptr;
  __try {

    localPawn = playerController->K2_GetPawn();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return;
  }

  if (!IsValidPtr(localPawn))
    return;

  auto world = SDK::UWorld::GetWorld();
  if (!IsValidPtr(world)) {
    return;
  }

  if (!IsValidPtr(world->OwningGameInstance) ||
      world->OwningGameInstance->LocalPlayers.Num() == 0) {
    return;
  }

  auto localController =
      world->OwningGameInstance->LocalPlayers[0]->PlayerController;
  if (!IsValidPtr(localController)) {
    return;
  }

  SDK::AActor *closestActor = nullptr;
  SDK::FRotator actorRotator{};
  SDK::FVector actorHead{};
  float maxDistance = FLT_MAX;

  SDK::TArray<SDK::AActor *> players;
  auto gameplayStatics =
      (SDK::UGameplayStatics *)SDK::UGameplayStatics::StaticClass();
  if (!IsValidPtr(gameplayStatics)) {
    return;
  }

  __try {

    gameplayStatics->GetAllActorsOfClass(
        world, SDK::AMarvelBaseCharacter::StaticClass(), &players);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return;
  }

  if (players.Num() == 0) {
    return;
  }

  for (int i = 0; i < players.Num(); i++) {

    __try {

      if (!players.IsValidIndex(i))
        continue;

      SDK::AActor *obj = players[i];
      if (!IsValidPtr(obj))
        continue;

      SDK::AMarvelBaseCharacter *baseClass = (SDK::AMarvelBaseCharacter *)obj;

      if (!IsValidPtr(baseClass))
        continue;

      SDK::UActorComponent *actorComp = baseClass->GetComponentByClass(
          SDK::UReactivePropertyComponent::StaticClass());

      if (!IsValidPtr(actorComp))
        continue;

      auto reactiveProp =
          static_cast<SDK::UReactivePropertyComponent *>(actorComp);

      if (!IsValidPtr(reactiveProp))
        continue;

      if (!IsValidPtr(reactiveProp->CachedAttributeSet))
        continue;

      auto marvelPlayerState =
          static_cast<SDK::AMarvelPlayerState *>(baseClass->PlayerState);

      if (!IsValidPtr(marvelPlayerState))
        continue;

      if (!marvelPlayerState->bIsAlive)
        continue;

      if (!IsValidPtr(G::pLocalPlayer->AcknowledgedPawn))
        continue;

      if (obj == G::pLocalPlayer->AcknowledgedPawn) {
        G::localTeam = marvelPlayerState->TeamID;

        G::pLocalMarvel = baseClass;

        continue;
      }

      if (!IsValidPtr(G::pLocalMarvel))
        continue;

      if (!G::pLocalMarvel->bCharacterValid)
        continue;

      if (G::pLocalMarvel->bReviving)
        continue;

      if (G::localTeam == marvelPlayerState->TeamID)
        continue;

      if (!IsValidPtr(reactiveProp->CachedAttributeSet))
        continue;

      auto health = reactiveProp->CachedAttributeSet->Health.CurrentValue;

      if (health <= 0)
        continue;

      if (Settings::Aimbot::bFocusSquish && health > 300)
        continue;

      if (!IsValidPtr(G::pLocalPlayer->PlayerCameraManager))
        continue;

      auto localPlayerPov =
          G::pLocalPlayer->PlayerCameraManager->CameraCachePrivate.POV.Location;

      if (!IsValidPtr(G::pLocalPlayer))
        continue;

      bool bVisible =
          G::pLocalPlayer->LineOfSightTo(baseClass, localPlayerPov, false);

      if (!bVisible && Settings::Aimbot::bVisibleCheck)
        continue;

      auto Mesh = baseClass->GetMesh();

      if (!IsValidPtr(Mesh))
        continue;

      if (!Mesh->DoesSocketExist(Utils::ToFName(_X(L"Head"))) ||
          !Mesh->DoesSocketExist(Utils::ToFName(_X(L"root"))))
        continue;

      SDK::FVector head_bone_pos =
          Mesh->GetSocketLocation(Utils::ToFName(_X(L"Head")));
      SDK::FVector feet_bone_pos =
          Mesh->GetSocketLocation(Utils::ToFName(_X(L"root")));
      SDK::FVector feet_middle_pos = {feet_bone_pos.X, feet_bone_pos.Y,
                                      head_bone_pos.Z};

      if (!IsValidPtr(localController))
        continue;

      SDK::FVector2D root2D{}, head2D{};

      if (localController->ProjectWorldLocationToScreen(feet_middle_pos, &root2D,
                                                     true) &&
          localController->ProjectWorldLocationToScreen(head_bone_pos, &head2D,
                                                     true)) {
        const float h = std::abs(head2D.Y - root2D.Y);
        const float w = h * 0.2f;

        if (Settings::Aimbot::bEnabled) {
          SDK::FVector cameraLoc =
              localController->PlayerCameraManager->GetCameraLocation();
          SDK::FRotator rot = SDK::UKismetMathLibrary::FindLookAtRotation(
              cameraLoc, head_bone_pos);

          SDK::FVector2D screenCenter = {sSize.width / 2, sSize.height / 2};
          float aimbotDistance =
              SDK::UKismetMathLibrary::Distance2D(head2D, screenCenter);

          ImGui::GetBackgroundDrawList()->AddLine(
              {static_cast<float>(sSize.width) / 2,
               static_cast<float>(sSize.height) / 2},
              {static_cast<float>(head2D.X), static_cast<float>(head2D.Y)},

              ImColor(255, 0, 0, 255));
          if (aimbotDistance < maxDistance &&
              aimbotDistance <= Settings::Aimbot::FOV) {
            maxDistance = aimbotDistance;
            closestActor = obj;
            actorRotator = rot;
            actorHead = head_bone_pos;
          }
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        U_LOG("SEH Exception caught! Code: 0x%08X - Continuing execution.",
            _exception_code());
      continue;
    }
  }

  if (closestActor && Settings::Aimbot::bEnabled) {
    if (GetAsyncKeyState(Settings::Aimbot::hotkey)) {
      if (!localController->PlayerCameraManager) {
        return;
      }

     Aim(localController->GetControlRotation(), actorRotator,
          Settings::Aimbot::RageFactor);

    
    }
  }
  
}

void ShitESP() {

  int x, y = 0;

  auto &io = ImGui::GetIO();

  SDK::UWorld *gWorld = SDK::UWorld::GetWorld();

  if (!gWorld)
    return;

  G::pWorld = gWorld;

  auto owningGameInstance = gWorld->OwningGameInstance;
  if (!owningGameInstance)
    return;

  auto localPlayer = owningGameInstance->LocalPlayers[0];
  if (!localPlayer)
    return;

  auto viewportClient = localPlayer->ViewportClient;
  if (!viewportClient)
    return;

  auto playerController = localPlayer->PlayerController;
  if (!playerController)
    return;

  G::pLocalPlayer = playerController;

  playerController->GetViewportSize(&x, &y);

  auto localPawn = playerController->K2_GetPawn();

  if (localPawn == nullptr)
    return;

  SDK::TArray<SDK::AActor *> Actors;

  G::pActors = Actors;

  auto gameplayStatics =
      (SDK::UGameplayStatics *)SDK::UGameplayStatics::StaticClass();

  if (!gameplayStatics)
    return;

  gameplayStatics->GetAllActorsOfClass(
      gWorld, SDK::AMarvelBaseCharacter::StaticClass(), &Actors);

  auto MathLib =
      (SDK::UKismetMathLibrary *)SDK::UKismetMathLibrary::StaticClass();
  if (!MathLib)
    return;

  for (int i = 0; i < Actors.Num(); i++) {

    if (!Actors.IsValidIndex(i))
      continue;

    auto obj = Actors[i];
    if (!obj)
      continue;

    auto baseClass = (SDK::AMarvelBaseCharacter *)obj;
    
    if (!baseClass)
      continue;

    auto actorComp = baseClass->GetComponentByClass(
        SDK::UReactivePropertyComponent::StaticClass());

    if (!actorComp)
      continue;

    auto reactiveProp =
        static_cast<SDK::UReactivePropertyComponent *>(actorComp);

    if (!reactiveProp)
      continue;

    auto marvelPlayerState =
        static_cast<SDK::AMarvelPlayerState *>(baseClass->PlayerState);

    if (!marvelPlayerState)
      continue;

    if (!marvelPlayerState->bIsAlive)
      continue;

    auto Mesh = baseClass->GetMesh();
    if (!Mesh)
      continue;

    if (obj == G::pLocalPlayer->AcknowledgedPawn) {

      G::localTeam = marvelPlayerState->TeamID;

      G::pLocalMarvel = baseClass;

      G::pLocalState = marvelPlayerState;


      if (Settings::Exploits::bAmmo) {
    
      }

      if (Settings::Exploits::bSpeed) {


      }
      if (Settings::Exploits::bDamage) {

      }

      if (Settings::Aimbot::bSpin) {

       
      }


      if (Settings::ESP::bChamsEnabled && Settings::ESP::bChamsLocal)
        Chams::ApplyChams(Mesh, Chams::chamsMaterial, {1.0f, 1.0f, 1.0f, 1.0f},
                          {1.f, 0.0f, 0.0f, 0.6f}, true);
      continue;
    }

    if (!G::pLocalMarvel)
      continue;

    if (!G::pLocalMarvel->bCharacterValid)
      continue;

    if (G::pLocalMarvel->bReviving)
      continue;

    if (G::localTeam == marvelPlayerState->TeamID)
      continue;

    if (!reactiveProp->CachedAttributeSet)
      continue;

    auto health = reactiveProp->CachedAttributeSet->Health.CurrentValue;

    if (health <= 0)
      continue;

    if (!Mesh)
      continue;

    if (!Mesh->DoesSocketExist(Utils::ToFName(_X(L"Head"))) ||
        !Mesh->DoesSocketExist(Utils::ToFName(_X(L"root"))))
      continue;

    SDK::FVector headLoc = Mesh->GetSocketLocation(Utils::ToFName(_X(L"Head")));
    SDK::FVector rootLoc = Mesh->GetSocketLocation(Utils::ToFName(_X(L"root")));

    SDK::FVector location = baseClass->K2_GetActorLocation();

    if (!baseClass)
      continue;

    if (!baseClass->PlayerState)
      continue;

    auto name = baseClass->PlayerState->GetPlayerName();
    bool bVisible = playerController->LineOfSightTo(obj, {0, 0, 0}, false);

    if (Settings::ESP::bChamsEnabled && Settings::ESP::bChamsEnemies)
      Chams::ApplyChams(Mesh, Chams::chamsMaterial, {1.0f, 1.0f, 1.0f, 1.0f},
                        {1.f, 0.0f, 0.0f, 0.6f}, bVisible);

    SDK::FVector2D head, Bottom;

    if (playerController->ProjectWorldLocationToScreen(rootLoc, &Bottom,
                                                       false) &&
        playerController->ProjectWorldLocationToScreen(headLoc, &head, false)) {

      float cornerHeight = abs(head.Y - Bottom.Y);
      float cornerWidth = cornerHeight * 0.6f;

      if (G::pLocalPlayer == nullptr)
        continue;

      if (G::pLocalPlayer->AcknowledgedPawn == nullptr)
        continue;

      if (G::pLocalPlayer->AcknowledgedPawn->RootComponent == nullptr)
        continue;

      auto delta =
          G::pLocalPlayer->AcknowledgedPawn->RootComponent->RelativeLocation -
          location;

      float distance =
          sqrt(powf(delta.X, 2) + powf(delta.Y, 2) + powf(delta.Z, 2)) / 1000.f;

      if (Settings::ESP::bBox)
        DrawBox(head.X - (cornerWidth / 2), head.Y, cornerWidth, cornerHeight,
                bVisible ? ImColor(0, 255, 0) : ImColor(255, 0, 0), 1.0f);

      if (Settings::ESP::bDistance) {
        char dist[64];
        sprintf_s(dist, "[%.fm]", distance);

        ImVec2 TextSize = ImGui::CalcTextSize(dist);
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(Bottom.X - TextSize.x / 2 + 35,
                   Bottom.Y - TextSize.y / 2 + 30),
            ImGui::GetColorU32({255, 155, 0, 255}), dist);
      }

      if (Settings::ESP::bName) {

        auto pName = name.IsValid() ? name.ToString().c_str()
                                          : _X("UNKNOWN");

        ImVec2 textSize = ImGui::CalcTextSize(
            name.IsValid() ? name.ToString().c_str()
                                 : _X("UNKNOWN"));
        ImVec2 textPos = {static_cast<float>(head.X) - textSize.x / 2,
                          static_cast<float>(head.Y) - textSize.y - 5.0f};
        ImGui::GetOverlayDrawList()->AddText(textPos, IM_COL32_WHITE,
                                             name.IsValid()
                                                 ? name.ToString().c_str()
                                                 : _X("UNKNOWN"));
      }

      if (Settings::ESP::bHealthBar) {

        float maxValue = reactiveProp->CachedAttributeSet->MaxHealth.BaseValue;
        health = max(0.0f, min(health, maxValue));

        char healthStr[64];
        sprintf_s(healthStr, "%.0f", health);

        ImDrawList *drawList = ImGui::GetBackgroundDrawList();

        float barWidth = 40.0f;
        float barHeight = 10.0f;

        ImVec2 barStart = {static_cast<float>(Bottom.X) - barWidth / 2,
                           static_cast<float>(Bottom.Y)};
        ImVec2 barEnd = {static_cast<float>(Bottom.X) + barWidth / 2,
                         static_cast<float>(Bottom.Y) + barHeight};
        ImVec2 healthEnd = {static_cast<float>(Bottom.X) - barWidth / 2 +
                                (barWidth * (health / maxValue)),
                            static_cast<float>(Bottom.Y) + barHeight};

        drawList->AddRectFilled(barStart, barEnd, IM_COL32(0, 0, 0, 155));

        drawList->AddRectFilled(barStart, healthEnd, IM_COL32(0, 255, 0, 250));

        drawList->AddRect(barStart, barEnd, IM_COL32(0, 0, 0, 200));

        ImVec2 textSize = ImGui::CalcTextSize(healthStr);
        ImVec2 textPos = {static_cast<float>(Bottom.X) - textSize.x / 2,
                          static_cast<float>(Bottom.Y) - textSize.y + 2.0f +
                              20.0f};

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), healthStr);
        ImGui::PopStyleVar(2);
      }
    }

    SDK::FVector2D screen;

    if (Settings::ESP::bSnapLines) {

      if (playerController->ProjectWorldLocationToScreen(location, &screen,
                                                         false)) {
        ImVec2 pos(screen.X, screen.Y);

        ImGui::GetOverlayDrawList()->AddLine(
            ImVec2(static_cast<float>(io.DisplaySize.x / 2),
                   static_cast<float>(io.DisplaySize.y)),
            pos, ImColor(255, 255, 255), 0.7);
      }
    }
  }
}

//=========================================================================================================================//

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);
LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (ShowMenu) {
    ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
    return true;
  }
  return CallWindowProc(Process::WndProc, hwnd, uMsg, wParam, lParam);
}

//=========================================================================================================================//

HRESULT APIENTRY hkPresent(IDXGISwapChain3 *pSwapChain, UINT SyncInterval,
                           UINT Flags) {
  if (!ImGui_Initialised) {
    if (SUCCEEDED(pSwapChain->GetDevice(
            __uuidof(ID3D12Device), (void **)&DirectX12Interface::Device))) {
      ImGui::CreateContext();

      ImGuiIO &io = ImGui::GetIO();
      (void)io;
      ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantTextInput ||
          ImGui::GetIO().WantCaptureKeyboard;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

      DXGI_SWAP_CHAIN_DESC Desc;
      pSwapChain->GetDesc(&Desc);
      Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
      Desc.OutputWindow = Process::Hwnd;
      Desc.Windowed =
          ((GetWindowLongPtr(Process::Hwnd, GWL_STYLE) & WS_POPUP) != 0) ? false
                                                                         : true;

      DirectX12Interface::BuffersCounts = Desc.BufferCount;
      DirectX12Interface::FrameContext = new DirectX12Interface::_FrameContext
          [DirectX12Interface::BuffersCounts];

      D3D12_DESCRIPTOR_HEAP_DESC DescriptorImGuiRender = {};
      DescriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      DescriptorImGuiRender.NumDescriptors = DirectX12Interface::BuffersCounts;
      DescriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

      if (DirectX12Interface::Device->CreateDescriptorHeap(
              &DescriptorImGuiRender,
              IID_PPV_ARGS(&DirectX12Interface::DescriptorHeapImGuiRender)) !=
          S_OK)
        return oPresent(pSwapChain, SyncInterval, Flags);

      ID3D12CommandAllocator *Allocator;
      if (DirectX12Interface::Device->CreateCommandAllocator(
              D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator)) != S_OK)
        return oPresent(pSwapChain, SyncInterval, Flags);

      for (size_t i = 0; i < DirectX12Interface::BuffersCounts; i++) {
        DirectX12Interface::FrameContext[i].CommandAllocator = Allocator;
      }

      if (DirectX12Interface::Device->CreateCommandList(
              0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator, NULL,
              IID_PPV_ARGS(&DirectX12Interface::CommandList)) != S_OK ||
          DirectX12Interface::CommandList->Close() != S_OK)
        return oPresent(pSwapChain, SyncInterval, Flags);

      D3D12_DESCRIPTOR_HEAP_DESC DescriptorBackBuffers;
      DescriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      DescriptorBackBuffers.NumDescriptors = DirectX12Interface::BuffersCounts;
      DescriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      DescriptorBackBuffers.NodeMask = 1;

      if (DirectX12Interface::Device->CreateDescriptorHeap(
              &DescriptorBackBuffers,
              IID_PPV_ARGS(&DirectX12Interface::DescriptorHeapBackBuffers)) !=
          S_OK)
        return oPresent(pSwapChain, SyncInterval, Flags);

      const auto RTVDescriptorSize =
          DirectX12Interface::Device->GetDescriptorHandleIncrementSize(
              D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle =
          DirectX12Interface::DescriptorHeapBackBuffers
              ->GetCPUDescriptorHandleForHeapStart();

      for (size_t i = 0; i < DirectX12Interface::BuffersCounts; i++) {
        ID3D12Resource *pBackBuffer = nullptr;
        DirectX12Interface::FrameContext[i].DescriptorHandle = RTVHandle;
        pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        DirectX12Interface::Device->CreateRenderTargetView(pBackBuffer, nullptr,
                                                           RTVHandle);
        DirectX12Interface::FrameContext[i].Resource = pBackBuffer;
        RTVHandle.ptr += RTVDescriptorSize;
      }

      ImGui_ImplWin32_Init(Process::Hwnd);
      ImGui_ImplDX12_Init(DirectX12Interface::Device,
                          DirectX12Interface::BuffersCounts,
                          DXGI_FORMAT_R8G8B8A8_UNORM,
                          DirectX12Interface::DescriptorHeapImGuiRender,
                          DirectX12Interface::DescriptorHeapImGuiRender
                              ->GetCPUDescriptorHandleForHeapStart(),
                          DirectX12Interface::DescriptorHeapImGuiRender
                              ->GetGPUDescriptorHandleForHeapStart());
      ImGui_ImplDX12_CreateDeviceObjects();
      ImGui::GetIO().ImeWindowHandle = Process::Hwnd;
      Process::WndProc = (WNDPROC)SetWindowLongPtr(
          Process::Hwnd, GWLP_WNDPROC, (__int3264)(LONG_PTR)WndProc);
    }
    ImGui_Initialised = true;
  }

  if (DirectX12Interface::CommandQueue == nullptr)
    return oPresent(pSwapChain, SyncInterval, Flags);

  if (GetAsyncKeyState(VK_F5) & 1)
    ShowMenu = !ShowMenu;
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  ImGui::GetIO().MouseDrawCursor = ShowMenu;


  __try {

    ShitESP();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    U_LOG("SEH Exception caught! Code: 0x%08X - Continuing execution.",
          _exception_code());

    return oPresent(pSwapChain, SyncInterval, Flags);
  }

  Shitbot();

  ImGui::GetBackgroundDrawList()->AddCircle(
      ImVec2(sSize.width / 2, sSize.height / 2), Settings::Aimbot::FOV,
      IM_COL32_WHITE, Settings::Aimbot::FOV, 1.f);

  ImGui::GetBackgroundDrawList()->AddText(
      {10, 30}, IM_COL32_WHITE,
      _X("ANGOLA_HOOK by thugzin3 _[unknowncheats.me]_ "));

  if (ShowMenu == true) {
    Menu();
  }

  ImGui::EndFrame();

  DirectX12Interface::_FrameContext &CurrentFrameContext =
      DirectX12Interface::FrameContext[pSwapChain->GetCurrentBackBufferIndex()];
  CurrentFrameContext.CommandAllocator->Reset();

  D3D12_RESOURCE_BARRIER Barrier;
  Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  Barrier.Transition.pResource = CurrentFrameContext.Resource;
  Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  DirectX12Interface::CommandList->Reset(CurrentFrameContext.CommandAllocator,
                                         nullptr);
  DirectX12Interface::CommandList->ResourceBarrier(1, &Barrier);
  DirectX12Interface::CommandList->OMSetRenderTargets(
      1, &CurrentFrameContext.DescriptorHandle, FALSE, nullptr);
  DirectX12Interface::CommandList->SetDescriptorHeaps(
      1, &DirectX12Interface::DescriptorHeapImGuiRender);

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
                                DirectX12Interface::CommandList);
  Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  DirectX12Interface::CommandList->ResourceBarrier(1, &Barrier);
  DirectX12Interface::CommandList->Close();
  DirectX12Interface::CommandQueue->ExecuteCommandLists(
      1, reinterpret_cast<ID3D12CommandList *const *>(
             &DirectX12Interface::CommandList));
  return oPresent(pSwapChain, SyncInterval, Flags);
}

//=========================================================================================================================//

void hkExecuteCommandLists(ID3D12CommandQueue *queue, UINT NumCommandLists,
                           ID3D12CommandList *ppCommandLists) {
  if (!DirectX12Interface::CommandQueue)
    DirectX12Interface::CommandQueue = queue;

  oExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
}

//=========================================================================================================================//

void APIENTRY hkDrawInstanced(ID3D12GraphicsCommandList *dCommandList,
                              UINT VertexCountPerInstance, UINT InstanceCount,
                              UINT StartVertexLocation,
                              UINT StartInstanceLocation) {

  return oDrawInstanced(dCommandList, VertexCountPerInstance, InstanceCount,
                        StartVertexLocation, StartInstanceLocation);
}

//=========================================================================================================================//

void APIENTRY hkDrawIndexedInstanced(ID3D12GraphicsCommandList *dCommandList,
                                     UINT IndexCountPerInstance,
                                     UINT InstanceCount,
                                     UINT StartIndexLocation,
                                     INT BaseVertexLocation,
                                     UINT StartInstanceLocation) {

  /*
  //cyberpunk 2077 no pants hack (low settings)
  if (nopants_enabled)
          if (IndexCountPerInstance == 10068 || //bargirl pants near
                  IndexCountPerInstance == 3576) //med range
                  return; //delete texture

  if (GetAsyncKeyState(VK_F12) & 1) //toggle key
          nopants_enabled = !nopants_enabled;


  //logger, hold down B key until a texture disappears, press END to log values
  of those textures if (GetAsyncKeyState('V') & 1) //- countnum--; if
  (GetAsyncKeyState('B') & 1) //+ countnum++; if (GetAsyncKeyState(VK_MENU) &&
  GetAsyncKeyState('9') & 1) //reset, set to -1 countnum = -1;

  if (countnum == IndexCountPerInstance / 100)
          if (GetAsyncKeyState(VK_END) & 1) //log
                  Log("IndexCountPerInstance == %d && InstanceCount == %d",
                          IndexCountPerInstance, InstanceCount);

  if (countnum == IndexCountPerInstance / 100)
          return;
  */

  return oDrawIndexedInstanced(dCommandList, IndexCountPerInstance,
                               InstanceCount, StartIndexLocation,
                               BaseVertexLocation, StartInstanceLocation);
}

//=========================================================================================================================//
bool bInitialized = false;

DWORD WINAPI MainThread(LPVOID lpParameter) {

  while (!bInitialized) {

    auto world = SDK::UWorld::GetWorld();

    if (world == nullptr) {

      continue;
    }

    G::pWorld = world;

    bInitialized = true;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Material M_FlareGlow.M_FlareGlow
  // Material M_Glow_6_002.M_Glow_6_002
  // Material M_30_Metal.M_30_Metal
  // Material M_Stone_6_702.M_Stone_6_702
  // Material M_Common_Crystal.M_Common_Crystal
  /// Material M_Ultimate_Glow.M_Ultimate_Glow
  // Material M_Versatile_5_100.M_Versatile_5_100

  Chams::CreateMaterial(G::pWorld, "Material M_30_Metal.M_30_Metal");

  bool WindowFocus = false;
  while (WindowFocus == false) {
    DWORD ForegroundWindowProcessID;
    GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundWindowProcessID);
    if (GetCurrentProcessId() == ForegroundWindowProcessID) {

      Process::ID = GetCurrentProcessId();
      Process::Handle = GetCurrentProcess();
      Process::Hwnd = GetForegroundWindow();

      RECT TempRect;
      GetWindowRect(Process::Hwnd, &TempRect);
      Process::WindowWidth = TempRect.right - TempRect.left;
      Process::WindowHeight = TempRect.bottom - TempRect.top;

      char TempTitle[MAX_PATH];
      GetWindowText(Process::Hwnd, TempTitle, sizeof(TempTitle));
      Process::Title = TempTitle;

      char TempClassName[MAX_PATH];
      GetClassName(Process::Hwnd, TempClassName, sizeof(TempClassName));
      Process::ClassName = TempClassName;

      char TempPath[MAX_PATH];
      GetModuleFileNameEx(Process::Handle, NULL, TempPath, sizeof(TempPath));
      Process::Path = TempPath;

      WindowFocus = true;
    }
  }
  bool InitHook = false;
  while (InitHook == false) {
    if (DirectX12::Init() == true) {
      CreateHook(54, (void **)&oExecuteCommandLists, hkExecuteCommandLists);
      CreateHook(140, (void **)&oPresent, hkPresent);
      // CreateHook(84, (void **)&oDrawInstanced, hkDrawInstanced);
      // CreateHook(85, (void **)&oDrawIndexedInstanced,
      // hkDrawIndexedInstanced);
      InitHook = true;
    }
  }
  return 0;
}

//=========================================================================================================================//

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
  switch (dwReason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hModule);
    Process::Module = hModule;
    /*GetModuleFileNameA(hModule, dlldir, 512);
    for (size_t i = strlen(dlldir); i > 0; i--) {
      if (dlldir[i] == '\\') {
        dlldir[i + 1] = 0;
        break;
      }
    }*/
    CreateThread(0, 0, MainThread, 0, 0, 0); // useless
    break;
  case DLL_PROCESS_DETACH:
    FreeLibraryAndExitThread(hModule, TRUE);
    DisableAll();
    break;
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  default:
    break;
  }
  return TRUE;
}

//=========================================================================================================================//

// D3D12 Methods Table:
//[0]   QueryInterface
//[1]   AddRef
//[2]   Release
//[3]   GetPrivateData
//[4]   SetPrivateData
//[5]   SetPrivateDataInterface
//[6]   SetName
//[7]   GetNodeCount
//[8]   CreateCommandQueue
//[9]   CreateCommandAllocator
//[10]  CreateGraphicsPipelineState
//[11]  CreateComputePipelineState
//[12]  CreateCommandList
//[13]  CheckFeatureSupport
//[14]  CreateDescriptorHeap
//[15]  GetDescriptorHandleIncrementSize
//[16]  CreateRootSignature
//[17]  CreateConstantBufferView
//[18]  CreateShaderResourceView
//[19]  CreateUnorderedAccessView
//[20]  CreateRenderTargetView
//[21]  CreateDepthStencilView
//[22]  CreateSampler
//[23]  CopyDescriptors
//[24]  CopyDescriptorsSimple
//[25]  GetResourceAllocationInfo
//[26]  GetCustomHeapProperties
//[27]  CreateCommittedResource
//[28]  CreateHeap
//[29]  CreatePlacedResource
//[30]  CreateReservedResource
//[31]  CreateSharedHandle
//[32]  OpenSharedHandle
//[33]  OpenSharedHandleByName
//[34]  MakeResident
//[35]  Evict
//[36]  CreateFence
//[37]  GetDeviceRemovedReason
//[38]  GetCopyableFootprints
//[39]  CreateQueryHeap
//[40]  SetStablePowerState
//[41]  CreateCommandSignature
//[42]  GetResourceTiling
//[43]  GetAdapterLuid
//[44]  QueryInterface
//[45]  AddRef
//[46]  Release
//[47]  GetPrivateData
//[48]  SetPrivateData
//[49]  SetPrivateDataInterface
//[50]  SetName
//[51]  GetDevice
//[52]  UpdateTileMappings
//[53]  CopyTileMappings
//[54]  ExecuteCommandLists
//[55]  SetMarker
//[56]  BeginEvent
//[57]  EndEvent
//[58]  Signal
//[59]  Wait
//[60]  GetTimestampFrequency
//[61]  GetClockCalibration
//[62]  GetDesc
//[63]  QueryInterface
//[64]  AddRef
//[65]  Release
//[66]  GetPrivateData
//[67]  SetPrivateData
//[68]  SetPrivateDataInterface
//[69]  SetName
//[70]  GetDevice
//[71]  Reset
//[72]  QueryInterface
//[73]  AddRef
//[74]  Release
//[75]  GetPrivateData
//[76]  SetPrivateData
//[77]  SetPrivateDataInterface
//[78]  SetName
//[79]  GetDevice
//[80]  GetType
//[81]  Close
//[82]  Reset
//[83]  ClearState
//[84]  DrawInstanced
//[85]  DrawIndexedInstanced
//[86]  Dispatch
//[87]  CopyBufferRegion
//[88]  CopyTextureRegion
//[89]  CopyResource
//[90]  CopyTiles
//[91]  ResolveSubresource
//[92]  IASetPrimitiveTopology
//[93]  RSSetViewports
//[94]  RSSetScissorRects
//[95]  OMSetBlendFactor
//[96]  OMSetStencilRef
//[97]  SetPipelineState
//[98]  ResourceBarrier
//[99]  ExecuteBundle
//[100] SetDescriptorHeaps
//[101] SetComputeRootSignature
//[102] SetGraphicsRootSignature
//[103] SetComputeRootDescriptorTable
//[104] SetGraphicsRootDescriptorTable
//[105] SetComputeRoot32BitConstant
//[106] SetGraphicsRoot32BitConstant
//[107] SetComputeRoot32BitConstants
//[108] SetGraphicsRoot32BitConstants
//[109] SetComputeRootConstantBufferView
//[110] SetGraphicsRootConstantBufferView
//[111] SetComputeRootShaderResourceView
//[112] SetGraphicsRootShaderResourceView
//[113] SetComputeRootUnorderedAccessView
//[114] SetGraphicsRootUnorderedAccessView
//[115] IASetIndexBuffer
//[116] IASetVertexBuffers
//[117] SOSetTargets
//[118] OMSetRenderTargets
//[119] ClearDepthStencilView
//[120] ClearRenderTargetView
//[121] ClearUnorderedAccessViewUint
//[122] ClearUnorderedAccessViewFloat
//[123] DiscardResource
//[124] BeginQuery
//[125] EndQuery
//[126] ResolveQueryData
//[127] SetPredication
//[128] SetMarker
//[129] BeginEvent
//[130] EndEvent
//[131] ExecuteIndirect
//[132] QueryInterface
//[133] AddRef
//[134] Release
//[135] SetPrivateData
//[136] SetPrivateDataInterface
//[137] GetPrivateData
//[138] GetParent
//[139] GetDevice
//[140] Present
//[141] GetBuffer
//[142] SetFullscreenState
//[143] GetFullscreenState
//[144] GetDesc
//[145] ResizeBuffers
//[146] ResizeTarget
//[147] GetContainingOutput
//[148] GetFrameStatistics
//[149] GetLastPresentCount