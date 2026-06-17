#pragma once

#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace shooting {

	/**
	 * @brief 実行中に発生した例外をファイルへ保存する簡易ロガー
	 *
	 * エラー処理中に使用するため、ログ保存に失敗しても例外を外へ出さない。
	 * ログは実行ファイルと同じ場所の ErrorLogs フォルダへ日単位で追記する。
	 */
	class ErrorLogger final
	{
	public:
		/**
		 * @brief エラー内容をログへ追記する
		 * @param source エラーが発生した処理名
		 * @param message エラーメッセージ
		 */
		static void Write(const char* source, const std::string& message) noexcept
		{
			try
			{
				std::lock_guard<std::mutex> lock(GetMutex());

				wchar_t modulePath[MAX_PATH] = {};
				const DWORD modulePathLength =
					GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(_countof(modulePath)));
				if (modulePathLength == 0 || modulePathLength >= _countof(modulePath))
				{
					return;
				}

				const std::filesystem::path moduleFilePath(modulePath);
				const std::filesystem::path logDirectory =
					moduleFilePath.parent_path() / L"ErrorLogs";
				std::error_code errorCode;
				std::filesystem::create_directories(logDirectory, errorCode);
				if (errorCode)
				{
					return;
				}

				SYSTEMTIME localTime = {};
				GetLocalTime(&localTime);

				wchar_t logFileName[MAX_PATH] = {};
				if (swprintf_s(
					logFileName,
					L"Error_%04u%02u%02u.log",
					localTime.wYear,
					localTime.wMonth,
					localTime.wDay) < 0)
				{
					return;
				}
				const std::filesystem::path logPath = logDirectory / logFileName;

				std::ostringstream entry;
				entry << "\r\n[" << localTime.wYear << '-';
				AppendTwoDigits(entry, localTime.wMonth);
				entry << '-';
				AppendTwoDigits(entry, localTime.wDay);
				entry << ' ';
				AppendTwoDigits(entry, localTime.wHour);
				entry << ':';
				AppendTwoDigits(entry, localTime.wMinute);
				entry << ':';
				AppendTwoDigits(entry, localTime.wSecond);
				entry << '.';
				AppendThreeDigits(entry, localTime.wMilliseconds);
				entry
					<< "]\r\n"
					<< "Build: " << GetBuildName() << "\r\n"
					<< "Source: " << (source ? source : "Unknown") << "\r\n"
					<< "Message: " << message << "\r\n";

				const std::string text = entry.str();
				std::ofstream file(
					logPath,
					std::ios::binary | std::ios::app);
				if (!file)
				{
					return;
				}

				file.write(text.data(), static_cast<std::streamsize>(text.size()));
			}
			catch (...)
			{
				// エラー処理中のログ保存失敗で、元の例外処理を妨げない。
			}
		}

		/**
		 * @brief HRESULTを含むエラー内容をログへ追記する
		 * @param source エラーが発生した処理名
		 * @param hr 記録するHRESULT
		 * @param message 補足メッセージ
		 */
		static void WriteHr(const char* source, HRESULT hr, const std::string& message) noexcept
		{
			try
			{
				char hrText[32] = {};
				sprintf_s(hrText, "0x%08X", static_cast<unsigned int>(hr));

				std::string fullMessage = message;
				if (!fullMessage.empty())
				{
					fullMessage += "\r\n";
				}
				fullMessage += "HRESULT: ";
				fullMessage += hrText;
				Write(source, fullMessage);
			}
			catch (...)
			{
				// エラー処理中のログ保存失敗で、元の例外処理を妨げない。
			}
		}

	private:
		static std::mutex& GetMutex() noexcept
		{
			static std::mutex mutex;
			return mutex;
		}

		static const char* GetBuildName() noexcept
		{
#if defined(_DEBUG)
			return "Debug";
#else
			return "Release";
#endif
		}

		static void AppendTwoDigits(std::ostringstream& stream, unsigned int value)
		{
			if (value < 10)
			{
				stream << '0';
			}
			stream << value;
		}

		static void AppendThreeDigits(std::ostringstream& stream, unsigned int value)
		{
			if (value < 100)
			{
				stream << '0';
			}
			if (value < 10)
			{
				stream << '0';
			}
			stream << value;
		}
	};

}
