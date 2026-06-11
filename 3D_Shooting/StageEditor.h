#pragma once

#include "stdafx.h"

namespace shooting {

	class Camera;
	class GameStage;

	/*!
	@brief CSVで構成されたステージグリッドをゲーム内で編集するデバッグ用エディタ

	編集値はメモリ上に保持し、保存時に stage_heights.csv と
	stage_objects.csv へ書き出す。ステージの再生成は Scene 側へ要求する。
	*/
	class StageEditor
	{
	public:
		bool IsActive() const { return m_active; }
		bool IsDirty() const { return m_dirty; }

		bool Enter(GameStage& stage);
		void Exit(GameStage& stage);
		void OnStageReloaded(GameStage& stage);

		/*!
		@brief ステージ編集用ImGuiと3Dグリッドを描画する
		@return ステージ再生成が必要な場合は true
		*/
		bool DrawImGui(GameStage& stage, float screenWidth, float screenHeight);

	private:
		bool Load();
		bool Save();
		void AttachEditorCamera(GameStage& stage);
		void RestoreCamera(GameStage& stage);
		void PickCell(
			const std::shared_ptr<Camera>& camera,
			float screenWidth,
			float screenHeight);
		void DrawGridOverlay(
			const std::shared_ptr<Camera>& camera,
			float screenWidth,
			float screenHeight) const;
		int GetIndex(int row, int column) const;
		bool IsValidCell(int row, int column) const;

		std::vector<int> m_objects;
		std::vector<int> m_heights;
		int m_rowCount = 0;
		int m_columnCount = 0;
		int m_selectedRow = -1;
		int m_selectedColumn = -1;
		bool m_active = false;
		bool m_loaded = false;
		bool m_dirty = false;
		bool m_cameraAttached = false;
		Vec3 m_savedCameraEye = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_savedCameraAt = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_savedCameraUp = Vec3(0.0f, 1.0f, 0.0f);
		std::string m_statusText;
	};

}
