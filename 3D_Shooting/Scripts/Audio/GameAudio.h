#pragma once

#include "Audio/AudioEngine.h"

#include <map>
#include <string>

namespace shooting {

	enum class GameSoundId
	{
		PlayerShot,
		BombThrow,
		BombExplode,
		ItemPickup,
		PlayerDamage,
		EnemyDamage,
		WaveStart,
		CursorMove,
		Decide,
		Cancel,
	};

	enum class GameBgmId
	{
		Title,
		InGame,
	};

	class GameAudio final
	{
	public:
		static GameAudio& Instance();

		bool Initialize();
		void Shutdown();
		void Update();

		void LoadDefaultSounds();
		bool RegisterSound(GameSoundId id, const std::wstring& path);
		bool RegisterBgm(GameBgmId id, const std::wstring& path);

		SoundInstanceId PlaySound(GameSoundId id, float volumeScale = 1.0f, float pitch = 1.0f);
		SoundInstanceId PlayBgm(GameBgmId id, float volumeScale = 1.0f);
		void StopBgm();
		void StopAll();

		void SetMasterVolume(float volume);
		void SetSeVolume(float volume);
		void SetBgmVolume(float volume);

		bool IsEnabled() const { return m_enabled; }

	private:
		GameAudio() = default;

		float Clamp01(float value) const;
		std::wstring ResolveAudioPath(const wchar_t* category, const wchar_t* fileName) const;
		bool RegisterSoundIfExists(GameSoundId id, const wchar_t* category, const wchar_t* fileName);
		bool RegisterBgmIfExists(GameBgmId id, const wchar_t* category, const wchar_t* fileName);

		AudioEngine m_engine;
		std::map<GameSoundId, AudioClipPtr> m_soundClips;
		std::map<GameBgmId, AudioClipPtr> m_bgmClips;
		SoundInstanceId m_currentBgm = 0;
		bool m_enabled = false;
		float m_masterVolume = 1.0f;
		float m_seVolume = 1.0f;
		float m_bgmVolume = 0.7f;
	};

}
