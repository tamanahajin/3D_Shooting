/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

    namespace
    {
        struct StageObjectLocalBounds
        {
            Vec3 center = Vec3(0.0f, 0.0f, 0.0f);
            Vec3 size = Vec3(1.0f, 1.0f, 1.0f);
            bool valid = false;
        };

        const float kSingleLogCollisionHorizontalScale = 0.75f;
        const float kSingleLogCollisionHeightScale = 0.80f;
        const float kTreeCollisionHeightScale = 6.0f;
        const float kTreeTrunkCollisionRadiusScale = 0.08f;
        const float kTreeJumpCollisionRadiusScale = 0.18f;
        const float kTreeJumpCollisionBottomOffset = 0.70f;
        // shadow map の境界ギリギリで影が欠けないよう、範囲判定に少し余白を持たせる。
        const float kStageObjectShadowCullMargin = 12.0f;
        // 注視点がこの距離以上動いたときだけ shadow 用インスタンス buffer を作り直す。
        const float kStageObjectShadowCullMoveThreshold = 2.0f;
        const std::wstring kStageObjectBoxShadowProxyMeshKey = L"DEFAULT_CUBE";
        const std::wstring kStageObjectSlopeShadowProxyMeshKey = L"STAGEOBJ_SHADOW_SLOPE_PROXY";

        bool TryGetStageObjectLocalBounds(const std::wstring& meshKey, StageObjectLocalBounds& outBounds);
        Vec3 TransformStageObjectPoint(const Vec3& localPoint, const Mat4x4& world);

        const StageObjectDef* FindStageObjectDefByKey(const std::wstring& key)
        {
            const auto& defs = StageObjectCatalog::GetAll();
            for (const auto& def : defs)
            {
                if (def.key == key)
                {
                    return &def;
                }
            }
            return nullptr;
        }

        bool ShouldCreateStageObjectCollision(const StageObjectDef* def)
        {
            if (!def || !def->blocksMovement)
            {
                return false;
            }

            // 草やきのこは移動を邪魔しない飾りとして扱う。
            if (def->category == StageObjectCategory::Plant ||
                def->category == StageObjectCategory::Mushroom)
            {
                return false;
            }

            // 外周崖はGameStage側の4枚の大きい壁コリジョンで扱う。見た目用の外周モデル全てに付けると数が増えすぎる。
            if (def->category == StageObjectCategory::OutSideWall)
            {
                return false;
            }

            // スロープと高低差ブロックは、GameStage側で高さグリッドから集約コリジョンを作る。
            // 見た目用に積んだブロック全部へOBBを付けると、3段構成で衝突数とデバッグ描画が増えすぎる。
            if (def->name.find(L"slope") != std::wstring::npos)
            {
                return false;
            }
            if (def->category == StageObjectCategory::Cliff &&
                def->name.find(L"block") != std::wstring::npos)
            {
                return false;
            }

            return true;
        }


        bool IsSlopeStageObject(const StageObjectDef* def)
        {
            return def && def->name.find(L"slope") != std::wstring::npos;
        }

        bool ShouldUseStageObjectShadowProxy(const StageObjectDef* def)
        {
            if (!def)
            {
                return false;
            }

            // 高台・坂は元モデルを shadow pass に出さず、軽量 proxy mesh だけで影を作る。
            return def->category == StageObjectCategory::Cliff ||
                def->category == StageObjectCategory::Platform ||
                IsSlopeStageObject(def);
        }

        const std::wstring& GetStageObjectShadowProxyMeshKey(const StageObjectDef* def)
        {
            // 坂を cube proxy で描くと箱の影になるため、坂だけ斜面形状の proxy mesh を使う。
            return IsSlopeStageObject(def)
                ? kStageObjectSlopeShadowProxyMeshKey
                : kStageObjectBoxShadowProxyMeshKey;
        }
        bool ShouldCastStageObjectShadow(const StageObjectDef* def)
        {
            if (!def)
            {
                return false;
            }

            // 外周壁は画面を囲う大きな面なので、影は受けるだけにして shadow pass には出さない。
            if (def->category == StageObjectCategory::OutSideWall)
            {
                return false;
            }

            if (ShouldUseStageObjectShadowProxy(def))
            {
                return true;
            }

            return def->category == StageObjectCategory::Tree ||
                def->category == StageObjectCategory::Log ||
                def->category == StageObjectCategory::Rock ||
                def->category == StageObjectCategory::Stone;
        }

        bool ShouldReceiveStageObjectShadow(const StageObjectDef* def)
        {
            if (!def)
            {
                return false;
            }

            // 高台・坂・外周壁は、木やログなどが落とす影を受ける。
            return def->category == StageObjectCategory::Cliff ||
                def->category == StageObjectCategory::Platform ||
                def->category == StageObjectCategory::OutSideWall ||
                def->name.find(L"slope") != std::wstring::npos;
        }

        // ライト情報が使えない場合の簡易判定用。shadow map サイズを円形範囲として扱う。
        float GetStageObjectShadowCullHalfExtent()
        {
            const float halfWidth = ShadowMap::GetViewWidth() * 0.5f;
            const float halfHeight = ShadowMap::GetViewHeight() * 0.5f;
            return (halfWidth > halfHeight ? halfWidth : halfHeight) + kStageObjectShadowCullMargin;
        }

        // 毎フレーム buffer を再作成すると重いので、注視点の移動量で更新必要性を判定する。
        bool HasStageObjectShadowCullCenterMoved(const Vec3& currentAt, const Vec3& lastAt)
        {
            const float dx = currentAt.x - lastAt.x;
            const float dy = currentAt.y - lastAt.y;
            const float dz = currentAt.z - lastAt.z;
            const float thresholdSq = kStageObjectShadowCullMoveThreshold * kStageObjectShadowCullMoveThreshold;
            return (dx * dx) + (dy * dy) + (dz * dz) >= thresholdSq;
        }

        // fallback 用。カメラ注視点周辺にあるインスタンスだけを shadow pass 候補にする。
        bool IsStageObjectInsideFocusRange(const Mat4x4& instanceWorld, const Vec3& focus)
        {
            const Vec3 position = instanceWorld.transInMatrix();
            const float range = GetStageObjectShadowCullHalfExtent();
            const float dx = position.x - focus.x;
            const float dz = position.z - focus.z;
            return (dx * dx) + (dz * dz) <= range * range;
        }

        Mat4x4 BuildStageObjectShadowProxyWorld(
            const StageObjectLocalBounds& bounds,
            const Mat4x4& instanceWorld)
        {
            Vec3 worldScale = instanceWorld.scaleInMatrix();
            Vec3 proxyScale(
                std::fabs(bounds.size.x * worldScale.x),
                std::fabs(bounds.size.y * worldScale.y),
                std::fabs(bounds.size.z * worldScale.z));

            const float minProxySize = 0.05f;
            if (proxyScale.x < minProxySize) proxyScale.x = minProxySize;
            if (proxyScale.y < minProxySize) proxyScale.y = minProxySize;
            if (proxyScale.z < minProxySize) proxyScale.z = minProxySize;

            Mat4x4 proxyWorld;
            proxyWorld.affineTransformation(
                proxyScale,
                Vec3(0.0f, 0.0f, 0.0f),
                instanceWorld.quatInMatrix(),
                TransformStageObjectPoint(bounds.center, instanceWorld));
            return proxyWorld;
        }

        // インスタンス位置をライト視点に変換し、現在の shadow map 正射影範囲に入るか調べる。
        bool IsStageObjectInsideShadowMapRange(
            const Mat4x4& instanceWorld,
            const Vec3& lightAt,
            const Light& mainLight)
        {
            Vec3 lightDir = -mainLight.m_directional;
            Vec3 lightEye = lightAt + (lightDir * ShadowMap::GetLightHeight());

            XMMATRIX lightView = XMMatrixLookAtLH(
                (XMVECTOR)lightEye,
                (XMVECTOR)lightAt,
                (XMVECTOR)Vec3(0.0f, 1.0f, 0.0f));

            const Vec3 position = instanceWorld.transInMatrix();
            XMVECTOR lightSpacePos = XMVector3TransformCoord(
                XMVectorSet(position.x, position.y, position.z, 1.0f),
                lightView);

            XMFLOAT3 p{};
            XMStoreFloat3(&p, lightSpacePos);

            const float halfWidth = (ShadowMap::GetViewWidth() * 0.5f) + kStageObjectShadowCullMargin;
            const float halfHeight = (ShadowMap::GetViewHeight() * 0.5f) + kStageObjectShadowCullMargin;
            const float nearZ = ShadowMap::GetLightNear() - kStageObjectShadowCullMargin;
            const float farZ = ShadowMap::GetLightFar() + kStageObjectShadowCullMargin;

            return std::fabs(p.x) <= halfWidth &&
                std::fabs(p.y) <= halfHeight &&
                p.z >= nearZ &&
                p.z <= farZ;
        }

        // 通常描画は全インスタンスを使うが、shadow pass では影に効く範囲のものだけを抜き出す。
        std::vector<Mat4x4> BuildStageObjectShadowInstanceWorlds(
            const StageObjectDef* def,
            const std::vector<Mat4x4>& instanceWorlds,
            const std::shared_ptr<Camera>& camera,
            const std::shared_ptr<LightSet>& lightSet)
        {
            if (!ShouldCastStageObjectShadow(def))
            {
                return {};
            }

            if (!camera)
            {
                // 初期化直後などカメラがまだ取れない場合は、全件投入せず次の更新で再判定する。
                return {};
            }

            const bool useProxyMesh = ShouldUseStageObjectShadowProxy(def);
            StageObjectLocalBounds proxyBounds;
            if (useProxyMesh && !TryGetStageObjectLocalBounds(def->key, proxyBounds))
            {
                return {};
            }

            const Vec3 focus = camera->GetAt();
            const bool useShadowMapRange = lightSet && lightSet->GetNumLights() > 0;
            const Light mainLight = useShadowMapRange ? lightSet->GetMainBaseLight() : Light();

            std::vector<Mat4x4> shadowWorlds;
            shadowWorlds.reserve(instanceWorlds.size());
            for (const auto& instanceWorld : instanceWorlds)
            {
                const bool insideRange = useShadowMapRange
                    ? IsStageObjectInsideShadowMapRange(instanceWorld, focus, mainLight)
                    : IsStageObjectInsideFocusRange(instanceWorld, focus);
                if (insideRange)
                {
                    // 高台・坂は元メッシュではなく、ローカル境界から作った軽量 proxy を shadow pass に渡す。
                    shadowWorlds.push_back(useProxyMesh
                        ? BuildStageObjectShadowProxyWorld(proxyBounds, instanceWorld)
                        : instanceWorld);
                }
            }

            return shadowWorlds;
        }

        void IncludeBoundsPoint(const aiVector3D& point, Vec3& minPoint, Vec3& maxPoint)
        {
            if (point.x < minPoint.x) minPoint.x = point.x;
            if (point.y < minPoint.y) minPoint.y = point.y;
            if (point.z < minPoint.z) minPoint.z = point.z;
            if (point.x > maxPoint.x) maxPoint.x = point.x;
            if (point.y > maxPoint.y) maxPoint.y = point.y;
            if (point.z > maxPoint.z) maxPoint.z = point.z;
        }

        bool TryGetStageObjectLocalBounds(const std::wstring& meshKey, StageObjectLocalBounds& outBounds)
        {
            static std::map<std::wstring, StageObjectLocalBounds> boundsCache;
            const auto cached = boundsCache.find(meshKey);
            if (cached != boundsCache.end())
            {
                outBounds = cached->second;
                return outBounds.valid;
            }

            const auto& meshes = BaseScene::Get()->GetModelMesh(meshKey);
            std::shared_ptr<BaseAssimp> assimp;
            for (const auto& mesh : meshes)
            {
                if (mesh && mesh->GetBaseAssimp())
                {
                    assimp = mesh->GetBaseAssimp();
                    break;
                }
            }

            if (!assimp || !assimp->m_pScene)
            {
                return false;
            }

            const float boundLimit = 1000000.0f;
            Vec3 minPoint(boundLimit, boundLimit, boundLimit);
            Vec3 maxPoint(-boundLimit, -boundLimit, -boundLimit);

            bool found = false;
            const aiScene* scene = assimp->m_pScene;
            for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
            {
                const aiMesh* mesh = scene->mMeshes[meshIndex];
                if (!mesh || !mesh->HasPositions())
                {
                    continue;
                }

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    IncludeBoundsPoint(mesh->mVertices[vertexIndex], minPoint, maxPoint);
                    found = true;
                }
            }

            if (!found)
            {
                return false;
            }

            outBounds.center = (minPoint + maxPoint) * 0.5f;
            outBounds.size = maxPoint - minPoint;
            outBounds.valid = true;
            boundsCache[meshKey] = outBounds;
            return true;
        }

        Vec3 TransformStageObjectPoint(const Vec3& localPoint, const Mat4x4& world)
        {
            return Vec3(
                (localPoint.x * world._11) + (localPoint.y * world._21) + (localPoint.z * world._31) + world._41,
                (localPoint.x * world._12) + (localPoint.y * world._22) + (localPoint.z * world._32) + world._42,
                (localPoint.x * world._13) + (localPoint.y * world._23) + (localPoint.z * world._33) + world._43);
        }

        void AddStageObjectCollisionInstance(
            const std::shared_ptr<Stage>& stage,
            const StageObjectDef* def,
            const StageObjectLocalBounds& bounds,
            const Mat4x4& instanceWorld)
        {
            Vec3 worldScale = instanceWorld.scaleInMatrix();
            Vec3 collisionSize(
                std::fabs(bounds.size.x * worldScale.x),
                std::fabs(bounds.size.y * worldScale.y),
                std::fabs(bounds.size.z * worldScale.z));

            const float minSize = 0.05f;
            if (collisionSize.x < minSize) collisionSize.x = minSize;
            if (collisionSize.y < minSize) collisionSize.y = minSize;
            if (collisionSize.z < minSize) collisionSize.z = minSize;

            TransParam collisionParam;
            collisionParam.scale = Vec3(1.0f, 1.0f, 1.0f);
            collisionParam.quaternion = instanceWorld.quatInMatrix();
            collisionParam.position = TransformStageObjectPoint(bounds.center, instanceWorld);


            if (def && def->category == StageObjectCategory::Tree)
            {
                const float maxHorizontalSize = collisionSize.x > collisionSize.z ? collisionSize.x : collisionSize.z;
                const float radiusBase = maxHorizontalSize * kTreeTrunkCollisionRadiusScale;
                const float capsuleRadius = radiusBase > minSize ? radiusBase : minSize;
                const float capsuleTotalHeight = collisionSize.y * kTreeCollisionHeightScale;
                const float originalBottomY = collisionParam.position.y - (collisionSize.y * 0.5f);

                // 幹用。地上で木の横を通るときの引っかかりを増やしすぎないよう細めにする。
                TransParam trunkParam = collisionParam;
                trunkParam.position.y = originalBottomY + (capsuleTotalHeight * 0.5f);
                const float heightBase = capsuleTotalHeight - (capsuleRadius * 2.0f);
                const float capsuleHeight = heightBase > minSize ? heightBase : minSize;
                stage->AddGameObject<StageCollisionCapsule>(trunkParam, capsuleRadius, capsuleHeight);

                // ジャンプ中に見た目の木の上部をすり抜けないよう、上側だけ少し太い補助カプセルを置く。
                const float jumpRadiusBase = maxHorizontalSize * kTreeJumpCollisionRadiusScale;
                const float jumpCapsuleRadius = jumpRadiusBase > capsuleRadius ? jumpRadiusBase : capsuleRadius;
                const float jumpBottomOffset = kTreeJumpCollisionBottomOffset < capsuleTotalHeight ? kTreeJumpCollisionBottomOffset : capsuleTotalHeight * 0.25f;
                const float jumpCapsuleBottomY = originalBottomY + jumpBottomOffset;
                const float jumpCapsuleTotalHeight = capsuleTotalHeight - jumpBottomOffset;
                if (jumpCapsuleTotalHeight > jumpCapsuleRadius * 2.0f)
                {
                    TransParam jumpParam = collisionParam;
                    jumpParam.position.y = jumpCapsuleBottomY + (jumpCapsuleTotalHeight * 0.5f);
                    stage->AddGameObject<StageCollisionCapsule>(
                        jumpParam,
                        jumpCapsuleRadius,
                        jumpCapsuleTotalHeight - (jumpCapsuleRadius * 2.0f));
                }
                return;
            }
            if (def && def->category == StageObjectCategory::Log && def->name == L"log")
            {
                collisionSize.x *= kSingleLogCollisionHorizontalScale;
                collisionSize.y *= kSingleLogCollisionHeightScale;
                collisionSize.z *= kSingleLogCollisionHorizontalScale;
            }

            if (def && def->category == StageObjectCategory::Cliff)
            {
                const float platformTopClearance = 0.6f;
                const float platformBottomExtension = 0.35f;
                if (collisionSize.y > platformTopClearance + minSize)
                {
                    collisionSize.y = collisionSize.y - platformTopClearance + platformBottomExtension;
                    collisionParam.position.y -= (platformTopClearance + platformBottomExtension) * 0.5f;
                }
            }
            stage->AddGameObject<StageCollisionBox>(collisionParam, collisionSize);
        }
    }
	SkyDome::SkyDome(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	SkyDome::~SkyDome() {}

	void SkyDome::OnCreate()
	{
		SetShadowActive(false);
		SetAlphaActive(false);

		auto ptrDraw = AddComponent<SkyDomeDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetRadius(450.0f);

		AddTag(L"Sky");
	}

	//--------------------------------------------------------------------------------------
	// フロアオブジェクト
	//--------------------------------------------------------------------------------------
	Floor::Floor(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix) :
		GameObject(stage),
		m_meshKey(meshKey),
		m_materialPrefix(materialPrefix)
	{
		m_transParam = param;
	}
	Floor::~Floor() {}

	void Floor::OnCreate()
	{
		AddTag(L"Floor");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_meshKey);
		ptrDraw->AddBaseModelMesh(meshes);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(
				m_materialPrefix + std::to_wstring(i)
			);
		}

		ptrDraw->SetOwnShadowActive(true);
	}

	FloorInstancedRenderer::FloorInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_meshKey(meshKey),
		m_materialPrefix(materialPrefix),
		m_instanceWorlds(instanceWorlds)
	{
	}

	FloorInstancedRenderer::~FloorInstancedRenderer() {}

	void FloorInstancedRenderer::OnCreate()
	{
		auto ptrDraw = AddComponent<InstancedStaticDraw>();

		ptrDraw->SetMeshKey(m_meshKey);
		ptrDraw->SetMaterialPrefix(m_materialPrefix);
		ptrDraw->SetInstanceWorlds(m_instanceWorlds);
		ptrDraw->SetBaseColorOverride(Col4(0.627f, 0.659f, 0.788f, 1.0f));
		ptrDraw->SetUseMaterialTexture(false);
		ptrDraw->SetLightingEnabled(true);
		ptrDraw->SetOwnShadowActive(true);
		ptrDraw->SetCastShadowActive(false);
		ptrDraw->BuildInstanceBuffer();

		AddTag(L"Floor");
	}



	StageObjectInstancedRenderer::StageObjectInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_meshKey(meshKey),
		m_materialPrefix(materialPrefix),
		m_instanceWorlds(instanceWorlds)
	{
	}

	StageObjectInstancedRenderer::~StageObjectInstancedRenderer() {}
    void StageObjectInstancedRenderer::RefreshShadowInstances(bool force)
    {
        const auto* def = FindStageObjectDefByKey(m_meshKey);
        if (!ShouldCastStageObjectShadow(def))
        {
            // 影を落とさない種類は shadow 用 buffer を空にして、shadow pass 自体も無効化する。
            if (force || !m_shadowInstanceWorlds.empty())
            {
                m_shadowInstanceWorlds.clear();
                SetShadowActive(false);
                if (m_draw)
                {
                    m_draw->SetCastShadowActive(false);
                    m_draw->SetShadowInstanceWorlds(m_shadowInstanceWorlds);
                    m_draw->BuildShadowInstanceBuffer();
                }
            }
            return;
        }

        auto camera = GetCamera();
        const Vec3 currentAt = camera ? camera->GetAt() : Vec3(0.0f, 0.0f, 0.0f);
        // 注視点がほぼ動いていない間は、前回作った shadow 用インスタンス配列を使い回す。
        if (!force && m_shadowCullInitialized &&
            !HasStageObjectShadowCullCenterMoved(currentAt, m_lastShadowCullAt))
        {
            return;
        }

        m_lastShadowCullAt = currentAt;
        m_shadowCullInitialized = true;
        // ここで shadow pass 専用にカリング済み配列を作り、GPU へ渡す instance buffer を軽くする。
        m_shadowInstanceWorlds = BuildStageObjectShadowInstanceWorlds(def, m_instanceWorlds, camera, GetLightSet());

        const bool castShadow = !m_shadowInstanceWorlds.empty();
        SetShadowActive(castShadow);
        if (m_draw)
        {
            m_draw->SetCastShadowActive(castShadow);
            m_draw->SetShadowInstanceWorlds(m_shadowInstanceWorlds);
            m_draw->BuildShadowInstanceBuffer();
        }
    }

    void StageObjectInstancedRenderer::OnUpdate(double elapsedTime)
    {
        (void)elapsedTime;
        RefreshShadowInstances(false);
    }

    void StageObjectInstancedRenderer::OnCreate()
    {
        m_draw = AddComponent<InstancedStaticDraw>();
        auto ptrDraw = m_draw;
        ptrDraw->SetMeshKey(m_meshKey);
        ptrDraw->SetMaterialPrefix(m_materialPrefix);
        ptrDraw->SetInstanceWorlds(m_instanceWorlds);
        ptrDraw->SetUseMaterialTexture(true);
        ptrDraw->SetLightingEnabled(true);

        const auto* def = FindStageObjectDefByKey(m_meshKey);
        const bool receiveShadow = ShouldReceiveStageObjectShadow(def);
        if (ShouldUseStageObjectShadowProxy(def))
        {
            // 高台は cube、坂は斜面形状の proxy を使う。通常描画は元モデルのまま。
            ptrDraw->SetShadowMeshKey(GetStageObjectShadowProxyMeshKey(def));
        }
        else
        {
            ptrDraw->ClearShadowMeshKey();
        }
        // Scene pass は全インスタンス、shadow pass は RefreshShadowInstances() で絞った配列を使う。
        ptrDraw->SetOwnShadowActive(receiveShadow);
        ptrDraw->SetCastShadowActive(false);
        ptrDraw->BuildInstanceBuffer();
        RefreshShadowInstances(true);
        if (ShouldCreateStageObjectCollision(def))
        {
            StageObjectLocalBounds bounds;
            if (TryGetStageObjectLocalBounds(m_meshKey, bounds))
            {
                for (const auto& instanceWorld : m_instanceWorlds)
                {
                    AddStageObjectCollisionInstance(GetStage(), def, bounds, instanceWorld);
                }
            }
        }

        AddTag(L"StageObject");
    }
	StageCollisionBox::StageCollisionBox(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const Vec3& collisionSize) :
		GameObject(stage),
		m_collisionSize(collisionSize)
	{
		m_transParam = param;
	}

	StageCollisionBox::~StageCollisionBox() {}

	void StageCollisionBox::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_collisionSize.x,
			m_collisionSize.y,
			m_collisionSize.z);
		ptrColl->SetFixed(true);

		SetAlphaActive(false);
		SetShadowActive(false);
		
		AddTag(L"StageObjectCollision");
		AddTag(L"Wall");
	}
	StageCollisionCapsule::StageCollisionCapsule(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		float radius,
		float height) :
		GameObject(stage),
		m_radius(radius),
		m_height(height)
	{
		m_transParam = param;
	}

	StageCollisionCapsule::~StageCollisionCapsule() {}

	void StageCollisionCapsule::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedRadius(m_radius);
		ptrColl->SetMakedHeight(m_height);
		ptrColl->SetFixed(true);

		SetAlphaActive(false);
		SetShadowActive(false);

		AddTag(L"StageObjectCollision");
		AddTag(L"Wall");
	}

	SlopeCollisionDebugBox::SlopeCollisionDebugBox(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param) :
		GameObject(stage)
	{
		m_transParam = param;
	}

	SlopeCollisionDebugBox::~SlopeCollisionDebugBox() {}

	void SlopeCollisionDebugBox::OnCreate()
	{
		SetAlphaActive(true);
		SetShadowActive(false);

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->SetDiffuseColor(Col4(0.0f, 0.85f, 1.0f, 0.35f));
		ptrDraw->SetLightingEnabled(false);
		ptrDraw->SetFogEnabled(false);
		ptrDraw->SetOwnShadowActive(false);

		AddTag(L"SlopeCollisionDebug");
	}

	FloorCollision::FloorCollision(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const Vec3& collisionSize) :
		GameObject(stage),
		m_collisionSize(collisionSize)
	{
		m_transParam = param;
	}

	FloorCollision::~FloorCollision() {}

	void FloorCollision::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_collisionSize.x,
			m_collisionSize.y,
			m_collisionSize.z);
		ptrColl->SetFixed(true);

		AddTag(L"Floor");
	}

	//--------------------------------------------------------------------------------------
	// ボックスオブジェクト
	//--------------------------------------------------------------------------------------
	FixedBox::FixedBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage)
	{
		m_transParam = param;
	}
	FixedBox::~FixedBox() {}

	void FixedBox::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		auto trans = GetComponent<Transform>();
		auto scale = trans->GetScale();

		ptrColl->SetMakedSize(scale.x, scale.y, scale.z);
		ptrColl->SetFixed(true);
		//タグをつける
		AddTag(L"FixedBox");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		// 壁は shadow pass に出さず、表示シェーダ側で影だけ受ける。
		SetShadowActive(false);
		ptrDraw->SetOwnShadowActive(true);
	}

	//--------------------------------------------------------------------------------------
	// 四角のオブジェクト
	//--------------------------------------------------------------------------------------
	WallBox::WallBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage),
		m_totalTime(0.0)
	{
		m_transParam = param;
	}
	WallBox::~WallBox() {}

	void WallBox::OnCreate()
	{
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		//重力をつける
		auto ptrGra = AddComponent<Gravity>();

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		// 壁は shadow pass に出さず、表示シェーダ側で影だけ受ける。
		SetShadowActive(false);
		ptrDraw->SetOwnShadowActive(true);
	}

	void WallBox::OnUpdate(double elapsedTime)
	{
		//Transformコンポーネントを取り出す
		auto ptrTrans = GetComponent<Transform>();
		auto& param = ptrTrans->GetTransParam();

		m_totalTime += elapsedTime;
		if (m_totalTime >= XM_2PI)
		{
			m_totalTime = 0.0;
		}
		param.position.x = (float)sin(m_totalTime) * 2.0f;
	}


	//--------------------------------------------------------------------------------------
	//	追いかける配置オブジェクト
	//--------------------------------------------------------------------------------------
	//構築と破棄
}
