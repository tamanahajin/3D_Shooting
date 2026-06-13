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

	読み込み完了後にまとめてWaveControllerへ反映することで、
	ファイル途中の不正値による設定の部分適用を防ぐ。
	*/
	struct EnemyWaveConfig
	{
		WaveSettings waveSettings;
		std::map<EnemyKind, EnemyStatus> enemyStatuses;
	};

	/*!
	@brief JSONを敵ステータスとWave設定へ変換・検証するクラス

	汎用JsonLoaderが解析したJSONをゲーム用構造体へ変換する。
	敵・Wave固有のキー名と値の範囲だけをこのクラスで扱う。
	*/
	class EnemyWaveConfigLoader
	{
	public:
		/*!
		@brief JSONファイルから敵・Wave設定を読み込む
		@param path 読み込むJSONファイルのパス
		@param outConfig 読み込み成功時の設定
		@param outError 読み込み失敗時の理由
		@return 読み込みと検証に成功した場合はtrue
		*/
		static bool Load(
			const std::wstring& path,
			EnemyWaveConfig& outConfig,
			std::string& outError);
	};

}
