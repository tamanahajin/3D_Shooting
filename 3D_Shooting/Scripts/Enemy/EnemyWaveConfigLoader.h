/*!
@file EnemyWaveConfigLoader.h
@brief JSONから敵ステータスとWave設定へ変換するローダー
*/

#pragma once
#include "WaveController.h"
#include <map>
#include <string>

namespace shooting {

	/*!
	@brief JSONから読み込んだ敵・Wave設定

	読み込み完了後にまとめて反映することで、ファイル途中の不正値による部分適用を防ぐ。
	*/
	struct EnemyWaveConfig
	{
		WaveSettings waveSettings;
		std::map<EnemyKind, EnemyStatus> enemyStatuses;
	};

	/*!
	@brief 汎用 JsonLoader の結果を敵・Wave 用の構造体へ変換する

	敵・Wave 固有のキー名と値の範囲検証だけをここで扱う。
	*/
	class EnemyWaveConfigLoader
	{
	public:
		/*!
		@brief JSONファイルから敵・Wave設定を読み込み、全項目の検証後に出力へ反映する
		*/
		static bool Load(
			const std::wstring& path,
			EnemyWaveConfig& outConfig,
			std::string& outError);
	};

}
