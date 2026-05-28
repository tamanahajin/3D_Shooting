/*!
@file EnemyInstancedRenderer.cpp
@brief 謨ｵ縺ｮ繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ謠冗判

EnemyBatchController縺御ｿ晄戟縺吶ｋ謨ｵ驟榊・縺九ｉ謠冗判逕ｨ繝・・繧ｿ縺縺代ｒ蜿励￠蜿悶ｊ縲！nstancedSkinnedDraw縺ｫ貂｡縺吶・*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	EnemyInstancedRenderer::EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	void EnemyInstancedRenderer::OnCreate()
	{
		m_Draw = AddComponent<InstancedSkinnedDraw>();
		m_Draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_Draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_Draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	void EnemyInstancedRenderer::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		if (!m_Draw)
		{
			return;
		}

		auto controllerObject = GetStage()->GetSharedGameObject(L"EnemyBatchController", false);
		auto controller = std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
		if (controller)
		{
			controller->FillInstanceSources(m_InstanceSources, m_ModelOffset);
		}
		else
		{
			m_InstanceSources.clear();
		}

		m_Draw->SetInstances(m_InstanceSources);
		m_Draw->BuildInstanceBuffer();
	}

	void EnemyBatchController::FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const
	{
		outSources.clear();
		outSources.reserve(m_Enemies.size());

		for (const auto& enemy : m_Enemies)
		{
			if (!enemy.active)
			{
				continue;
			}

			Mat4x4 world;
			world.affineTransformation(
				enemy.status.modelScale,
				Vec3(0.0f, 0.0f, 0.0f),
				enemy.rotation,
				enemy.position + modelOffset);

			SkinnedInstanceSource source{};
			source.world = world;
			source.animationIndex = static_cast<unsigned int>(enemy.animationState);
			source.animationTime = static_cast<float>(enemy.animationTime);
			source.damage = GetDamageFlashValue(enemy);
			outSources.push_back(source);
		}
	}
}

