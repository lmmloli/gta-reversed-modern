/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Object.h"
#include "Radar.h"
#include "BreakManager_c.h"
#include "Buoyancy.h"
#include "ObjectSaveStructure.h"
#include "Rope.h"
#include "Ropes.h"
#include "TheScripts.h"
#include "Shadows.h"
#include "FxPrtMult.h"

uint16& CObject::nNoTempObjects = *(uint16*)(0xBB4A70);
float& CObject::fDistToNearestTree = *(float*)0x8D0A20;
bool& CObject::bAircraftCarrierSamSiteDisabled = *(bool*)0x8D0A24;
bool& CObject::bArea51SamSiteDisabled = *(bool*)0xBB4A72;

void CObject::InjectHooks()
{
    RH_ScopedClass(CObject);
    RH_ScopedCategory("Entity/Object");

    RH_ScopedVirtualInstall(SetIsStatic, 0x5A0760);
    RH_ScopedVirtualInstall(CreateRwObject, 0x59F110);
    RH_ScopedVirtualInstall(ProcessControl, 0x5A2130);
    RH_ScopedVirtualInstall(Teleport, 0x5A17B0);
    RH_ScopedVirtualInstall(PreRender, 0x59FD50);
    RH_ScopedVirtualInstall(Render, 0x59F180);
    RH_ScopedVirtualInstall(SetupLighting, 0x554FA0);
    RH_ScopedVirtualInstall(RemoveLighting, 0x553E10);
    RH_ScopedVirtualInstall(SpecialEntityPreCollisionStuff, 0x59FEE0);
    RH_ScopedVirtualInstall(SpecialEntityCalcCollisionSteps, 0x5A02E0);
    RH_ScopedInstall(Init, 0x59F840);
    RH_ScopedInstall(ProcessGarageDoorBehaviour, 0x44A4D0);
    RH_ScopedInstall(CanBeDeleted, 0x59F120);
    RH_ScopedInstall(SetRelatedDummy, 0x59F160);
    RH_ScopedInstall(TryToExplode, 0x59F2D0);
    RH_ScopedInstall(SetObjectTargettable, 0x59F300);
    RH_ScopedInstall(CanBeTargetted, 0x59F320);
    RH_ScopedInstall(RefModelInfo, 0x59F330);
    RH_ScopedInstall(SetRemapTexture, 0x59F350);
    RH_ScopedInstall(GetRopeHeight, 0x59F380);
    RH_ScopedInstall(SetRopeHeight, 0x59F3A0);
    RH_ScopedInstall(GetObjectCarriedWithRope, 0x59F3C0);
    RH_ScopedInstall(ReleaseObjectCarriedWithRope, 0x59F3E0);
    RH_ScopedInstall(AddToControlCodeList, 0x59F400);
    RH_ScopedInstall(RemoveFromControlCodeList, 0x59F450);
    RH_ScopedInstall(ResetDoorAngle, 0x59F4B0);
    RH_ScopedInstall(LockDoor, 0x59F5C0);
    RH_ScopedInstall(DoBurnEffect, 0x59FB50);
    RH_ScopedInstall(GetLightingFromCollisionBelow, 0x59FD00);
    RH_ScopedInstall(ProcessSamSiteBehaviour, 0x5A07D0);
    RH_ScopedInstall(ProcessTrainCrossingBehaviour, 0x5A0B50);
    RH_ScopedInstall(ObjectDamage, 0x5A0D90);
    RH_ScopedInstall(Explode, 0x5A1340);
    RH_ScopedInstall(ObjectFireDamage, 0x5A1580);
    RH_ScopedInstall(GrabObjectToCarryWithRope, 0x5A1AB0);
    RH_ScopedInstall(CanBeUsedToTakeCoverBehind, 0x5A1B60);
    RH_ScopedInstall(ProcessControlLogic, 0x5A29A0);
    RH_ScopedOverloadedInstall(Create, "intbool", 0x5A1F60, CObject*(*)(int32, bool));
    RH_ScopedOverloadedInstall(Create, "dummy", 0x5A2070, CObject*(*)(CDummyObject*));
    RH_ScopedInstall(SetMatrixForTrainCrossing, 0x59F200);
    RH_ScopedInstall(TryToFreeUpTempObjects, 0x5A1840);
    RH_ScopedInstall(DeleteAllTempObjects, 0x5A18B0);
    RH_ScopedInstall(DeleteAllMissionObjects, 0x5A1910);
    RH_ScopedInstall(DeleteAllTempObjectsInArea, 0x5A1980);
    RH_ScopedGlobalInstall(IsObjectPointerValid_NotInWorld, 0x5A2B90);
    RH_ScopedGlobalInstall(IsObjectPointerValid, 0x5A2C20);
}

CObject::CObject() : CPhysical()
{
    m_pDummyObject = nullptr;
    Init();
    m_nObjectType = eObjectType::OBJECT_UNKNOWN;
}

CObject::CObject(int32 modelId, bool bCreate) : CPhysical()
{
    m_pDummyObject = nullptr;
    if (bCreate)
        CEntity::SetModelIndex(modelId);
    else
        CEntity::SetModelIndexNoCreate(modelId);

    Init();
}

CObject::CObject(CDummyObject* dummyObj) : CPhysical()
{
    CEntity::SetModelIndexNoCreate(dummyObj->m_nModelIndex);
    if (dummyObj->m_pRwObject)
        CEntity::AttachToRwObject(dummyObj->m_pRwObject, true);
    else
        CPlaceable::SetMatrix(dummyObj->GetMatrix());

    dummyObj->DetachFromRwObject();
    Init();

    m_nIplIndex = dummyObj->m_nIplIndex;
    m_nAreaCode = dummyObj->m_nAreaCode;
    m_bRenderDamaged = dummyObj->m_bRenderDamaged;

    if (m_pRwObject)
    {
        auto* atomic = m_pRwAtomic;
        if (RwObjectGetType(m_pRwObject) != rpATOMIC)
            atomic = GetFirstAtomic(m_pRwClump);

        if (!CCustomBuildingRenderer::IsCBPCPipelineAttached(atomic))
            m_bLightObject = true;
    }
}

CObject::~CObject()
{
    if (objectFlags.b0x200000 || objectFlags.b0x100000)
    {
        const auto iIndex = SCMToModelId(CTheScripts::ScriptsForBrains.m_aScriptForBrains[m_wScriptTriggerIndex].m_nIMGindex);
        CStreaming::SetMissionDoesntRequireModel(iIndex);
        objectFlags.b0x100000 = false;
        objectFlags.b0x200000 = false;
        CTheScripts::RemoveFromWaitingForScriptBrainArray(this, m_wScriptTriggerIndex);
        m_wScriptTriggerIndex = -1;
    }

    if (objectFlags.bHasNoModel)
    {
        auto* const colModel = CModelInfo::GetModelInfo((m_nModelIndex))->GetColModel();
        CColStore::RemoveRef(colModel->m_nColSlot);
    }

    CRadar::ClearBlipForEntity(eBlipType::BLIP_OBJECT, GetObjectPool()->GetRef(this));

    if (m_nRefModelIndex != -1)
        CModelInfo::GetModelInfo(m_nRefModelIndex)->RemoveRef();

    if (m_wRemapTxd != -1)
        CTxdStore::RemoveRef(m_wRemapTxd);

    if (IsTemporary() && nNoTempObjects)
        --nNoTempObjects;

    RemoveFromControlCodeList();
    if (m_nModelIndex == ModelIndices::MI_TRAINCROSSING1)
    {
        const auto& dummyPos = m_pDummyObject->GetPosition();
        ThePaths.SetLinksBridgeLights(dummyPos.x - 12.0F, dummyPos.x + 12.0F, dummyPos.y - 12.0F, dummyPos.y + 12.0F, false);
    }

    if (m_pFire)
        m_pFire->Extinguish();
}

void* CObject::operator new(unsigned size)
{
    return GetObjectPool()->New();
}

void* CObject::operator new(unsigned size, int32 poolRef)
{
    return GetObjectPool()->New(poolRef);
}

void CObject::operator delete(void* obj)
{
    GetObjectPool()->Delete(static_cast<CObject*>(obj));
}

// NOTSA
void CObject::operator delete(void* obj, int32 poolRef)
{
    GetObjectPool()->Delete(static_cast<CObject*>(obj));
}

// 0x5A0760
void CObject::SetIsStatic(bool isStatic)
{
    return SetIsStatic_Reversed(isStatic);
}
void CObject::SetIsStatic_Reversed(bool isStatic)
{
    m_bIsStatic = isStatic;
    physicalFlags.b31 = false;
    if (!isStatic && (physicalFlags.bDisableMoveForce && m_fDoorStartAngle < -1000.0F))
        m_fDoorStartAngle = GetHeading();
}

// 0x59F110
void CObject::CreateRwObject()
{
    CObject::CreateRwObject_Reversed();
}
void CObject::CreateRwObject_Reversed()
{
    CEntity::CreateRwObject();
}

// 0x5A2130
void CObject::ProcessControl()
{
    CObject::ProcessControl_Reversed();
}
void CObject::ProcessControl_Reversed()
{
    auto* mi = CModelInfo::GetModelInfo(m_nModelIndex);
    auto bIsAnimated = false;
    if (mi->GetRwModelType() == rpCLUMP && mi->bHasAnimBlend && m_pRwObject)
        bIsAnimated = true;

    if (m_fDamageIntensity > 0.0F
        && objectFlags.bDamaged
        && !m_pAttachedTo
        && !IsCraneMovingPart()
        && !physicalFlags.bInfiniteMass
        && !physicalFlags.bDisableMoveForce
        && m_pDamageEntity
    ) {
        const auto bCanCarryItems = m_pDamageEntity->m_nModelIndex == MODEL_DUMPER || m_pDamageEntity->m_nModelIndex == MODEL_FORKLIFT;
        if (bCanCarryItems && m_pDamageEntity->AsAutomobile()->m_wMiscComponentAngle
            && !CRopes::IsCarriedByRope(this))
        {
            if (m_nColDamageEffect != COL_DAMAGE_EFFECT_NONE && m_fDamageIntensity > 5.0F)
                ObjectDamage(m_fDamageIntensity, &m_vecLastCollisionPosn, &m_vecLastCollisionImpactVelocity, m_pDamageEntity, eWeaponType::WEAPON_RAMMEDBYCAR);

            if (m_vecLastCollisionImpactVelocity.z > 0.3F)
            {
                m_nFakePhysics = 0;
                const auto vecColDir = GetPosition() - m_pDamageEntity->GetPosition();
                const auto vecEntSpeed = m_pDamageEntity->AsPhysical()->GetSpeed(vecColDir);
                auto vecSpeedDiff = vecEntSpeed - m_vecMoveSpeed;
                if (vecSpeedDiff.SquaredMagnitude() < 0.0001F && m_vecTurnSpeed.SquaredMagnitude() < 0.0001F)
                {
                    CPhysical::AttachEntityToEntity(m_pDamageEntity->AsPhysical(), nullptr, nullptr);
                    m_fElasticity = 0.2F;
                }
            }
        }
    }

    objectFlags.bDamaged = false;
    if (!m_bIsStuck && !IsStatic())
    {
        if (!physicalFlags.bDisableZ && !physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce)
        {
            m_vecForce += m_vecMoveSpeed;
            m_vecForce /= 2.0F;
            m_vecTorque += m_vecTurnSpeed;
            m_vecTorque /= 2.0F;

            const auto fTimeStep = CTimer::GetTimeStep() * 0.003F;
            if (pow(fTimeStep, 2.0F) <= m_vecForce.SquaredMagnitude()
                || pow(fTimeStep, 2.0F) <= m_vecTorque.SquaredMagnitude())
            {
                m_nFakePhysics = 0;
            }
            else
            {
                m_nFakePhysics++;
                if (m_nFakePhysics > 10 && !physicalFlags.bAttachedToEntity)
                {
                    m_nFakePhysics = 10;
                    if (!bIsAnimated)
                        this->SetIsStatic(true);

                    m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
                    m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
                    m_vecFrictionMoveSpeed.Set(0.0F, 0.0F, 0.0F);
                    m_vecFrictionTurnSpeed.Set(0.0F, 0.0F, 0.0F);
                    return;
                }
            }
        }
    }

    if (!IsStatic())
        CPhysical::ProcessControl();

    if (bIsAnimated)
        this->SetIsStatic(false);

    CVector vecBuoyancyTurnPoint, vecBuoyancyForce;
    if (mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecBuoyancyTurnPoint, &vecBuoyancyForce))
    {
        physicalFlags.bTouchingWater = true;
        physicalFlags.bSubmergedInWater = true;
        this->SetIsStatic(false);

        CPhysical::ApplyMoveForce(vecBuoyancyForce);
        CPhysical::ApplyTurnForce(vecBuoyancyForce, vecBuoyancyTurnPoint);
        const auto fTimeStep = pow(0.97F, CTimer::GetTimeStep());
        m_vecMoveSpeed *= fTimeStep;
        m_vecTurnSpeed *= fTimeStep;
    }
    else if (m_nModelIndex != ModelIndices::MI_BUOY)
        physicalFlags.bTouchingWater = false; // Not clearing bSubmergedInWater, BUG?

    if (m_pObjectInfo->m_bCausesExplosion
        && objectFlags.bIsExploded
        && m_bIsVisible
        && (rand() % 32) == 10)
    {
        m_bUsesCollision = false;
        m_bIsVisible = false;
        physicalFlags.bExplosionProof = true;
        physicalFlags.bApplyGravity = false;
        m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
        this->DeleteRwObject();
    }

    if (m_nModelIndex == ModelIndices::MI_RCBOMB)
    {
        CPhysical::ApplyTurnForce(m_vecMoveSpeed * -0.05F, -GetForward());
        m_vecMoveSpeed *= pow(CTimer::GetTimeStep(), 1.0F - m_vecMoveSpeed.SquaredMagnitude() * 0.2F);
    }

    if (m_bIsBIGBuilding)
        m_bIsInSafePosition = true;

    if (physicalFlags.bDisableMoveForce && m_fDoorStartAngle > -1000.0F)
    {
        auto fHeading = GetHeading();
        if (m_fDoorStartAngle + PI < fHeading)
            fHeading -= TWO_PI;
        else if (m_fDoorStartAngle - PI > fHeading)
            fHeading += TWO_PI;

        auto fDiff = m_fDoorStartAngle - fHeading;
        if (fabs(fDiff) > PI / 6.0F)
            objectFlags.bIsDoorOpen = true;

        static float fMaxDoorDiff = 0.3F;
        static float fDoorCutoffSpeed = 0.02F;
        static float fDoorSpeedMult = 0.002F;
        fDiff = clamp(fDiff, -fMaxDoorDiff, fMaxDoorDiff);
        if (fDiff > 0.0F && m_vecTurnSpeed.z < fDoorCutoffSpeed
            || fDiff < 0.0F && m_vecTurnSpeed.z > -fDoorCutoffSpeed)
        {
            m_vecTurnSpeed.z += CTimer::GetTimeStep() * fDoorSpeedMult * fDiff;
        }

        if (fDiff != 0.0F && objectFlags.bIsDoorMoving)
            AudioEngine.ReportDoorMovement(this);

        if (!m_bIsBIGBuilding
            && !IsStatic()
            && fabs(fDiff) < 0.01F
            && (objectFlags.bIsDoorMoving || fabs(m_vecTurnSpeed.z) < 0.01F))
        {
            this->SetIsStatic(true);
            m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
            m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
            m_vecFrictionMoveSpeed.Set(0.0F, 0.0F, 0.0F);
            m_vecFrictionTurnSpeed.Set(0.0F, 0.0F, 0.0F);

            if (objectFlags.bIsDoorMoving && objectFlags.bIsDoorOpen)
                LockDoor();
        }
    }
}

// 0x5A17B0
void CObject::Teleport(CVector destination, bool resetRotation)
{
    CObject::Teleport_Reversed(destination, resetRotation);
}
void CObject::Teleport_Reversed(CVector destination, bool resetRotation)
{
    CWorld::Remove(this);
    SetPosn(destination);
    CEntity::UpdateRW();
    CEntity::UpdateRwFrame();
    CWorld::Add(this);
}

// 0x59FEE0
void CObject::SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled,
    bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck)
{
    CObject::SpecialEntityPreCollisionStuff_Reversed(colPhysical, bIgnoreStuckCheck, bCollisionDisabled,
        bCollidedEntityCollisionIgnored, bCollidedEntityUnableToMove, bThisOrCollidedEntityStuck);
}
void CObject::SpecialEntityPreCollisionStuff_Reversed(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled,
    bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck)
{
    if (m_pEntityIgnoredCollision == colPhysical || colPhysical->m_pEntityIgnoredCollision == this)
    {
        bCollidedEntityCollisionIgnored = true;
        return;
    }

    if (m_pAttachedTo == colPhysical)
        bCollisionDisabled = true;
    else if (colPhysical->m_pAttachedTo == this || m_pAttachedTo && m_pAttachedTo == colPhysical->m_pAttachedTo)
        bCollisionDisabled = true;
    else if (physicalFlags.bDisableZ && !physicalFlags.bApplyGravity && !colPhysical->physicalFlags.bDisableZ)
        bCollisionDisabled = true;
    else
    {
        if (!physicalFlags.bDisableZ) {
            if (physicalFlags.bDisableMoveForce || physicalFlags.bInfiniteMass)
            {
                if (bIgnoreStuckCheck || m_bIsStuck)
                    bCollisionDisabled = true;
                else if (!colPhysical->m_bIsStuck) { /* Do nothing pretty much, and skip further calc */ }
                else if (!colPhysical->m_bHasHitWall)
                    bThisOrCollidedEntityStuck = true;
                else
                    bCollidedEntityUnableToMove = true;
            }
            else if (objectFlags.bIsLampPost && (GetUp().z < 0.66F || m_bIsStuck))
            {
                if (colPhysical->IsVehicle() || colPhysical->IsPed()) {
                    bCollidedEntityCollisionIgnored = true;
                    if (colPhysical->IsVehicle())
                        return;

                    m_pEntityIgnoredCollision = colPhysical;
                }
            }
            else if ( colPhysical->IsVehicle())
            {
                if (IsModelTempCollision())
                    bCollisionDisabled = true;
                else if (IsTemporary() || IsExploded() || !CEntity::IsStatic())
                {
                    if (colPhysical->AsVehicle()->IsConstructionVehicle())
                    {
                        if (m_bIsStuck || colPhysical->m_bIsStuck)
                            bThisOrCollidedEntityStuck = true;
                    }
                    else if (!CanBeSmashed())
                    {
                        auto tempMat = CMatrix();
                        auto* cm = CEntity::GetColModel();
                        auto vecSize = cm->GetBoundingBox().GetSize();
                        auto vecTransformed = *m_matrix * vecSize;

                        auto& vecCollidedPos = colPhysical->GetPosition();
                        if (vecTransformed.z < vecCollidedPos.z)
                        {
                            bCollidedEntityCollisionIgnored = true;
                            m_pEntityIgnoredCollision = colPhysical;
                        }
                        else
                        {
                            Invert(colPhysical->GetMatrix(), tempMat);
                            if ((tempMat * vecTransformed).z < 0.0F)
                            {
                                bCollidedEntityCollisionIgnored = true;
                                m_pEntityIgnoredCollision = colPhysical;
                            }
                        }
                    }
                }
            }
            else if (m_nModelIndex != MODEL_GRENADE
                || !colPhysical->IsPed()
                || m_matrix->GetPosition().z >= colPhysical->m_matrix->GetPosition().z)
            {
                if (colPhysical->IsObject() && colPhysical->AsObject()->m_pObjectInfo->m_fUprootLimit > 0.0F && !colPhysical->m_pAttachedTo)
                {
                    if ((!colPhysical->physicalFlags.bDisableCollisionForce || colPhysical->physicalFlags.bCollidable)
                        && colPhysical->m_fMass * 10.0F > m_fMass)
                    {
                        bCollidedEntityUnableToMove = true;
                    }
                }
            }
        }
        else
        {
            if (bIgnoreStuckCheck)
                bCollisionDisabled = true;
            else if (m_bIsStuck || colPhysical->m_bIsStuck)
                bThisOrCollidedEntityStuck = true;
        }
    }

    if (!bCollidedEntityCollisionIgnored && (bIgnoreStuckCheck || m_bIsStuck))
        bThisOrCollidedEntityStuck = true;

}

// 0x5A02E0
uint8 CObject::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2)
{
    return CObject::SpecialEntityCalcCollisionSteps_Reversed(bProcessCollisionBeforeSettingTimeStep, unk2);
}
uint8 CObject::SpecialEntityCalcCollisionSteps_Reversed(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2)
{
    if (physicalFlags.bDisableZ
        || m_pObjectInfo->m_nSpecialColResponseCase == COL_SPECIAL_RESPONSE_GRENADE)
    {
        auto* cm = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
        const auto fMove = m_vecMoveSpeed.SquaredMagnitude() * pow(CTimer::GetTimeStep(), 2.0F);
        if (fMove >= pow(cm->GetBoundRadius(), 2.0F))
            return static_cast<uint8>(ceil(sqrt(fMove) / cm->GetBoundRadius()));

        return 1;
    }

    if (!physicalFlags.bDisableMoveForce)
    {
        if (physicalFlags.bInfiniteMass)
        {
            auto cm = CEntity::GetColModel();
            auto vecMin = Multiply3x3(GetMatrix(), cm->GetBoundingBox().m_vecMin);
            auto vecSpeed = CPhysical::GetSpeed(vecMin);
            const auto fMove = vecSpeed.SquaredMagnitude() * pow(CTimer::GetTimeStep(), 2.0F);
            if (fMove >= 0.0225F) // pow(0.15F, 2.0F)
                return static_cast<uint8>(ceil(sqrt(fMove) / 0.15F));

            return 1;
        }

        if (IsTemporary() && !objectFlags.bIsLiftable)
            return 1;

        if (m_pObjectInfo->m_nSpecialColResponseCase == COL_SPECIAL_RESPONSE_LAMPOST)
        {
            auto* cm = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();

            auto vecMin = CVector(0.0F, 0.0F, cm->GetBoundingBox().m_vecMin.z);
            vecMin = Multiply3x3(GetMatrix(), vecMin);
            vecMin = CPhysical::GetSpeed(vecMin);

            auto vecMax = CVector(0.0F, 0.0F, cm->GetBoundingBox().m_vecMax.z);
            vecMax = Multiply3x3(GetMatrix(), vecMax);
            vecMax = CPhysical::GetSpeed(vecMax);

            auto& vecUsed = vecMin.SquaredMagnitude() >= vecMax.SquaredMagnitude() ? vecMin : vecMax;
            const auto fMove = vecUsed.SquaredMagnitude() * pow(CTimer::GetTimeStep(), 2.0F);
            if (fMove >= 0.09F)
                return static_cast<uint8>(ceil(sqrt(fMove) / 0.3F));

            return 1;
        }

        if (objectFlags.bIsLiftable
            || m_pObjectInfo->m_nSpecialColResponseCase == COL_SPECIAL_RESPONSE_SMALLBOX
            || m_pObjectInfo->m_nSpecialColResponseCase == COL_SPECIAL_RESPONSE_FENCEPART)
        {
            auto* cm = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
            const auto vecSize = cm->GetBoundingBox().m_vecMax - cm->GetBoundingBox().m_vecMin;
            const auto fEdge = std::max({ vecSize.x, vecSize.y, vecSize.z }) / 2.0F;
            const auto fMove = m_vecMoveSpeed.SquaredMagnitude() * pow(CTimer::GetTimeStep(), 2.0F);
            if (fMove >= pow(fEdge, 2.0F))
                return static_cast<uint8>(ceil(sqrt(fMove) / fEdge));

            return 1;
        }

        const auto fMove = m_vecMoveSpeed.SquaredMagnitude() * pow(CTimer::GetTimeStep(), 2.0F);
        if (fMove >= 0.09F)
            return static_cast<uint8>(ceil(sqrt(fMove) / 0.3F));

        return 1;
    }

    auto& box = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox();
    auto fLongest = std::max({box.m_vecMax.x, box.m_vecMax.y, -box.m_vecMin.x, -box.m_vecMin.y });
    const auto fMove = fabs(fLongest * m_vecTurnSpeed.z);
    if (fMove > 0.1F)
        return static_cast<uint8>(ceil(fMove * 10.0F));

    return 1;
}

// 0x59FD50
void CObject::PreRender()
{
    CObject::PreRender_Reversed();
}
void CObject::PreRender_Reversed()
{
    if (objectFlags.bAffectedByColBrightness)
        GetLightingFromCollisionBelow();

    if (m_nBurnTime > CTimer::GetTimeInMS())
        DoBurnEffect();

    if (!m_pAttachedTo)
        m_fContactSurfaceBrightness = m_nColLighting.GetCurrentLighting();

    if (m_pRwObject && RwObjectGetType(m_pRwObject) == rpCLUMP && objectFlags.bFadingIn)
    {
        auto iAlpha = CVisibilityPlugins::GetClumpAlpha(m_pRwClump) - 16;
        iAlpha = std::max(0, iAlpha);
        CVisibilityPlugins::SetClumpAlpha(m_pRwClump, iAlpha);
    }

    CEntity::PreRender();

    if (m_fScale != 1.0F || objectFlags.bIsScaled)
    {
        auto vecScale = CVector(m_fScale, m_fScale, m_fScale);
        CEntity::UpdateRW();
        RwMatrixScale(CEntity::GetModellingMatrix(), &vecScale, RwOpCombineType::rwCOMBINEPRECONCAT);
        CEntity::UpdateRwFrame();
        objectFlags.bIsScaled = true;
        if (objectFlags.bIsScaled)
            objectFlags.bIsScaled = false; //BUG? It's unsetting the flag straight after setting it
    }

    if (m_pRwObject && RwObjectGetType(m_pRwObject) == rpCLUMP)
        CEntity::UpdateRpHAnim();
}

// 0x59F180
void CObject::Render()
{
    CObject::Render_Reversed();
}
void CObject::Render_Reversed()
{
    if (objectFlags.bDoNotRender)
        return;

    const auto iRefModel = m_nRefModelIndex;
    if (iRefModel != -1 && IsTemporary() && objectFlags.bChangesVehColor)
    {
        auto* vehModelInfo = CModelInfo::GetModelInfo(iRefModel)->AsVehicleModelInfoPtr();
        CVehicleModelInfo::ms_pRemapTexture = m_pRemapTexture;
        vehModelInfo->SetVehicleColour(m_nCarColor[0], m_nCarColor[1], m_nCarColor[2], m_nCarColor[3]);
    }

    CEntity::Render();
}

// 0x554FA0
bool CObject::SetupLighting()
{
    return CObject::SetupLighting_Reversed();
}
bool CObject::SetupLighting_Reversed()
{
    if (physicalFlags.bDestroyed)
    {
        WorldReplaceNormalLightsWithScorched(Scene.m_pRpWorld, 0.18F);
        return true;
    }

    if (m_bLightObject)
    {
        ActivateDirectional();
        return CRenderer::SetupLightingForEntity(this);
    }

    return false;
}

// 0x553E10
void CObject::RemoveLighting(bool bRemove)
{
    CObject::RemoveLighting_Reversed(bRemove);
}
void CObject::RemoveLighting_Reversed(bool bRemove)
{
    if (!bRemove)
        return;

    if (!physicalFlags.bDestroyed)
        CPointLights::RemoveLightsAffectingObject();

    SetAmbientColours();
    DeActivateDirectional();
}

//0x5D2870 - Deserializes object from save storage buffer
bool CObject::Load()
{
    int32 iSaveStructSize; // unused
    CObjectSaveStructure saveStruct;
    CGenericGameStorage::LoadDataFromWorkBuffer(&iSaveStructSize, 4);
    CGenericGameStorage::LoadDataFromWorkBuffer(&saveStruct, sizeof(saveStruct));
    saveStruct.Extract(this);
    return true;
}

//0x5D2830 - Serializes object to save storage buffer
bool CObject::Save()
{
    CObjectSaveStructure saveStruct;
    saveStruct.Construct(this);

    auto iStructSize = sizeof(CObjectSaveStructure);
    CGenericGameStorage::SaveDataToWorkBuffer(&iStructSize, 4);
    CGenericGameStorage::SaveDataToWorkBuffer(&saveStruct, iStructSize);
    return true;
}

// 0x44A4D0
void CObject::ProcessGarageDoorBehaviour() {
    if (m_nGarageDoorGarageIndex < 0)
        m_nGarageDoorGarageIndex = CGarages::FindGarageForObject(this);

    if (m_nGarageDoorGarageIndex < 0)
        return;

    auto& vecDummyPos = m_pDummyObject->GetPosition();
    auto* mi = CModelInfo::GetModelInfo(m_nModelIndex);
    const auto fHeight = mi->GetColModel()->GetBoundingBox().GetHeight();
    auto& garage = CGarages::GetGarage(m_nGarageDoorGarageIndex);
    if (garage.m_bDoorOpensUp)
    {
        m_matrix->GetPosition().z = vecDummyPos.z + fHeight * garage.m_fDoorPosition * 0.48F;
        float fDoorPos = garage.m_fDoorPosition;
        if (garage.m_bDoorGoesIn)
            fDoorPos = -fDoorPos;

        CGarage::BuildRotatedDoorMatrix(this, fDoorPos);
    }
    else
    {
        if (garage.m_nType == eGarageType::HANGAR_AT400)
            m_matrix->GetPosition().z = vecDummyPos.z - fHeight * garage.m_fDoorPosition;
        else if (garage.m_nType == eGarageType::HANGAR_ABANDONED_AIRPORT)
            m_matrix->GetPosition().x = vecDummyPos.x - garage.m_fDoorPosition * m_matrix->GetRight().x * 14.0F;
        else
            m_matrix->GetPosition().z = vecDummyPos.z + fHeight * garage.m_fDoorPosition / 1.1F;
    }

    m_bUsesCollision = garage.m_bDoorClosed;
    CEntity::UpdateRW();
    CEntity::UpdateRwFrame();
}

// 0x59F120
bool CObject::CanBeDeleted() {
    switch (m_nObjectType)
    {
    case eObjectType::OBJECT_MISSION:
    case eObjectType::OBJECT_TYPE_CUTSCENE:
    case eObjectType::OBJECT_TYPE_DECORATION:
    case eObjectType::OBJECT_MISSION2:
        return false;

    default:
        return true;
    }
}

// 0x59F160
void CObject::SetRelatedDummy(CDummyObject* relatedDummy) {
    m_pDummyObject = relatedDummy;
    assert(m_pDummyObject);
    m_pDummyObject->RegisterReference(reinterpret_cast<CEntity**>(&m_pDummyObject));
}

// 0x59F2D0
bool CObject::TryToExplode() {
    if (!m_pObjectInfo->m_bCausesExplosion)
        return false;

    if (objectFlags.bIsExploded)
        return false;

    objectFlags.bIsExploded = true;
    Explode();
    return true;
}

// 0x59F300
void CObject::SetObjectTargettable(bool targetable) {
    objectFlags.bIsTargetable = targetable;
}

// 0x59F320
bool CObject::CanBeTargetted() {
    return objectFlags.bIsTargetable;
}

// 0x59F330
void CObject::RefModelInfo(int32 modelIndex) {
    m_nRefModelIndex = modelIndex;
    CModelInfo::GetModelInfo(modelIndex)->AddRef();
}

// 0x59F350
void CObject::SetRemapTexture(RwTexture* remapTexture, int16 txdIndex) {
    m_pRemapTexture = remapTexture;
    m_wRemapTxd = txdIndex;
    if (txdIndex != -1)
        CTxdStore::AddRef(txdIndex);
}

// 0x59F380
float CObject::GetRopeHeight() {
    const auto ropeIndex = CRopes::FindRope(reinterpret_cast<uint32>(this));
    return CRopes::GetRope(ropeIndex).m_fSegmentLength;
}

// 0x59F3A0
void CObject::SetRopeHeight(float height) {
    const auto ropeIndex = CRopes::FindRope(reinterpret_cast<uint32>(this));
    CRopes::GetRope(ropeIndex).m_fSegmentLength = height;
}

// 0x59F3C0
CEntity* CObject::GetObjectCarriedWithRope() {
    const auto ropeIndex = CRopes::FindRope(reinterpret_cast<uint32>(this));
    return CRopes::GetRope(ropeIndex).m_pAttachedEntity;
}

// 0x59F3E0
void CObject::ReleaseObjectCarriedWithRope() {
    const auto ropeIndex = CRopes::FindRope(reinterpret_cast<uint32>(this));
    CRopes::GetRope(ropeIndex).ReleasePickedUpObject();
}

// 0x59F400
void CObject::AddToControlCodeList() {
   m_pControlCodeList = CWorld::ms_listObjectsWithControlCode.AddItem(this);
}

// 0x59F450
void CObject::RemoveFromControlCodeList() {
    if (!m_pControlCodeList)
        return;

    CWorld::ms_listObjectsWithControlCode.DeleteNode(m_pControlCodeList);
    m_pControlCodeList = nullptr;
}

// 0x59F4B0
void CObject::ResetDoorAngle() {
    if (!physicalFlags.bDisableMoveForce || m_fDoorStartAngle <= -1000.0F)
        return;

    CPlaceable::SetHeading(m_fDoorStartAngle);
    this->SetIsStatic(true);
    m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
    m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
    m_vecFrictionMoveSpeed.Set(0.0F, 0.0F, 0.0F);
    m_vecFrictionTurnSpeed.Set(0.0F, 0.0F, 0.0F);
    CEntity::UpdateRW();
    CEntity::UpdateRwFrame();
}

// 0x59F5C0
void CObject::LockDoor() {
    objectFlags.bIsDoorOpen = false;
    physicalFlags.bCollidable = true;
    physicalFlags.bDisableCollisionForce = true;
    ResetDoorAngle();
}

// 0x59F840
void CObject::Init() {
    m_nType = ENTITY_TYPE_OBJECT;
    m_pObjectInfo = &CObjectData::GetDefault();
    m_nColDamageEffect = COL_DAMAGE_EFFECT_NONE;
    m_nSpecialColResponseCase = COL_SPECIAL_RESPONSE_NONE;
    m_nObjectType = eObjectType::OBJECT_GAME;
    this->SetIsStatic(true);

    m_nObjectFlags &= 0x0FC040000;
    objectFlags.bCanBeAttachedToMagnet = true;

    if (m_nModelIndex == 0xFFFF)
    {
        objectFlags.bHasNoModel = false;
    }
    else
    {
        CObjectData::SetObjectData(m_nModelIndex, *this);
        auto* mi = CModelInfo::GetModelInfo(m_nModelIndex);
        if (mi->GetColModel()->m_bNotEmpty)
        {
            CColStore::AddRef(mi->GetColModel()->m_nColSlot);
            objectFlags.bHasNoModel = true;

            auto* ami = mi->AsAtomicModelInfoPtr();
            if (ami && ami->SwaysInWind() && !physicalFlags.bDisableCollisionForce)
            {
                auto& pBndBox = mi->GetColModel()->GetBoundingBox();
                m_vecCentreOfMass.z = pBndBox.m_vecMin.z + (pBndBox.m_vecMax.z - pBndBox.m_vecMin.z) * 0.2F;
            }
        }
    }

    if (physicalFlags.bDisableMoveForce)
    {
        auto* cd = CEntity::GetColModel()->m_pColData;
        if (cd)
            cd->m_nNumSpheres = 0;
    }

    m_fHealth = 1000.0F;
    m_fDoorStartAngle = -1001.0F;
    m_nRemovalTime = 0;
    m_nBonusValue = 0;
    m_wCostValue = 0;
    for (auto& col : m_nCarColor)
        col = 0;

    m_nRefModelIndex = -1;
    m_nGarageDoorGarageIndex = -1;
    m_nLastWeaponDamage = -1;
    m_pFire = nullptr;

    if (m_nModelIndex == ModelIndices::MI_BUOY)
        physicalFlags.bTouchingWater = true;

    if (m_nModelIndex != 0xFFFF && CModelInfo::GetModelInfo(m_nModelIndex)->GetModelType() == ModelInfoType::MODEL_INFO_WEAPON)
        m_bLightObject = true;

    if (m_nModelIndex == ModelIndices::MI_MLAMPPOST
        || m_nModelIndex == ModelIndices::MI_TRAFFICLIGHTS_MIAMI
        || m_nModelIndex == ModelIndices::MI_TRAFFICLIGHTS_VEGAS
        || m_nModelIndex == ModelIndices::MI_TRAFFICLIGHTS_TWOVERTICAL
        || m_nModelIndex == ModelIndices::MI_SINGLESTREETLIGHTS1
        || m_nModelIndex == ModelIndices::MI_SINGLESTREETLIGHTS2
        || m_nModelIndex == ModelIndices::MI_SINGLESTREETLIGHTS3
        || m_nModelIndex == ModelIndices::MI_DOUBLESTREETLIGHTS
        || m_nModelIndex != 0xFFFF && CModelInfo::GetModelInfo(m_nModelIndex)->AsAtomicModelInfoPtr() && CModelInfo::GetModelInfo(m_nModelIndex)->SwaysInWind())
    {
        objectFlags.bIsLampPost = true;
    }
    else
    {
        objectFlags.bIsLampPost = false;
    }

    objectFlags.bIsTargetable = false;
    physicalFlags.bAttachedToEntity = false;

    m_nAreaCode = eAreaCodes::AREA_CODE_13;
    m_wRemapTxd = -1;
    m_pRemapTexture = nullptr;
    m_pControlCodeList = nullptr;

    if (m_nModelIndex == ModelIndices::MI_SAMSITE
        || m_nModelIndex == ModelIndices::MI_SAMSITE2
        || m_nModelIndex == ModelIndices::MI_TRAINCROSSING
        || m_nModelIndex == ModelIndices::MI_TRAINCROSSING1
        || m_nModelIndex == ModelIndices::MI_MAGNOCRANE
        || m_nModelIndex == ModelIndices::MI_CRANETROLLEY
        || m_nModelIndex == ModelIndices::MI_QUARRYCRANE_ARM
        || CGarages::IsModelIndexADoor(static_cast<int16>(m_nModelIndex)))
    {
        AddToControlCodeList();
    }

    m_nBurnTime = 0;
    m_fScale = 1.0F;

    m_nColLighting.day = 0x8;
    m_nColLighting.night = 0x4;
    m_wScriptTriggerIndex = -1;
}

// 0x59FB50
void CObject::DoBurnEffect() {
    const auto& box = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox();
    const auto& vecSize = box.GetSize();
    const auto nUsedSize = static_cast<int32>(vecSize.x * vecSize.y * vecSize.z * m_fBurnDamage / 20.0F);
    if (nUsedSize <= 0)
        return;

    for (auto i = 0; i < nUsedSize; ++i)
    {
        const auto fRandX = CGeneral::GetRandomNumberInRange(box.m_vecMin.x, box.m_vecMax.x);
        const auto fRandY = CGeneral::GetRandomNumberInRange(box.m_vecMin.y, box.m_vecMax.y);
        const auto fRandZ = CGeneral::GetRandomNumberInRange(box.m_vecMin.z, box.m_vecMax.z);
        auto vecParticlePos = *m_matrix * CVector(fRandX, fRandY, fRandZ);

        // auto smokePart = FxPrtMult_c() Originally overwritten right after
        auto smokePart = FxPrtMult_c(0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.4F);
        auto vecVelocity = CVector(0.0F, 0.0F, 0.02F);
        g_fx.m_SmokeII3expand->AddParticle(&vecParticlePos, &vecVelocity, 0.0F, &smokePart, -1.0F, 1.2F, 0.6F, false);
    }
}

// 0x59FD00
void CObject::GetLightingFromCollisionBelow() {
    CColPoint colPoint{};
    CEntity* entity;
    if (CWorld::ProcessVerticalLine(GetPosition(), -1000.0F, colPoint, entity, true, false, false, false, true, false, nullptr))
        m_nColLighting = colPoint.m_nLightingB;
}

// 0x5A07D0
void CObject::ProcessSamSiteBehaviour() {
    if (objectFlags.bIsBroken)
        return;

    auto& vecPos = GetPosition();
    bool bDisabled = bAircraftCarrierSamSiteDisabled;
    if (vecPos.x >= -1000)
        bDisabled = bArea51SamSiteDisabled;

    if (bDisabled)
        return;

    CEntity* targetEntity = nullptr;
    auto fHeading = CGeneral::GetATanOfXY(m_matrix->GetForward().x, m_matrix->GetForward().y);
    auto* playerVeh = FindPlayerVehicle();
    if (!playerVeh
        || playerVeh->GetVehicleAppearance() == eVehicleAppearance::VEHICLE_APPEARANCE_BIKE
        || playerVeh->GetVehicleAppearance() == eVehicleAppearance::VEHICLE_APPEARANCE_AUTOMOBILE)
    {
        auto* player = FindPlayerPed();
        if (player->GetIntelligence()->GetTaskJetPack())
            targetEntity = player;
    }
    else
    {
        targetEntity = playerVeh;
    }

    if (targetEntity)
    {
        auto& vecTargetPos = targetEntity->GetPosition();
        if (vecPos.z <= vecTargetPos.z + 2.0F
            && vecTargetPos.y <= 2100.0F)
        {
            const auto vecDir = vecTargetPos - vecPos;
            const auto fAngle = CGeneral::GetATanOfXY(vecDir.x, vecDir.y);
            const auto fAngleDiff = CGeneral::LimitRadianAngle(fAngle - fHeading);

            float fNewAngle;
            const auto fTimeStep = CTimer::GetTimeStep() / 20.0F;
            if (abs(fAngleDiff) <= fTimeStep)
                fNewAngle = fAngle;
            else if (fAngleDiff < 0.0F)
                fNewAngle = fHeading - fTimeStep;
            else
                fNewAngle = fHeading + fTimeStep;

            CPlaceable::SetHeading(fNewAngle - HALF_PI);
            CEntity::UpdateRW();
            CEntity::UpdateRwFrame();

            auto vecShootDir = vecPos - vecTargetPos;
            if (vecShootDir.Magnitude2D() >= 120.0F)
                return;

            if (fabs(fAngleDiff) >= 0.1F || CTimer::GetTimeInMS() / 4000 == CTimer::GetPreviousTimeInMS() / 4000)
                return;

            auto vecRocketDir = m_matrix->GetForward() + m_matrix->GetUp();
            const auto vecSrcPos = *m_matrix * CVector(0.0F, 2.0F, 4.0F);
            CProjectileInfo::AddProjectile(this, eWeaponType::WEAPON_ROCKET_HS, vecSrcPos, 0.0F, &vecRocketDir, targetEntity);
            return;
        }
    }

    fHeading += CTimer::GetTimeStep() / 200.0F;
    CPlaceable::SetHeading(fHeading - HALF_PI);
    CEntity::UpdateRW();
    CEntity::UpdateRwFrame();
}

// 0x5A0B50
void CObject::ProcessTrainCrossingBehaviour() {
    if (!(static_cast<uint8>(CTimer::GetFrameCounter() + m_nRandomSeedUpperByte) & 0x10))
    {
        const auto& vecPos = GetPosition();
        const auto bWasEnabled = objectFlags.bTrainCrossEnabled;
        objectFlags.bTrainCrossEnabled = false;
        auto* train = CTrain::FindNearestTrain(vecPos, true);
        if (train)
        {
            auto vecDist = train->GetPosition() - vecPos;
            if (vecDist.Magnitude2D() < 120.0F)
                objectFlags.bTrainCrossEnabled = true;
        }

        if (m_nModelIndex == ModelIndices::MI_TRAINCROSSING1 && objectFlags.bTrainCrossEnabled != bWasEnabled)
        {
            const auto& dummyPos = m_pDummyObject->GetPosition();
            ThePaths.SetLinksBridgeLights(dummyPos.x - 12.0F, dummyPos.x + 12.0F, dummyPos.y - 12.0F, dummyPos.y + 12.0F, !bWasEnabled);
        }
    }

    if (m_nModelIndex == ModelIndices::MI_TRAINCROSSING1)
        return;

    const auto fAngle = acos(m_matrix->GetUp().z);
    const auto fTimeStep = CTimer::GetTimeStep() / 200.0F;
    if (objectFlags.bTrainCrossEnabled)
        SetMatrixForTrainCrossing(m_matrix, std::max(0.0F, fAngle - fTimeStep));
    else
        SetMatrixForTrainCrossing(m_matrix, std::min(PI * 0.43F, fAngle + fTimeStep));

    CEntity::UpdateRW();
    CEntity::UpdateRwFrame();
}

// 0x5A0D90
void CObject::ObjectDamage(float damage, CVector* fxOrigin, CVector* fxDirection, CEntity* damager, eWeaponType weaponType) {
    if (!m_bUsesCollision)
        return;

    if (weaponType == eWeaponType::WEAPON_UNIDENTIFIED && damager && damager->IsVehicle())
        weaponType = eWeaponType::WEAPON_RUNOVERBYCAR;

    if (!CPhysical::CanPhysicalBeDamaged(weaponType, nullptr))
        return;

    m_fHealth -= damage * m_pObjectInfo->m_fColDamageMultiplier;
    m_fHealth = std::max(0.0F, m_fHealth);

    if (!m_nColDamageEffect || physicalFlags.bInvulnerable && damager != FindPlayerPed() && damager != FindPlayerVehicle())
        return;

    // Big Smoke crack palace wall break checks
    if (m_nModelIndex == ModelIndices::MI_IMY_SHASH_WALL)
    {
        if (!damager)
            return;

        if (damager->IsPed())
        {
            auto* ped = damager->AsPed();
            if (!ped->bInVehicle || !ped->m_pVehicle || ped->m_pVehicle->m_nModelIndex != MODEL_SWATVAN)
                return;
        }
        else if (damager->IsVehicle())
        {
            if (damager->m_nModelIndex != MODEL_SWATVAN)
                return;
        }
        else
            return;
    }

    if (damager && damager->m_nModelIndex == MODEL_FORKLIFT)
        return;

    m_nLastWeaponDamage = weaponType;
    bool bWasDestroyed = false;

    if (damage * m_pObjectInfo->m_fColDamageMultiplier > 150.0F || m_fHealth == 0.0F)
    {
        switch (m_nColDamageEffect)
        {
        case COL_DAMAGE_EFFECT_CHANGE_MODEL:
            if (!m_bRenderDamaged)
            {
                bWasDestroyed = true;
                this->DeleteRwObject();
            }
            m_bRenderDamaged = true;
            break;

        case COL_DAMAGE_EFFECT_SMASH_COMPLETELY:
            m_bUsesCollision = false;
            m_bIsVisible = false;
            if (!CEntity::IsStatic())
                CPhysical::RemoveFromMovingList();

            m_bIsStatic = true;
            physicalFlags.bExplosionProof = true;
            m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
            m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
            this->DeleteRwObject();
            bWasDestroyed = true;
            break;

        case COL_DAMAGE_EFFECT_CHANGE_THEN_SMASH:
            if (m_bRenderDamaged)
            {
                m_bUsesCollision = false;
                m_bIsVisible = false;
                if (!CEntity::IsStatic())
                    CPhysical::RemoveFromMovingList();

                m_bIsStatic = true;
                physicalFlags.bExplosionProof = true;
                m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
                m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
                this->DeleteRwObject();
                bWasDestroyed = true;
            }
            else
            {
                this->DeleteRwObject();
                m_bRenderDamaged = true;
            }
            break;

        case COL_DAMAGE_EFFECT_BREAKABLE:
        case COL_DAMAGE_EFFECT_BREAKABLE_REMOVED:
            {
                const auto bJustFaces = damage * m_pObjectInfo->m_fColDamageMultiplier > m_pObjectInfo->m_fSmashMultiplier * 150.0F;
                g_breakMan.Add(this, &m_pObjectInfo->m_vecBreakVelocity, m_pObjectInfo->m_fBreakVelocityRand, bJustFaces);

                m_bUsesCollision = false;
                m_bIsVisible = false;
                if (!CEntity::IsStatic())
                    CPhysical::RemoveFromMovingList();

                m_bIsStatic = true;
                physicalFlags.bExplosionProof = true;
                m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
                m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
                objectFlags.bIsBroken = true;
                this->DeleteRwObject();
                bWasDestroyed = true;
                break;
            }
        }

        if (!m_bUsesCollision && !m_bIsVisible)
            m_fHealth = 0.0F;
    }

    bool bExploded = false;
    if (bWasDestroyed)
    {
        if (TryToExplode())
            bExploded = true;

        AudioEngine.ReportObjectDestruction(this);
    }

    // Particle creation
    if (!bWasDestroyed || !bExploded)
    {
        if (m_pObjectInfo->m_nFxType == eObjectFxType::NO_FX)
            return;

        bool bCreateParticle = false;
        if (m_pObjectInfo->m_nFxType == eObjectFxType::PLAY_ON_HIT_DESTROYED)
            bCreateParticle = true;
        else if (m_pObjectInfo->m_nFxType == eObjectFxType::PLAY_ON_DESTROYED)
            bCreateParticle = bWasDestroyed;
        else if (m_pObjectInfo->m_nFxType == eObjectFxType::PLAY_ON_HIT)
            bCreateParticle = damage > 30.0F;

        if (!bCreateParticle)
            return;

        if (m_pObjectInfo->m_vFxOffset.x < -500.0F)
        {
            if (!fxOrigin)
                return;

            RwMatrix particleMat;
            g_fx.CreateMatFromVec(&particleMat, fxOrigin, fxDirection);
            auto* fxSystem = g_fxMan.CreateFxSystem(m_pObjectInfo->m_pFxSystemBP, &particleMat, nullptr, false);
            if (fxSystem)
                fxSystem->PlayAndKill();

            return;
        }

        auto particleMat = CMatrix(*m_matrix);
        auto vecPoint = Multiply3x3(particleMat, m_pObjectInfo->m_vFxOffset);
        vecPoint += GetPosition();
        auto* fxSystem = g_fxMan.CreateFxSystem(m_pObjectInfo->m_pFxSystemBP, &vecPoint, nullptr, false);
        if (fxSystem)
            fxSystem->PlayAndKill();
    }
}

// 0x5A1340
void CObject::Explode() {
    CVector vecPos = GetPosition();
    vecPos.z += 0.5F;
    auto* player = FindPlayerPed();
    CExplosion::AddExplosion(this, player, eExplosionType::EXPLOSION_OBJECT, vecPos, 100, true, -1.0F, false);
    if (m_nColDamageEffect == COL_DAMAGE_EFFECT_BREAKABLE
        || m_nColDamageEffect == COL_DAMAGE_EFFECT_BREAKABLE_REMOVED)
    {
        vecPos.z -= 1.0F;
        auto vecDir = CVector(0.0F, 0.0F, 1.0F);
        ObjectDamage(10000.0F, &vecPos, &vecDir, this, eWeaponType::WEAPON_EXPLOSION);
    }
    else if (!physicalFlags.bDisableCollisionForce)
    {
        m_vecMoveSpeed.x += CGeneral::GetRandomNumberInRange(-0.0256F, 0.0256F);
        m_vecMoveSpeed.y += CGeneral::GetRandomNumberInRange(-0.0256F, 0.0256F);
        m_vecMoveSpeed.z += 0.5F;

        if (IsStatic())
        {
            this->SetIsStatic(false);
            CPhysical::AddToMovingList();
        }
    }

    if (m_pObjectInfo->m_nFxType == eObjectFxType::PLAY_ON_DESTROYED)
    {
        auto particleMat = CMatrix(*m_matrix);
        auto vecPoint = Multiply3x3(particleMat, m_pObjectInfo->m_vFxOffset);
        vecPoint += GetPosition();
        auto* fxSystem = g_fxMan.CreateFxSystem(m_pObjectInfo->m_pFxSystemBP, &vecPoint, nullptr, false);
        if (fxSystem)
            fxSystem->PlayAndKill();
    }
}

// 0x5A1580
void CObject::ObjectFireDamage(float damage, CEntity* damager) {
    if (m_nModelIndex != ModelIndices::MI_GRASSPLANT && m_nModelIndex != ModelIndices::MI_GRASSHOUSE)
        return;

    m_fHealth -= damage;
    m_fHealth = std::max(0.0F, m_fHealth);

    if (!m_nColDamageEffect || physicalFlags.bInvulnerable && damager != FindPlayerPed() && damager != FindPlayerVehicle())
        return;

    if (m_nModelIndex == ModelIndices::MI_GRASSPLANT)
    {
        m_fBurnDamage = 1.0F - (m_fHealth / 2000.0F);
        m_nBurnTime = CTimer::GetTimeInMS() + 3000;
    }

    if (m_fHealth != 0.0F)
        return;

    m_nBurnTime = 0;

    if (m_nColDamageEffect == COL_DAMAGE_EFFECT_BREAKABLE
     || m_nColDamageEffect == COL_DAMAGE_EFFECT_BREAKABLE_REMOVED)
    {
        if (!objectFlags.bIsBroken)
            AudioEngine.ReportObjectDestruction(this);

        g_breakMan.Add(this, &m_pObjectInfo->m_vecBreakVelocity, m_pObjectInfo->m_fBreakVelocityRand, true);

        m_bUsesCollision = false;
        m_bIsVisible = false;
        if (!CEntity::IsStatic())
            CPhysical::RemoveFromMovingList();

        m_bIsStatic = true;
        physicalFlags.bExplosionProof = true;
        m_vecMoveSpeed.Set(0.0F, 0.0F, 0.0F);
        m_vecTurnSpeed.Set(0.0F, 0.0F, 0.0F);
        objectFlags.bIsBroken = true;
        DeleteRwObject();
    } else if (m_nColDamageEffect == COL_DAMAGE_EFFECT_CHANGE_MODEL && !m_bRenderDamaged) {
            m_bRenderDamaged = true;
            DeleteRwObject();
            CShadows::AddPermanentShadow(
                eShadowType::SHADOW_DEFAULT,
                gpShadowHeliTex,
                &GetPosition(),
                3.0F,
                0.0F,
                0.0F,
                -3.0F,
                200,
                0,
                0,
                0,
                10.0F,
                30000,
                1.0F
            );
    }
}

// 0x5A1840
void CObject::TryToFreeUpTempObjects(int32 numObjects) {
    const auto poolSize = GetObjectPool()->GetSize();
    if (!poolSize)
        return;

    for (auto i = poolSize - 1; i >= 0; --i) {
        if (numObjects <= 0)
            return;

        auto* obj = GetObjectPool()->GetAt(i);
        if (obj && obj->IsTemporary() && !obj->IsVisible()) {
            CWorld::Remove(obj);
            delete obj;
            --numObjects;
        }
    }
}

// 0x5A18B0
void CObject::DeleteAllTempObjects() {
    const auto poolSize = GetObjectPool()->GetSize();
    if (!poolSize)
        return;

    for (auto i = 0; i < poolSize; ++i) {
        auto* obj = GetObjectPool()->GetAt(i);
        if (obj && obj->IsTemporary()) {
            CWorld::Remove(obj);
            delete obj;
        }
    }
}

// 0x5A1910
void CObject::DeleteAllMissionObjects() {
    const auto poolSize = GetObjectPool()->GetSize();
    if (!poolSize)
        return;

    for (auto i = 0; i < poolSize; ++i)  {
        auto* obj = GetObjectPool()->GetAt(i);
        if (obj && obj->IsMissionObject()) {
            CWorld::Remove(obj);
            delete obj;
        }
    }
}

// 0x5A1980
void CObject::DeleteAllTempObjectsInArea(CVector point, float radius) {
    const auto poolSize = GetObjectPool()->GetSize();
    if (!poolSize)
        return;

    for (auto i = 0; i < poolSize; ++i) {
        auto* obj = GetObjectPool()->GetAt(i);
        if (!obj || !obj->IsTemporary())
            continue;

        if (DistanceBetweenPointsSquared(obj->GetPosition(), point) < pow(radius, 2.0F)) {
            CWorld::Remove(obj);
            delete obj;
        }
    }
}

// 0x5A1AB0
void CObject::GrabObjectToCarryWithRope(CPhysical* attachTo) {
    const auto iRopeInd = CRopes::FindRope(reinterpret_cast<uint32>(this));
    auto& rope = CRopes::GetRope(iRopeInd);
    rope.ReleasePickedUpObject();
    rope.m_pAttachedEntity = attachTo;
    assert(rope.m_pAttachedEntity);
    rope.m_pAttachedEntity->RegisterReference(&rope.m_pAttachedEntity);

    auto vecRopePoint = CVector();
    vecRopePoint.z = CRopes::FindPickupHeight(attachTo);
    vecRopePoint *= *attachTo->m_matrix * vecRopePoint;

    rope.m_pRopeAttachObject->SetPosn(vecRopePoint);
    rope.m_pRopeAttachObject->m_bUsesCollision = false;
}

// 0x5A1B60
bool CObject::CanBeUsedToTakeCoverBehind() {
    if (m_nObjectType == eObjectType::OBJECT_MISSION)
        return false;

    if (m_nModelIndex == ModelIndices::MI_FIRE_HYDRANT)
        return true;

    auto* mi = CModelInfo::GetModelInfo(m_nModelIndex);
    if (mi->AsAtomicModelInfoPtr() && mi->AsAtomicModelInfoPtr()->IsBreakableStatuePart())
        return false;

    const auto fHeight = mi->GetColModel()->GetBoundingBox().GetHeight();
    if (fHeight <= 0.75F || fHeight >= 1.25F || GetUp().z <= 0.9F)
        return false;

    return true;
}

// 0x5A1F60
CObject* CObject::Create(int32 modelIndex, bool bUnused) {
    GetObjectPool()->m_bIsLocked = true;
    auto* obj = new CObject(modelIndex, false); //BUG? most likely the unused parameter was supposed to be passed to the constructor
    GetObjectPool()->m_bIsLocked = false;

    if (obj)
        return obj;

    TryToFreeUpTempObjects(5);
    g_waterCreatureMan.TryToFreeUpWaterCreatures(5);

    return new CObject(modelIndex, false);
}

// 0x5A2070
CObject* CObject::Create(CDummyObject* dummyObject) {
    GetObjectPool()->m_bIsLocked = true;
    auto* obj = new CObject(dummyObject);
    GetObjectPool()->m_bIsLocked = false;

    if (obj)
        return obj;

    TryToFreeUpTempObjects(5);
    g_waterCreatureMan.TryToFreeUpWaterCreatures(5);

    return new CObject(dummyObject);
}

// 0x59F200
void CObject::SetMatrixForTrainCrossing(CMatrix* matrix, float fAngle) {
    auto vecForward = CrossProduct(CVector(0.0F, 0.0F, 1.0F), matrix->GetRight());
    const auto fCos = cos(fAngle);
    const auto fSin = sin(fAngle);
    vecForward *= fCos;
    vecForward.z += fSin;

    matrix->GetUp() = CrossProduct(matrix->GetRight(), vecForward);
    matrix->GetForward() = vecForward;
}

// 0x5A29A0
void CObject::ProcessControlLogic() {
    if (m_nModelIndex == ModelIndices::MI_SAMSITE || m_nModelIndex == ModelIndices::MI_SAMSITE2)
        ProcessSamSiteBehaviour();

    if (m_nModelIndex == ModelIndices::MI_TRAINCROSSING || m_nModelIndex == ModelIndices::MI_TRAINCROSSING1)
        ProcessTrainCrossingBehaviour();

    if (m_nModelIndex == ModelIndices::MI_MAGNOCRANE
        || m_nModelIndex == ModelIndices::MI_CRANETROLLEY
        || m_nModelIndex == ModelIndices::MI_QUARRYCRANE_ARM)
    {
        const auto iRopeInd = CRopes::FindRope(reinterpret_cast<uint32>(this));
        uint8 nSegments = 0;
        auto fRopeLengthChange = 0.0F;
        if (iRopeInd >= 0)
        {
            auto& rope = CRopes::GetRope(iRopeInd);
            nSegments = static_cast<uint8>(rope.m_fSegmentLength * 32.0F);
            fRopeLengthChange = rope.m_fMass * rope.m_fSegmentLength - static_cast<float>(nSegments) * rope.m_fTotalLength;
        }

        if (m_nModelIndex == ModelIndices::MI_MAGNOCRANE)
        {
            auto vecRopePoint = *m_matrix * CVector(0.0F, 36.64F, -1.69F);
            vecRopePoint.z += fRopeLengthChange;
            CRopes::RegisterRope(this, static_cast<uint32>(eRopeType::CRANE_MAGNO), vecRopePoint, false, nSegments, 1u, this, 20000u);
        }
        else if (m_nModelIndex == ModelIndices::MI_CRANETROLLEY)
        {
            const auto nRopeType = static_cast<const uint32>(GetPosition().x >= 0 ? eRopeType::CRANE_TROLLEY : eRopeType::WRECKING_BALL);
            auto vecRopePoint = *m_matrix * CVector(0.0F, 0.0F, 0.0F);
            vecRopePoint.z += fRopeLengthChange;
            CRopes::RegisterRope(this, nRopeType, vecRopePoint, false, nSegments, 1u, this, 20000u);
        }
        else
        {
            auto vecRopePoint = *m_matrix * CVector(0.0F, 0.0F, 59.0F);
            vecRopePoint.z += fRopeLengthChange;
            CRopes::RegisterRope(this, static_cast<uint32>(eRopeType::QUARRY_CRANE_ARM), vecRopePoint, false, nSegments, 1u, this, 20000u);
        }
    }

    if (CGarages::IsModelIndexADoor(m_nModelIndex))
        ProcessGarageDoorBehaviour();
}

// 0x5A2B90
bool IsObjectPointerValid_NotInWorld(CObject* object) {
    return GetObjectPool()->IsObjectValid(object);
}

// 0x5A2C20
bool IsObjectPointerValid(CObject* object) {
    if (!GetObjectPool()->IsObjectValid(object))
        return false;

    if (object->m_bIsBIGBuilding)
        return true;

    return !object->m_pCollisionList.IsEmpty();
}
