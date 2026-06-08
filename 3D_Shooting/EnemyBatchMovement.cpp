/*!
@file EnemyBatchMovement.cpp
@brief 敵バッチの移動、分離、地形追従
敵同士の簡易分離はグリッドで近傍だけを見て軽量化する。
このファイルでは追跡、重力、坂や高台の地形解決もまとめて行う。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	/*!
	@brief 空間グリッドのセル座標を m_CellMap 用のキーへ変換する
	@param x セルX座標
	@param z セルZ座標
	@return 2次元セルを表す64bitキー
	*/
	long long EnemyBatchController::MakeCellKey(int x, int z) const
	{
		// 空間グリッドのセル座標をmapのキーにするため、X/Zを1つの64bit値へ詰める。
		// 負の座標もunsigned経由でビット列として扱うことで、ステージ中心をまたいでも同じ計算で管理できる。
		const unsigned long long ux = static_cast<unsigned int>(x);
		const unsigned long long uz = static_cast<unsigned int>(z);
		return static_cast<long long>((ux << 32) ^ uz);
	}

	/*!
	@brief 敵同士の分離計算に使う空間グリッドを作る

	敵数が増えても全組み合わせ比較を避けるため、現在位置からセル座標を求めて
	m_CellMap に敵インデックスを登録する。
	*/
	void EnemyBatchController::BuildSpatialGrid()
	{
		// 敵同士の分離計算で全組み合わせを見ると重くなるため、
		// 毎フレーム「近くにいる可能性がある敵」だけを引ける簡易グリッドを作る。
		m_CellMap.clear();
		if (m_CellSize <= 0.0f)
		{
			m_CellSize = 1.0f;
		}

		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			const auto& enemy = m_Enemies[i];
			// 死亡済み・非アクティブの敵は移動にも分離にも参加させない。
			if (!enemy.active || enemy.isDead)
			{
				continue;
			}

			const int cellX = static_cast<int>(floorf(enemy.position.x / m_CellSize));
			const int cellZ = static_cast<int>(floorf(enemy.position.z / m_CellSize));
			m_CellMap[MakeCellKey(cellX, cellZ)].push_back(i);
		}
	}

	/*!
	@brief 指定敵に働く分離力を計算する
	@param index 対象敵のインデックス
	@return 近い敵から離れる水平ベクトル

	自分のセルと周囲8セルだけを調べ、近い敵ほど強く押し返す。
	*/
	Vec3 EnemyBatchController::CalculateSeparation(size_t index) const
	{
		if (index >= m_Enemies.size())
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const Vec3 myPos = m_Enemies[index].position;
		const int cellX = static_cast<int>(floorf(myPos.x / m_CellSize));
		const int cellZ = static_cast<int>(floorf(myPos.z / m_CellSize));
		const float rangeSq = m_SeparationRange * m_SeparationRange;
		Vec3 steeringForce(0.0f, 0.0f, 0.0f);

		// 自分がいるセルと周囲8セルだけを見れば、分離範囲内の敵はほぼ拾える。
		// 大量敵でも近傍だけを処理するため、敵数が増えても計算量が跳ねにくい。
		for (int z = -1; z <= 1; ++z)
		{
			for (int x = -1; x <= 1; ++x)
			{
				auto it = m_CellMap.find(MakeCellKey(cellX + x, cellZ + z));
				if (it == m_CellMap.end())
				{
					continue;
				}

				for (const auto otherIndex : it->second)
				{
					if (otherIndex == index || otherIndex >= m_Enemies.size())
					{
						continue;
					}

					// 水平方向だけで押し合う。Yを含めると段差や落下中に不自然な浮き沈みが出やすい。
					Vec3 toAgent = myPos - m_Enemies[otherIndex].position;
					toAgent.y = 0.0f;
					const float distSq = bsmUtil::lengthSqr(toAgent);
					if (distSq <= 1e-6f || distSq > rangeSq)
					{
						continue;
					}

					// 近い敵ほど強く離れるよう、距離の二乗に反比例する力を足す。
					steeringForce += toAgent * (1.0f / distSq);
				}
			}
		}

		return steeringForce;
	}

	/*!
	@brief 敵の向きを水平速度方向へなめらかに補間する
	@param enemy 更新対象の敵状態
	@param lerpFact 回転補間率。1.0以上なら即座に目標向きへ合わせる

	上下方向の速度は向きに使わず、見た目のY軸回転だけを調整する。
	*/
	void EnemyBatchController::RotateToVelocity(EnemyState& enemy, float lerpFact)
	{
		// 見た目の向きだけを移動方向へ寄せる。移動ベクトルがほぼゼロなら向きを維持する。
		if (lerpFact <= 0.0f || bsmUtil::lengthSqr(enemy.velocity) <= 1e-6f)
		{
			return;
		}

		Vec3 direction = enemy.velocity;
		direction.y = 0.0f;
		if (bsmUtil::lengthSqr(direction) <= 1e-6f)
		{
			return;
		}

		direction.normalize();
		// モデルはZ+を正面として扱うため、水平移動方向からY軸回転角を作る。
		const float angle = atan2(direction.x, direction.z);
		Quat target;
		target.rotationRollPitchYawFromVector(Vec3(0.0f, angle, 0.0f));
		target.normalize();

		if (lerpFact >= 1.0f)
		{
			enemy.rotation = target;
		}
		else
		{
			// 急に向きを変えると群れ全体がカクつくため、Slerpで少しずつ回す。
			enemy.rotation = XMQuaternionSlerp(enemy.rotation, target, lerpFact);
			enemy.rotation.normalize();
		}
	}

	/*!
	@brief CSV地形と中央床に対して敵の接地位置を解決する
	@param enemy 更新対象の敵状態
	@param elapsedTime 経過時間
	@return 接地できた場合は true

	敵バッチ側の状態を StageGroundResolveState へ詰め替え、
	StageGroundResolver の共通処理で段差・坂・床高さを解決する。
	*/
	bool EnemyBatchController::ResolveGeneratedGround(EnemyState& enemy, double elapsedTime)
	{
		// StageGroundResolver側の共通構造へ詰め替え、敵バッチ側の状態と地形解決処理を分離する。
		StageGroundResolveState groundState;
		groundState.position = enemy.position;
		groundState.previousPosition = enemy.previousPosition;
		groundState.gravityVelocity = enemy.gravityVelocity;
		groundState.footOffset = 0.35f;
		groundState.wasGrounded = enemy.isGround;
		groundState.elapsedTime = static_cast<float>(elapsedTime);

		bool terrainBlockedX = false;
		bool terrainBlockedZ = false;
		if (TrySlideAgainstGeneratedTerrainStep(*m_GameStage, groundState, 0.75f, terrainBlockedX, terrainBlockedZ))
		{
			// 段差に引っかかった軸だけ速度を消す。両軸を止めると壁沿いに滑れなくなる。
			if (terrainBlockedX)
			{
				enemy.velocity.x = 0.0f;
				enemy.force.x = 0.0f;
				enemy.knockbackVelocity.x = 0.0f;
			}
			if (terrainBlockedZ)
			{
				enemy.velocity.z = 0.0f;
				enemy.force.z = 0.0f;
				enemy.knockbackVelocity.z = 0.0f;
			}
		}

		// CSV地形で接地できない場合でも、中央の旧ベース床だけはフォールバックとして扱う。
		// これがないとCSV外の通常床上で敵が落下扱いになり続ける。
		if (!TryResolveStageGround(*m_GameStage, groundState))
		{
			const float baseFloorHalf = 32.5f;
			const bool insideBaseFloor = fabsf(groundState.position.x) <= baseFloorHalf &&
				fabsf(groundState.position.z) <= baseFloorHalf;
			if (!insideBaseFloor || !TryResolveGroundHeight(0.0f, groundState))
			{
				return false;
			}
		}

		// 解決に成功した結果だけを敵状態へ戻す。失敗時は呼び出し側で未接地として扱う。
		enemy.position = groundState.position;
		enemy.gravityVelocity = groundState.gravityVelocity;
		enemy.isGround = groundState.isGrounded;
		return groundState.isGrounded;
	}

	/*!
	@brief 敵配列の移動、重力、地形追従、死亡状態を更新する
	@param elapsedTime 経過時間

	1フレーム内で追跡目標取得、分離力計算、通常移動またはノックバック、
	地形解決、アニメーション、プロキシ同期の順に処理する。
	*/
	void EnemyBatchController::OnUpdate(double elapsedTime)
	{
		// この関数が敵バッチの主更新。処理順は「目標取得 → 分離力計算 → 各敵の移動/接地/同期」。
		ScopedBenchmarkTimer benchmarkTimer(BenchmarkSection::EnemyUpdate);

		if (m_Enemies.empty())
		{
			return;
		}

		if (m_GameStage)
		{
			// ヒットストップなどでゲーム側の実効デルタが変わるため、敵更新も同じ時間に合わせる。
			elapsedTime = m_GameStage->GetGameDeltaTime(elapsedTime);
		}

		// 敵の追跡先。プレイヤーが取れないフレームは原点を目標にして、NaNや未初期化値を避ける。
		Vec3 targetPosition(0.0f, 0.0f, 0.0f);
		auto player = m_GameStage->GetSharedGameObject(L"Player", false);
		if (player)
		{
			auto playerTransform = player->GetComponent<Transform>(false);
			if (playerTransform)
			{
				targetPosition = playerTransform->GetWorldPosition();
			}
		}

		// 分離力は全敵の現在位置を使うため、各敵更新に入る前にまとめて計算しておく。
		m_SeparationForces.assign(m_Enemies.size(), Vec3(0.0f, 0.0f, 0.0f));
		BuildSpatialGrid();
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			if (m_Enemies[i].active && !m_Enemies[i].isDead)
			{
				m_SeparationForces[i] = CalculateSeparation(i);
			}
		}

		const float dt = static_cast<float>(elapsedTime);
		const Vec3 gravity(0.0f, -9.8f, 0.0f);
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			// ここから先は1体分の状態更新。GameObjectを増やさず配列上の状態だけを進める。
			auto& enemy = m_Enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			enemy.previousPosition = enemy.position;

			// 描画演出用タイマー。実座標には影響せず、インスタンス描画時のフラッシュや押されに使う。
			if (enemy.damageFlashTimer > 0.0)
			{
				enemy.damageFlashTimer -= elapsedTime;
				if (enemy.damageFlashTimer < 0.0)
				{
					enemy.damageFlashTimer = 0.0;
				}
			}

			if (enemy.hitPushTimer > 0.0)
			{
				enemy.hitPushTimer -= elapsedTime;
				if (enemy.hitPushTimer < 0.0)
				{
					enemy.hitPushTimer = 0.0;
				}
			}

			// 爆風の操作不能時間。残っている間は通常の追跡AIを止め、吹っ飛びを優先する。
			if (enemy.knockbackControlTimer > 0.0)
			{
				enemy.knockbackControlTimer -= elapsedTime;
				if (enemy.knockbackControlTimer < 0.0)
				{
					enemy.knockbackControlTimer = 0.0;
				}
			}

			// 爆弾でHPが0になった敵は、最低時間だけ飛ばしてから接地死亡にする。
			if (enemy.delayedDeathMinTimer > 0.0)
			{
				enemy.delayedDeathMinTimer -= elapsedTime;
				if (enemy.delayedDeathMinTimer < 0.0)
				{
					enemy.delayedDeathMinTimer = 0.0;
				}
			}

			const bool knockbackActive = enemy.knockbackControlTimer > 0.0
				|| (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f && !enemy.isGround);

			// ステージ外へ落ちた敵は、地形解決より先に死亡扱いへ切り替える。
			KillByFall(enemy);

			if (enemy.isDead)
			{
				// 死亡後も落下や残ったノックバックは少し進め、アニメーション終了後にプロキシを外す。
				ChangeAnimation(enemy, AnimState::Dead);
				enemy.gravityVelocity += gravity * dt;
				enemy.position.y += enemy.gravityVelocity.y * dt;
				if (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f)
				{
					enemy.position += enemy.knockbackVelocity * dt;
				}
				UpdateAnimation(enemy, elapsedTime);
				SyncProxyTransform(i);

				if (enemy.animationFinished)
				{
					RemoveEnemyProxy(i);
				}
				continue;
			}

			if (!enemy.isGround || knockbackActive)
			{
				ChangeAnimation(enemy, AnimState::Fall);
			}
			else
			{
				ChangeAnimation(enemy, AnimState::Sprint);
			}

			if (!knockbackActive)
			{
				// 追跡力は毎フレームではなく一定間隔で更新する。
				// 敵数が多い時の負荷を抑えつつ、速度積分自体は毎フレーム行う。
				enemy.steeringTimer -= elapsedTime;
				if (enemy.steeringTimer <= 0.0)
				{
					enemy.steeringTimer += enemy.steeringInterval;
					if (enemy.steeringTimer < 0.0)
					{
						enemy.steeringTimer = 0.0;
					}

					Vec3 toTarget = targetPosition - enemy.position;
					toTarget.y = 0.0f;
					Vec3 seekForce(0.0f, 0.0f, 0.0f);
					if (bsmUtil::lengthSqr(toTarget) > 1e-6f)
					{
						// 目標速度との差分を力として使う簡易Seek。現在速度を引くので急加速しすぎない。
						toTarget.normalize();
						seekForce = toTarget * (enemy.status.moveSpeed * m_MoveSpeedMultiplier) - enemy.velocity;
					}

					Vec3 separation = m_SeparationForces[i];
					separation.y = 0.0f;
					enemy.force = Vec3(0.0f, 0.0f, 0.0f);
					// 追跡と分離を同じ上限内で積む。分離が強すぎて追跡不能になるのを避ける。
					Steering::AccumulateForce(enemy.force, seekForce, 20.0f);
					Steering::AccumulateForce(enemy.force, separation, 20.0f);
				}

				// 通常移動は水平面だけで積分する。上下方向は重力と地形解決に任せる。
				enemy.velocity += enemy.force * dt;
				enemy.velocity.y = 0.0f;
				enemy.position += enemy.velocity * dt;
			}
			else
			{
				// 吹っ飛び中はAIの力を切り、通常速度を減衰させて爆風速度を目立たせる。
				enemy.force = Vec3(0.0f, 0.0f, 0.0f);
				enemy.velocity *= 1.0f / (1.0f + 10.0f * dt);
			}

			if (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f)
			{
				if (enemy.isGround && enemy.knockbackControlTimer <= 0.0)
				{
					// 接地して操作不能時間も終わったら、水平ノックバックを止めて通常追跡へ戻す。
					enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
				}
				else
				{
					// 空中または操作不能中は、爆風で与えた水平速度をそのまま位置へ反映する。
					enemy.position += enemy.knockbackVelocity * dt;
				}
			}

			// 上下方向は常に重力で更新し、接地判定で必要なら地面高さへ戻す。
			enemy.gravityVelocity += gravity * dt;
			enemy.position.y += enemy.gravityVelocity.y * dt;
			enemy.gravityVelocity.x = 0.0f;
			enemy.gravityVelocity.z = 0.0f;

			// CSV地形や中央床に合わせて最終位置を補正する。失敗時は未接地として落下を継続する。
			const bool resolvedGeneratedGround = m_GameStage
				? ResolveGeneratedGround(enemy, elapsedTime)
				: false;

			if (enemy.delayDeathUntilLanding)
			{
				if (!resolvedGeneratedGround)
				{
					// 一度でも空中になったことを記録し、地面上で即死亡しないようにする。
					enemy.delayedDeathWasAirborne = true;
				}
				else if (enemy.delayedDeathWasAirborne && enemy.delayedDeathMinTimer <= 0.0)
				{
					// 爆弾で致死ダメージを受けた敵は、吹っ飛びが見えるよう着地してから死亡させる。
					KillEnemy(enemy);
					UpdateAnimation(enemy, elapsedTime);
					SyncProxyTransform(i);
					continue;
				}
			}

			UpdateAnimation(enemy, elapsedTime);
			RotateToVelocity(enemy, 0.35f);
			// CollisionManagerが見るプロキシTransformへ、配列側で決めた最終位置を同期する。
			SyncProxyTransform(i);
			enemy.isGround = resolvedGeneratedGround;
		}
	}

	/*!
	@brief プロキシ側で補正された Transform を敵配列へ戻す
	@param elapsedTime 経過時間。この処理では使用しない

	CollisionManager が押し戻した位置を次フレームのバッチ更新に反映し、
	配列状態と当たり判定位置のずれを防ぐ。
	*/
	void EnemyBatchController::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		// CollisionManager側で押し戻された結果を、次フレームの配列更新に反映する。
		// バッチ本体とプロキシのTransformがずれると、弾や床との当たり判定位置が古くなる。
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			auto& enemy = m_Enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			auto proxy = enemy.proxy.lock();
			if (!proxy)
			{
				continue;
			}

			auto transform = proxy->GetComponent<Transform>(false);
			if (!transform)
			{
				continue;
			}

			enemy.position = transform->GetPosition();
			enemy.rotation = transform->GetQuaternion();
		}
	}
}

