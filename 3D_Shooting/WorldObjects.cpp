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
                const float radiusBase = (collisionSize.x > collisionSize.z ? collisionSize.x : collisionSize.z) * 0.08f;
                const float capsuleRadius = radiusBase > minSize ? radiusBase : minSize;
                const float heightBase = collisionSize.y - (capsuleRadius * 2.0f);
                const float capsuleHeight = heightBase > minSize ? heightBase : minSize;
                stage->AddGameObject<StageCollisionCapsule>(collisionParam, capsuleRadius, capsuleHeight);
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
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix)
	{
		m_transParam = param;
	}
	Floor::~Floor() {}

	void Floor::OnCreate()
	{
		AddTag(L"Floor");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		ptrDraw->AddBaseModelMesh(meshes);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(
				m_MaterialPrefix + std::to_wstring(i)
			);
		}

		ptrDraw->SetOwnShadowActive(false);
	}

	FloorInstancedRenderer::FloorInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix),
		m_InstanceWorlds(instanceWorlds)
	{
	}

	FloorInstancedRenderer::~FloorInstancedRenderer() {}

	void FloorInstancedRenderer::OnCreate()
	{
		auto ptrDraw = AddComponent<InstancedStaticDraw>();

		ptrDraw->SetMeshKey(m_MeshKey);
		ptrDraw->SetMaterialPrefix(m_MaterialPrefix);
		ptrDraw->SetInstanceWorlds(m_InstanceWorlds);
		ptrDraw->SetBaseColorOverride(Col4(0.627f, 0.659f, 0.788f, 1.0f));
		ptrDraw->SetUseMaterialTexture(false);
		ptrDraw->SetLightingEnabled(true);
		ptrDraw->SetOwnShadowActive(false);
		ptrDraw->BuildInstanceBuffer();

		AddTag(L"Floor");
	}



	StageObjectInstancedRenderer::StageObjectInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix),
		m_InstanceWorlds(instanceWorlds)
	{
	}

	StageObjectInstancedRenderer::~StageObjectInstancedRenderer() {}
    void StageObjectInstancedRenderer::OnCreate()
    {
        auto ptrDraw = AddComponent<InstancedStaticDraw>();
        ptrDraw->SetMeshKey(m_MeshKey);
        ptrDraw->SetMaterialPrefix(m_MaterialPrefix);
        ptrDraw->SetInstanceWorlds(m_InstanceWorlds);
        ptrDraw->SetUseMaterialTexture(true);
        ptrDraw->SetLightingEnabled(true);
        ptrDraw->SetOwnShadowActive(false);
        ptrDraw->BuildInstanceBuffer();

        const auto* def = FindStageObjectDefByKey(m_MeshKey);
        if (ShouldCreateStageObjectCollision(def))
        {
            StageObjectLocalBounds bounds;
            if (TryGetStageObjectLocalBounds(m_MeshKey, bounds))
            {
                for (const auto& instanceWorld : m_InstanceWorlds)
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
		m_CollisionSize(collisionSize)
	{
		m_transParam = param;
	}

	StageCollisionBox::~StageCollisionBox() {}

	void StageCollisionBox::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_CollisionSize.x,
			m_CollisionSize.y,
			m_CollisionSize.z);
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
		m_Radius(radius),
		m_Height(height)
	{
		m_transParam = param;
	}

	StageCollisionCapsule::~StageCollisionCapsule() {}

	void StageCollisionCapsule::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedRadius(m_Radius);
		ptrColl->SetMakedHeight(m_Height);
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
		m_CollisionSize(collisionSize)
	{
		m_transParam = param;
	}

	FloorCollision::~FloorCollision() {}

	void FloorCollision::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_CollisionSize.x,
			m_CollisionSize.y,
			m_CollisionSize.z);
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
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
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

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
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
