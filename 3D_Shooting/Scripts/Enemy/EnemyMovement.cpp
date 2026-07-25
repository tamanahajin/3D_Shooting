#include "stdafx.h"
#include "Project.h"

namespace shooting {

	long long EnemyController::MakeCellKey(int x, int z) const
	{
		// 空間グリッドのセル座標をmapのキーにするため、X/Zを1つの64bit値へ詰める。
		// 負の座標もunsigned経由でビット列として扱うことで、ステージ中心をまたいでも同じ計算で管理できる。
		const unsigned long long ux = static_cast<unsigned int>(x);
		const unsigned long long uz = static_cast<unsigned int>(z);
		return static_cast<long long>((ux << 32) ^ uz);
	}

	void EnemyController::BuildSpatialGrid()
	{
		// 敵同士の分離計算で全組み合わせを見ると重くなるため、
		// 毎フレーム「近くにいる可能性がある敵」だけを引ける簡易グリッドを作る。
		m_cellMap.clear();
		if (m_cellSize <= 0.0f)
		{
			m_cellSize = 1.0f;
		}

		for (size_t i = 0; i < m_enemies.size(); ++i)
		{
			const auto& enemy = m_enemies[i];
			if (!enemy.active || enemy.lifeState == EnemyLifeState::Deading)
			{
				continue;
			}

			const int cellX = static_cast<int>(floorf(enemy.position.x / m_cellSize));
			const int cellZ = static_cast<int>(floorf(enemy.position.z / m_cellSize));
			m_cellMap[MakeCellKey(cellX, cellZ)].push_back(i);
		}
	}

	Vec3 EnemyController::CalculateSeparation(size_t index) const
	{
		if (index >= m_enemies.size())
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const Vec3 myPos = m_enemies[index].position;
		const int cellX = static_cast<int>(floorf(myPos.x / m_cellSize));
		const int cellZ = static_cast<int>(floorf(myPos.z / m_cellSize));
		const float rangeSq = m_separationRange * m_separationRange;
		Vec3 steeringForce(0.0f, 0.0f, 0.0f);

		// 自分がいるセルと周囲8セルだけを見れば、分離範囲内の敵はほぼ拾える。
		// 大量敵でも近傍だけを処理するため、敵数が増えても計算量が跳ねにくい。
		for (int z = -1; z <= 1; ++z)
		{
			for (int x = -1; x <= 1; ++x)
			{
				auto it = m_cellMap.find(MakeCellKey(cellX + x, cellZ + z));
				if (it == m_cellMap.end())
				{
					continue;
				}

				for (const auto otherIndex : it->second)
				{
					if (otherIndex == index || otherIndex >= m_enemies.size())
					{
						continue;
					}

					// 水平方向だけで押し合う。Yを含めると段差や落下中に不自然な浮き沈みが出やすい。
					Vec3 toAgent = myPos - m_enemies[otherIndex].position;
					toAgent.y = 0.0f;
					const float distSq = bsmUtil::lengthSqr(toAgent);
					if (distSq <= 1e-6f || distSq > rangeSq)
					{
						continue;
					}

					steeringForce += toAgent * (1.0f / distSq);
				}
			}
		}

		return steeringForce;
	}

	void EnemyController::RotateToVelocity(EnemyState& enemy, float lerpFact)
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

	bool EnemyController::ResolveGeneratedGround(EnemyState& enemy, double elapsedTime)
	{
		auto gameStage = m_gameStage.lock();
		if (!gameStage)
		{
			return false;
		}

		// StageGroundResolver側の共通構造へ詰め替え、敵バッチ側の状態と地形解決処理を分離する。
		StageGroundResolveState groundState;
		groundState.position = enemy.position;
		groundState.previousPosition = enemy.previousPosition;
		groundState.gravityVelocity = enemy.gravityVelocity;
		groundState.footOffset = enemy.status.groundFootOffset;
		groundState.wasGrounded = enemy.isGround;
		groundState.elapsedTime = static_cast<float>(elapsedTime);

		bool terrainBlockedX = false;
		bool terrainBlockedZ = false;
		if (TrySlideAgainstGeneratedTerrainStep(*gameStage, groundState, 0.75f, terrainBlockedX, terrainBlockedZ))
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

		if (enemy.knockbackLaunchTimer > 0.0 && groundState.gravityVelocity.y > 0.0f)
		{
			// 爆風直後は浅い地面への食い込みを接地として拾うと上昇速度が消えるため、離陸を優先する。
			enemy.position = groundState.position;
			enemy.gravityVelocity = groundState.gravityVelocity;
			enemy.isGround = false;
			return false;
		}

		// CSV地形で接地できない場合でも、中央の旧ベース床だけはフォールバックとして扱う。
		// これがないとCSV外の通常床上で敵が落下扱いになり続ける。
		if (!TryResolveStageGround(*gameStage, groundState))
		{
			const float baseFloorHalf = 32.5f;
			const bool insideBaseFloor = fabsf(groundState.position.x) <= baseFloorHalf &&
				fabsf(groundState.position.z) <= baseFloorHalf;
			if (!insideBaseFloor || !TryResolveGroundHeight(0.0f, groundState))
			{
				return false;
			}
		}

		enemy.position = groundState.position;
		enemy.gravityVelocity = groundState.gravityVelocity;
		enemy.isGround = groundState.isGrounded;
		return groundState.isGrounded;
	}

	bool EnemyController::IsKnockbackActive(const EnemyState& enemy) const
	{
		return enemy.lifeState == EnemyLifeState::PendingDeathAirborne ||
			enemy.lifeState == EnemyLifeState::PendingDeathLanding ||
			enemy.knockbackSpinSpeed != 0.0f ||
			enemy.knockbackLaunchTimer > 0.0 ||
			enemy.knockbackControlTimer > 0.0 ||
			(bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f && !enemy.isGround);
	}

	void EnemyController::OnUpdate(double elapsedTime)
	{
		ScopedBenchmarkTimer benchmarkTimer(BenchmarkSection::EnemyUpdate);

		if (m_enemies.empty())
		{
			return;
		}

		auto gameStage = m_gameStage.lock();
		if (!gameStage)
		{
			return;
		}

		// ヒットストップなどでゲーム側の実効デルタが変わるため、敵更新も同じ時間に合わせる。
		elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);

		// 敵の追跡先。プレイヤーが取れないフレームは原点を目標にして、NaNや未初期化値を避ける。
		Vec3 targetPosition(0.0f, 0.0f, 0.0f);
		auto player = gameStage->GetSharedGameObject(L"Player", false);
		if (player)
		{
			auto playerTransform = player->GetComponent<Transform>(false);
			if (playerTransform)
			{
				targetPosition = playerTransform->GetWorldPosition();
			}
		}

		// 分離力は全敵の現在位置を使うため、各敵更新に入る前にまとめて計算しておく。
		m_separationForces.assign(m_enemies.size(), Vec3(0.0f, 0.0f, 0.0f));
		BuildSpatialGrid();
		for (size_t i = 0; i < m_enemies.size(); ++i)
		{
			if (m_enemies[i].active && m_enemies[i].lifeState != EnemyLifeState::Deading)
			{
				m_separationForces[i] = CalculateSeparation(i);
			}
		}

		const float dt = static_cast<float>(elapsedTime);
		const Vec3 gravity(0.0f, -9.8f, 0.0f);
		for (size_t i = 0; i < m_enemies.size(); ++i)
		{
			auto& enemy = m_enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			enemy.previousPosition = enemy.position;

			if (enemy.spawnIntroTimer > 0.0)
			{
				enemy.spawnIntroTimer -= elapsedTime;
				if (enemy.spawnIntroTimer < 0.0)
				{
					enemy.spawnIntroTimer = 0.0;
				}
			}

			if (enemy.lifeState == EnemyLifeState::Spawning)
			{
				enemy.velocity = Vec3(0.0f, 0.0f, 0.0f);
				enemy.force = Vec3(0.0f, 0.0f, 0.0f);
				enemy.gravityVelocity = Vec3(0.0f, 0.0f, 0.0f);

				if (enemy.spawnIntroTimer <= 0.0)
				{
					enemy.lifeState = EnemyLifeState::Alive;
				}
				else
				{
					ChangeAnimation(enemy, AnimState::Idle);
					UpdateAnimation(enemy, elapsedTime);
					SyncProxyTransform(i);
					continue;
				}
			}

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

			if (enemy.knockbackLaunchTimer > 0.0)
			{
				enemy.knockbackLaunchTimer -= elapsedTime;
				if (enemy.knockbackLaunchTimer < 0.0)
				{
					enemy.knockbackLaunchTimer = 0.0;
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

			const bool knockbackActive = IsKnockbackActive(enemy);

			// 爆風回転は描画用クォータニオンだけを進める。
			if (enemy.knockbackSpinTimer > 0.0)
			{
				enemy.knockbackSpinTimer -= elapsedTime;
				if (enemy.knockbackSpinTimer < 0.0)
				{
					enemy.knockbackSpinTimer = 0.0;
				}

				Quat deltaRotation;
				deltaRotation.rotationAxis(enemy.knockbackSpinAxis, enemy.knockbackSpinSpeed * dt);

				enemy.knockbackSpinRotation = deltaRotation * enemy.knockbackSpinRotation;
				enemy.knockbackSpinRotation.normalize();
			}

			// 接地して操作不能時間も終わったら、爆風姿勢を解除して通常の移動方向回転へ戻す。
			// タイマー終了だけで戻すと空中で急に姿勢が戻るため、接地を待つ。
			if (enemy.isGround && enemy.knockbackControlTimer <= 0.0 && enemy.knockbackSpinSpeed != 0.0f)
			{
				enemy.knockbackSpinRotation.identity();
				enemy.knockbackSpinSpeed = 0.0f;
				enemy.knockbackSpinTimer = 0.0;
			}

			KillByFall(enemy);

			if (enemy.lifeState == EnemyLifeState::Deading)
			{
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
						seekForce = toTarget * (enemy.status.moveSpeed * m_moveSpeedMultiplier) - enemy.velocity;
					}

					Vec3 separation = m_separationForces[i];
					separation.y = 0.0f;
					enemy.force = Vec3(0.0f, 0.0f, 0.0f);
					// 追跡と分離を同じ上限内で積む。分離が強すぎて追跡不能になるのを避ける。
					Steering::AccumulateForce(enemy.force, seekForce, 20.0f);
					Steering::AccumulateForce(enemy.force, separation, 20.0f);
				}

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
					enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
				}
				else
				{
					enemy.position += enemy.knockbackVelocity * dt;
				}
			}

			enemy.gravityVelocity += gravity * dt;
			enemy.position.y += enemy.gravityVelocity.y * dt;
			enemy.gravityVelocity.x = 0.0f;
			enemy.gravityVelocity.z = 0.0f;

			// CSV地形や中央床に合わせて最終位置を補正する。失敗時は未接地として落下を継続する。
			const bool resolvedGeneratedGround = ResolveGeneratedGround(enemy, elapsedTime);

			if (enemy.lifeState == EnemyLifeState::PendingDeathAirborne &&
				!resolvedGeneratedGround)
			{
				enemy.lifeState = EnemyLifeState::PendingDeathLanding;
			}
			else if (enemy.lifeState == EnemyLifeState::PendingDeathLanding &&
				resolvedGeneratedGround &&
				enemy.delayedDeathMinTimer <= 0.0)
			{
				// 空中状態を経由した敵だけを着地時に死亡させ、爆風で飛ぶ演出を保証する。
				KillEnemy(enemy);
				UpdateAnimation(enemy, elapsedTime);
				SyncProxyTransform(i);
				continue;
			}

			UpdateAnimation(enemy, elapsedTime);
			RotateToVelocity(enemy, 0.35f);
			SyncProxyTransform(i);
			enemy.isGround = resolvedGeneratedGround;
		}
	}

	void EnemyController::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		// CollisionManager側で押し戻された結果を、次フレームの配列更新に反映する。
		// バッチ本体とプロキシのTransformがずれると、弾や床との当たり判定位置が古くなる。
		for (size_t i = 0; i < m_enemies.size(); ++i)
		{
			auto& enemy = m_enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			if (enemy.lifeState == EnemyLifeState::Spawning)
			{
				SyncProxyTransform(i);
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

