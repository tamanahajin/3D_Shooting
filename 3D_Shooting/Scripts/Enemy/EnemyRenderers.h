/*!
@file EnemyRenderers.h
@brief 敵のインスタンシング描画と比較用通常描画を扱う
*/

#pragma once
#include "stdafx.h"
#include <memory>
#include <vector>

namespace shooting {

	class EnemyController;
	class EnemyIndividualDrawProxy;
	class BcPNTBoneDraw;

	/*!
	@brief EnemyController の描画データを InstancedSkinnedDraw へ渡すレンダラー

	敵1体ごとの GameObject 描画を避け、大量敵をまとめて描画する。
	*/
	class EnemyInstancedRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyController> m_controller;
		std::shared_ptr<InstancedSkinnedDraw> m_draw;
		std::vector<SkinnedInstanceSource> m_instanceSources;
		Vec3 m_modelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_renderingEnabled = true;

	public:
		explicit EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage);
		EnemyInstancedRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyController>& controller);
		virtual ~EnemyInstancedRenderer();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief 敵配列から最新の描画インスタンスを受け取り、GPU用バッファを更新する
		*/
		virtual void OnUpdate2(double elapsedTime) override;
		void SetRenderingEnabled(bool enabled);
		bool IsRenderingEnabled() const { return m_renderingEnabled; }
	};

	/*!
	@brief 比較動画用に敵1体を通常スキンメッシュ描画するプロキシ
	*/
	class EnemyIndividualDrawProxy : public GameObject
	{
	private:
		std::shared_ptr<BcPNTBoneDraw> m_draw;

	public:
		explicit EnemyIndividualDrawProxy(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyIndividualDrawProxy();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief インスタンシング用データを通常描画用 Transform とアニメーションへ反映する
		*/
		void ApplyInstanceSource(const SkinnedInstanceSource& source);
		void SetRenderingEnabled(bool enabled);
	};

	/*!
	@brief 敵を1体ずつ通常描画する比較用レンダラー

	インスタンシング描画との差を比較するため、EnemyIndividualDrawProxy を敵数分だけ同期する。
	*/
	class EnemyIndividualRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyController> m_controller;
		std::vector<std::shared_ptr<EnemyIndividualDrawProxy>> m_drawProxies;
		std::vector<SkinnedInstanceSource> m_instanceSources;
		Vec3 m_modelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_renderingEnabled = false;

		void ResizeDrawProxies(size_t requiredCount);
		void ClearDrawProxies();

	public:
		explicit EnemyIndividualRenderer(const std::shared_ptr<Stage>& stage);
		EnemyIndividualRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyController>& controller);
		virtual ~EnemyIndividualRenderer();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnUpdate2(double elapsedTime) override;
		void SetRenderingEnabled(bool enabled);
		bool IsRenderingEnabled() const { return m_renderingEnabled; }
	};

}
