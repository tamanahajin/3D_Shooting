#include "stdafx.h"
#include "Audio/GameAudio.h"

namespace shooting {

	GameAudio& GameAudio::Instance()
	{
		static GameAudio instance;
		return instance;
	}

	bool GameAudio::Initialize()
	{
		if (m_enabled)
		{
			return true;
		}

		m_enabled = m_engine.Initialize();
		m_engine.SetMaxActiveVoices(24);
		m_engine.SetMasterVolume(m_masterVolume);
		return m_enabled;
	}

	void GameAudio::Shutdown()
	{
		StopAll();
		m_soundClips.clear();
		m_bgmClips.clear();
		m_engine.Shutdown();
		m_enabled = false;
	}

	void GameAudio::Update()
	{
		if (!m_enabled)
		{
			return;
		}

		m_engine.Update();
	}

	void GameAudio::LoadDefaultSounds()
	{
		if (!m_enabled)
		{
			return;
		}

		// ゲーム内の呼び出し側にはファイル名を持たせず、ここに「どのイベントでどの音を鳴らすか」を集約する。
		// WAVがまだ無い場合は登録をスキップするので、音素材を段階的に追加しても起動エラーにならない。
		RegisterSoundIfExists(GameSoundId::Wormhole, L"SE", L"wormhole.wav");
		RegisterSoundIfExists(GameSoundId::PlayerShot, L"SE", L"player_shot.wav");
		RegisterSoundIfExists(GameSoundId::BombThrow, L"SE", L"bomb_throw.wav");
		RegisterSoundIfExists(GameSoundId::BombExplode, L"SE", L"bomb_explode.wav");
		RegisterSoundIfExists(GameSoundId::ItemPickup, L"SE", L"item_pickup.wav");
		RegisterSoundIfExists(GameSoundId::Heal, L"SE", L"heal.wav");
		RegisterSoundIfExists(GameSoundId::PlayerDamage, L"SE", L"player_damage.wav");
		RegisterSoundIfExists(GameSoundId::EnemyDamage, L"SE", L"enemy_damage.wav");
		RegisterSoundIfExists(GameSoundId::WaveStart, L"SE", L"wave_start.wav");

		RegisterSoundIfExists(GameSoundId::CursorMove, L"UI", L"cursor_move.wav");
		RegisterSoundIfExists(GameSoundId::Decide, L"UI", L"decide.wav");
		RegisterSoundIfExists(GameSoundId::Cancel, L"UI", L"cancel.wav");

		RegisterBgmIfExists(GameBgmId::Title, L"BGM", L"title.wav");
		RegisterBgmIfExists(GameBgmId::InGame, L"BGM", L"ingame.wav");
	}

	bool GameAudio::RegisterSound(GameSoundId id, const std::wstring& path)
	{
		if (!m_enabled)
		{
			return false;
		}

		auto clip = m_engine.LoadWaveFile(path);
		if (!clip)
		{
			return false;
		}

		m_soundClips[id] = clip;
		return true;
	}

	bool GameAudio::RegisterBgm(GameBgmId id, const std::wstring& path)
	{
		if (!m_enabled)
		{
			return false;
		}

		auto clip = m_engine.LoadWaveFile(path);
		if (!clip)
		{
			return false;
		}

		m_bgmClips[id] = clip;
		return true;
	}

	SoundInstanceId GameAudio::PlaySound(GameSoundId id, float volumeScale, float pitch)
	{
		if (!m_enabled)
		{
			return 0;
		}

		auto it = m_soundClips.find(id);
		if (it == m_soundClips.end())
		{
			return 0;
		}

		AudioPlayDesc desc;
		desc.volume = Clamp01(m_seVolume * GetSoundDefaultVolume(id) * volumeScale);
		desc.pitch = pitch;
		desc.loop = false;
		return m_engine.Play(it->second, desc);
	}

	SoundInstanceId GameAudio::PlayBgm(GameBgmId id, float volumeScale)
	{
		if (!m_enabled)
		{
			return 0;
		}

		auto it = m_bgmClips.find(id);
		if (it == m_bgmClips.end())
		{
			return 0;
		}

		StopBgm();

		AudioPlayDesc desc;
		desc.volume = Clamp01(m_bgmVolume * GetBgmDefaultVolume(id) * volumeScale);
		desc.pitch = 1.0f;
		desc.loop = true;
		m_currentBgm = m_engine.Play(it->second, desc);
		if (m_currentBgm != 0)
		{
			m_currentBgmId = id;
			m_currentBgmVolumeScale = volumeScale;
		}
		return m_currentBgm;
	}

	void GameAudio::StopBgm()
	{
		if (m_currentBgm != 0)
		{
			m_engine.Stop(m_currentBgm);
			m_currentBgm = 0;
		}
	}

	void GameAudio::StopAll()
	{
		m_currentBgm = 0;
		m_engine.StopAll();
	}

	void GameAudio::SetMasterVolume(float volume)
	{
		m_masterVolume = Clamp01(volume);
		m_engine.SetMasterVolume(m_masterVolume);
	}

	void GameAudio::SetSeVolume(float volume)
	{
		m_seVolume = Clamp01(volume);
	}

	void GameAudio::SetBgmVolume(float volume)
	{
		m_bgmVolume = Clamp01(volume);
		if (m_currentBgm != 0)
		{
			m_engine.SetVolume(
				m_currentBgm,
				Clamp01(m_bgmVolume * GetBgmDefaultVolume(m_currentBgmId) * m_currentBgmVolumeScale));
		}
	}

	float GameAudio::Clamp01(float value) const
	{
		if (value < 0.0f) return 0.0f;
		if (value > 1.0f) return 1.0f;
		return value;
	}

	float GameAudio::GetSoundDefaultVolume(GameSoundId id) const
	{
		switch (id)
		{
		case GameSoundId::Wormhole:
			return 0.50f;
		case GameSoundId::PlayerShot:
			return 0.55f;
		case GameSoundId::BombThrow:
			return 0.60f;
		case GameSoundId::BombExplode:
			return 0.90f;
		case GameSoundId::ItemPickup:
			return 0.65f;
		case GameSoundId::Heal:
			return 0.75f;
		case GameSoundId::PlayerDamage:
			return 0.85f;
		case GameSoundId::EnemyDamage:
			return 0.55f;
		case GameSoundId::WaveStart:
			return 0.75f;
		case GameSoundId::CursorMove:
			return 0.45f;
		case GameSoundId::Decide:
			return 0.65f;
		case GameSoundId::Cancel:
			return 0.60f;
		default:
			return 1.0f;
		}
	}

	float GameAudio::GetBgmDefaultVolume(GameBgmId id) const
	{
		switch (id)
		{
		case GameBgmId::Title:
			return 1.0f;
		case GameBgmId::InGame:
			return 0.55f;
		default:
			return 1.0f;
		}
	}

	std::wstring GameAudio::ResolveAudioPath(const wchar_t* category, const wchar_t* fileName) const
	{
		static const wchar_t* kRoots[] =
		{
			L"..\\3D_Shooting\\Audio\\",
			L"Audio\\",
		};

		for (const auto root : kRoots)
		{
			std::wstring path = root;
			path += category;
			path += L"\\";
			path += fileName;

			if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				return path;
			}
		}

		return std::wstring();
	}

	bool GameAudio::RegisterSoundIfExists(GameSoundId id, const wchar_t* category, const wchar_t* fileName)
	{
		const std::wstring path = ResolveAudioPath(category, fileName);
		if (path.empty())
		{
			return false;
		}

		return RegisterSound(id, path);
	}

	bool GameAudio::RegisterBgmIfExists(GameBgmId id, const wchar_t* category, const wchar_t* fileName)
	{
		const std::wstring path = ResolveAudioPath(category, fileName);
		if (path.empty())
		{
			return false;
		}

		return RegisterBgm(id, path);
	}

}
