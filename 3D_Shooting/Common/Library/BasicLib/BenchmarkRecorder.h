#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace shooting {

	enum class BenchmarkSection
	{
		EnemyUpdate = 0,
		Collision,
		Count
	};

	struct BenchmarkFrameStats
	{
		int currentWave = 0;
		int totalEnemyCount = 0;
		int aliveEnemyCount = 0;
		int collisionCheckCount = 0;
	};

	struct BenchmarkSummary
	{
		std::string buildName;
		std::wstring outputPath;
		double durationSeconds = 0.0;
		int frameCount = 0;
		double averageFps = 0.0;
		double minimumFps = 0.0;
		double onePercentLowFps = 0.0;
		double averageFrameMs = 0.0;
		double maximumFrameMs = 0.0;
		double averageGpuFrameMs = 0.0;
		double maximumGpuFrameMs = 0.0;
		double onePercentWorstGpuFrameMs = 0.0;
		int gpuFrameCount = 0;
		double averageEnemyUpdateMs = 0.0;
		double maximumEnemyUpdateMs = 0.0;
		double averageCollisionMs = 0.0;
		double maximumCollisionMs = 0.0;
		int totalRaycastCount = 0;
		double averageDrawCallCount = 0.0;
		int maximumDrawCallCount = 0;
		int maximumCollisionCheckCount = 0;
		int maximumTotalEnemyCount = 0;
		int maximumAliveEnemyCount = 0;
		int lastWave = 0;
	};

	class BenchmarkRecorder
	{
	public:
		static BenchmarkRecorder& Instance();

		void Start(double durationSeconds = 30.0);
		void Stop(bool writeCsv = true);
		void Toggle(double durationSeconds = 30.0);
		bool IsRunning() const { return m_IsRunning; }

		void RecordFrame(double elapsedSeconds, const BenchmarkFrameStats& stats);
		void RecordGpuFrameTime(double milliseconds);
		void AddSectionTime(BenchmarkSection section, double milliseconds);
		void IncrementRaycastCount();
		void UpdateNotification(double elapsedSeconds);
		void ClearNotification();
		void CountDrawCall();

		const BenchmarkSummary& GetLastSummary() const { return m_LastSummary; }
		const std::wstring& GetLastOutputPath() const { return m_LastSummary.outputPath; }
		bool HasNotification() const { return m_NotificationSecondsRemaining > 0.0 && !m_NotificationText.empty(); }
		const std::wstring& GetNotificationText() const { return m_NotificationText; }

	private:
		BenchmarkRecorder();

		void ResetRunState();
		void ResetCurrentFrameCounters();
		BenchmarkSummary BuildSummary() const;
		bool WriteSummaryCsv(const BenchmarkSummary& summary);
		std::filesystem::path MakeOutputPath() const;
		static std::string GetBuildName();
		void ShowNotification(const std::wstring& text, double durationSeconds);

		bool m_IsRunning = false;
		double m_TargetDurationSeconds = 30.0;
		double m_ElapsedSeconds = 0.0;
		int m_FrameCount = 0;

		std::vector<double> m_FrameTimesMs;
		std::vector<double> m_GpuFrameTimesMs;
		std::array<double, static_cast<size_t>(BenchmarkSection::Count)> m_CurrentSectionMs;
		std::array<double, static_cast<size_t>(BenchmarkSection::Count)> m_TotalSectionMs;
		std::array<double, static_cast<size_t>(BenchmarkSection::Count)> m_MaxSectionMs;

		int m_CurrentFrameRaycastCount = 0;
		int m_TotalRaycastCount = 0;
		int m_currentFrameDrawCallCount = 0;
		int m_totalDrawCallCount = 0;
		int m_maximumDrawCallCount = 0;
		int m_MaxCollisionCheckCount = 0;
		int m_MaxTotalEnemyCount = 0;
		int m_MaxAliveEnemyCount = 0;
		int m_LastWave = 0;
		std::wstring m_NotificationText;
		double m_NotificationSecondsRemaining = 0.0;
		BenchmarkSummary m_LastSummary;
	};

	class ScopedBenchmarkTimer
	{
	public:
		explicit ScopedBenchmarkTimer(BenchmarkSection section);
		~ScopedBenchmarkTimer();

		ScopedBenchmarkTimer(const ScopedBenchmarkTimer&) = delete;
		ScopedBenchmarkTimer& operator=(const ScopedBenchmarkTimer&) = delete;

	private:
		BenchmarkSection m_Section;
		LARGE_INTEGER m_StartCount;
		bool m_IsActive = false;
	};
}
