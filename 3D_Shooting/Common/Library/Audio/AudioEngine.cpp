#include "stdafx.h"
#include "Audio/AudioEngine.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace shooting {
	namespace
	{
		bool IsFourCc(const char actual[4], const char expected[4])
		{
			return std::memcmp(actual, expected, 4) == 0;
		}

		bool ReadBytes(std::ifstream& file, void* dst, std::streamsize size)
		{
			file.read(static_cast<char*>(dst), size);
			return file.good();
		}

		float Clamp01(float value)
		{
			if (value < 0.0f) return 0.0f;
			if (value > 1.0f) return 1.0f;
			return value;
		}

		float ClampPitch(float value)
		{
			if (value < XAUDIO2_MIN_FREQ_RATIO) return XAUDIO2_MIN_FREQ_RATIO;
			if (value > XAUDIO2_MAX_FREQ_RATIO) return XAUDIO2_MAX_FREQ_RATIO;
			return value;
		}
	}

	const WAVEFORMATEX* AudioClip::GetFormat() const
	{
		return reinterpret_cast<const WAVEFORMATEX*>(m_formatBytes.data());
	}

	AudioEngine::~AudioEngine()
	{
		Shutdown();
	}

	bool AudioEngine::Initialize()
	{
		if (IsInitialized())
		{
			return true;
		}

		HRESULT hr = XAudio2Create(&m_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr))
		{
			m_xaudio = nullptr;
			return false;
		}

		hr = m_xaudio->CreateMasteringVoice(&m_masterVoice);
		if (FAILED(hr))
		{
			Shutdown();
			return false;
		}

		return true;
	}

	void AudioEngine::Shutdown()
	{
		StopAll();

		if (m_masterVoice)
		{
			m_masterVoice->DestroyVoice();
			m_masterVoice = nullptr;
		}

		if (m_xaudio)
		{
			m_xaudio->Release();
			m_xaudio = nullptr;
		}
	}

	void AudioEngine::Update()
	{
		if (!IsInitialized())
		{
			return;
		}

		// XAudio2のSourceVoiceは、再生終了後も自動では破棄されない。
		// ここで再生が終わったVoiceだけを掃除して、短いSEの鳴らしっぱなしによるメモリ増加を防ぐ。
		auto it = m_activeVoices.begin();
		while (it != m_activeVoices.end())
		{
			XAUDIO2_VOICE_STATE state = {};
			it->voice->GetState(&state);

			if (state.BuffersQueued == 0)
			{
				it->voice->DestroyVoice();
				it = m_activeVoices.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	AudioClipPtr AudioEngine::LoadWaveFile(const std::wstring& path) const
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return nullptr;
		}

		char riff[4] = {};
		std::uint32_t riffSize = 0;
		char wave[4] = {};
		if (!ReadBytes(file, riff, sizeof(riff)) ||
			!ReadBytes(file, &riffSize, sizeof(riffSize)) ||
			!ReadBytes(file, wave, sizeof(wave)))
		{
			return nullptr;
		}

		if (!IsFourCc(riff, "RIFF") || !IsFourCc(wave, "WAVE"))
		{
			return nullptr;
		}

		std::vector<BYTE> formatBytes;
		std::vector<BYTE> audioBytes;

		while (file)
		{
			char chunkId[4] = {};
			std::uint32_t chunkSize = 0;
			if (!ReadBytes(file, chunkId, sizeof(chunkId)) ||
				!ReadBytes(file, &chunkSize, sizeof(chunkSize)))
			{
				break;
			}

			const std::streampos chunkDataStart = file.tellg();
			const std::streamsize maxStreamSize = (std::numeric_limits<std::streamsize>::max)();
			if (static_cast<unsigned long long>(chunkSize) > static_cast<unsigned long long>(maxStreamSize))
			{
				return nullptr;
			}

			if (IsFourCc(chunkId, "fmt "))
			{
				// fmtチャンクはWAVの音声形式そのもの。
				// PCMのfmtチャンクは16byteのことがあるが、XAudio2へ渡す形はWAVEFORMATEXなので、
				// 足りない分を0で埋めて cbSize が無い古いPCM WAVでも安全に扱えるようにする。
				std::vector<BYTE> rawFormat(chunkSize);
				if (!rawFormat.empty() &&
					!ReadBytes(file, rawFormat.data(), static_cast<std::streamsize>(rawFormat.size())))
				{
					return nullptr;
				}

				const size_t formatSize = rawFormat.size() < sizeof(WAVEFORMATEX)
					? sizeof(WAVEFORMATEX)
					: rawFormat.size();
				formatBytes.assign(formatSize, 0);
				std::copy(rawFormat.begin(), rawFormat.end(), formatBytes.begin());
			}
			else if (IsFourCc(chunkId, "data"))
			{
				audioBytes.resize(chunkSize);
				if (!audioBytes.empty() &&
					!ReadBytes(file, audioBytes.data(), static_cast<std::streamsize>(audioBytes.size())))
				{
					return nullptr;
				}
			}

			// WAVチャンクは偶数バイト境界にそろう。未知チャンクも含めて、必ず次のチャンク先頭へ進める。
			const std::streamoff paddedSize = static_cast<std::streamoff>(chunkSize + (chunkSize & 1));
			file.seekg(chunkDataStart + paddedSize);
		}

		if (formatBytes.empty() || audioBytes.empty())
		{
			return nullptr;
		}

		auto clip = std::make_shared<AudioClip>();
		clip->m_formatBytes = std::move(formatBytes);
		clip->m_audioBytes = std::move(audioBytes);
		return clip;
	}

	SoundInstanceId AudioEngine::Play(const AudioClipPtr& clip, const AudioPlayDesc& desc)
	{
		if (!IsInitialized() || !clip || !clip->IsValid())
		{
			return 0;
		}

		Update();

		if (m_activeVoices.size() >= m_maxActiveVoices && !m_activeVoices.empty())
		{
			// 低優先度かつ古いSEから停止する。
			// 新しい音より重要なVoiceしか残っていない場合は、重要な音を守るため新規再生を諦める。
			auto disposableIt = std::min_element(
				m_activeVoices.begin(),
				m_activeVoices.end(),
				[](const ActiveVoice& lhs, const ActiveVoice& rhs)
				{
					if (lhs.loop != rhs.loop)
					{
						return !lhs.loop;
					}
					return lhs.priority < rhs.priority;
				});

			if (disposableIt == m_activeVoices.end() ||
				disposableIt->loop ||
				disposableIt->priority > desc.priority)
			{
				return 0;
			}

			disposableIt->voice->Stop(0);
			disposableIt->voice->FlushSourceBuffers();
			disposableIt->voice->DestroyVoice();
			m_activeVoices.erase(disposableIt);
		}

		IXAudio2SourceVoice* sourceVoice = nullptr;
		HRESULT hr = m_xaudio->CreateSourceVoice(&sourceVoice, clip->GetFormat());
		if (FAILED(hr) || !sourceVoice)
		{
			return 0;
		}

		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = static_cast<UINT32>(clip->m_audioBytes.size());
		buffer.pAudioData = clip->m_audioBytes.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = desc.loop ? XAUDIO2_LOOP_INFINITE : 0;

		hr = sourceVoice->SubmitSourceBuffer(&buffer);
		if (FAILED(hr))
		{
			sourceVoice->DestroyVoice();
			return 0;
		}

		sourceVoice->SetVolume(Clamp01(desc.volume));
		sourceVoice->SetFrequencyRatio(ClampPitch(desc.pitch));

		hr = sourceVoice->Start(0);
		if (FAILED(hr))
		{
			sourceVoice->DestroyVoice();
			return 0;
		}

		const SoundInstanceId id = m_nextVoiceId++;
		m_activeVoices.push_back({ id, sourceVoice, clip, desc.loop, desc.priority });
		return id;
	}

	void AudioEngine::Stop(SoundInstanceId id)
	{
		if (id == 0)
		{
			return;
		}

		auto it = std::find_if(m_activeVoices.begin(), m_activeVoices.end(),
			[id](const ActiveVoice& activeVoice)
			{
				return activeVoice.id == id;
			});

		if (it == m_activeVoices.end())
		{
			return;
		}

		it->voice->Stop(0);
		it->voice->FlushSourceBuffers();
		it->voice->DestroyVoice();
		m_activeVoices.erase(it);
	}

	void AudioEngine::StopAll()
	{
		for (auto& activeVoice : m_activeVoices)
		{
			if (activeVoice.voice)
			{
				activeVoice.voice->Stop(0);
				activeVoice.voice->FlushSourceBuffers();
				activeVoice.voice->DestroyVoice();
			}
		}
		m_activeVoices.clear();
	}

	void AudioEngine::SetVolume(SoundInstanceId id, float volume)
	{
		if (id == 0)
		{
			return;
		}

		auto it = std::find_if(m_activeVoices.begin(), m_activeVoices.end(),
			[id](const ActiveVoice& activeVoice)
			{
				return activeVoice.id == id;
			});

		if (it == m_activeVoices.end() || !it->voice)
		{
			return;
		}

		it->voice->SetVolume(Clamp01(volume));
	}

	void AudioEngine::SetMasterVolume(float volume)
	{
		if (m_masterVoice)
		{
			m_masterVoice->SetVolume(Clamp01(volume));
		}
	}

	void AudioEngine::SetMaxActiveVoices(size_t maxActiveVoices)
	{
		m_maxActiveVoices = maxActiveVoices < 1 ? 1 : maxActiveVoices;
		while (m_activeVoices.size() > m_maxActiveVoices)
		{
			// 上限を縮小した場合も、重要度の低いSEから整理する。
			auto disposableIt = std::min_element(
				m_activeVoices.begin(),
				m_activeVoices.end(),
				[](const ActiveVoice& lhs, const ActiveVoice& rhs)
				{
					if (lhs.loop != rhs.loop)
					{
						return !lhs.loop;
					}
					return lhs.priority < rhs.priority;
				});

			if (disposableIt == m_activeVoices.end() || disposableIt->loop)
			{
				break;
			}

			disposableIt->voice->Stop(0);
			disposableIt->voice->FlushSourceBuffers();
			disposableIt->voice->DestroyVoice();
			m_activeVoices.erase(disposableIt);
		}
	}

}
