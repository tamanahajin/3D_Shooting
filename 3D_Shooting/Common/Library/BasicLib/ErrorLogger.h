#pragma once

#include <windows.h>

#include <cstdio>
#include <cwchar>
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

				wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
				if (!lastSlash)
				{
					return;
				}
				*(lastSlash + 1) = L'\0';

				wchar_t logDirectory[MAX_PATH] = {};
				if (swprintf_s(logDirectory, L"%sErrorLogs", modulePath) < 0)
				{
					return;
				}

				if (!CreateDirectoryW(logDirectory, nullptr) &&
					GetLastError() != ERROR_ALREADY_EXISTS)
				{
					return;
				}

				SYSTEMTIME localTime = {};
				GetLocalTime(&localTime);

				wchar_t logPath[MAX_PATH] = {};
				if (swprintf_s(
					logPath,
					L"%s\\Error_%04u%02u%02u.log",
					logDirectory,
					localTime.wYear,
					localTime.wMonth,
					localTime.wDay) < 0)
				{
					return;
				}

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
				const HANDLE file = CreateFileW(
					logPath,
					FILE_APPEND_DATA,
					FILE_SHARE_READ,
					nullptr,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					nullptr);
				if (file == INVALID_HANDLE_VALUE)
				{
					return;
				}

				DWORD written = 0;
				WriteFile(
					file,
					text.data(),
					static_cast<DWORD>(text.size()),
					&written,
					nullptr);
				CloseHandle(file);
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
