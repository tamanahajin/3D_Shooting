/*!
@file DamageEffectComponent.h
@brief ダメージエフェクトコンポーネント（モデル全体を赤くする）
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	// ダメージエフェクト用シェーダ
	//  - VSDamageEffect.hlsl : 通常の頂点変換
	//  - PSDamageEffect.hlsl : 赤色で塗る（alpha = damage）
	DECLARE_DX12SHADER(VSDamageEffect)
	DECLARE_DX12SHADER(VSDamageEffectSkinning)
	DECLARE_DX12SHADER(PSDamageEffect)

	//--------------------------------------------------------------------------------------
	/// ダメージエフェクト用コンスタントバッファ
	/// static / skinning の両方で使う
	//--------------------------------------------------------------------------------------
	struct DamageEffectConstantBuffer
	{
		XMFLOAT4X4 World;        //!< ワールド行列（転置済み）
		XMFLOAT4X4 ViewProj;     //!< ViewProj行列（転置済み）
		float      OutlineWidth; //!< HLSL側とのレイアウト一致用
		float      Damage;       //!< 0..1
		XMFLOAT2   Pad;          //!< 16byteアライン用
		Vec4       Bones[3 * MAX_BONES]; //!< skinning用

		DamageEffectConstantBuffer()
		{
			memset(this, 0, sizeof(DamageEffectConstantBuffer));
		}
	};

	//--------------------------------------------------------------------------------------
	/// ダメージエフェクトコンポーネント
	///
	/// 目的：
	///   被弾中にモデル全体を薄く赤くする。
	//--------------------------------------------------------------------------------------
	class DamageEffect : public Component
	{
	private:
		double m_EffectTimer = 0.0;     //!< 残り時間（秒）
		bool   m_IsEffectActive = false;//!< エフェクト有効フラグ
		bool   m_UseSkinning = false;   //!< スキニングモデルかどうか
		float  m_EffectDuration = 0.2f; //!< 持続時間（秒）
		float  m_OutlineWidth = 0.03f;

		// GPUへ渡す定数
		DamageEffectConstantBuffer m_ConstantBuffer{};
		size_t m_ConstantBufferIndex = 0; //!< FrameResource内のCBスロット番号

		// ダメージエフェクト用PSOのキー（PipelineStatePoolに登録して使い回す）
		static constexpr const wchar_t* kDamageEffectStaticPSOKey = L"DamageEffectOverlayStatic";
		static constexpr const wchar_t* kDamageEffectSkinningPSOKey = L"DamageEffectOverlaySkinning";

	public:
		//--------------------------------------------------------------------------------------
		/// コンストラクタ
		//--------------------------------------------------------------------------------------
		explicit DamageEffect(const std::shared_ptr<GameObject>& gameObjectPtr);

		//--------------------------------------------------------------------------------------
		/// デストラクタ
		//--------------------------------------------------------------------------------------
		virtual ~DamageEffect();

		//--------------------------------------------------------------------------------------
		/// 初期化（CB確保＆ダメージエフェクトPSO作成）
		//--------------------------------------------------------------------------------------
		virtual void OnCreate() override;

		//--------------------------------------------------------------------------------------
		/// 更新（タイマーを減らして時間切れで停止）
		//--------------------------------------------------------------------------------------
		virtual void OnUpdate(double elapsedTime) override;

		//--------------------------------------------------------------------------------------
		/// 描画（通常描画の「後」に呼ぶのが推奨）
		/// @param pCommandList コマンドリスト
		//--------------------------------------------------------------------------------------
		void OnDraw(ID3D12GraphicsCommandList* pCommandList);

		//--------------------------------------------------------------------------------------
		/// エフェクト開始（duration秒）
		//--------------------------------------------------------------------------------------
		void StartEffect(float duration = 0.2f);

		//--------------------------------------------------------------------------------------
		/// エフェクト停止
		//--------------------------------------------------------------------------------------
		void StopEffect();

		//--------------------------------------------------------------------------------------
		/// 有効かどうか
		//--------------------------------------------------------------------------------------
		bool IsEffectActive() const { return m_IsEffectActive; }

		//--------------------------------------------------------------------------------------
		/// 持続時間設定
		//--------------------------------------------------------------------------------------
		void SetEffectDuration(float duration) { m_EffectDuration = duration; }

		void SetOutlineWidth(float width) { m_OutlineWidth = width; }
		float GetOutlineWidth() const { return m_OutlineWidth; }
	private:
		// 定数（行列・damage等）を更新
		void UpdateConstantBuffer();
		// ダメージエフェクトPSOが未作成なら作る（OnCreateで呼ぶ）
		void EnsureDamageEffectPipelineState();
	};

} // namespace shooting
