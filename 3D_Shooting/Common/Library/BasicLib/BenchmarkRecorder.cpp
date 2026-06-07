#include "stdafx.h"
#include "BenchmarkRecorder.h"

#include <iomanip>
#include <numeric>

namespace shooting {
	namespace
	{
		constexpr const char* kBenchmarkOutputDirectory = "BenchmarkResults";
		constexpr double kBenchmarkNotificationSeconds = 2.0;

		double CalculateAverage(const std::vector<double>& values)
		{
			if (values.empty())
			{
				return 0.0;
			}

			const double total = std::accumulate(values.begin(), values.end(), 0.0);
			return total / static_cast<double>(values.size());
		}

		double CalculateOnePercentLowFps(const std::vector<double>& frameTimesMs)
		{
			if (frameTimesMs.empty())
			{
				return 0.0;
			}

			std::vector<double> worstFrames = frameTimesMs;
			std::sort(worstFrames.begin(), worstFrames.end(), std::greater<double>());

			// 遅い上位1%が対象。
			size_t sampleCount = worstFrames.size() / 100;
			if (sampleCount < 1)
			{
				sampleCount = 1;
			}

			double worstTotalMs = 0.0;
			for (size_t i = 0; i < sampleCount; ++i)
			{
				worstTotalMs += worstFrames[i];
			}

			const double averageWorstFrameMs = worstTotalMs / static_cast<double>(sampleCount);
			return averageWorstFrameMs > 0.0 ? 1000.0 / averageWorstFrameMs : 0.0;
		}

		std::wstring ToWideString(const std::string& text)
		{
			return std::wstring(text.begin(), text.end());
		}
	}

	BenchmarkRecorder& BenchmarkRecorder::Instance()
	{
		static BenchmarkRecorder recorder;
		return recorder;
	}

	BenchmarkRecorder::BenchmarkRecorder()
	{
		ResetRunState();
	}

	void BenchmarkRecorder::Start(double durationSeconds)
	{
		ResetRunState();
		m_IsRunning = true;
		m_TargetDurationSeconds = durationSeconds > 0.0 ? durationSeconds : 30.0;
		ShowNotification(L"ベンチマーク計測開始", kBenchmarkNotificationSeconds);

		std::wostringstream message;
		message << L"Benchmark started: " << m_TargetDurationSeconds << L" sec\n";
		OutputDebugStringW(message.str().c_str());
	}

	void BenchmarkRecorder::Stop(bool writeCsv)
	{
		if (!m_IsRunning && m_FrameCount <= 0)
		{
			return;
		}

		m_IsRunning = false;
		m_LastSummary = BuildSummary();
		if (writeCsv)
		{
			WriteSummaryCsv(m_LastSummary);
		}
		ShowNotification(L"ベンチマーク計測終了", kBenchmarkNotificationSeconds);

		std::wostringstream message;
		message << L"Benchmark stopped. CSV: " << ToWideString(m_LastSummary.outputPath) << L"\n";
		OutputDebugStringW(message.str().c_str());
	}

	void BenchmarkRecorder::Toggle(double durationSeconds)
	{
		if (m_IsRunning)
		{
			Stop(true);
		}
		else
		{
			Start(durationSeconds);
		}
	}

	void BenchmarkRecorder::RecordFrame(double elapsedSeconds, const BenchmarkFrameStats& stats)
	{
		if (!m_IsRunning)
		{
			ResetCurrentFrameCounters();
			return;
		}

		const double safeElapsedSeconds = elapsedSeconds > 0.0 ? elapsedSeconds : 0.0;
		const double frameMs = safeElapsedSeconds * 1000.0;
		m_FrameTimesMs.push_back(frameMs);
		m_ElapsedSeconds += safeElapsedSeconds;
		m_FrameCount++;

		// ScopedBenchmarkTimerがフレーム中に積んだ値を、ここで1フレーム分の記録として確定する。
		for (size_t i = 0; i < m_CurrentSectionMs.size(); ++i)
		{
			m_TotalSectionMs[i] += m_CurrentSectionMs[i];
			if (m_CurrentSectionMs[i] > m_MaxSectionMs[i])
			{
				m_MaxSectionMs[i] = m_CurrentSectionMs[i];
			}
		}

		m_TotalRaycastCount += m_CurrentFrameRaycastCount;
		if (stats.collisionCheckCount > m_MaxCollisionCheckCount)
		{
			m_MaxCollisionCheckCount = stats.collisionCheckCount;
		}
		if (stats.totalEnemyCount > m_MaxTotalEnemyCount)
		{
			m_MaxTotalEnemyCount = stats.totalEnemyCount;
		}
		if (stats.aliveEnemyCount > m_MaxAliveEnemyCount)
		{
			m_MaxAliveEnemyCount = stats.aliveEnemyCount;
		}
		m_LastWave = stats.currentWave;
		ResetCurrentFrameCounters();

		if (m_ElapsedSeconds >= m_TargetDurationSeconds)
		{
			Stop(true);
		}
	}

	void BenchmarkRecorder::AddSectionTime(BenchmarkSection section, double milliseconds)
	{
		if (!m_IsRunning || milliseconds <= 0.0)
		{
			return;
		}

		const size_t index = static_cast<size_t>(section);
		if (index >= m_CurrentSectionMs.size())
		{
			return;
		}

		m_CurrentSectionMs[index] += milliseconds;
	}

	void BenchmarkRecorder::IncrementRaycastCount()
	{
		if (!m_IsRunning)
		{
			return;
		}

		m_CurrentFrameRaycastCount++;
	}

	void BenchmarkRecorder::UpdateNotification(double elapsedSeconds)
	{
		if (m_NotificationSecondsRemaining <= 0.0)
		{
			return;
		}

		m_NotificationSecondsRemaining -= elapsedSeconds > 0.0 ? elapsedSeconds : 0.0;
		if (m_NotificationSecondsRemaining <= 0.0)
		{
			m_NotificationSecondsRemaining = 0.0;
			m_NotificationText.clear();
		}
	}

	void BenchmarkRecorder::ResetRunState()
	{
		m_ElapsedSeconds = 0.0;
		m_FrameCount = 0;
		m_FrameTimesMs.clear();
		m_CurrentSectionMs.fill(0.0);
		m_TotalSectionMs.fill(0.0);
		m_MaxSectionMs.fill(0.0);
		m_CurrentFrameRaycastCount = 0;
		m_TotalRaycastCount = 0;
		m_MaxCollisionCheckCount = 0;
		m_MaxTotalEnemyCount = 0;
		m_MaxAliveEnemyCount = 0;
		m_LastWave = 0;
	}

	void BenchmarkRecorder::ResetCurrentFrameCounters()
	{
		m_CurrentSectionMs.fill(0.0);
		m_CurrentFrameRaycastCount = 0;
	}

	BenchmarkSummary BenchmarkRecorder::BuildSummary() const
	{
		BenchmarkSummary summary;
		summary.buildName = GetBuildName();
		summary.outputPath = MakeOutputPath();
		summary.durationSeconds = m_ElapsedSeconds;
		summary.frameCount = m_FrameCount;

		const double averageFrameMs = CalculateAverage(m_FrameTimesMs);
		const auto maxFrameIt = std::max_element(m_FrameTimesMs.begin(), m_FrameTimesMs.end());
		const double maximumFrameMs = maxFrameIt != m_FrameTimesMs.end() ? *maxFrameIt : 0.0;

		summary.averageFrameMs = averageFrameMs;
		summary.maximumFrameMs = maximumFrameMs;
		summary.averageFps = m_ElapsedSeconds > 0.0 ? static_cast<double>(m_FrameCount) / m_ElapsedSeconds : 0.0;
		summary.minimumFps = maximumFrameMs > 0.0 ? 1000.0 / maximumFrameMs : 0.0;
		summary.onePercentLowFps = CalculateOnePercentLowFps(m_FrameTimesMs);

		const double frameDivisor = m_FrameCount > 0 ? static_cast<double>(m_FrameCount) : 1.0;
		const size_t enemyIndex = static_cast<size_t>(BenchmarkSection::EnemyUpdate);
		const size_t collisionIndex = static_cast<size_t>(BenchmarkSection::Collision);
		summary.averageEnemyUpdateMs = m_TotalSectionMs[enemyIndex] / frameDivisor;
		summary.maximumEnemyUpdateMs = m_MaxSectionMs[enemyIndex];
		summary.averageCollisionMs = m_TotalSectionMs[collisionIndex] / frameDivisor;
		summary.maximumCollisionMs = m_MaxSectionMs[collisionIndex];

		summary.totalRaycastCount = m_TotalRaycastCount;
		summary.maximumCollisionCheckCount = m_MaxCollisionCheckCount;
		summary.maximumTotalEnemyCount = m_MaxTotalEnemyCount;
		summary.maximumAliveEnemyCount = m_MaxAliveEnemyCount;
		summary.lastWave = m_LastWave;
		return summary;
	}

	void BenchmarkRecorder::WriteSummaryCsv(const BenchmarkSummary& summary)
	{
		CreateDirectoryA(kBenchmarkOutputDirectory, nullptr);

		std::ofstream file(summary.outputPath);
		if (!file)
		{
			std::wstring message = L"Benchmark CSV write failed: " + ToWideString(summary.outputPath) + L"\n";
			OutputDebugStringW(message.c_str());
			return;
		}

		file << "Build,計測時間,平均FPS,最低FPS,遅かったフレーム群の平均FPS,平均ms,"
			<< "平均敵更新ms,最大敵更新ms,平均衝突ms,最大衝突ms,"
			<< "Raycast回数,最大衝突回数,敵数,最後のWave\n";

		file << std::fixed << std::setprecision(3)
			<< summary.buildName << ','
			<< summary.durationSeconds << ','
			<< summary.averageFps << ','
			<< summary.minimumFps << ','
			<< summary.onePercentLowFps << ','
			<< summary.averageFrameMs << ','
			<< summary.averageEnemyUpdateMs << ','
			<< summary.maximumEnemyUpdateMs << ','
			<< summary.averageCollisionMs << ','
			<< summary.maximumCollisionMs << ','
			<< summary.totalRaycastCount << ','
			<< summary.maximumCollisionCheckCount << ','
			<< summary.maximumAliveEnemyCount << ','
			<< summary.lastWave << '\n';
	}

	std::string BenchmarkRecorder::MakeOutputPath() const
	{
		SYSTEMTIME time{};
		GetLocalTime(&time);

		char path[MAX_PATH] = {};
		sprintf_s(
			path,
			"%s\\benchmark_%04d%02d%02d_%02d%02d%02d.csv",
			kBenchmarkOutputDirectory,
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond);
		return path;
	}

	std::string BenchmarkRecorder::GetBuildName()
	{
#if defined(_DEBUG)
		return "Debug";
#else
		return "Release";
#endif
	}

	void BenchmarkRecorder::ShowNotification(const std::wstring& text, double durationSeconds)
	{
		m_NotificationText = text;
		m_NotificationSecondsRemaining = durationSeconds > 0.0 ? durationSeconds : 0.0;
	}

	ScopedBenchmarkTimer::ScopedBenchmarkTimer(BenchmarkSection section) :
		m_Section(section),
		m_StartCount{}
	{
		m_IsActive = BenchmarkRecorder::Instance().IsRunning();
		if (m_IsActive)
		{
			QueryPerformanceCounter(&m_StartCount);
		}
	}

	ScopedBenchmarkTimer::~ScopedBenchmarkTimer()
	{
		if (!m_IsActive)
		{
			return;
		}

		LARGE_INTEGER endCount{};
		LARGE_INTEGER frequency{};
		QueryPerformanceCounter(&endCount);
		QueryPerformanceFrequency(&frequency);
		if (frequency.QuadPart <= 0)
		{
			return;
		}

		const double elapsedMs =
			static_cast<double>(endCount.QuadPart - m_StartCount.QuadPart) * 1000.0 /
			static_cast<double>(frequency.QuadPart);
		BenchmarkRecorder::Instance().AddSectionTime(m_Section, elapsedMs);
	}
}
