/*!
@file CollisionManager.h
@brief 衝突判定マネージャクラス
*/


#include "stdafx.h"
#include "Common/Library/BasicLib/BenchmarkRecorder.h"

namespace shooting {

	/// <summary>
	/// 空間分割による衝突判定の最適化のために使用される、4分木構造のノードを表す構造体
	/// </summary>
	struct CollisionPiece {
		CollisionPiece* m_Children[4];
		AABB m_AABB;
		std::vector<std::shared_ptr<GameObject>> m_ObjVec;
		CollisionPiece() :
			m_AABB()
		{
			for (int i = 0; i < 4; i++)
			{
				m_Children[i] = nullptr;
			}
			m_ObjVec.clear();
		}
		void SetAABB(const AABB& aabb)
		{
			m_AABB = aabb;
		}
		void Clear()
		{
			for (int i = 0; i < 4; i++)
			{
				if (m_Children[i])
				{
					m_Children[i]->Clear();
				}
				m_Children[i] = nullptr;
			}
			m_ObjVec.clear();
			m_AABB = AABB();
		}
	};

#define MAX_PIECE_COUNT 8192

	struct CollisionBlocks {
		UINT m_CollisionCountOfTern;
		CollisionPiece m_RootPiece;
		AABB m_RootAABB;
		CollisionPiece m_PiecePool[MAX_PIECE_COUNT];
		UINT m_NextPoolIndex;
		CollisionBlocks() :
			m_CollisionCountOfTern(0),
			m_NextPoolIndex(0)
		{
			AABB aabb(Vec3(-100.0f, -1000, -100.0f), Vec3(100.0f, 1000, 100.0f));
			m_RootAABB = aabb;
			m_RootPiece.SetAABB(m_RootAABB);
		}
		void SetRootAABB(const AABB& aabb)
		{
			m_RootAABB = aabb;
		}
		void AllClear()
		{
			m_RootPiece.Clear();
			m_RootPiece.SetAABB(m_RootAABB);
			m_NextPoolIndex = 0;
		}

		void SetCollisionBlockSub(CollisionPiece& tgt, const std::shared_ptr<GameObject>& Obj)
		{
			auto Col = Obj->GetComponent<Collision>();
			auto colAABB = Col->GetWrappedAABB();
			if (HitTest::AABB_AABB(tgt.m_AABB, colAABB))
			{
				if (tgt.m_Children[0])
				{
					//子供ピースがあったら
					for (int i = 0; i < 4; i++)
					{
						SetCollisionBlockSub(*tgt.m_Children[i], Obj);
					}
				}
				else
				{
					//子供ピースがなかった
					//AABBが小さい場合はこれ以上増やさない
					if (tgt.m_AABB.GetWidth() < 0.125f)
					{
						tgt.m_ObjVec.push_back(Obj);
					}
					else
					{
						if (tgt.m_ObjVec.size() < 5)
						{
							//AABBの幅が定数未満か
							//あるいはまだ余裕がある
							tgt.m_ObjVec.push_back(Obj);
						}
						else
						{
							//余裕がない子供ブロックの作成
							for (int i = 0; i < 4; i++)
							{
								tgt.m_Children[i] = &m_PiecePool[m_NextPoolIndex];
								tgt.m_Children[i]->Clear();
								m_NextPoolIndex++;
								if (m_NextPoolIndex >= MAX_PIECE_COUNT)
								{
									throw BaseException(
										L"これ以上衝突判定は行えません。",
										L"if (m_NextPoolIndex >= MAX_PIECE_COUNT)",
										L"CollisionBlocks::SetCollisionBlock2Sub()"
									);
								}
								AABB childAABB = tgt.m_AABB;
								switch (i)
								{
								case 0:
									childAABB.m_Min.z = tgt.m_AABB.GetCenter().z;
									childAABB.m_Max.x = tgt.m_AABB.GetCenter().x;
									break;
								case 1:
									childAABB.m_Min.x = tgt.m_AABB.GetCenter().x;
									childAABB.m_Min.z = tgt.m_AABB.GetCenter().z;
									break;
								case 2:
									childAABB.m_Max.x = tgt.m_AABB.GetCenter().x;
									childAABB.m_Max.z = tgt.m_AABB.GetCenter().z;
									break;
								case 3:
									childAABB.m_Min.x = tgt.m_AABB.GetCenter().x;
									childAABB.m_Max.z = tgt.m_AABB.GetCenter().z;
									break;
								}
								tgt.m_Children[i]->SetAABB(childAABB);
							}
							//子供作成終了
							//オブジェクトを子供に振り分ける
							for (auto& v : tgt.m_ObjVec)
							{
								auto objCol = v->GetComponent<Collision>();
								auto objColAABB = objCol->GetWrappedAABB();
								for (int i = 0; i < 4; i++)
								{
									if (HitTest::AABB_AABB(tgt.m_Children[i]->m_AABB, objColAABB))
									{
										tgt.m_Children[i]->m_ObjVec.push_back(v);
									}
								}
							}
							//振り分けが終わったので配列はクリア
							tgt.m_ObjVec.clear();
							//子供ピースができたので、そこに調査
							for (int i = 0; i < 4; i++)
							{
								SetCollisionBlockSub(*tgt.m_Children[i], Obj);
							}
						}
					}
				}
			}
		}

		void SetCollisionBlock(const std::shared_ptr<GameObject>& Obj)
		{
			if (Obj->IsUpdateActive())
			{
				auto Col = Obj->GetComponent<Collision>(false);
				if (Col)
				{
					if (!Col->IsUpdateActive())
					{
						return;
					}
					SetCollisionBlockSub(m_RootPiece, Obj);
				}
			}
		}

		void SetNewCollisionSubSub(const std::shared_ptr<GameObject>& Src, CollisionPiece& piece, const std::shared_ptr<CollisionManager>& manager)
		{
			for (auto& v : piece.m_ObjVec)
			{
				if (manager->EnableedCollisionPair(Src, v))
				{
					auto SrcColl = Src->GetComponent<Collision>();
					auto DestColl = v->GetComponent<Collision>();
					if (!manager->IsInPair(SrcColl, DestColl, true) && !manager->IsInPair(SrcColl, DestColl, false))
					{
						//キープされている中になかったら
						//Collisionによる衝突判定
						m_CollisionCountOfTern++;
						DestColl->CollisionCall(SrcColl);
					}
				}
			}
		}


		void SetNewCollisionSub(CollisionPiece& tgt, const std::shared_ptr<CollisionManager>& manager)
		{
			if (tgt.m_ObjVec.size() > 1)
			{
				for (auto& v : tgt.m_ObjVec)
				{
					SetNewCollisionSubSub(v, tgt, manager);
				}
			}
			else
			{
				if (tgt.m_Children[0])
				{
					for (int i = 0; i < 4; i++)
					{
						SetNewCollisionSub(*tgt.m_Children[i], manager);
					}
				}
			}
		}


		void SetNewCollisionAgainstSubSub(const std::shared_ptr<GameObject>& Src, CollisionPiece& piece, const std::shared_ptr<CollisionManager>& manager)
		{
			for (auto& v : piece.m_ObjVec)
			{
				if (manager->EnableedCollisionPair(Src, v))
				{
					auto SrcColl = Src->GetComponent<Collision>();
					auto DestColl = v->GetComponent<Collision>();
					if (!manager->IsInPair(SrcColl, DestColl, true) && !manager->IsInPair(SrcColl, DestColl, false))
					{
						m_CollisionCountOfTern++;
						DestColl->CollisionCall(SrcColl);
					}
				}
			}
		}

		void SetNewCollisionAgainstSub(const std::shared_ptr<GameObject>& Src, const AABB& srcAABB, CollisionPiece& tgt, const std::shared_ptr<CollisionManager>& manager)
		{
			if (!HitTest::AABB_AABB(tgt.m_AABB, srcAABB))
			{
				return;
			}

			if (tgt.m_Children[0])
			{
				for (int i = 0; i < 4; i++)
				{
					SetNewCollisionAgainstSub(Src, srcAABB, *tgt.m_Children[i], manager);
				}
				return;
			}

			SetNewCollisionAgainstSubSub(Src, tgt, manager);
		}

		void SetNewCollisionAgainst(const std::vector<std::shared_ptr<GameObject>>& srcObjects, const std::shared_ptr<CollisionManager>& manager)
		{
			m_CollisionCountOfTern = 0;
			for (auto& src : srcObjects)
			{
				auto srcColl = src->GetComponent<Collision>(false);
				if (!srcColl)
				{
					continue;
				}
				// 高速移動やジャンプで固定物をまたぐケースを拾うため、移動前後を含むAABBで問い合わせる。
				SetNewCollisionAgainstSub(src, srcColl->GetEnclosingAABB(), m_RootPiece, manager);
			}
		}

		void SetNewCollision(const std::shared_ptr<CollisionManager>& manager)
		{
			m_CollisionCountOfTern = 0;
			SetNewCollisionSub(m_RootPiece, manager);
		}

	};

	//--------------------------------------------------------------------------------------
	//	struct CollisionManager::Impl;
	//	用途: Implイディオム
	//--------------------------------------------------------------------------------------
	struct CollisionManager::Impl {
		//衝突判定マネージャの内部処理用パフォーマンス
		PerformanceCounter m_MiscPerformance;
		//毎フレーム更新する動的コリジョン用ブロック
		CollisionBlocks m_CollisionBlocks;
		//ステージ固定物だけを登録して再利用する静的コリジョン用ブロック
		CollisionBlocks m_StaticCollisionBlocks;
		std::vector<const GameObject*> m_StaticCollisionObjectKeys;
		bool m_StaticCollisionBlocksDirty;
		Impl() :
			m_StaticCollisionBlocksDirty(true)
		{
		}
		~Impl() {}
	};


	//--------------------------------------------------------------------------------------
	//	衝突判定管理者
	//--------------------------------------------------------------------------------------
	CollisionManager::CollisionManager(const std::shared_ptr<Stage>& StagePtr) :
		GameObject(StagePtr),
		m_NewIndex(0),
		m_KeepIndex(1),
		m_PriorityUnderEscapeY(false),
		m_EscapeFloor(3),
		pImpl(new Impl())
	{
		m_CollisionPairVec[0].resize(1024);
		m_CollisionPairVec[0].clear();
		m_CollisionPairVec[1].resize(1024);
		m_CollisionPairVec[1].clear();
		m_TempKeepVec.resize(1024);
		m_TempKeepVec.clear();
		m_TempExitVec.resize(1024);
		m_TempExitVec.clear();
	}
	CollisionManager::~CollisionManager() {}

	void CollisionManager::OnDestroy()
	{
		for (auto& pairs : m_CollisionPairVec)
		{
			pairs.clear();
			pairs.shrink_to_fit();
		}
		m_TempKeepVec.clear();
		m_TempKeepVec.shrink_to_fit();
		m_TempExitVec.clear();
		m_TempExitVec.shrink_to_fit();

		if (pImpl)
		{
			// 各ノードのm_ObjVecはshared_ptrを持つため、Manager破棄前に明示的に切り離す。
			pImpl->m_CollisionBlocks.AllClear();
			pImpl->m_StaticCollisionBlocks.AllClear();
			pImpl->m_StaticCollisionObjectKeys.clear();
			pImpl->m_StaticCollisionObjectKeys.shrink_to_fit();
			pImpl->m_StaticCollisionBlocksDirty = true;
		}
	}

	void CollisionManager::SetRootAABB(const AABB& aabb)
	{
		pImpl->m_CollisionBlocks.SetRootAABB(aabb);
		pImpl->m_StaticCollisionBlocks.SetRootAABB(aabb);
		pImpl->m_StaticCollisionBlocksDirty = true;
	}

	void CollisionManager::SetRootXZ(float f)
	{
		float h = f / 2.0f;
		AABB aabb(Vec3(-h, -1000, -h), Vec3(h, 1000, h));
		pImpl->m_CollisionBlocks.SetRootAABB(aabb);
		pImpl->m_StaticCollisionBlocks.SetRootAABB(aabb);
		pImpl->m_StaticCollisionBlocksDirty = true;

	}



	bool CollisionManager::SimpleCollisionPair(CollisionPair& Pair)
	{
		auto Src = Pair.m_Src.lock();
		auto Dest = Pair.m_Dest.lock();
		if (Src && Dest)
		{
			if (Src->IsSleep())
			{
				return false;
			}
			if (!Src->GetGameObject()->IsUpdateActive() || !Dest->GetGameObject()->IsUpdateActive())
			{
				return false;
			}
			if (!Src->IsUpdateActive() || !Dest->IsUpdateActive())
			{
				return false;
			}
			if (Src->IsFixed())
			{
				return false;
			}
			if (Src->IsExcludeCollisionObject(Dest->GetGameObject()) || Dest->IsExcludeCollisionObject(Src->GetGameObject()))
			{
				return false;
			}
			Vec3 SrcCenter = Src->GetCenterPosition();
			Vec3 DestCenter = Dest->GetCenterPosition();
			Vec3 DestMoveVec = DestCenter - Pair.m_DestCalcHitCenter;
			Vec3 SrcLocalVec = SrcCenter - Pair.m_SrcCalcHitCenter - DestMoveVec;
			float SrcV = bsmUtil::dot(SrcLocalVec, Pair.m_SrcHitNormal);
			if (SrcV >= 0.0f)
			{
				return false;
			}
			return Dest->SimpleCollisionCall(Src);
		}
		return false;
	}

	bool CollisionManager::EnableedCollisionPair(const std::shared_ptr<GameObject>& Src, const std::shared_ptr<GameObject>& Dest)
	{
		if (Src == Dest)
		{
			return false;
		}
		if (!Src->IsUpdateActive() || !Dest->IsUpdateActive())
		{
			return false;
		}
		auto SrcColl = Src->GetComponent<Collision>(false);
		auto DestColl = Dest->GetComponent<Collision>(false);
		if (!SrcColl || !DestColl)
		{
			return false;
		}
		if (!SrcColl->IsUpdateActive() || !DestColl->IsUpdateActive())
		{
			return false;
		}
		if (SrcColl->IsFixed())
		{
			return false;
		}
		if (SrcColl->IsSleep())
		{
			return false;
		}
		if (SrcColl->IsExcludeCollisionObject(Dest) || DestColl->IsExcludeCollisionObject(Src))
		{
			return false;
		}
		const AABB srcAABB = SrcColl->IsFixed() ? SrcColl->GetWrappedAABB() : SrcColl->GetEnclosingAABB();
		const AABB destAABB = DestColl->IsFixed() ? DestColl->GetWrappedAABB() : DestColl->GetEnclosingAABB();
		if (!HitTest::AABB_AABB(srcAABB, destAABB))
		{
			return false;
		}
		return true;
	}

	void CollisionManager::SetNewCollision()
	{
		pImpl->m_MiscPerformance.Start();
		auto& ObjVec = GetStage()->GetGameObjectVec();

		std::vector<const GameObject*> staticKeys;
		staticKeys.reserve(pImpl->m_StaticCollisionObjectKeys.size());
		for (auto& v : ObjVec)
		{
			if (!v->IsUpdateActive()) continue;

			auto col = v->GetComponent<Collision>(false);
			if (!col) continue;
			if (!col->IsUpdateActive()) continue;
			if (!col->IsFixed()) continue;
			if (!v->FindTag(L"StageObjectCollision") && !v->FindTag(L"Floor") && !v->FindTag(L"FixedBox")) continue;

			staticKeys.push_back(v.get());
		}

		if (pImpl->m_StaticCollisionBlocksDirty || staticKeys != pImpl->m_StaticCollisionObjectKeys)
		{
			pImpl->m_StaticCollisionBlocks.AllClear();
			pImpl->m_StaticCollisionObjectKeys = staticKeys;

			for (auto& v : ObjVec)
			{
				if (!v->IsUpdateActive()) continue;

				auto col = v->GetComponent<Collision>(false);
				if (!col) continue;
				if (!col->IsUpdateActive()) continue;
				if (!col->IsFixed()) continue;
				if (!v->FindTag(L"StageObjectCollision") && !v->FindTag(L"Floor") && !v->FindTag(L"FixedBox")) continue;

				pImpl->m_StaticCollisionBlocks.SetCollisionBlock(v);
			}

			pImpl->m_StaticCollisionBlocksDirty = false;
		}

		// コリジョンブロックのクリア
		pImpl->m_CollisionBlocks.AllClear();

		std::vector<std::shared_ptr<GameObject>> dynamicSources;
		dynamicSources.reserve(ObjVec.size());

		// 不要なオブジェクトを早期スキップ
		for (auto& v : ObjVec)
		{
			if (!v->IsUpdateActive()) continue;

			auto col = v->GetComponent<Collision>(false);
			if (!col) continue;
			if (!col->IsUpdateActive()) continue;
			if (col->IsSleep()) continue;  // スリープ中はスキップ

			const bool isStaticStageCollision =
				col->IsFixed() &&
				(v->FindTag(L"StageObjectCollision") || v->FindTag(L"Floor") || v->FindTag(L"FixedBox"));
			if (isStaticStageCollision)
			{
				continue;
			}

			pImpl->m_CollisionBlocks.SetCollisionBlock(v);
			const bool needsStageObjectCollision = v->FindTag(L"UseStageObjectCollision");
			if (!col->IsFixed() && (!v->FindTag(L"NoStaticStageCollision") || needsStageObjectCollision))
			{
				dynamicSources.push_back(v);
			}
		}

		// 動的同士・動的対アイテムなど、毎フレーム変わるものは従来通り判定する。
		pImpl->m_CollisionBlocks.SetNewCollision(GetThis<CollisionManager>());
		// ステージ固定物は再利用済みの静的ブロックに対して、動く側だけを問い合わせる。
		pImpl->m_StaticCollisionBlocks.SetNewCollisionAgainst(dynamicSources, GetThis<CollisionManager>());
		pImpl->m_CollisionBlocks.m_CollisionCountOfTern += pImpl->m_StaticCollisionBlocks.m_CollisionCountOfTern;
		pImpl->m_MiscPerformance.End();
	}

	void CollisionManager::InsertNewPair(const CollisionPair& NewPair)
	{
		m_CollisionPairVec[m_NewIndex].push_back(NewPair);
	}

	void CollisionManager::EscapeCollisionPair(CollisionPair& Pair)
	{
		auto ShSrc = Pair.m_Src.lock();
		auto ShDest = Pair.m_Dest.lock();
		if (ShSrc->GetAfterCollision() == AfterCollision::None || ShDest->GetAfterCollision() == AfterCollision::None)
		{
			return;
		}
		Vec3 SrcCenter = ShSrc->GetCenterPosition();
		Vec3 DestCenter = ShDest->GetCenterPosition();
		Vec3 DestMoveVec = DestCenter - Pair.m_DestCalcHitCenter;
		Vec3 SrcLocalVec = SrcCenter - Pair.m_SrcCalcHitCenter - DestMoveVec;
		float SrcV = bsmUtil::dot(SrcLocalVec, Pair.m_SrcHitNormal);
		if (SrcV < 0.0f)
		{
			//まだ衝突していたら
			float EscapeLen = abs(SrcV);
			if (!ShDest->IsFixed())
			{
				EscapeLen *= 0.5f;
			}
			//Srcのエスケープ
			SrcCenter += Pair.m_SrcHitNormal * EscapeLen;
			if (!ShDest->IsFixed())
			{
				//Destのエスケープ
				DestCenter += -Pair.m_SrcHitNormal * EscapeLen;
			}
			SrcCenter.floor(GetEscapeFloor());
			auto PtrSrcTransform = ShSrc->GetGameObject()->GetComponent<Transform>();
			//Srcのエスケープ
			PtrSrcTransform->SetWorldPosition(SrcCenter);
			if (!ShDest->IsFixed())
			{
				DestCenter.floor(GetEscapeFloor());
				ShDest->WakeUp();
				auto PtrDestTransform = ShDest->GetGameObject()->GetComponent<Transform>();
				//Destのエスケープ
				PtrDestTransform->SetWorldPosition(DestCenter);
			}
		}
	}

	void CollisionManager::SleepCheckSet()
	{
		auto& ObjVec = GetStage()->GetGameObjectVec();
		for (auto& v : ObjVec)
		{
			auto ptrColl = v->GetComponent<Collision>(false);
			if (ptrColl)
			{
				ptrColl->SleepCheckSet();
			}
		}
	}

	bool CollisionManager::Raycast(
		const Vec3& origin,
		const Vec3& dir,
		float maxDist,
		RaycastHit& outHit,
		const std::shared_ptr<GameObject>& ignoreObj,
		std::initializer_list<std::wstring> ignoreTags
	)
	{
		BenchmarkRecorder::Instance().IncrementRaycastCount();

		if (maxDist <= 0.0f) return false;

		Vec3 nDir = dir;
		nDir.normalize();

		// レイを「超小さい球のスイープ」として扱う（ゼロ半径だと数値的に不安定な実装もあるので微小値）
		const float kRayRadius = 0.001f;

		// elapsedTime=1.0 として、velocity を「この1秒で maxDist 進む速度」にする
		const float kElapsed = 1.0f;
		const Vec3  rayVel = nDir * maxDist;

		SPHERE rayStart(origin, kRayRadius);

		bool  hitAny = false;
		float bestDist = maxDist + 1.0f;

		auto& objVec = GetStage()->GetGameObjectVec();

		for (auto& obj : objVec)
		{
			if (!obj) continue;
			if (!obj->IsUpdateActive()) continue;
			if (ignoreObj && obj == ignoreObj) continue;
			if (ignoreTags.size() != 0)
			{
				bool ignored = false;
				for (const auto& t : ignoreTags)
				{
					if (obj->FindTag(t)) { ignored = true; break; }
				}
				if (ignored) continue;
			}

			auto col = obj->GetComponent<Collision>(false);
			if (!col) continue;
			if (!col->IsUpdateActive()) continue;

			float hitTime = 0.0f;

			// --- Sphere ---
			if (auto sp = std::dynamic_pointer_cast<CollisionSphere>(col))
			{
				SPHERE target = sp->GetSphere();
				if (HitTest::CollisionTestSphereSphere(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					Vec3 n = p - target.m_Center;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					Vec3 hitPoint = target.m_Center + n * target.m_Radius;
					//float dist = bsmUtil::length(hitPoint - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = hitPoint;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			// --- Capsule ---
			if (auto cap = std::dynamic_pointer_cast<CollisionCapsule>(col))
			{
				CAPSULE target = cap->GetCapsule();
				if (HitTest::CollisionTestSphereCapsule(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					// 既存の最近接点計算を流用（点pの最近接点retが「当たり点」扱い）
					SPHERE chk(p, kRayRadius);
					Vec3 ret;
					HitTest::SPHERE_CAPSULE(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			// --- OBB ---
			if (auto obb = std::dynamic_pointer_cast<CollisionObb>(col))
			{
				OBB target = obb->GetObb();
				if (HitTest::CollisionTestSphereObb(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					SPHERE chk(p, kRayRadius);
					Vec3 ret;
					HitTest::SPHERE_OBB(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			// --- Rect(Plane) ---
			if (auto rc = std::dynamic_pointer_cast<CollisionRect>(col))
			{
				COLRECT target = rc->GetColRect();
				if (HitTest::CollisionTestSphereRect(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					SPHERE chk(p, kRayRadius);
					Vec3 ret;
					HitTest::SPHERE_COLRECT(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}
		}

		return hitAny;
	}

	bool CollisionManager::SphereCast(
		const Vec3& origin,
		const Vec3& dir,
		float maxDist,
		float radius,
		RaycastHit& outHit,
		const std::shared_ptr<GameObject>& ignoreObj,
		std::initializer_list<std::wstring> ignoreTags
	)
	{
		BenchmarkRecorder::Instance().IncrementRaycastCount();

		if (maxDist <= 0.0f) return false;

		Vec3 nDir = dir;
		nDir.normalize();

		const float kRadius = (radius > 0.0f) ? radius : 0.001f;

		const float kElapsed = 1.0f;
		const Vec3  rayVel = nDir * maxDist;

		SPHERE rayStart(origin, kRadius);

		bool  hitAny = false;
		float bestDist = maxDist + 1.0f;

		auto& objVec = GetStage()->GetGameObjectVec();

		for (auto& obj : objVec)
		{
			if (!obj) continue;
			if (!obj->IsUpdateActive()) continue;
			if (ignoreObj && obj == ignoreObj) continue;
			if (ignoreTags.size() != 0)
			{
				bool ignored = false;
				for (const auto& t : ignoreTags)
				{
					if (obj->FindTag(t)) { ignored = true; break; }
				}
				if (ignored) continue;
			}

			auto col = obj->GetComponent<Collision>(false);
			if (!col) continue;
			if (!col->IsUpdateActive()) continue;

			float hitTime = 0.0f;

			if (auto sp = std::dynamic_pointer_cast<CollisionSphere>(col))
			{
				SPHERE target = sp->GetSphere();
				if (HitTest::CollisionTestSphereSphere(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					Vec3 n = p - target.m_Center;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					Vec3 hitPoint = target.m_Center + n * target.m_Radius;
					//float dist = bsmUtil::length(hitPoint - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = hitPoint;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			if (auto cap = std::dynamic_pointer_cast<CollisionCapsule>(col))
			{
				CAPSULE target = cap->GetCapsule();
				if (HitTest::CollisionTestSphereCapsule(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					SPHERE chk(p, kRadius);
					Vec3 ret;
					HitTest::SPHERE_CAPSULE(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			if (auto obb = std::dynamic_pointer_cast<CollisionObb>(col))
			{
				OBB target = obb->GetObb();
				if (HitTest::CollisionTestSphereObb(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					SPHERE chk(p, kRadius);
					Vec3 ret;
					HitTest::SPHERE_OBB(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}

			if (auto rc = std::dynamic_pointer_cast<CollisionRect>(col))
			{
				COLRECT target = rc->GetColRect();
				if (HitTest::CollisionTestSphereRect(rayStart, rayVel, target, 0.0f, kElapsed, hitTime))
				{
					Vec3 p = origin + rayVel * hitTime;

					SPHERE chk(p, kRadius);
					Vec3 ret;
					HitTest::SPHERE_COLRECT(chk, target, ret);

					Vec3 n = p - ret;
					if (bsmUtil::dot(n, n) < 1e-8f) n = -nDir;
					n.normalize();

					//float dist = bsmUtil::length(ret - origin);
					const float dist = hitTime * maxDist;

					if (dist < bestDist)
					{
						bestDist = dist;
						outHit.m_Object = obj;
						outHit.m_Collision = col;
						outHit.m_Point = ret;
						outHit.m_Normal = n;
						outHit.m_Distance = dist;
						hitAny = true;
					}
				}
				continue;
			}
		}

		return hitAny;
	}

	void CollisionManager::OnCreate()
	{
		pImpl->m_MiscPerformance.SetActive(true);
	}

	void CollisionManager::OnUpdate(double elapsedTime)
	{
		// 1.キープされているペアをチェック
		m_TempKeepVec.clear();
		m_TempExitVec.clear();
		for (auto& v : m_CollisionPairVec[m_KeepIndex])
		{
			if (SimpleCollisionPair(v))
			{
				//まだ衝突している
				m_TempKeepVec.push_back(v);
			}
			else
			{
				m_TempExitVec.push_back(v);
			}
		}
		//テンポラリの内容をkeepペアにコピー
		m_CollisionPairVec[m_KeepIndex].resize(m_TempKeepVec.size());
		m_CollisionPairVec[m_KeepIndex] = m_TempKeepVec;
		//キープされているペアのSrcにもしGravityがセットされていたら0にする
		for (auto& v : m_CollisionPairVec[m_KeepIndex])
		{
			auto ShSrc = v.m_Src.lock();
			auto ShDest = v.m_Dest.lock();

			if (ShSrc)
			{
				auto Gr = ShSrc->GetGameObject()->GetComponent<Gravity>(false);
				if (Gr)
				{
					auto f = bsmUtil::angleBetweenNormals(v.m_SrcHitNormal, Vec3(0, 1, 0));
					if (abs(f) < XM_PIDIV4)
					{
						Gr->SetGravityVelocityZero();
					}
				}
			}
		}
		//新規のペア配列のクリア
		m_CollisionPairVec[m_NewIndex].clear();
		//新規の衝突判定
		SetNewCollision();
		//追加されたペアをキープに追加
		for (auto& v : m_CollisionPairVec[m_NewIndex])
		{
			//追加ペアのSrcにもしGravityがセットされていたら0にする
			auto ShSrc = v.m_Src.lock();
			if (ShSrc)
			{
				auto Gr = ShSrc->GetGameObject()->GetComponent<Gravity>(false);
				if (Gr)
				{
					auto f = bsmUtil::angleBetweenNormals(v.m_SrcHitNormal, Vec3(0, 1, 0));
					if (abs(f) < XM_PIDIV4)
					{
						Gr->SetGravityVelocityZero();
					}
				}
			}
			m_CollisionPairVec[m_KeepIndex].push_back(v);
		}

		//--------------------------------------------------------
		//キープ配列のソート(IsPriorityUnderEscapeY())の場合Yが小さい優先
		//--------------------------------------------------------
		auto func = [&](CollisionPair& Left, CollisionPair& Right)->bool {
			if (IsPriorityUnderEscapeY())
			{
				if (Left.m_CalcHitPoint.y < Right.m_CalcHitPoint.y)
				{
					return true;
				}
			}
			else
			{
				if (Left.m_CalcHitPoint.y > Right.m_CalcHitPoint.y)
				{
					return true;
				}
			}
			return false;
			};
		//衝突点でソート
		std::sort(m_CollisionPairVec[m_KeepIndex].begin(), m_CollisionPairVec[m_KeepIndex].end(), func);
		//エスケープ
		for (auto& v : m_CollisionPairVec[m_KeepIndex])
		{
			auto SrcSh = v.m_Src.lock();
			auto DestSh = v.m_Dest.lock();
			if (SrcSh && DestSh)
			{
				if (!SrcSh->IsFixed())
				{
					EscapeCollisionPair(v);
				}
			}
		}
		//衝突メッセージの送信
		//Exit
		for (auto& v : m_TempExitVec)
		{
			auto ShSrc = v.m_Src.lock();
			auto ShDest = v.m_Dest.lock();
			if (ShSrc && ShDest)
			{
				auto ShSrcObj = ShSrc->GetGameObject();
				auto ShDestObj = ShDest->GetGameObject();
				if (ShSrcObj && ShDestObj)
				{
					ShSrcObj->OnCollisionExit(ShDestObj);
					ShSrcObj->OnCollisionExit(v);
				}
			}
		}
		//キープ
		for (auto& v : m_TempKeepVec)
		{
			auto ShSrc = v.m_Src.lock();
			auto ShDest = v.m_Dest.lock();
			if (ShSrc && ShDest)
			{
				auto ShSrcObj = ShSrc->GetGameObject();
				auto ShDestObj = ShDest->GetGameObject();
				if (ShSrcObj && ShDestObj)
				{
					ShSrcObj->OnCollisionExecute(ShDestObj);
					ShSrcObj->OnCollisionExecute(v);
				}
			}
		}
		//新規
		for (auto& v : m_CollisionPairVec[m_NewIndex])
		{
			auto ShSrc = v.m_Src.lock();
			auto ShDest = v.m_Dest.lock();
			if (ShSrc && ShDest)
			{
				auto ShSrcObj = ShSrc->GetGameObject();
				auto ShDestObj = ShDest->GetGameObject();
				if (ShSrcObj && ShDestObj)
				{
					ShSrcObj->OnCollisionEnter(ShDestObj);
					ShSrcObj->OnCollisionEnter(v);
				}
			}
		}
		SleepCheckSet();
	}

	float CollisionManager::GetMiscPerformanceTime() const
	{
		return pImpl->m_MiscPerformance.GetPerformanceTime();

	}

	UINT CollisionManager::GetCollisionCountOfTern() const
	{
		return pImpl->m_CollisionBlocks.m_CollisionCountOfTern;
	}

}
