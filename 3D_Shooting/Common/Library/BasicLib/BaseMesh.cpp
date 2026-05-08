/*!
@file BaseMesh.cpp
@brief メッシュクラス
@copyright Copyright (c) 2022 WiZ Tamura Hiroki,Yamanoi Yasushi.
 MIT License URL: https://opensource.org/license/mit
*/

#include "stdafx.h"
#include <filesystem>
#include <sstream>

namespace shooting {

	//--------------------------------------------------------------------------------------
	///	Assimpローダー
	//--------------------------------------------------------------------------------------

/*


Assimp::Importer importer;
 uint32_t flag = 0;
 flag |= aiProcess_ConvertToLeftHanded;
 flag |= aiProcess_Triangulate;
 importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false); // ←ピボットを読み込まない設定
 auto constScene = importer.ReadFile(modelFileName, flag);
 if (!constScene)
 {
  MessageBox(0, L"aiSceneの読み込みに失敗しました", 0, 0);
 }

*/

	BaseAssimp::BaseAssimp(const std::string& modelFile) :
		m_ModelFile(modelFile)
	{
		try{

			uint32_t flag = ASSIMP_LOAD_FLAGS;
			//flag |= aiProcess_ConvertToLeftHanded;
			flag |= aiProcess_Triangulate;
			m_importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false); // ←ピボットを読み込まない設定

			m_pScene = m_importer.ReadFile(m_ModelFile, flag);
			if (nullptr == m_pScene) {
				throw BaseException(
					L"データの読み込みに失敗しました。",
					L"BaseAssimp::BaseAssimp()"
				);
			}
			//座標変換の逆行列を初期化
			Mat4x4 tmpMat(m_pScene->mRootNode->mTransformation);
			m_GlobalInverseTransform = tmpMat;
			m_GlobalInverseTransform.inverse();

		}
		catch (...) {
			throw;
		}
	}

	bool BaseAssimp::InitSingleScene(UINT meshIndex, std::vector<VertexPositionNormalTextureSkinning>& vertices,
		std::vector<uint32_t>& indices) {

		m_Meshes.resize(m_pScene->mNumMeshes);
		//		m_Materials.resize(pScene->mNumMaterials);

		unsigned int NumVertices = 0;
		unsigned int NumIndices = 0;

		CountVerticesAndIndices(NumVertices, NumIndices);

		ReserveSpace(NumVertices, NumIndices);
		//メッシュを読み込む
		InitSingleMeshBase(meshIndex);

		//if (!InitMaterials(pScene, Filename)) {
		//	return false;
		//}

//		PopulateBuffers();

		int a = 0;

		for (auto& v : m_SkinnedVertices) {
			VertexPositionNormalTextureSkinning tempV;
			tempV.position = v.Position;
			tempV.normal = v.Normal;
			tempV.textureCoordinate = v.TexCoords;
			for (int i = 0; i < MAX_NUM_BONES_PER_VERTEX; i++) {
				tempV.indices[i] = v.Bones.BoneIDs[i];
				tempV.weights[i] = v.Bones.Weights[i];
			}
			vertices.push_back(tempV);
		}
		for (auto& i : m_Indices) {
			indices.push_back(i);
		}
		

		return true;
		//		return GLCheckError();



	}

	bool BaseAssimp::InitMultiScene(std::vector<SkinningMeshSet>& meshSetVec)
	{
		meshSetVec.clear();

		m_Meshes.clear();
		m_AllSkinnedVertices.clear();
		m_AllIndices.clear();
		m_SkinnedVertices.clear();
		m_Indices.clear();

		m_Meshes.resize(m_pScene->mNumMeshes);

		unsigned int NumVertices = 0;
		unsigned int NumIndices = 0;
		CountVerticesAndIndices(NumVertices, NumIndices);
		ReserveSpace(NumVertices, NumIndices);
		InitAllMeshes();

		for (size_t i = 0; i < m_AllSkinnedVertices.size(); i++)
		{
			std::vector<VertexPositionNormalTextureSkinning> vertices;
			std::vector<uint32_t> indices;

			for (const auto& v : m_AllSkinnedVertices[i])
			{
				VertexPositionNormalTextureSkinning tempV{};
				tempV.position = v.Position;
				tempV.normal = v.Normal;
				tempV.textureCoordinate = v.TexCoords;

				for (int j = 0; j < MAX_NUM_BONES_PER_VERTEX; j++)
				{
					tempV.indices[j] = v.Bones.BoneIDs[j];
					tempV.weights[j] = v.Bones.Weights[j];
				}

				vertices.push_back(tempV);
			}

			indices = m_AllIndices[i];

			if (!vertices.empty() && !indices.empty())
			{
				SkinningMeshSet meshSet;
				meshSet.vertices = std::move(vertices);
				meshSet.indices = std::move(indices);
				meshSet.sourceMeshIndex = static_cast<uint32_t>(i);
				meshSetVec.push_back(std::move(meshSet));
			}
		}

		return true;
	}


	bool BaseAssimp::InitMergedScene(
		std::vector<VertexPositionNormalTextureSkinning>& vertices,
		std::vector<uint32_t>& indices)
	{
		// 出力バッファをクリア
		vertices.clear();
		indices.clear();

		// 内部データをすべてクリア
		m_Meshes.clear();
		m_SkinnedVertices.clear();
		m_Indices.clear();
		m_AllSkinnedVertices.clear();
		m_AllIndices.clear();
		m_BoneInfo.clear();
		m_BoneNameToIndexMap.clear();
		m_requiredNodeMap.clear();

		// メッシュ配列を確保
		m_Meshes.resize(m_pScene->mNumMeshes);

		// 全メッシュの頂点数とインデックス数をカウント
		unsigned int NumVertices = 0;
		unsigned int NumIndices = 0;
		CountVerticesAndIndices(NumVertices, NumIndices);
		ReserveSpace(NumVertices, NumIndices);

		// テクスチャ座標のデフォルト値
		const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

		// 全メッシュを結合して単一のメッシュデータを構築
		for (unsigned int meshIndex = 0; meshIndex < m_pScene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* paiMesh = m_pScene->mMeshes[meshIndex];
			// 現在の頂点配列の末尾を基点インデックスとして記録
			const uint32_t baseVertex = static_cast<uint32_t>(m_SkinnedVertices.size());

			// 頂点データの読み込み
			for (unsigned int i = 0; i < paiMesh->mNumVertices; ++i)
			{
				SkinnedVertex v{};

				// 位置座標
				const aiVector3D& pPos = paiMesh->mVertices[i];
				v.Position = Vec3(pPos.x, pPos.y, pPos.z);

				// 法線ベクトル
				if (paiMesh->mNormals)
				{
					const aiVector3D& pNormal = paiMesh->mNormals[i];
					v.Normal = Vec3(pNormal.x, pNormal.y, pNormal.z);
				}
				else
				{
					// 法線がない場合はデフォルトで上向きに設定
					v.Normal = Vec3(0.0f, 1.0f, 0.0f);
				}

				// UV座標(テクスチャ座標)
				const aiVector3D& pTexCoord =
					paiMesh->HasTextureCoords(0) ? paiMesh->mTextureCoords[0][i] : Zero3D;

				// FBX/Assimpから取得したUVは上下が反転している場合があるため、Vを反転
				v.TexCoords = Vec2(pTexCoord.x, 1.0f - pTexCoord.y);
				m_SkinnedVertices.push_back(v);
			}

			// インデックスデータの読み込み
			for (unsigned int i = 0; i < paiMesh->mNumFaces; ++i)
			{
				const aiFace& Face = paiMesh->mFaces[i];
				// baseVertexを加算してグローバルインデックスに変換
				m_Indices.push_back(baseVertex + Face.mIndices[0]);
				m_Indices.push_back(baseVertex + Face.mIndices[1]);
				m_Indices.push_back(baseVertex + Face.mIndices[2]);
			}

			// ボーン情報の読み込み(スキニング用のウェイトとボーンインデックス)
			LoadMeshBones(meshIndex, paiMesh, m_SkinnedVertices, baseVertex);
		}

		// 内部形式からエンジン用の頂点形式に変換
		vertices.reserve(m_SkinnedVertices.size());
		for (const auto& v : m_SkinnedVertices)
		{
			VertexPositionNormalTextureSkinning tempV{};
			tempV.position = v.Position;
			tempV.normal = v.Normal;
			tempV.textureCoordinate = v.TexCoords;

			// ボーンインデックスとウェイトをコピー
			for (int i = 0; i < MAX_NUM_BONES_PER_VERTEX; ++i)
			{
				tempV.indices[i] = v.Bones.BoneIDs[i];
				tempV.weights[i] = v.Bones.Weights[i];
			}

			vertices.push_back(tempV);
		}

		// インデックスバッファをコピー
		indices = m_Indices;
		return true;
	}


	void BaseAssimp::CountVerticesAndIndices(uint32_t& NumVertices, uint32_t& NumIndices) {
		for (unsigned int i = 0; i < m_Meshes.size(); i++) {
			m_Meshes[i].MaterialIndex = m_pScene->mMeshes[i]->mMaterialIndex;
			m_Meshes[i].NumIndices = m_pScene->mMeshes[i]->mNumFaces * 3;
			m_Meshes[i].BaseVertex = NumVertices;
			m_Meshes[i].BaseIndex = NumIndices;

			NumVertices += m_pScene->mMeshes[i]->mNumVertices;
			NumIndices += m_Meshes[i].NumIndices;
		}

	}


	void BaseAssimp::InitSingleMeshBase(UINT meshIndex) {
		if (m_Meshes.size() == 0) {
			throw BaseException(
				L"メッシュが見つかりません",
				L"BaseAssimp::InitSingleMeshBase()"
			);
		}
		const aiMesh* paiMesh = m_pScene->mMeshes[meshIndex];
		m_SkinnedVertices.clear();
		m_Indices.clear();
		InitSingleMesh(meshIndex, paiMesh);
	}


	void BaseAssimp::InitAllMeshes() {
		for (unsigned int i = 0; i < m_Meshes.size(); i++) {
			const aiMesh* paiMesh = m_pScene->mMeshes[i];
			m_SkinnedVertices.clear();
			m_Indices.clear();
			InitSingleMesh(i, paiMesh);
			if (m_SkinnedVertices.size() > 0 && m_Indices.size() > 0) {
				m_AllSkinnedVertices.push_back(m_SkinnedVertices);
				m_AllIndices.push_back(m_Indices);
			}
		}
	}

	void BaseAssimp::InitializeRequiredNodeMap(const aiNode* pNode)
	{
		std::string NodeName(pNode->mName.C_Str());

		NodeInfo info(pNode);

		m_requiredNodeMap[NodeName] = info;

		for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
			InitializeRequiredNodeMap(pNode->mChildren[i]);
		}
	}

	void BaseAssimp::ReserveSpace(unsigned int NumVertices, unsigned int NumIndices)
	{
		m_Vertices.reserve(NumVertices);
		m_Indices.reserve(NumIndices);
		InitializeRequiredNodeMap(m_pScene->mRootNode);
	}



	int BaseAssimp::GetBoneId(const aiBone* pBone)
	{
		int BoneIndex = 0;
		std::string BoneName(pBone->mName.C_Str());

		if (m_BoneNameToIndexMap.find(BoneName) == m_BoneNameToIndexMap.end()) {
			// Allocate an index for a new bone
			BoneIndex = (int)m_BoneNameToIndexMap.size();
			m_BoneNameToIndexMap[BoneName] = BoneIndex;
		}
		else {
			BoneIndex = m_BoneNameToIndexMap[BoneName];
		}

		return BoneIndex;
	}

	void BaseAssimp::MarkRequiredNodesForBone(const aiBone* pBone)
	{
		std::string NodeName(pBone->mName.C_Str());

		const aiNode* pParent = NULL;

		do {
			std::map<std::string, NodeInfo>::iterator it = m_requiredNodeMap.find(NodeName);

			if (it == m_requiredNodeMap.end()) {
				printf("Cannot find bone %s in the hierarchy\n", NodeName.c_str());
				assert(0);
			}

			it->second.isRequired = true;

			pParent = it->second.pNode->mParent;

			if (pParent) {
				NodeName = std::string(pParent->mName.C_Str());
			}

		} while (pParent);
	}



	void BaseAssimp::LoadSingleBone(uint32_t MeshIndex, const aiBone* pBone, std::vector<SkinnedVertex>& SkinnedVertices, int BaseVertex)
	{
		int BoneId = GetBoneId(pBone);


		if (BoneId == m_BoneInfo.size()) {
			BoneInfo bi(pBone->mOffsetMatrix);
			// bi.OffsetMatrix.Print();
			m_BoneInfo.push_back(bi);
		}

		for (uint32_t i = 0; i < pBone->mNumWeights; i++) {
			const aiVertexWeight& vw = pBone->mWeights[i];
			uint32_t GlobalVertexID = BaseVertex + pBone->mWeights[i].mVertexId;
			// printf("%d: %d %f\n",i, pBone->mWeights[i].mVertexId, vw.mWeight);
			SkinnedVertices[GlobalVertexID].Bones.AddBoneData(BoneId, vw.mWeight);
		}

		MarkRequiredNodesForBone(pBone);
	}


	void BaseAssimp::LoadMeshBones(uint32_t MeshIndex, const aiMesh* pMesh, std::vector<SkinnedVertex>& SkinnedVertices, int BaseVertex)
	{
		if (pMesh->mNumBones > MAX_BONES) {
			printf("The number of bones in the model (%d) is larger than the maximum supported (%d)\n", pMesh->mNumBones, MAX_BONES);
			printf("Make sure to increase the macro MAX_BONES in the C++ header as well as in the shader to the same value\n");
			assert(0);
		}

		// printf("Loading mesh bones %d\n", MeshIndex);
		for (uint32_t i = 0; i < pMesh->mNumBones; i++) {
			// printf("Bone %d %s\n", i, pMesh->mBones[i]->mName.C_Str());
			LoadSingleBone(MeshIndex, pMesh->mBones[i], SkinnedVertices, BaseVertex);
		}
	}


	std::wstring BaseAssimp::GetMeshTexturePath(uint32_t meshIndex) const
	{
		if (!m_pScene || meshIndex >= m_pScene->mNumMeshes)
		{
			return L"";
		}

		const aiMesh* mesh = m_pScene->mMeshes[meshIndex];
		const unsigned int materialIndex = mesh->mMaterialIndex;

		if (materialIndex >= m_pScene->mNumMaterials)
		{
			return L"";
		}

		const aiMaterial* material = m_pScene->mMaterials[materialIndex];
		aiString texturePath;

		if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == aiReturn_SUCCESS)
		{
			return Util::ToWStringSimple(texturePath.C_Str());
		}

		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
		{
			return Util::ToWStringSimple(texturePath.C_Str());
		}

		return L"";
	}

	Col4 BaseAssimp::GetMeshBaseColor(uint32_t meshIndex) const
	{
		if (!m_pScene || meshIndex >= m_pScene->mNumMeshes)
		{
			return Col4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		const aiMesh* mesh = m_pScene->mMeshes[meshIndex];
		const unsigned int materialIndex = mesh->mMaterialIndex;
		if (materialIndex >= m_pScene->mNumMaterials)
		{
			return Col4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		const aiMaterial* material = m_pScene->mMaterials[materialIndex];
		aiColor4D color;
		if (material->Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS ||
			material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
		{
			return Col4(color.r, color.g, color.b, color.a > 0.0f ? color.a : 1.0f);
		}

		return Col4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	void BaseAssimp::InitSingleMesh(uint32_t MeshIndex, const aiMesh* paiMesh)
	{
		const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

		SkinnedVertex v{};

		for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D& pPos = paiMesh->mVertices[i];
			v.Position = Vec3(pPos.x, pPos.y, pPos.z);

			if (paiMesh->mNormals)
			{
				const aiVector3D& pNormal = paiMesh->mNormals[i];
				v.Normal = Vec3(pNormal.x, pNormal.y, pNormal.z);
			}
			else
			{
				v.Normal = Vec3(0.0f, 1.0f, 0.0f);
			}

			const aiVector3D& pTexCoord =
				paiMesh->HasTextureCoords(0) ? paiMesh->mTextureCoords[0][i] : Zero3D;

			// FBX/Assimp から取得した UV は描画側の座標系と上下が逆になる場合があるため
			// Vを反転して使用する
			v.TexCoords = Vec2(pTexCoord.x, 1.0f - pTexCoord.y);
			m_SkinnedVertices.push_back(v);
		}

		for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& Face = paiMesh->mFaces[i];
			m_Indices.push_back(Face.mIndices[0]);
			m_Indices.push_back(Face.mIndices[1]);
			m_Indices.push_back(Face.mIndices[2]);
		}

		// 複数mesh版では m_SkinnedVertices は「このmeshだけ」なので BaseVertex は 0
		LoadMeshBones(MeshIndex, paiMesh, m_SkinnedVertices, 0);
	}

	void BaseAssimp::InitMultiMesh(uint32_t MeshIndex, const aiMesh* paiMesh)
	{
		const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

		SkinnedVertex v{};

		for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D& pPos = paiMesh->mVertices[i];
			v.Position = Vec3(pPos.x, pPos.y, pPos.z);

			if (paiMesh->mNormals)
			{
				const aiVector3D& pNormal = paiMesh->mNormals[i];
				v.Normal = Vec3(pNormal.x, pNormal.y, pNormal.z);
			}
			else
			{
				v.Normal = Vec3(0.0f, 1.0f, 0.0f);
			}

			const aiVector3D& pTexCoord =
				paiMesh->HasTextureCoords(0) ? paiMesh->mTextureCoords[0][i] : Zero3D;

			v.TexCoords = Vec2(pTexCoord.x, pTexCoord.y);
			m_SkinnedVertices.push_back(v);
		}

		for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& Face = paiMesh->mFaces[i];
			m_Indices.push_back(Face.mIndices[0]);
			m_Indices.push_back(Face.mIndices[1]);
			m_Indices.push_back(Face.mIndices[2]);
		}

		// 複数mesh版では m_SkinnedVertices は「このmeshだけ」なので BaseVertex は 0
		LoadMeshBones(MeshIndex, paiMesh, m_SkinnedVertices, 0);
	}

	bool BaseAssimp::TryGetNodeGlobalTransform(const std::string& nodeName, Mat4x4& outTransform) const
	{
		auto it = m_requiredNodeMap.find(nodeName);
		if (it == m_requiredNodeMap.end() || !it->second.pNode)
		{
			return false;
		}

		std::vector<const aiNode*> chain;
		for (const aiNode* node = it->second.pNode; node; node = node->mParent)
		{
			chain.push_back(node);
		}

		outTransform.identity();
		for (auto rit = chain.rbegin(); rit != chain.rend(); ++rit)
		{
			Mat4x4 local((*rit)->mTransformation);
			outTransform *= local;
		}

		return true;
	}

	void BaseAssimp::GetBoneTransforms(float TimeInSeconds, std::vector<Mat4x4>& Transforms, unsigned int AnimationIndex)
	{
		if (AnimationIndex >= m_pScene->mNumAnimations)
		{
			printf("Invalid animation index %d, max is %d\n", AnimationIndex, m_pScene->mNumAnimations);
			assert(0);
		}

		Mat4x4 Identity;
		Identity.identity();

		float AnimationTimeTicks = CalcAnimationTimeTicks(TimeInSeconds, AnimationIndex);
		const aiAnimation& Animation = *m_pScene->mAnimations[AnimationIndex];

		m_NodeGlobalTransforms.clear();
		ReadNodeHierarchy(AnimationTimeTicks, m_pScene->mRootNode, Identity, Animation);
		Transforms.resize(m_BoneInfo.size());

		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
		{
			Transforms[i] = m_BoneInfo[i].FinalTransformation;
		}
	}

	float BaseAssimp::CalcAnimationTimeTicks(float TimeInSeconds, unsigned int AnimationIndex)
	{
		const double TicksPerSecond =
			(m_pScene->mAnimations[AnimationIndex]->mTicksPerSecond != 0.0)
			? m_pScene->mAnimations[AnimationIndex]->mTicksPerSecond
			: 25.0;

		const double RawDuration = m_pScene->mAnimations[AnimationIndex]->mDuration;
		if (RawDuration <= 0.0)
		{
			return 0.0f;
		}

		// ループ終端の重複フレームを踏まないように、終端を少しだけ手前にする
		const double LoopEpsilon = bsmUtil::Max(1e-3, RawDuration * 1e-4);
		const double EffectiveDuration = bsmUtil::Max(RawDuration - LoopEpsilon, LoopEpsilon);

		const double TimeInTicks = TimeInSeconds * TicksPerSecond;
		const double Tick = fmod(TimeInTicks, EffectiveDuration);

		return static_cast<float>(Tick);
	}

	float BaseAssimp::GetAnimationDurationSeconds(unsigned int AnimationIndex) const
	{
		if (!m_pScene || AnimationIndex >= m_pScene->mNumAnimations)
		{
			return 0.0f;
		}

		const aiAnimation* anim = m_pScene->mAnimations[AnimationIndex];
		const double ticksPerSecond =
			(anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 25.0;

		if (ticksPerSecond <= 0.0)
		{
			return 0.0f;
		}

		return static_cast<float>(anim->mDuration / ticksPerSecond);
	}

	int BaseAssimp::GetAnimationCount() const
	{
		if (!m_pScene)
		{
			return 0;
		}

		return static_cast<int>(m_pScene->mNumAnimations);
	}

	std::wstring BaseAssimp::GetAnimationName(int index) const
	{
		if (!m_pScene)
		{
			return L"";
		}

		if (index < 0 || index >= static_cast<int>(m_pScene->mNumAnimations))
		{
			return L"";
		}

		const aiAnimation* anim = m_pScene->mAnimations[index];
		if (!anim)
		{
			return L"";
		}

		std::string name = anim->mName.C_Str();

		// 名前がないFBXもある
		if (name.empty())
		{
			return L"Anim_" + std::to_wstring(index);
		}

		return std::wstring(name.begin(), name.end());
	}

	void BaseAssimp::ReadNodeHierarchy(float AnimationTimeTicks, const aiNode* pNode, const Mat4x4& ParentTransform, const aiAnimation& Animation)
	{
		std::string NodeName(pNode->mName.data);

		Mat4x4 NodeTransformation(pNode->mTransformation);

		const aiNodeAnim* pNodeAnim = FindNodeAnim(Animation, NodeName);
		
		float AnimationDuration = (float)Animation.mDuration;

		if (pNodeAnim) {
			LocalTransform Transform;
			CalcLocalTransform(Transform, AnimationTimeTicks, pNodeAnim, AnimationDuration);

			Mat4x4 ScalingM;
			ScalingM.scale(Vec3(
				Transform.Scaling.x,
				Transform.Scaling.y,
				Transform.Scaling.z
			));
			//ScalingM.InitScaleTransform(Transform.Scaling.x, Transform.Scaling.y, Transform.Scaling.z);
			//        printf("Scaling %f %f %f\n", Transoform.Scaling.x, Transform.Scaling.y, Transform.Scaling.z);
			Mat4x4 RotationM;
			Quat qt;
			qt.x = Transform.Rotation.x;
			qt.y = Transform.Rotation.y;
			qt.z = Transform.Rotation.z;
			qt.w = Transform.Rotation.w;

			RotationM.rotation(qt);

			//Mat4x4 RotationM = Mat4x4(Transform.Rotation.GetMatrix());

			Mat4x4 TranslationM;
			TranslationM.translation(
				Vec3(Transform.Translation.x,
					Transform.Translation.y,
					Transform.Translation.z
				)
			);
			//TranslationM.InitTranslationTransform(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z);
			//        printf("Translation %f %f %f\n", Transform.Translation.x, Transform.Translation.y, Transform.Translation.z);

			// Combine the above transformations
			NodeTransformation = ScalingM * RotationM * TranslationM;
			//NodeTransformation = TranslationM * RotationM * ScalingM;
			NodeTransformation.transpose();
		}

//		Mat4x4 GlobalTransformation = NodeTransformation * ParentTransform;
		Mat4x4 GlobalTransformation = ParentTransform * NodeTransformation;
		m_NodeGlobalTransforms[NodeName] = GlobalTransformation;

		if (m_BoneNameToIndexMap.find(NodeName) != m_BoneNameToIndexMap.end()) {
			uint32_t BoneIndex = m_BoneNameToIndexMap[NodeName];
			m_BoneInfo[BoneIndex].FinalTransformation = m_GlobalInverseTransform * GlobalTransformation * m_BoneInfo[BoneIndex].OffsetMatrix;
		}

		for (uint32_t i = 0; i < pNode->mNumChildren; i++) {
			ReadNodeHierarchy(AnimationTimeTicks, pNode->mChildren[i], GlobalTransformation, Animation);
		}
	}

	const aiNodeAnim* BaseAssimp::FindNodeAnim(const aiAnimation&
		Animation, const std::string& NodeName)
	{
		for (uint32_t i = 0; i < Animation.mNumChannels; i++) {
			const aiNodeAnim* pNodeAnim = Animation.mChannels[i];

			if (std::string(pNodeAnim->mNodeName.data) == NodeName) {
				return pNodeAnim;
			}
		}

		return NULL;
	}

	void BaseAssimp::CalcLocalTransform(LocalTransform& Transform,
										float AnimationTimeTicks,
										const aiNodeAnim* pNodeAnim,
										float AnimationDuration)
	{
		CalcInterpolatedScaling(Transform.Scaling, AnimationTimeTicks, pNodeAnim, AnimationDuration);
		CalcInterpolatedRotation(Transform.Rotation, AnimationTimeTicks, pNodeAnim, AnimationDuration);
		CalcInterpolatedPosition(Transform.Translation, AnimationTimeTicks, pNodeAnim, AnimationDuration);
	}


	uint32_t BaseAssimp::FindPosition(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		for (uint32_t i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
		{
			float t = (float)pNodeAnim->mPositionKeys[i + 1].mTime;
			if (AnimationTimeTicks < t)
			{
				return i;
			}
		}

		return pNodeAnim->mNumPositionKeys - 1;
	}


	void BaseAssimp::CalcInterpolatedPosition(
		aiVector3D& Out,
		float AnimationTimeTicks,
		const aiNodeAnim* pNodeAnim,
		float AnimationDuration)
	{
		if (pNodeAnim->mNumPositionKeys == 1)
		{
			Out = pNodeAnim->mPositionKeys[0].mValue;
			return;
		}

		const uint32_t keyCount = pNodeAnim->mNumPositionKeys;
		uint32_t i1 = FindPosition(AnimationTimeTicks, pNodeAnim);
		uint32_t i2 = (i1 + 1) % keyCount;

		float t1 = (float)pNodeAnim->mPositionKeys[i1].mTime;
		float t2 = (float)pNodeAnim->mPositionKeys[i2].mTime;

		if (i2 == 0)
		{
			t2 += AnimationDuration;
			if (AnimationTimeTicks < t1)
			{
				AnimationTimeTicks += AnimationDuration;
			}
		}

		float dt = t2 - t1;
		if (dt <= 0.0f)
		{
			Out = pNodeAnim->mPositionKeys[i1].mValue;
			return;
		}

		float t = (AnimationTimeTicks - t1) / dt;
		t = bsmUtil::Clamp(t, 0.0f, 1.0f);

		// root だけ滑らか補間
		if (std::string(pNodeAnim->mNodeName.C_Str()) == "root" && keyCount >= 4)
		{
			uint32_t i0 = (i1 + keyCount - 1) % keyCount;
			uint32_t i3 = (i2 + 1) % keyCount;

			const aiVector3D& p0 = pNodeAnim->mPositionKeys[i0].mValue;
			const aiVector3D& p1 = pNodeAnim->mPositionKeys[i1].mValue;
			const aiVector3D& p2 = pNodeAnim->mPositionKeys[i2].mValue;
			const aiVector3D& p3 = pNodeAnim->mPositionKeys[i3].mValue;

			float t2v = t * t;
			float t3v = t2v * t;

			Out =
				(p1 * 2.0f +
				 (p2 - p0) * t +
				 (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2v +
				 (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3v) * 0.5f;
			return;
		}

		const aiVector3D& Start = pNodeAnim->mPositionKeys[i1].mValue;
		const aiVector3D& End = pNodeAnim->mPositionKeys[i2].mValue;
		Out = Start + t * (End - Start);
	}

	uint32_t BaseAssimp::FindRotation(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		assert(pNodeAnim->mNumRotationKeys > 0);

		for (uint32_t i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
			float t = (float)pNodeAnim->mRotationKeys[i + 1].mTime;
			if (AnimationTimeTicks < t) {
				return i;
			}
		}

		return pNodeAnim->mNumRotationKeys - 1;
	}


	void BaseAssimp::CalcInterpolatedRotation(aiQuaternion& Out,
											  float AnimationTimeTicks,
											  const aiNodeAnim* pNodeAnim,
											  float AnimationDuration)
	{
		// we need at least two values to interpolate...
		if (pNodeAnim->mNumRotationKeys == 1) {
			Out = pNodeAnim->mRotationKeys[0].mValue;
			return;
		}

		uint32_t RotationIndex = FindRotation(AnimationTimeTicks, pNodeAnim);
		uint32_t NextRotationIndex = (RotationIndex + 1) % pNodeAnim->mNumRotationKeys;

		float t1 = (float)pNodeAnim->mRotationKeys[RotationIndex].mTime;
		float t2 = (float)pNodeAnim->mRotationKeys[NextRotationIndex].mTime;

		if (NextRotationIndex == 0)
		{
			t2 += AnimationDuration;
			if (AnimationTimeTicks < t1)
			{
				AnimationTimeTicks += AnimationDuration;
			}
		}

		float DeltaTime = t2 - t1;
		if (DeltaTime <= 0.0f)
		{
			Out = pNodeAnim->mRotationKeys[RotationIndex].mValue;
			return;
		}

		float Factor = (AnimationTimeTicks - t1) / DeltaTime;
		Factor = bsmUtil::Clamp(Factor, 0.0f, 1.0f);

		aiQuaternion StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
		aiQuaternion EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;

		// 最短経路になるように符号をそろえる
		float dot =
			StartRotationQ.x * EndRotationQ.x +
			StartRotationQ.y * EndRotationQ.y +
			StartRotationQ.z * EndRotationQ.z +
			StartRotationQ.w * EndRotationQ.w;

		if (dot < 0.0f)
		{
			EndRotationQ.x = -EndRotationQ.x;
			EndRotationQ.y = -EndRotationQ.y;
			EndRotationQ.z = -EndRotationQ.z;
			EndRotationQ.w = -EndRotationQ.w;
		}

		aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
		Out.Normalize();
	}


	uint32_t BaseAssimp::FindScaling(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		assert(pNodeAnim->mNumScalingKeys > 0);

		for (uint32_t i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
			float t = (float)pNodeAnim->mScalingKeys[i + 1].mTime;
			if (AnimationTimeTicks < t) {
				return i;
			}
		}

		return pNodeAnim->mNumScalingKeys - 1;
	}


	void BaseAssimp::CalcInterpolatedScaling(aiVector3D& Out,
											 float AnimationTimeTicks,
											 const aiNodeAnim* pNodeAnim,
											 float AnimationDuration)
	{
		// we need at least two values to interpolate...
		if (pNodeAnim->mNumScalingKeys == 1) {
			Out = pNodeAnim->mScalingKeys[0].mValue;
			return;
		}

		uint32_t ScalingIndex = FindScaling(AnimationTimeTicks, pNodeAnim);
		uint32_t NextScalingIndex = (ScalingIndex + 1) % pNodeAnim->mNumScalingKeys;

		float t1 = (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime;
		float t2 = (float)pNodeAnim->mScalingKeys[NextScalingIndex].mTime;

		if (NextScalingIndex == 0)
		{
			t2 += AnimationDuration;
			if (AnimationTimeTicks < t1)
			{
				AnimationTimeTicks += AnimationDuration;
			}
		}

		float DeltaTime = t2 - t1;
		if (DeltaTime <= 0.0f)
		{
			Out = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
			return;
		}

		float Factor = (AnimationTimeTicks - t1) / DeltaTime;
		Factor = bsmUtil::Clamp(Factor, 0.0f, 1.0f);

		const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
		const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
		Out = Start + Factor * (End - Start);
	}

	//--------------------------------------------------------------------------------------
	///	メッシュ
	//--------------------------------------------------------------------------------------
	std::shared_ptr<BaseMesh> BaseMesh::CreateSquare(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateSquare(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateCube(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateCube(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateSphere(ID3D12GraphicsCommandList* pCommandList, float diameter, size_t tessellation) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateSphere(diameter, tessellation, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateCapsule(ID3D12GraphicsCommandList* pCommandList, float diameter, float height, size_t tessellation) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		XMFLOAT3 pointA(0, -height / 2.0f, 0);
		XMFLOAT3 pointB(0, height / 2.0f, 0);
		//Capsuleの作成(ヘルパー関数を利用)
		MeshUtill::CreateCapsule(diameter, pointA, pointB, tessellation, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateCylinder(ID3D12GraphicsCommandList* pCommandList, float height, float diameter, size_t tessellation) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateCylinder(height, diameter, tessellation, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateCone(ID3D12GraphicsCommandList* pCommandList, float diameter, float height, size_t tessellation) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateCone(diameter,height,tessellation,vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateTorus(ID3D12GraphicsCommandList* pCommandList, float diameter, float thickness, size_t tessellation) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateTorus( diameter, thickness, tessellation,vertices,indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateTetrahedron(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateTetrahedron(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateOctahedron(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateOctahedron(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateDodecahedron(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateDodecahedron(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateIcosahedron(ID3D12GraphicsCommandList* pCommandList, float size) {
		std::vector<VertexPositionNormalTexture> vertices;
		std::vector<uint32_t> indices;
		MeshUtill::CreateIcosahedron(size, vertices, indices);
		return BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(pCommandList, vertices, indices);
	}


	std::shared_ptr<BaseMesh> BaseMesh::CreateSingleBoneModelMesh(
		ID3D12GraphicsCommandList* pCommandList,
		const std::wstring& dataDir, 
		const std::wstring& dataFile,
		UINT modelIndex)
	{
		try {
			std::wstring modelFile = dataDir + dataFile;
			if (modelFile.size() > 0) {
				std::string mbModelFile;
				Util::WStoMB(modelFile, mbModelFile);

				std::shared_ptr<BaseAssimp> ptrBaseAssimp = std::shared_ptr<BaseAssimp>(new BaseAssimp(mbModelFile));

				std::vector<VertexPositionNormalTextureSkinning> vertices;
				std::vector<uint32_t> indices;
				ptrBaseAssimp->InitSingleScene(modelIndex,vertices, indices);

				//std::vector < SkinningMeshSet> meshVec;
				//ptrBaseAssimp->InitMuliScene(meshVec);

				if (vertices.size() > 0 && indices.size() > 0) {
					std::shared_ptr<BaseMesh> mesh = BaseMesh::CreateBaseMesh<VertexPositionNormalTextureSkinning>(pCommandList, vertices, indices);
					mesh->m_BaseAssimp = ptrBaseAssimp;


					return mesh;
				}
				else {
					return nullptr;
				}


			}
		}
		catch (...) {
			throw;
		}
		return nullptr;
	}

	std::vector<std::shared_ptr<BaseMesh>> BaseMesh::CreateModelMesh(
		ID3D12GraphicsCommandList* pCommandList,
		const std::wstring& dataDir,
		const std::wstring& dataFile)
	{
		try
		{
			std::vector<std::shared_ptr<BaseMesh>> result;

			const std::wstring modelFile = dataDir + dataFile;
			if (modelFile.empty())
			{
				return result;
			}

			std::string mbModelFile;
			Util::WStoMB(modelFile, mbModelFile);

			std::shared_ptr<BaseAssimp> ptrBaseAssimp =
				std::shared_ptr<BaseAssimp>(new BaseAssimp(mbModelFile));

			std::vector<SkinningMeshSet> meshVec;
			ptrBaseAssimp->InitMultiScene(meshVec);

			for (const auto& meshSet : meshVec)
			{
				if (meshSet.vertices.empty() || meshSet.indices.empty())
				{
					continue;
				}

				std::vector<VertexPositionNormalTexture> staticVertices;
				staticVertices.reserve(meshSet.vertices.size());

				for (const auto& v : meshSet.vertices)
				{
					VertexPositionNormalTexture sv{};
					sv.position = v.position;
					sv.normal = v.normal;
					sv.textureCoordinate = v.textureCoordinate;
					staticVertices.push_back(sv);
				}

				auto mesh = BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(
					pCommandList,
					staticVertices,
					meshSet.indices
				);

				mesh->m_BaseAssimp = ptrBaseAssimp;
				result.push_back(mesh);
			}

			return result;
		}
		catch (...)
		{
			throw;
		}
	}

	std::vector<ModelMaterialPart> BaseMesh::CreateModelMeshWithMaterial(
		ID3D12GraphicsCommandList* pCommandList,
		const std::wstring& dataDir,
		const std::wstring& dataFile)
	{
		try
		{
			std::vector<ModelMaterialPart> result;

			const std::wstring modelFile = dataDir + dataFile;
			if (modelFile.empty())
			{
				return result;
			}

			std::string mbModelFile;
			Util::WStoMB(modelFile, mbModelFile);

			auto ptrBaseAssimp = std::shared_ptr<BaseAssimp>(new BaseAssimp(mbModelFile));

			std::vector<SkinningMeshSet> meshVec;
			ptrBaseAssimp->InitMultiScene(meshVec);

			for (const auto& meshSet : meshVec)
			{
				if (meshSet.vertices.empty() || meshSet.indices.empty())
				{
					continue;
				}

				std::vector<VertexPositionNormalTexture> staticVertices;
				staticVertices.reserve(meshSet.vertices.size());

				for (const auto& src : meshSet.vertices)
				{
					VertexPositionNormalTexture dst{};
					dst.position = src.position;
					dst.normal = src.normal;
					dst.textureCoordinate = src.textureCoordinate;
					staticVertices.push_back(dst);
				}

				auto mesh = BaseMesh::CreateBaseMesh<VertexPositionNormalTexture>(
					pCommandList,
					staticVertices,
					meshSet.indices
				);
				mesh->m_BaseAssimp = ptrBaseAssimp;

				auto material = std::make_shared<BaseMaterial>();
				material->SetBaseColor(ptrBaseAssimp->GetMeshBaseColor(meshSet.sourceMeshIndex));

				const auto relativeTexturePath =
					ptrBaseAssimp->GetMeshTexturePath(meshSet.sourceMeshIndex);
				const auto fullTexturePath =
					Util::ResolveTexturePath(modelFile, relativeTexturePath);

				if (!fullTexturePath.empty())
				{
					try
					{
						auto texture = BaseTexture::CreateTextureFlomFile(
							pCommandList,
							fullTexturePath
						);
						material->SetBaseColorTexture(texture);
					}
					catch (...)
					{
						OutputDebugString(L"[MAT-STATIC] texture load failed\n");
					}
				} 
				ModelMaterialPart part;
				part.mesh = mesh;
				part.material = material;
				result.push_back(std::move(part));
			}

			return result;
		}
		catch (...)
		{
			throw;
		}
	}

	std::vector<ModelSkinnedMaterialPart> BaseMesh::CreateSkinnedModelMeshWithMaterial(
		ID3D12GraphicsCommandList* pCommandList,
		const std::wstring& dataDir,
		const std::wstring& dataFile)
	{
		try
		{
			std::vector<ModelSkinnedMaterialPart> result;

			const std::wstring modelFile = dataDir + dataFile;
			if (modelFile.empty())
			{
				return result;
			}

			std::string mbModelFile;
			Util::WStoMB(modelFile, mbModelFile);

			auto ptrBaseAssimp = std::shared_ptr<BaseAssimp>(new BaseAssimp(mbModelFile));

			std::vector<SkinningMeshSet> meshVec;
			ptrBaseAssimp->InitMultiScene(meshVec);

			for (const auto& meshSet : meshVec)
			{
				if (meshSet.vertices.empty() || meshSet.indices.empty())
				{
					continue;
				}

				auto mesh = BaseMesh::CreateBaseMesh<VertexPositionNormalTextureSkinning>(
					pCommandList,
					meshSet.vertices,
					meshSet.indices
				);
				mesh->m_BaseAssimp = ptrBaseAssimp;

				auto material = std::make_shared<BaseMaterial>();
				material->SetBaseColor(ptrBaseAssimp->GetMeshBaseColor(meshSet.sourceMeshIndex));

				const auto relativeTexturePath =
					ptrBaseAssimp->GetMeshTexturePath(meshSet.sourceMeshIndex);
				const auto fullTexturePath =
					Util::ResolveTexturePath(modelFile, relativeTexturePath);

				if (!fullTexturePath.empty())
				{
					try
					{
						auto texture = BaseTexture::CreateTextureFlomFile(
							pCommandList,
							fullTexturePath
						);
						material->SetBaseColorTexture(texture);
					}
					catch (...)
					{
						OutputDebugString(L"[MAT-SKINNED] texture load failed\n");
					}
				}
				else
				{
					OutputDebugString(L"[MAT-SKINNED] texture path empty\n");
				}

				ModelSkinnedMaterialPart part;
				part.mesh = mesh;
				part.material = material;
				result.push_back(std::move(part));
			}

			return result;
		}
		catch (...)
		{
			throw;
		}
	}

	std::shared_ptr<BaseMesh> BaseMesh::CreateMergedBoneModelMesh(
		ID3D12GraphicsCommandList* pCommandList,
		const std::wstring& dataDir,
		const std::wstring& dataFile)
	{
		try
		{
			std::wstring modelFile = dataDir + dataFile;
			if (modelFile.empty())
			{
				return nullptr;
			}

			std::string mbModelFile;
			Util::WStoMB(modelFile, mbModelFile);

			auto ptrBaseAssimp = std::shared_ptr<BaseAssimp>(new BaseAssimp(mbModelFile));

			std::vector<VertexPositionNormalTextureSkinning> vertices;
			std::vector<uint32_t> indices;
			ptrBaseAssimp->InitMergedScene(vertices, indices);

			if (vertices.empty() || indices.empty())
			{
				return nullptr;
			}

			auto mesh = BaseMesh::CreateBaseMesh<VertexPositionNormalTextureSkinning>(
				pCommandList,
				vertices,
				indices
			);
			mesh->m_BaseAssimp = ptrBaseAssimp;
			return mesh;
		}
		catch (...)
		{
			throw;
		}
	}
}

