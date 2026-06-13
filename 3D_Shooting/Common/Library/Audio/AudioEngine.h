#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <xaudio2.h>

namespace shooting {

	class AudioClip;
	using AudioClipPtr = std::shared_ptr<AudioClip>;
	using SoundInstanceId = std::uint64_t;

	/**
	 * @brief 同時再生数が上限に達したときの音声の保護優先度
	 *
	 * 低優先度のSEが大量に発生しても、爆発音や死亡音などの重要な音を
	 * 途中で停止しないために使用する。
	 */
	enum class AudioPriority : std::uint8_t
	{
		Low,
		Normal,
		High,
		Critical,
	};

	struct AudioPlayDesc
	{
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		AudioPriority priority = AudioPriority::Normal;
	};

	class AudioClip final
	{
		friend class AudioEngine;

	public:
		bool IsValid() const { return !m_formatBytes.empty() && !m_audioBytes.empty(); }

	private:
		const WAVEFORMATEX* GetFormat() const;

		std::vector<BYTE> m_formatBytes;
		std::vector<BYTE> m_audioBytes;
	};

	class AudioEngine final
	{
	public:
		AudioEngine() = default;
		~AudioEngine();

		AudioEngine(const AudioEngine&) = delete;
		AudioEngine& operator=(const AudioEngine&) = delete;

		bool Initialize();
		void Shutdown();
		void Update();

		bool IsInitialized() const { return m_xaudio != nullptr && m_masterVoice != nullptr; }

		AudioClipPtr LoadWaveFile(const std::wstring& path) const;
		SoundInstanceId Play(const AudioClipPtr& clip, const AudioPlayDesc& desc = AudioPlayDesc());

		void Stop(SoundInstanceId id);
		void StopAll();
		void SetVolume(SoundInstanceId id, float volume);
		void SetMasterVolume(float volume);
		void SetMaxActiveVoices(size_t maxActiveVoices);

	private:
		struct ActiveVoice
		{
			SoundInstanceId id = 0;
			IXAudio2SourceVoice* voice = nullptr;
			AudioClipPtr clip;
			bool loop = false;
			AudioPriority priority = AudioPriority::Normal;
		};

		SoundInstanceId m_nextVoiceId = 1;
		IXAudio2* m_xaudio = nullptr;
		IXAudio2MasteringVoice* m_masterVoice = nullptr;
		std::vector<ActiveVoice> m_activeVoices;
		size_t m_maxActiveVoices = 32;
	};

}
