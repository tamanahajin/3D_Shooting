#pragma once

#include "stdafx.h"

namespace shooting {

	/*!
	@brief ステージグリッド上へ手動配置する自然物1件分のデータ
	*/
	struct StagePropPlacement
	{
		int row = 0;
		int column = 0;
		std::wstring modelName;
		float yRotationDegrees = 0.0f;
		int subRow = 1;
		int subColumn = 1;
	};

	const int kStagePropSubcellCount = 3;

	/*!
	@brief 3x3サブセル番号から、親セル中心を基準にしたXZオフセットを求める

	subRowは画面上から下、subColumnは画面左から右の順で0～2を使用する。
	*/
	Vec3 CalculateStagePropSubcellOffset(
		int subRow,
		int subColumn,
		float parentCellSize);

	/*!
	@brief ステージ配置物CSVの読み書きを担当する

	エディタとゲーム本体で同じ形式を使うため、CSVの解析処理をこのクラスへ集約する。
	*/
	class StagePropPlacementFile
	{
	public:
		static const wchar_t* GetRelativePath();
		static bool Load(
			const std::wstring& path,
			std::vector<StagePropPlacement>& outPlacements,
			std::string& outErrorMessage);
		static bool Save(
			const std::wstring& path,
			const std::vector<StagePropPlacement>& placements,
			std::string& outErrorMessage);
	};

}
