#pragma once
#include "stdafx.h"
#include "Project.h"

namespace shooting {

	struct PlayerBombAim;
	struct PlayerNormalShotAim;

	class Player : public GameObject
	{
	private:
		//プレイヤーが使用するコントローラとキーボードの入力
		Vec2 GetInputState() const;
		Vec2 GetInputKey() const;
		// 方向ベクトルを得る
		Vec3 GetMoveVector() const;
		//プレイヤーの移動
		void MovePlayer(float elapsedTime);
		void BeginSpawnIntro();
		bool UpdateSpawnIntro(double elapsedTime);
		void UpdateSpawnIntroCamera(const Vec3& playerPosition);
		void SetSpawnIntroCharacterVisible(bool visible);
		/*!
		@brief プレイヤー死亡時の共通演出を開始する

		BGM停止とヒットストップを行う。通常ダメージによる死亡と落下死の両方から呼ぶ。
		*/
		void StartDeathPresentation();
		/*!
		@brief 予約された死亡SEの遅延時間を実時間で更新する
		@param rawElapsedTime 時間倍率を適用していない経過時間
		*/
		void UpdateDeathSound(double rawElapsedTime);
		/*! @brief ヒットストップと死亡演出をアニメーション再生倍率へ反映する */
		void UpdateAnimationPlaybackRate(double rawElapsedTime, double gameElapsedTime);
		/*! @brief 死亡中または落下死を処理する @return 以降の通常更新を止める場合はtrue */
		bool UpdateDeathState();
		/*! @brief 登場演出を更新する @return 登場演出中の場合はtrue */
		bool UpdateSpawnIntroState(double elapsedTime);
		/*! @brief 移動アニメーション、移動、ジャンプを更新する */
		void UpdateMovementState(double elapsedTime, bool hitStopActive);
		/*!
		@brief 射撃入力、照準解決、プレビュー、発射を更新する
		*/
		void UpdateCombat(double elapsedTime, bool hitStopActive);
		/*! @brief 水平方向だけを使って攻撃対象へプレイヤーを向ける */
		void FaceAttackTarget(const Vec3& target);
		/*! @brief 解決済みの照準結果を使って爆弾を投擲する */
		void FireBomb(const PlayerBombAim& aim);
		/*! @brief 解決済みの照準結果を使って通常弾を発射する */
		void FireNormalShot(const PlayerNormalShotAim& aim);
		//入力ハンドラー
		InputHandler<Player> m_InputHandler;
		//スピード
		float m_Speed;
		// 地面にいるかどうか
		bool m_IsGround;
		// 弾発射間隔
		double m_ShotCool = 0.0;
		std::shared_ptr<MainCamera> m_MainCamera;
		// CollisionManager側も空間分割ノードからPlayerを保持するため、弱参照にして循環所有を防ぐ。
		std::weak_ptr<CollisionManager> m_CollisionManager;

		// 地面衝突判定の共通処理
		void CheckGroundCollision(const CollisionPair& pair);
		void CheckItemPickup(const CollisionPair& pair);
		void ResolveSlopeCollision(double elapsedTime);
		// 現在の弾タイプ
		BulletType m_CurrentBullet = BulletType::Default;
		int m_BombAmmo = 0;

		std::shared_ptr<BombAimPreview> m_BombPreview;

		bool m_IsDead = false;
		bool m_DeathAnimFinished = false;
		bool m_DeathSoundPending = false;
		double m_DeathSoundDelayTimer = 0.0;
		bool m_SpawnIntroActive = false;
		bool m_SpawnIntroCharacterVisible = true;
		bool m_SpawnIntroSePlayed = false;
		double m_SpawnIntroTimer = 0.0;
		Vec3 m_SpawnIntroStartPosition = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_SpawnIntroEndPosition = Vec3(0.0f, 0.0f, 0.0f);
	public:
		Player(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~Player() {}
		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);
		virtual void OnUpdate2(double elapsedTime) override;
		//衝突開始時処理
		virtual void OnCollisionEnter(const CollisionPair& pair)override;
		//衝突継続時処理
		virtual void OnCollisionExecute(const CollisionPair& pair)override;
		//Aボタン
		void OnPushA();
		//Bボタン
		void OnPushB() {}

		void AddBombAmmo(int amount);
		int GetBombAmmo() const { return m_BombAmmo; }
		bool IsBombMode() const { return m_CurrentBullet == BulletType::Bomb && m_BombAmmo > 0; }

		bool IsDead() const { return m_IsDead; }
		bool IsDeathAnimationFinished() const { return m_DeathAnimFinished; }
		bool IsSpawnIntroActive() const { return m_SpawnIntroActive; }
		bool IsSpawnIntroCharacterVisible() const { return m_SpawnIntroCharacterVisible; }
	};

}

