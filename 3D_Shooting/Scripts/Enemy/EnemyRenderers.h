/*!
@file EnemyRenderers.h
@brief 敵のインスタンシング描画と比較用個別描画
*/

#pragma once
#include "stdafx.h"
#include <memory>
#include <vector>

namespace shooting {

	class EnemyBatchController;
	class EnemyIndividualDrawProxy;
	class BcPNTBoneDraw;

	/*!
	@brief 敵バッチをまとめて描画するインスタンシング描画オブジェクト

	EnemyBatchController から描画用インスタンス配列を受け取り、
	InstancedSkinnedDraw へ渡す。敵1体ごとの GameObject 描画は行わない。
	*/
	class EnemyInstancedRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		std::shared_ptr<InstancedSkinnedDraw> m_Draw;
		std::vector<SkinnedInstanceSource> m_InstanceSources;
		Vec3 m_ModelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_RenderingEnabled = true;

	public:
		/*!
		@brief 敵描画オブジェクトを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage);
		/*!
		@brief 敵描画オブジェクトを生成し、参照する敵バッチコントローラを保持する
		@param stage 所属するステージ
		@param controller 描画元になる敵バッチコントローラ
		*/
		EnemyInstancedRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller);
		virtual ~EnemyInstancedRenderer();
		/*!
		@brief 描画コンポーネントと敵描画タグを初期化する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief EnemyBatchController の状態を描画インスタンスへ反映する
		@param elapsedTime 経過時間。この処理では使用しない
		*/
		virtual void OnUpdate2(double elapsedTime) override;
		/*!
		@brief インスタンシング描画の有効状態を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
		/*!
		@brief 現在インスタンシング描画が有効かを取得する
		@return 有効なら true
		*/
		bool IsRenderingEnabled() const { return m_RenderingEnabled; }
	};

	/*!
	@brief 敵1体を通常のスキンメッシュ描画で表示する軽量プロキシ
	*/
	class EnemyIndividualDrawProxy : public GameObject
	{
	private:
		std::shared_ptr<BcPNTBoneDraw> m_Draw;

	public:
		/*!
		@brief 通常描画プロキシを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyIndividualDrawProxy(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyIndividualDrawProxy();
		/*!
		@brief BcPNTBoneDraw を作成し、敵モデルとテクスチャを設定する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief インスタンス描画用データを通常描画用 Transform とアニメーションへ反映する
		@param source EnemyBatchController が作成した描画用データ
		*/
		void ApplyInstanceSource(const SkinnedInstanceSource& source);
		/*!
		@brief プロキシの描画・更新を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
	};

	/*!
	@brief 敵を1体ずつ通常描画するレンダラー

	EnemyBatchController の描画用データを、敵数分の EnemyIndividualDrawProxy へ同期する。
	インスタンシングを使わない比較動画用の描画経路。
	*/
	class EnemyIndividualRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		std::vector<std::shared_ptr<EnemyIndividualDrawProxy>> m_DrawProxies;
		std::vector<SkinnedInstanceSource> m_InstanceSources;
		Vec3 m_ModelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_RenderingEnabled = false;

		/*!
		@brief 描画に必要なプロキシ数を揃える
		@param requiredCount 必要なプロキシ数
		*/
		void ResizeDrawProxies(size_t requiredCount);
		/*!
		@brief 生成済みプロキシをステージから外す
		*/
		void ClearDrawProxies();

	public:
		/*!
		@brief 通常描画レンダラーを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyIndividualRenderer(const std::shared_ptr<Stage>& stage);
		/*!
		@brief 通常描画レンダラーを生成し、参照する敵バッチコントローラを保持する
		@param stage 所属するステージ
		@param controller 描画元になる敵バッチコントローラ
		*/
		EnemyIndividualRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller);
		virtual ~EnemyIndividualRenderer();
		/*!
		@brief レンダラー本体のタグと初期無効状態を設定する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief EnemyBatchController の状態を通常描画プロキシへ反映する
		@param elapsedTime 経過時間。この処理では使用しない
		*/
		virtual void OnUpdate2(double elapsedTime) override;
		/*!
		@brief 通常描画経路の有効状態を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
		/*!
		@brief 現在通常描画経路が有効かを取得する
		@return 有効なら true
		*/
		bool IsRenderingEnabled() const { return m_RenderingEnabled; }
	};

}
