#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		std::shared_ptr<BaseMesh> CreateBombPreviewDiscMesh(ID3D12GraphicsCommandList* pCommandList, size_t segments)
		{
			if (segments < 64) segments = 64;

			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(segments + 1);
			indices.reserve(segments * 3);

			const XMFLOAT3 normal(0.0f, 1.0f, 0.0f);
			vertices.push_back(VertexPositionNormalTexture(
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				normal,
				XMFLOAT2(0.5f, 0.5f)));

			for (size_t i = 0; i < segments; ++i)
			{
				const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(segments);
				const float c = std::cos(angle);
				const float s = std::sin(angle);
				vertices.push_back(VertexPositionNormalTexture(
					XMFLOAT3(c, 0.0f, s),
					normal,
					XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f)));
			}

			for (size_t i = 0; i < segments; ++i)
			{
				const uint32_t current = static_cast<uint32_t>(i + 1);
				const uint32_t next = static_cast<uint32_t>(((i + 1) % segments) + 1);

				indices.push_back(0);
				indices.push_back(next);
				indices.push_back(current);
			}

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}

		std::shared_ptr<BaseMesh> CreatePlayerSpawnPortalDiscMesh(ID3D12GraphicsCommandList* pCommandList, size_t segments)
		{
			return CreateBombPreviewDiscMesh(pCommandList, segments);
		}

		std::shared_ptr<BaseMesh> CreateBombPreviewLineMesh(ID3D12GraphicsCommandList* pCommandList)
		{
			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(4);
			indices.reserve(12);

			const XMFLOAT3 normal(0.0f, 1.0f, 0.0f);
			vertices.push_back(VertexPositionNormalTexture(XMFLOAT3(-0.5f, 0.0f, -0.5f), normal, XMFLOAT2(0.0f, 1.0f)));
			vertices.push_back(VertexPositionNormalTexture(XMFLOAT3( 0.5f, 0.0f, -0.5f), normal, XMFLOAT2(1.0f, 1.0f)));
			vertices.push_back(VertexPositionNormalTexture(XMFLOAT3( 0.5f, 0.0f,  0.5f), normal, XMFLOAT2(1.0f, 0.0f)));
			vertices.push_back(VertexPositionNormalTexture(XMFLOAT3(-0.5f, 0.0f,  0.5f), normal, XMFLOAT2(0.0f, 0.0f)));

			indices.push_back(0);
			indices.push_back(2);
			indices.push_back(1);
			indices.push_back(0);
			indices.push_back(3);
			indices.push_back(2);
			indices.push_back(0);
			indices.push_back(1);
			indices.push_back(2);
			indices.push_back(0);
			indices.push_back(2);
			indices.push_back(3);

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}

		std::shared_ptr<BaseMesh> CreatePlayerSpawnPortalRingMesh(ID3D12GraphicsCommandList* pCommandList, size_t segments)
		{
			if (segments < 64) segments = 64;

			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(segments * 2);
			indices.reserve(segments * 12);

			const float outerRadius = 1.0f;
			const float innerRadius = 0.62f;
			const XMFLOAT3 normal(0.0f, 1.0f, 0.0f);

			for (size_t i = 0; i < segments; ++i)
			{
				const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(segments);
				const float c = std::cos(angle);
				const float s = std::sin(angle);

				// ローカルXZ平面に穴あき円を作る。Player側で縦向きに回転してワープホールとして使う。
				vertices.push_back(VertexPositionNormalTexture(
					XMFLOAT3(c * outerRadius, 0.0f, s * outerRadius),
					normal,
					XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f)));
				vertices.push_back(VertexPositionNormalTexture(
					XMFLOAT3(c * innerRadius, 0.0f, s * innerRadius),
					normal,
					XMFLOAT2(0.5f + c * innerRadius * 0.5f, 0.5f - s * innerRadius * 0.5f)));
			}

			for (size_t i = 0; i < segments; ++i)
			{
				const uint32_t outer0 = static_cast<uint32_t>(i * 2);
				const uint32_t inner0 = outer0 + 1;
				const uint32_t outer1 = static_cast<uint32_t>(((i + 1) % segments) * 2);
				const uint32_t inner1 = outer1 + 1;

				indices.push_back(outer0);
				indices.push_back(outer1);
				indices.push_back(inner1);
				indices.push_back(outer0);
				indices.push_back(inner1);
				indices.push_back(inner0);

				// 裏面から見ても消えないように、逆巻きの面も入れておく。
				indices.push_back(outer0);
				indices.push_back(inner1);
				indices.push_back(outer1);
				indices.push_back(outer0);
				indices.push_back(inner0);
				indices.push_back(inner1);
			}

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}

		std::shared_ptr<BaseMesh> CreateStageSlopeShadowProxyMesh(ID3D12GraphicsCommandList* pCommandList)
		{
			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(60);
			indices.reserve(60);

			auto addTriangle = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
			{
				const XMFLOAT3 normal(0.0f, 1.0f, 0.0f);
				uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
				vertices.push_back(VertexPositionNormalTexture(a, normal, XMFLOAT2(0.0f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(b, normal, XMFLOAT2(1.0f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(c, normal, XMFLOAT2(1.0f, 1.0f)));
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1);
				indices.push_back(baseIndex + 2);

				// shadow pass の back-face culling で面が消えないよう、同じ三角形を逆向きにも入れる。
				baseIndex = static_cast<uint32_t>(vertices.size());
				vertices.push_back(VertexPositionNormalTexture(a, normal, XMFLOAT2(0.0f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(c, normal, XMFLOAT2(1.0f, 1.0f)));
				vertices.push_back(VertexPositionNormalTexture(b, normal, XMFLOAT2(1.0f, 0.0f)));
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1);
				indices.push_back(baseIndex + 2);
			};

			auto addQuad = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d)
			{
				addTriangle(a, b, c);
				addTriangle(a, c, d);
			};

			// 1x1x1 の範囲に収まる三角柱。WorldObjects 側で実モデルの bounds に合わせて拡縮する。
			const XMFLOAT3 lowLeft(-0.5f, -0.5f, 0.5f);
			const XMFLOAT3 lowRight(0.5f, -0.5f, 0.5f);
			const XMFLOAT3 highLeft(-0.5f, 0.5f, -0.5f);
			const XMFLOAT3 highRight(0.5f, 0.5f, -0.5f);
			const XMFLOAT3 backBottomLeft(-0.5f, -0.5f, -0.5f);
			const XMFLOAT3 backBottomRight(0.5f, -0.5f, -0.5f);

			addQuad(lowLeft, lowRight, highRight, highLeft);               // 斜面
			addQuad(backBottomLeft, highLeft, highRight, backBottomRight); // 上側の垂直面
			addTriangle(backBottomLeft, lowLeft, highLeft);                // 左側面
			addTriangle(lowRight, backBottomRight, highRight);             // 右側面
			addQuad(backBottomLeft, backBottomRight, lowRight, lowLeft);   // 底面

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}
		std::shared_ptr<BaseMesh> CreateBulletImpactSparkMesh(ID3D12GraphicsCommandList* pCommandList)
		{
			struct ImpactSparkShard
			{
				float angle;   // 中心からどの方向へ火花を伸ばすか
				float inner;   // 中心から少し離して、真ん中を詰めすぎないための距離
				float length;  // 火花の長さ
				float width;   // 火花の根元の太さ
				float skew;    // 先端を横へずらして、機械的な放射状に見えにくくする
			};

			// 小さい着弾用なので、数枚の三角片だけで軽く見える形にする。
			static const ImpactSparkShard kShards[] =
			{
				{ XMConvertToRadians(  8.0f), 0.02f, 1.00f, 0.18f,  0.02f },
				{ XMConvertToRadians( 48.0f), 0.04f, 0.55f, 0.11f, -0.01f },
				{ XMConvertToRadians(104.0f), 0.03f, 0.72f, 0.13f,  0.03f },
				{ XMConvertToRadians(176.0f), 0.02f, 0.48f, 0.10f, -0.02f },
				{ XMConvertToRadians(228.0f), 0.05f, 0.66f, 0.12f,  0.01f },
				{ XMConvertToRadians(292.0f), 0.01f, 0.86f, 0.15f, -0.03f },
			};

			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve((sizeof(kShards) / sizeof(kShards[0])) * 6);
			indices.reserve((sizeof(kShards) / sizeof(kShards[0])) * 6);

			auto addDoubleSidedTriangle = [&](const XMFLOAT3& tip, const XMFLOAT3& left, const XMFLOAT3& right)
			{
				// 着弾面が壁でも床でも見えるよう、表裏どちらからでも描ける三角形にする。
				uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
				vertices.push_back(VertexPositionNormalTexture(tip, XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.5f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(left, XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.15f, 1.0f)));
				vertices.push_back(VertexPositionNormalTexture(right, XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.85f, 1.0f)));
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1);
				indices.push_back(baseIndex + 2);

				baseIndex = static_cast<uint32_t>(vertices.size());
				vertices.push_back(VertexPositionNormalTexture(tip, XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.5f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(right, XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.85f, 1.0f)));
				vertices.push_back(VertexPositionNormalTexture(left, XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.15f, 1.0f)));
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1);
				indices.push_back(baseIndex + 2);
			};

			// ローカルXY平面に火花を作る。PlayerCombat側でローカル+Zを着弾面の法線へ向ける。
			for (const auto& shard : kShards)
			{
				const float c = std::cos(shard.angle);
				const float s = std::sin(shard.angle);
				const XMFLOAT2 dir(c, s);
				const XMFLOAT2 tangent(-s, c);
				const float halfWidth = shard.width * 0.5f;

				const XMFLOAT3 tip(
					dir.x * (shard.inner + shard.length) + tangent.x * shard.skew,
					dir.y * (shard.inner + shard.length) + tangent.y * shard.skew,
					0.0f);
				const XMFLOAT3 left(
					dir.x * shard.inner - tangent.x * halfWidth,
					dir.y * shard.inner - tangent.y * halfWidth,
					0.0f);
				const XMFLOAT3 right(
					dir.x * shard.inner + tangent.x * halfWidth,
					dir.y * shard.inner + tangent.y * halfWidth,
					0.0f);

				addDoubleSidedTriangle(tip, left, right);
			}

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}

		std::shared_ptr<BaseMesh> CreateMuzzleFlashMesh(ID3D12GraphicsCommandList* pCommandList, int patternIndex)
		{
			struct MuzzleFlashShard
			{
				float angle;   // 飛び散る方向
				float radius;  // 中心から少し離す量
				float length;  // 三角形の長さ
				float width;   // 三角形の太さ
				float skew;    // 先端の横ずらし
			};

			// 形状違いを出すために、三角片の並びを3セット用意する。
			static const MuzzleFlashShard kPatternA[] =
			{
				{ XMConvertToRadians(   0.0f), 0.00f, 1.62f, 0.38f,  0.02f },
				{ XMConvertToRadians(  32.0f), 0.04f, 1.08f, 0.25f, -0.03f },
				{ XMConvertToRadians(  72.0f), 0.02f, 0.82f, 0.20f,  0.04f },
				{ XMConvertToRadians( 128.0f), 0.06f, 1.24f, 0.29f,  0.01f },
				{ XMConvertToRadians( 185.0f), 0.03f, 0.66f, 0.18f, -0.02f },
				{ XMConvertToRadians( 226.0f), 0.05f, 0.99f, 0.24f,  0.05f },
				{ XMConvertToRadians( 286.0f), 0.01f, 1.40f, 0.31f, -0.04f },
				{ XMConvertToRadians( 326.0f), 0.07f, 0.74f, 0.17f,  0.02f },
			};
			static const MuzzleFlashShard kPatternB[] =
			{
				{ XMConvertToRadians( -12.0f), 0.00f, 1.52f, 0.30f, -0.05f },
				{ XMConvertToRadians(  18.0f), 0.06f, 0.78f, 0.17f,  0.02f },
				{ XMConvertToRadians(  58.0f), 0.03f, 1.28f, 0.27f,  0.06f },
				{ XMConvertToRadians( 112.0f), 0.07f, 0.88f, 0.19f, -0.03f },
				{ XMConvertToRadians( 170.0f), 0.02f, 1.10f, 0.24f,  0.04f },
				{ XMConvertToRadians( 244.0f), 0.05f, 0.70f, 0.16f, -0.02f },
				{ XMConvertToRadians( 302.0f), 0.01f, 1.48f, 0.33f,  0.03f },
			};
			static const MuzzleFlashShard kPatternC[] =
			{
				{ XMConvertToRadians(   8.0f), 0.02f, 1.74f, 0.42f,  0.00f },
				{ XMConvertToRadians(  45.0f), 0.04f, 0.86f, 0.18f, -0.05f },
				{ XMConvertToRadians(  94.0f), 0.00f, 1.18f, 0.26f,  0.04f },
				{ XMConvertToRadians( 148.0f), 0.08f, 0.72f, 0.16f,  0.02f },
				{ XMConvertToRadians( 214.0f), 0.03f, 1.34f, 0.28f, -0.06f },
				{ XMConvertToRadians( 270.0f), 0.06f, 0.92f, 0.21f,  0.03f },
				{ XMConvertToRadians( 318.0f), 0.01f, 1.06f, 0.23f, -0.02f },
				{ XMConvertToRadians( 350.0f), 0.05f, 0.62f, 0.15f,  0.04f },
			};

			// 登録時に渡された番号から使うパターンを選ぶ。
			const MuzzleFlashShard* shards = kPatternA;
			size_t shardCount = sizeof(kPatternA) / sizeof(kPatternA[0]);
			switch (patternIndex)
			{
			case 1:
				shards = kPatternB;
				shardCount = sizeof(kPatternB) / sizeof(kPatternB[0]);
				break;
			case 2:
				shards = kPatternC;
				shardCount = sizeof(kPatternC) / sizeof(kPatternC[0]);
				break;
			default:
				break;
			}

			std::vector<VertexPositionNormalTexture> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(shardCount * 3);
			indices.reserve(shardCount * 3);

			const XMFLOAT3 normal(0.0f, 0.0f, -1.0f);
			// 各三角片を1枚の三角形に展開して、1つのメッシュにまとめる。
			for (size_t i = 0; i < shardCount; ++i)
			{
				const auto& shard = shards[i];
				const float c = std::cos(shard.angle);
				const float s = std::sin(shard.angle);
				const XMFLOAT2 dir(c, s);
				const XMFLOAT2 tangent(-s, c);
				const float halfWidth = shard.width * 0.5f;

				const XMFLOAT3 tip(
					dir.x * (shard.radius + shard.length) + tangent.x * shard.skew,
					dir.y * (shard.radius + shard.length) + tangent.y * shard.skew,
					0.0f);
				const XMFLOAT3 left(
					dir.x * shard.radius - tangent.x * halfWidth,
					dir.y * shard.radius - tangent.y * halfWidth,
					0.0f);
				const XMFLOAT3 right(
					dir.x * shard.radius + tangent.x * halfWidth,
					dir.y * shard.radius + tangent.y * halfWidth,
					0.0f);

				const uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
				vertices.push_back(VertexPositionNormalTexture(tip, normal, XMFLOAT2(0.5f, 0.0f)));
				vertices.push_back(VertexPositionNormalTexture(left, normal, XMFLOAT2(0.15f, 1.0f)));
				vertices.push_back(VertexPositionNormalTexture(right, normal, XMFLOAT2(0.85f, 1.0f)));

				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1);
				indices.push_back(baseIndex + 2);
			}

			return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
		}
	}
	IMPLEMENT_DX12SHADER(SpVSPCStatic, App::GetShadersDir() + L"SpVSPCStatic.cso")
	IMPLEMENT_DX12SHADER(SpPSPCStatic, App::GetShadersDir() + L"SpPSPCStatic.cso")

	using namespace SceneEnums;

	Scene::Scene(UINT frameCount, PrimDevice* pPrimDevice) :
		BaseScene(frameCount, pPrimDevice)
	{
	}

	Scene::~Scene()
	{
		GameAudio::Instance().Shutdown();
	}

	void Scene::Destroy()
	{
		if (m_activeStage)
		{
			// PhysXを破棄する前に、ステージ内コンポーネントへ外部リソースの解放を通知する。
			m_activeStage->OnDestroy();
			m_activeStage.reset();
		}
	}

	bool Scene::IsMouseInRect(const D2D1_RECT_F& rect) const
	{
		const auto& mouse = App::GetInputDevice().GetMouseState();
		const float x = static_cast<float>(mouse.now.x);
		const float y = static_cast<float>(mouse.now.y);
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	void Scene::SetMouseCursorVisible(bool visible)
	{
		if (m_cursorVisible == visible)
		{
			return;
		}

		m_cursorVisible = visible;

		if (visible)
		{
			while (::ShowCursor(TRUE) < 0) {}
		}
		else
		{
			while (::ShowCursor(FALSE) >= 0) {}
		}
	}

	void Scene::CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
	{
		// 音声はD3Dリソースではないが、シーンで使う共通アセットなのでここでまとめて初期化する。
		// WAVが未配置でもLoadDefaultSounds側で無視するため、音素材を後から追加しやすい。
		GameAudio::Instance().Initialize();
		GameAudio::Instance().LoadDefaultSounds();

		// テクスチャ
		auto texFile = App::GetRelativeAssetsDir() + L"Textures/wall.png";
		auto texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"WALL_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/sky.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"SKY_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/trace.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"TRACE_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/trace3.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"TRACE3_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/particle_fire.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"EXPLOSION_FIRE_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/wall_brick_sand_both.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"WALL_SAND_TX", texture);

		RegisterMesh(L"BOMB_PREVIEW_DISC", CreateBombPreviewDiscMesh(pCommandList, 96));
		RegisterMesh(L"BOMB_PREVIEW_LINE", CreateBombPreviewLineMesh(pCommandList));
		RegisterMesh(L"PLAYER_SPAWN_PORTAL_DISC", CreatePlayerSpawnPortalDiscMesh(pCommandList, 96));
		RegisterMesh(L"PLAYER_SPAWN_PORTAL_RING", CreatePlayerSpawnPortalRingMesh(pCommandList, 96));
		RegisterMesh(L"STAGEOBJ_SHADOW_SLOPE_PROXY", CreateStageSlopeShadowProxyMesh(pCommandList));
		// 通常射撃の着弾位置に出す、小さい火花用のコード生成メッシュ。
		RegisterMesh(L"BULLET_IMPACT_SPARK_MESH", CreateBulletImpactSparkMesh(pCommandList));
		// 発射ごとに切り替えられるよう、形状違いのメッシュを別キーで登録する。
		RegisterMesh(L"MUZZLE_FLASH_MESH_0", CreateMuzzleFlashMesh(pCommandList, 0));
		RegisterMesh(L"MUZZLE_FLASH_MESH_1", CreateMuzzleFlashMesh(pCommandList, 1));
		RegisterMesh(L"MUZZLE_FLASH_MESH_2", CreateMuzzleFlashMesh(pCommandList, 2));
		// 汎用テクスチャ
		texFile = App::GetRelativeAssetsDir() + L"Model/Textures/colormap.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"CHARACTER_TEXTURE_SKINNED", texture);

		// 床
		{
			auto floorParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/floor.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> floorMeshes;
			for (size_t i = 0; i < floorParts.size(); ++i)
			{
				floorMeshes.push_back(floorParts[i].mesh);
				RegisterMaterial(
					L"FLOOR_MAT_" + std::to_wstring(i),
					floorParts[i].material
				);
			}
			RegisterModelMesh(L"FLOOR_MODEL", floorMeshes);
		}

		// 岩付きの床
		{
			auto floorDetailParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/floor-detail.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> floorDetailMeshes;
			for (size_t i = 0; i < floorDetailParts.size(); ++i)
			{
				floorDetailMeshes.push_back(floorDetailParts[i].mesh);
				RegisterMaterial(
					L"FLOOR_DETAIL_MAT_" + std::to_wstring(i),
					floorDetailParts[i].material
				);
			}
			RegisterModelMesh(L"FLOOR_DETAIL_MODEL", floorDetailMeshes);
		}

		// Enemy skinned instancing mesh.
		auto enemySkinnedMesh = BaseMesh::CreateMergedBoneModelMesh(
			pCommandList,
			App::GetRelativeAssetsDir(),
			L"Model/character-orc.fbx"
		);
		RegisterMesh(L"ENEMY_MODEL_SKINNED", enemySkinnedMesh);

		// プレイヤー
		auto skinnedMesh = BaseMesh::CreateMergedBoneModelMesh(
			pCommandList,
			App::GetRelativeAssetsDir(),
			L"Model/character-human.fbx"
		);
		RegisterMesh(L"PLAYER_MODEL_SKINNED", skinnedMesh);

		// Player blaster
		{
			auto blasterParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/blaster-a.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> blasterMeshes;
			for (size_t i = 0; i < blasterParts.size(); ++i)
			{
				blasterMeshes.push_back(blasterParts[i].mesh);
				RegisterMaterial(
					L"PLAYER_BLASTER_MAT_" + std::to_wstring(i),
					blasterParts[i].material
				);
			}
			RegisterModelMesh(L"PLAYER_BLASTER_MODEL", blasterMeshes);
		}

		// Bomb grenade
		{
			auto grenadeParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/grenade-b.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> grenadeMeshes;
			for (size_t i = 0; i < grenadeParts.size(); ++i)
			{
				grenadeMeshes.push_back(grenadeParts[i].mesh);
				RegisterMaterial(
					L"BOMB_MAT_" + std::to_wstring(i),
					grenadeParts[i].material
				);
			}
			RegisterModelMesh(L"BOMB_MODEL", grenadeMeshes);
		}
		// HP recovery item
		{
			auto heartParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/heart.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> heartMeshes;
			for (size_t i = 0; i < heartParts.size(); ++i)
			{
				heartMeshes.push_back(heartParts[i].mesh);
				RegisterMaterial(
					L"HP_RECOVERY_ITEM_MAT_" + std::to_wstring(i),
					heartParts[i].material
				);
			}
			RegisterModelMesh(L"HP_RECOVERY_ITEM_MODEL", heartMeshes);
		}
		// StageObjects
		StageObjectCatalog::RegisterAssets(*this, pCommandList);

		m_pTgtCommandList = pCommandList;
		StartTitle();
	}

	void Scene::UpdateConstantBuffers()
	{
		if (m_activeStage)
		{
			m_activeStage->OnUpdateConstantBuffers();
		}
	}

	void Scene::CommitConstantBuffers()
	{
		if (m_activeStage)
		{
			m_activeStage->OnCommitConstantBuffers();
		}
	}


	void Scene::UpdateImGui()
	{
#if defined(_DEBUG)
		if (m_gameState != GameState::Playing || !m_stageEditor.IsActive())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		auto device = BaseDevice::GetBaseDevice();
		if (!gameStage || !device)
		{
			return;
		}

		if (m_stageEditor.DrawImGui(
				*gameStage,
				static_cast<float>(device->GetWidth()),
				static_cast<float>(device->GetHeight())))
		{
			m_stageEditorReloadRequested = true;
		}
#endif
	}

	void Scene::Update(double elapsedTime)
	{
		s_elapsedTime = elapsedTime;
		m_titleTime += elapsedTime;
		m_screenTransition.Update(elapsedTime);
		GameAudio::Instance().Update();

		if (m_gameState == GameState::Title)
		{
			if (!m_activeStage)
			{
				StartTitle();
			}

			UpdateTitleInput();
			if (m_gameState != GameState::Title)
			{
				if (m_activeStage)
				{
					UpdateConstantBuffers();
					CommitConstantBuffers();
				}
				return;
			}

			if (m_gameState == GameState::Title && m_activeStage)
			{
				m_activeStage->UpdateStage();
				UpdateConstantBuffers();
				CommitConstantBuffers();
			}
			return;
		}

		if (m_gameState == GameState::Playing)
		{
			UpdateInitialControlGuide(elapsedTime);

			if (m_activeStage)
			{
#if defined(_DEBUG)
				if (!m_optionOpen &&
					!m_screenTransition.IsInputBlocked() &&
					App::GetInputDevice().KeyPressed(VK_F3))
				{
					if (m_stageEditor.IsActive())
					{
						ExitStageEditor();
					}
					else
					{
						EnterStageEditor();
					}

					UpdateConstantBuffers();
					CommitConstantBuffers();
					return;
				}

				if (m_stageEditor.IsActive())
				{
					if (App::GetInputDevice().KeyPressed(VK_ESCAPE))
					{
						ExitStageEditor();
					}
					else if (m_stageEditorReloadRequested)
					{
						ReloadStageForEditor();
					}

					// 編集中は敵・プレイヤー・ウェーブを進めず、描画用定数だけを更新する。
					UpdateConstantBuffers();
					CommitConstantBuffers();
					return;
				}
#endif

				if (m_optionOpen)
				{
					UpdateOptionInput();
					UpdateConstantBuffers();
					CommitConstantBuffers();
					return;
				}

				if (m_waitingForOptionMouseRelease)
				{
					const auto& input = App::GetInputDevice();
					if (!input.MouseDown(VK_LBUTTON) &&
						!input.MouseDown(VK_RBUTTON) &&
						!input.MouseDown(VK_MBUTTON))
					{
						m_waitingForOptionMouseRelease = false;
					}

					// 解放を検出したフレームも更新せず、次フレームからゲーム入力を受け付ける。
					UpdateConstantBuffers();
					CommitConstantBuffers();
					return;
				}

				if (App::GetInputDevice().KeyPressed(VK_ESCAPE))
				{
					OpenOptionMenu();
					UpdateConstantBuffers();
					CommitConstantBuffers();
					return;
				}

				m_activeStage->UpdateStage();

				auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
				auto player = gameStage ? gameStage->GetSharedGameObjectEx<Player>(L"Player", false) : nullptr;

				if (player && player->IsDeathAnimationFinished())
				{
					auto& benchmark = BenchmarkRecorder::Instance();
					if (benchmark.IsRunning())
					{
						// ゲームオーバー後は本編外なので、実行中の計測をここで確定する。
						benchmark.Stop(true);
					}
					benchmark.ClearNotification();

					// GameStageはこの後更新を止めるため、リザルト用の値をここで確定する。
					m_lastSurvivalTime = gameStage->GetSurvivalTime();
					m_lastDefeatedEnemyCount = gameStage->GetDefeatedEnemyCount();
					m_lastReachedWave = gameStage->GetCurrentWave();
					m_lastPlayerLevel = player->GetLevel();
					m_lastTotalDamageDealt = gameStage->GetTotalDamageDealt();
					m_lastBestExplosionKills = gameStage->GetBestExplosionKills();
					m_gameState = GameState::Result;

					SetMouseCursorVisible(true);
				}

				UpdateConstantBuffers();
				CommitConstantBuffers();
			}
			return;
		}

		if (m_gameState == GameState::Result)
		{
			if (m_activeStage)
			{
				UpdateConstantBuffers();
				CommitConstantBuffers();
			}
			return;
		}
	}

	void Scene::ShadowPass(ID3D12GraphicsCommandList* pCommandList)
	{
		// Set necessary state.
		auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rootSignature.Get());
		//PipelineState
		auto shadowPipeline = PipelineStatePool::GetPipelineState(L"PNTShadowMap", false);
		if (shadowPipeline)
		{
			pCommandList->SetPipelineState(shadowPipeline.Get());
		}
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->RSSetViewports(1, &m_viewport);
		pCommandList->RSSetScissorRects(1, &m_scissorRect);
		// No render target needed for the shadow pass.
		pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &m_depthDsvs[DepthGenPass::Shadow]);

		if (m_activeStage)
		{
			m_pTgtCommandList = pCommandList;
			m_activeStage->OnShadowDraw(pCommandList);
		}
	}

	void Scene::ScenePass(ID3D12GraphicsCommandList* pCommandList)
	{
		auto& viewport = GetViewport();
		auto& scissorRect = GetScissorRect();
		auto depthDsvs = GetDepthDsvs();
		// set RootSignature
		auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rootSignature.Get());
		// set Viewports & ScissorRects
		pCommandList->RSSetViewports(1, &viewport);
		pCommandList->RSSetScissorRects(1, &scissorRect);
		// set RenderTargets
		pCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferRtvCpuHandle(), FALSE, &depthDsvs[DepthGenPass::Scene]);
		if (m_activeStage)
		{
			m_pTgtCommandList = pCommandList;
			m_activeStage->OnSceneDraw(pCommandList);
		}
	}
}


