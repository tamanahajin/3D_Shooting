#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const float kSceneTransitionFadeOutSeconds = 0.35f;
		const float kSceneTransitionFadeInSeconds = 0.45f;
		const wchar_t* kOptionIconPath = L"UI/option.png";
		const wchar_t* kBombHudIconPath = L"UI/bom_icon.png";
		const int kOptionSliderNone = -1;
		const int kOptionSliderBgm = 0;
		const int kOptionSliderSe = 1;
		const float kOptionIconSize = 46.0f;
		const float kOptionIconMargin = 20.0f;
		const float kOptionSliderHeight = 34.0f;
		const float kOptionSliderTrackOffset = 118.0f;
		const float kBombHudIconSize = 56.0f;
		const float kBombHudMargin = 24.0f;

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
			// ワープホール用の薄い黒円。形状は単純な円盤にして、揺れ方は頂点シェーダー側で足す。
			// メッシュとシェーダーの責務を分けると、後から別の円盤や別演出にも同じWaveEffectを使いやすい。
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

	void Scene::StartTitle()
	{
		if (m_stageEditor.IsActive())
		{
			auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
			if (gameStage)
			{
				m_stageEditor.Exit(*gameStage);
			}
		}
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);

		auto& benchmark = BenchmarkRecorder::Instance();
		if (benchmark.IsRunning())
		{
			benchmark.Stop(true);
		}
		benchmark.ClearNotification();

		m_optionOpen = false;
		m_waitingForOptionMouseRelease = false;
		m_optionDraggingSlider = kOptionSliderNone;
		m_gameState = GameState::Title;
		m_titleMenuIndex = 0;
		m_titleTime = 0.0;

		SetMouseCursorVisible(true);

		ResetActiveStage<TitleStage>(App::GetD3D12Device());
	}

	void Scene::StartGame()
	{
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);

		// ベンチマークはインゲーム中だけ扱う。前ステートの通知は新しいプレイへ持ち越さない。
		BenchmarkRecorder::Instance().ClearNotification();

		m_optionOpen = false;
		m_waitingForOptionMouseRelease = false;
		m_optionDraggingSlider = kOptionSliderNone;
		m_lastSurvivalTime = 0.0;
		m_lastDefeatedEnemyCount = 0;
		m_lastReachedWave = 0;
		m_lastTotalDamageDealt = 0;
		m_lastBestExplosionKills = 0;
		m_gameState = GameState::Playing;

		SetMouseCursorVisible(false);
		// インゲームBGMはGameStage側でプレイヤー登場演出が終わった後に開始する。
		GameAudio::Instance().StopBgm();

		ResetActiveStage<GameStage>(App::GetD3D12Device());
	}

	void Scene::EnterStageEditor()
	{
#if defined(_DEBUG)
		if (m_gameState != GameState::Playing ||
			m_optionOpen ||
			m_screenTransition.IsInputBlocked())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage || !m_stageEditor.Enter(*gameStage))
		{
			return;
		}

		auto& benchmark = BenchmarkRecorder::Instance();
		if (benchmark.IsRunning())
		{
			// エディタ中はゲーム更新を止めるため、計測中なら編集開始前までで確定する。
			benchmark.Stop(true);
		}

		m_stageEditorReloadRequested = false;
		SetFogEnabled(false);
		SetMouseCursorVisible(true);
#endif
	}

	void Scene::ExitStageEditor()
	{
#if defined(_DEBUG)
		if (!m_stageEditor.IsActive())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (gameStage)
		{
			m_stageEditor.Exit(*gameStage);
		}

		const auto& input = App::GetInputDevice();
		// エディタを閉じたクリックが射撃や爆弾入力へ流れないよう、ボタンの解放を待つ。
		m_waitingForOptionMouseRelease =
			input.MouseDown(VK_LBUTTON) ||
			input.MouseDown(VK_RBUTTON) ||
			input.MouseDown(VK_MBUTTON);
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);
		SetMouseCursorVisible(false);
#endif
	}

	void Scene::ReloadStageForEditor()
	{
#if defined(_DEBUG)
		if (!m_stageEditor.IsActive())
		{
			m_stageEditorReloadRequested = false;
			return;
		}

		// 描画中にステージを破棄しないよう、ImGuiからの要求を次のUpdateで処理する。
		auto gameStage = ResetActiveStage<GameStage>(App::GetD3D12Device());
		m_stageEditor.OnStageReloaded(*gameStage);
		m_stageEditorReloadRequested = false;
#endif
	}

	void Scene::RequestStartGame()
	{
		if (m_gameState != GameState::Title || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[this]()
			{
				StartGame();
			});
	}

	void Scene::RequestStartTitle()
	{
		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[this]()
			{
				StartTitle();
			});
	}

	void Scene::RequestExitGame()
	{
		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		// EXIT直後に終了すると決定音が聞こえる前にアプリが閉じるため、フェードアウト後に終了する。
		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[]()
			{
				::PostQuitMessage(0);
			});
	}

	void Scene::ConfirmTitleMenuSelection()
	{
		GameAudio::Instance().PlaySound(GameSoundId::Decide);

		if (m_titleMenuIndex == 0)
		{
			RequestStartGame();
		}
		else
		{
			RequestExitGame();
		}
	}

	void Scene::SetTitleMenuIndex(int index, bool playCursorMoveSound)
	{
		index = bsmUtil::Clamp(index, 0, 1);
		if (m_titleMenuIndex == index)
		{
			return;
		}

		m_titleMenuIndex = index;
		if (playCursorMoveSound)
		{
			GameAudio::Instance().PlaySound(GameSoundId::CursorMove);
		}
	}

	void Scene::OpenOptionMenu()
	{
		if (m_optionOpen || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_optionOpen = true;
		m_optionDraggingSlider = kOptionSliderNone;
		GameAudio::Instance().PlaySound(GameSoundId::Decide);

		if (m_gameState == GameState::Playing)
		{
			SetMouseCursorVisible(true);
		}
	}

	void Scene::CloseOptionMenu()
	{
		if (!m_optionOpen)
		{
			return;
		}

		m_optionOpen = false;
		m_optionDraggingSlider = kOptionSliderNone;
		GameAudio::Instance().PlaySound(GameSoundId::Cancel);

		if (m_gameState == GameState::Playing)
		{
			const auto& input = App::GetInputDevice();
			// UIを閉じたクリックが射撃入力へ流れないよう、押されているボタンの解放を待つ。
			m_waitingForOptionMouseRelease =
				input.MouseDown(VK_LBUTTON) ||
				input.MouseDown(VK_RBUTTON) ||
				input.MouseDown(VK_MBUTTON);
			SetMouseCursorVisible(false);
		}
	}

	void Scene::UpdateOptionInput()
	{
		if (!m_optionOpen || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		if (App::GetInputDevice().KeyPressed(VK_ESCAPE))
		{
			CloseOptionMenu();
		}
	}

	float Scene::UpdateOptionSliderValue(int sliderIndex, const D2D1_RECT_F& rect, float currentValue)
	{
		const auto& input = App::GetInputDevice();
		const auto& mouse = input.GetMouseState();

		const float trackLeft = rect.left + kOptionSliderTrackOffset;
		const float trackRight = rect.right;
		const D2D1_RECT_F hitRect = D2D1::RectF(
			trackLeft - 18.0f,
			rect.top - 8.0f,
			trackRight + 18.0f,
			rect.bottom + 8.0f);

		if (input.MousePressed(VK_LBUTTON) && IsMouseInRect(hitRect))
		{
			m_optionDraggingSlider = sliderIndex;
		}

		float value = currentValue;
		if (m_optionDraggingSlider == sliderIndex && input.MouseDown(VK_LBUTTON))
		{
			const float width = trackRight - trackLeft;
			if (width > 0.0f)
			{
				value = bsmUtil::Clamp((static_cast<float>(mouse.now.x) - trackLeft) / width, 0.0f, 1.0f);
			}
		}

		if (m_optionDraggingSlider == sliderIndex && input.MouseReleased(VK_LBUTTON))
		{
			m_optionDraggingSlider = kOptionSliderNone;
		}

		return value;
	}

	void Scene::UpdateTitleInput()
	{
		if (m_optionOpen)
		{
			UpdateOptionInput();
			return;
		}

		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		const auto& input = App::GetInputDevice();

		if (input.KeyPressed(VK_UP) || input.KeyPressed('W'))
		{
			SetTitleMenuIndex(0, true);
		}
		if (input.KeyPressed(VK_DOWN) || input.KeyPressed('S'))
		{
			SetTitleMenuIndex(1, true);
		}

		if (input.KeyPressed(VK_RETURN) || input.KeyPressed(VK_SPACE) || input.KeyPressed('J'))
		{
			ConfirmTitleMenuSelection();
		}

		if (input.KeyPressed(VK_ESCAPE))
		{
			OpenOptionMenu();
		}
	}

	void Scene::DrawOptionButton()
	{
		UIButtonBehavior behavior;
		behavior.enabled = !m_screenTransition.IsInputBlocked();
		// 開閉時は決定音とキャンセル音を使い分けるため、ボタン共通の決定音は鳴らさない。
		behavior.playClickSound = false;

		const auto optionButton = m_uiManager.AddImageButton(
			App::GetRelativeAssetsDir() + kOptionIconPath,
			L"OptionButton",
			UIAnchor::TopRight,
			{ -kOptionIconMargin, kOptionIconMargin },
			{ kOptionIconSize, kOptionIconSize },
			0.82f,
			1.0f,
			behavior);

		if (optionButton.clicked)
		{
			if (m_optionOpen)
			{
				CloseOptionMenu();
			}
			else
			{
				OpenOptionMenu();
			}
		}
	}

	void Scene::DrawOptionMenu(UILayer& uiLayer)
	{
		const float screenW = uiLayer.GetWidth();
		const float screenH = uiLayer.GetHeight();
		const float sliderWidth = bsmUtil::Max(240.0f, bsmUtil::Min(420.0f, screenW - 80.0f));
		const UISizeF sliderSize = { sliderWidth, kOptionSliderHeight };

		auto makeSliderRect = [&](float yOffset)
		{
			const float left = (screenW - sliderWidth) * 0.5f;
			const float top = (screenH - kOptionSliderHeight) * 0.5f + yOffset;
			return D2D1::RectF(left, top, left + sliderWidth, top + kOptionSliderHeight);
		};

		auto& audio = GameAudio::Instance();
		const D2D1_RECT_F bgmRect = makeSliderRect(-46.0f);
		const D2D1_RECT_F seRect = makeSliderRect(18.0f);
		const float bgmVolume = UpdateOptionSliderValue(kOptionSliderBgm, bgmRect, audio.GetBgmVolume());
		const float seVolume = UpdateOptionSliderValue(kOptionSliderSe, seRect, audio.GetSeVolume());

		if (std::fabs(bgmVolume - audio.GetBgmVolume()) > 0.001f)
		{
			audio.SetBgmVolume(bgmVolume);
		}
		if (std::fabs(seVolume - audio.GetSeVolume()) > 0.001f)
		{
			audio.SetSeVolume(seVolume);
		}

		m_uiManager.AddFullscreenBackgroundOverlay(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.48f));

		m_uiManager.AddSlider(
			L"BGM",
			audio.GetBgmVolume(),
			UIAnchor::Center,
			{ 0.0f, -46.0f },
			sliderSize);

		m_uiManager.AddSlider(
			L"SE",
			audio.GetSeVolume(),
			UIAnchor::Center,
			{ 0.0f, 18.0f },
			sliderSize);

		UIButtonBehavior exitButtonBehavior;
		exitButtonBehavior.enabled = !m_screenTransition.IsInputBlocked();
		auto exitButton = m_uiManager.AddButton(
			L"EXIT",
			UIAnchor::Center,
			{ 0.0f, 104.0f },
			{ 180.0f, 52.0f },
			D2D1::ColorF(0.08f, 0.09f, 0.11f, 0.92f),
			D2D1::ColorF(0.18f, 0.20f, 0.24f, 0.96f),
			D2D1::ColorF(D2D1::ColorF::White),
			exitButtonBehavior);

		if (exitButton.clicked)
		{
			RequestExitGame();
		}
	}

	void Scene::RenderUIWithTransition(UILayer& uiLayer)
	{
		if (m_screenTransition.GetAlpha() > 0.0f)
		{
			m_uiManager.AddFullscreenOverlay(m_screenTransition.GetOverlayColor());
		}

		m_uiManager.Render(uiLayer);
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


	void Scene::UpdateUI(std::unique_ptr<UILayer>& uiLayer)
	{
		if (!uiLayer)
		{
			return;
		}

		m_uiManager.BeginFrame();

		if (m_gameState == GameState::Title)
		{
			if (m_optionOpen)
			{
				DrawOptionMenu(*uiLayer);
				DrawOptionButton();
				uiLayer->SetCrosshairEnabled(false);
				RenderUIWithTransition(*uiLayer);
				return;
			}

			const bool inputBlocked = m_screenTransition.IsInputBlocked();
			const float titleBob = std::sin(static_cast<float>(m_titleTime) * 1.8f) * 10.0f;
			const D2D1_COLOR_F selectedColor = D2D1::ColorF(1.0f, 0.86f, 0.12f, 1.0f);
			const D2D1_COLOR_F normalColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.92f);
			const D2D1_COLOR_F buttonBaseColor = D2D1::ColorF(0.04f, 0.05f, 0.06f, 0.74f);
			const D2D1_COLOR_F buttonHoverColor = D2D1::ColorF(0.14f, 0.16f, 0.18f, 0.88f);
			const float screenW = uiLayer->GetWidth();
			const UISizeF menuSize = { 260.0f, 38.0f };
			const float logoWidth = bsmUtil::Min(780.0f, screenW * 0.74f);
			const float logoHeight = logoWidth * (173.0f / 1365.0f);
			UIButtonBehavior titleButtonBehavior;
			titleButtonBehavior.enabled = !inputBlocked;
			// キーボード決定と同じ経路で音を鳴らすため、マウスクリック時の自動再生は無効にする。
			titleButtonBehavior.playClickSound = false;

			m_uiManager.AddImage(
				App::GetRelativeAssetsDir() + L"Textures/TitleLogo.png",
				UIAnchor::Center,
				{ 0.0f, -220.0f + titleBob },
				{ logoWidth, logoHeight });

			auto startButton = m_uiManager.AddButton(
				L"START",
				UIAnchor::Center,
				{ 0.0f, 110.0f },
				menuSize,
				buttonBaseColor,
				buttonHoverColor,
				m_titleMenuIndex == 0 ? selectedColor : normalColor,
				titleButtonBehavior);

			auto exitButton = m_uiManager.AddButton(
				L"EXIT",
				UIAnchor::Center,
				{ 0.0f, 158.0f },
				menuSize,
				buttonBaseColor,
				buttonHoverColor,
				m_titleMenuIndex == 1 ? selectedColor : normalColor,
				titleButtonBehavior);

			if (startButton.hovered)
			{
				SetTitleMenuIndex(0, false);
			}
			else if (exitButton.hovered)
			{
				SetTitleMenuIndex(1, false);
			}

			if (startButton.clicked)
			{
				SetTitleMenuIndex(0, false);
				ConfirmTitleMenuSelection();
			}
			if (exitButton.clicked)
			{
				SetTitleMenuIndex(1, false);
				ConfirmTitleMenuSelection();
			}

			DrawOptionButton();
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		if (m_gameState == GameState::Result)
		{
			const D2D1_COLOR_F white = D2D1::ColorF(D2D1::ColorF::White);
			const D2D1_COLOR_F red = D2D1::ColorF(0.92f, 0.12f, 0.10f, 1.0f);
			const D2D1_COLOR_F yellow = D2D1::ColorF(1.0f, 0.82f, 0.12f, 1.0f);

			// ゲームオーバー時の3D画面を残し、UIだけを読みやすく暗くする。
			m_uiManager.AddFullscreenBackgroundOverlay(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.68f));

			m_uiManager.AddText(
				L"GAME OVER",
				UIAnchor::Center,
				{ 0.0f, -230.0f },
				{ 720.0f, 72.0f },
				UITextAlign::Center,
				red,
				56.0f);

			m_uiManager.AddText(
				L"生存時間",
				UIAnchor::Center,
				{ 0.0f, -145.0f },
				{ 360.0f, 32.0f },
				UITextAlign::Center,
				white,
				22.0f);

			const int survivalSeconds = static_cast<int>(m_lastSurvivalTime);
			wchar_t survivalText[32];
			swprintf_s(
				survivalText,
				L"%02d:%02d",
				survivalSeconds / 60,
				survivalSeconds % 60);
			m_uiManager.AddText(
				survivalText,
				UIAnchor::Center,
				{ 0.0f, -108.0f },
				{ 360.0f, 46.0f },
				UITextAlign::Center,
				yellow,
				38.0f);

			auto addResultRow = [&](const std::wstring& label, const std::wstring& value, float y)
			{
				m_uiManager.AddText(
					label,
					UIAnchor::Center,
					{ -150.0f, y },
					{ 250.0f, 38.0f },
					UITextAlign::Right,
					white,
					22.0f);
				m_uiManager.AddText(
					value,
					UIAnchor::Center,
					{ 100.0f, y - 2.0f },
					{ 220.0f, 42.0f },
					UITextAlign::Left,
					yellow,
					28.0f);
			};

			addResultRow(L"総撃破数", std::to_wstring(m_lastDefeatedEnemyCount), -42.0f);
			addResultRow(L"到達ウェーブ", std::to_wstring(m_lastReachedWave), 4.0f);
			addResultRow(L"与えた総ダメージ", std::to_wstring(m_lastTotalDamageDealt), 50.0f);

			m_uiManager.AddText(
				L"BEST EXPLOSION",
				UIAnchor::Center,
				{ 0.0f, 108.0f },
				{ 460.0f, 36.0f },
				UITextAlign::Center,
				white,
				26.0f);

			wchar_t bestExplosionText[64];
			swprintf_s(
				bestExplosionText,
				L"1 BOMB / %d KILLS",
				m_lastBestExplosionKills);
			m_uiManager.AddText(
				bestExplosionText,
				UIAnchor::Center,
				{ 0.0f, 145.0f },
				{ 500.0f, 48.0f },
				UITextAlign::Center,
				yellow,
				34.0f);

			UIButtonBehavior titleButtonBehavior;
			titleButtonBehavior.enabled = !m_screenTransition.IsInputBlocked();
			auto titleButton = m_uiManager.AddButton(
				L"TITLE",
				UIAnchor::Center,
				{ 0.0f, 220.0f },
				{ 240.0f, 58.0f },
				D2D1::ColorF(0.35f, 0.12f, 0.12f, 0.95f),
				D2D1::ColorF(0.65f, 0.20f, 0.20f, 0.95f),
				white,
				titleButtonBehavior);

			if (titleButton.clicked)
			{
				RequestStartTitle();
			}

			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage)
		{
			if (m_screenTransition.GetAlpha() > 0.0f)
			{
				RenderUIWithTransition(*uiLayer);
			}
			else
			{
				uiLayer->ClearDrawCommands();
			}
			return;
		}

#if defined(_DEBUG)
		if (m_stageEditor.IsActive())
		{
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}
#endif

		auto device = BaseDevice::GetBaseDevice();
		auto player = gameStage->GetSharedGameObjectEx<Player>(L"Player", false);
		auto hp = player ? player->GetComponent<Health>() : nullptr;

		if (m_optionOpen)
		{
			DrawOptionMenu(*uiLayer);
			DrawOptionButton();
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		// 左上：デバッグ表示
		{
			const auto& debug = GameDebugSettingsStore::Get();
			wchar_t buff[256] = {};

			// 表示項目を個別に無効化できるよう、改行を含む文字列を組み立て分ける。
			if (debug.showFps && debug.showElapsedTime)
			{
				swprintf_s(
					buff,
					L"FPS: %.1f\nElapsed Time: %.6f",
					device->GetStableFps(),
					device->GetStableElapsedTime());
			}
			else if (debug.showFps)
			{
				swprintf_s(buff, L"FPS: %.1f", device->GetStableFps());
			}
			else if (debug.showElapsedTime)
			{
				swprintf_s(buff, L"Elapsed Time: %.6f", device->GetStableElapsedTime());
			}

			if (buff[0] != L'\0')
			{
				m_uiManager.AddText(
					buff,
					UIAnchor::TopLeft,
					{ 20.0f, 20.0f },
					{ 300.0f, 70.0f },
					UITextAlign::Left);
			}
		}

		// 右上：ベンチマーク開始/終了通知
		{
			auto& benchmark = BenchmarkRecorder::Instance();
			if (benchmark.HasNotification())
			{
				m_uiManager.AddText(
					benchmark.GetNotificationText(),
					UIAnchor::TopRight,
					{ -20.0f, 20.0f },
					{ 360.0f, 36.0f },
					UITextAlign::Right,
					D2D1::ColorF(1.0f, 0.88f, 0.18f, 1.0f),
					22.0f);
			}
		}

		// 右上：撃破数
		//{
		//	wchar_t buff[128];
		//	swprintf_s(
		//		buff,
		//		L"Kills  %d / %d",
		//		gameStage->GetDefeatedEnemyCount(),
		//		gameStage->GetTotalEnemyCount());

		//	m_uiManager.AddText(
		//		buff,
		//		UIAnchor::TopRight,
		//		{ -20.0f, 20.0f },
		//		{ 260.0f, 40.0f },
		//		UITextAlign::Right);
		//}

		// 下中央：HPゲージ
		if (hp)
		{
			wchar_t hpLabel[128];
			swprintf_s(hpLabel, L"HP  %d / %d", hp->GetHP(), hp->GetMaxHP());

			m_uiManager.AddProgressBar(
				hpLabel,
				static_cast<float>(hp->GetHP()),
				static_cast<float>(hp->GetMaxHP()),
				UIAnchor::BottomCenter,
				{ 0.0f, -48.0f },
				{ 320.0f, 28.0f });
		}

		// 左下：現在所持している爆弾数
		if (player)
		{
			const std::wstring bombCountText =
				L"x " + std::to_wstring(player->GetBombAmmo());

			m_uiManager.AddImage(
				App::GetRelativeAssetsDir() + kBombHudIconPath,
				UIAnchor::BottomLeft,
				{ kBombHudMargin, -kBombHudMargin },
				{ kBombHudIconSize, kBombHudIconSize });

			// 数字へ影を付け、明暗の異なるステージ背景でも読み取れるようにする。
			m_uiManager.AddText(
				bombCountText,
				UIAnchor::BottomLeft,
				{ kBombHudMargin + kBombHudIconSize + 10.0f, -kBombHudMargin + 2.0f },
				{ 100.0f, kBombHudIconSize },
				UITextAlign::Left,
				D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.85f),
				30.0f);
			m_uiManager.AddText(
				bombCountText,
				UIAnchor::BottomLeft,
				{ kBombHudMargin + kBombHudIconSize + 8.0f, -kBombHudMargin },
				{ 100.0f, kBombHudIconSize },
				UITextAlign::Left,
				D2D1::ColorF(1.0f, 0.88f, 0.18f, 1.0f),
				30.0f);
		}


		// ダメージ数
		if (auto camera = gameStage->GetCamera())
		{
			const float screenW = uiLayer->GetWidth();
			const float screenH = uiLayer->GetHeight();
			auto view = (XMMATRIX)((Mat4x4)camera->GetViewMatrix());
			auto proj = (XMMATRIX)((Mat4x4)camera->GetProjMatrix());
			auto world = XMMatrixIdentity();

			for (const auto& damageNumber : gameStage->GetDamageNumbers())
			{
				auto projected = XMVector3Project(
					(XMVECTOR)damageNumber.position,
					0.0f,
					0.0f,
					screenW,
					screenH,
					0.0f,
					1.0f,
					proj,
					view,
					world);

				XMFLOAT3 screenPos;
				XMStoreFloat3(&screenPos, projected);
				if (screenPos.z < 0.0f || screenPos.z > 1.0f)
				{
					continue;
				}
				if (screenPos.x < -100.0f || screenPos.x > screenW + 100.0f ||
					screenPos.y < -60.0f || screenPos.y > screenH + 60.0f)
				{
					continue;
				}

				const float alpha = damageNumber.GetAlpha();
				const float width = 96.0f;
				const float height = 34.0f;
				const float left = screenPos.x - width * 0.5f;
				const float top = screenPos.y - height * 0.5f;

				m_uiManager.AddText(
					damageNumber.text,
					UIAnchor::TopLeft,
					{ left + 2.0f, top + 2.0f },
					{ width, height },
					UITextAlign::Center,
					D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f * alpha));

				m_uiManager.AddText(
					damageNumber.text,
					UIAnchor::TopLeft,
					{ left, top },
					{ width, height },
					UITextAlign::Center,
					D2D1::ColorF(1.0f, 0.82f, 0.16f, alpha));
			}
		}
		DrawOptionButton();
		uiLayer->SetCrosshairEnabled(true);
		RenderUIWithTransition(*uiLayer);
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


