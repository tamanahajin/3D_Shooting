/*!
@file JsonLoader.cpp
@brief 汎用JSONファイルローダー実装
*/

#include "stdafx.h"
#include "JsonLoader.h"
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace shooting {

	namespace {

		/*!
		@brief UTF-8 JSON文字列をJsonValueへ変換する再帰下降パーサー

		オブジェクト、配列、文字列、数値、真偽値、nullをJSON仕様に沿って解析する。
		*/
		class JsonParser
		{
		public:
			explicit JsonParser(const std::string& text) :
				m_Text(text)
			{
			}

			bool Parse(JsonValue& outValue, std::string& outError)
			{
				SkipWhitespace();
				if (!ParseValue(outValue))
				{
					outError = BuildError();
					return false;
				}

				SkipWhitespace();
				if (m_Position != m_Text.size())
				{
					SetError("JSONの末尾に解釈できない文字があります。");
					outError = BuildError();
					return false;
				}
				return true;
			}

		private:
			const std::string& m_Text;
			size_t m_Position = 0;
			size_t m_ErrorPosition = 0;
			std::string m_Error;

			void SkipWhitespace()
			{
				while (m_Position < m_Text.size())
				{
					const char c = m_Text[m_Position];
					if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
					{
						break;
					}
					++m_Position;
				}
			}

			void SetError(const std::string& message)
			{
				if (m_Error.empty())
				{
					m_Error = message;
					m_ErrorPosition = m_Position;
				}
			}

			std::string BuildError() const
			{
				size_t line = 1;
				size_t column = 1;
				const size_t end = m_ErrorPosition < m_Text.size() ?
					m_ErrorPosition : m_Text.size();
				for (size_t i = 0; i < end; ++i)
				{
					if (m_Text[i] == '\n')
					{
						++line;
						column = 1;
					}
					else
					{
						++column;
					}
				}

				std::ostringstream stream;
				stream << m_Error << " (" << line << "行 " << column << "列)";
				return stream.str();
			}

			bool ParseValue(JsonValue& outValue)
			{
				SkipWhitespace();
				if (m_Position >= m_Text.size())
				{
					SetError("JSON値が必要です。");
					return false;
				}

				switch (m_Text[m_Position])
				{
				case '{':
					return ParseObject(outValue);
				case '[':
					return ParseArray(outValue);
				case '"':
				{
					std::string value;
					if (!ParseString(value))
					{
						return false;
					}
					outValue = JsonValue::CreateString(value);
					return true;
				}
				case 't':
					return ParseLiteral("true", JsonValue::CreateBoolean(true), outValue);
				case 'f':
					return ParseLiteral("false", JsonValue::CreateBoolean(false), outValue);
				case 'n':
					return ParseLiteral("null", JsonValue(), outValue);
				default:
					if (m_Text[m_Position] == '-' ||
						(m_Text[m_Position] >= '0' && m_Text[m_Position] <= '9'))
					{
						return ParseNumber(outValue);
					}
					SetError("JSON値の先頭文字が不正です。");
					return false;
				}
			}

			bool ParseObject(JsonValue& outValue)
			{
				++m_Position;
				SkipWhitespace();

				JsonValue::Object object;
				if (Consume('}'))
				{
					outValue = JsonValue::CreateObject(object);
					return true;
				}

				while (m_Position < m_Text.size())
				{
					std::string key;
					if (!ParseString(key))
					{
						return false;
					}
					if (object.find(key) != object.end())
					{
						SetError("同じJSONキーが複数定義されています: " + key);
						return false;
					}

					SkipWhitespace();
					if (!Consume(':'))
					{
						SetError("JSONオブジェクトのキーの後に ':' が必要です。");
						return false;
					}

					JsonValue value;
					if (!ParseValue(value))
					{
						return false;
					}
					object.insert(std::make_pair(key, value));

					SkipWhitespace();
					if (Consume('}'))
					{
						outValue = JsonValue::CreateObject(object);
						return true;
					}
					if (!Consume(','))
					{
						SetError("JSONオブジェクトの要素間に ',' が必要です。");
						return false;
					}
					SkipWhitespace();
				}

				SetError("JSONオブジェクトが閉じられていません。");
				return false;
			}

			bool ParseArray(JsonValue& outValue)
			{
				++m_Position;
				SkipWhitespace();

				JsonValue::Array array;
				if (Consume(']'))
				{
					outValue = JsonValue::CreateArray(array);
					return true;
				}

				while (m_Position < m_Text.size())
				{
					JsonValue value;
					if (!ParseValue(value))
					{
						return false;
					}
					array.push_back(value);

					SkipWhitespace();
					if (Consume(']'))
					{
						outValue = JsonValue::CreateArray(array);
						return true;
					}
					if (!Consume(','))
					{
						SetError("JSON配列の要素間に ',' が必要です。");
						return false;
					}
					SkipWhitespace();
				}

				SetError("JSON配列が閉じられていません。");
				return false;
			}

			bool ParseString(std::string& outValue)
			{
				SkipWhitespace();
				if (!Consume('"'))
				{
					SetError("JSON文字列は '\"' で開始する必要があります。");
					return false;
				}

				std::string value;
				while (m_Position < m_Text.size())
				{
					const unsigned char c =
						static_cast<unsigned char>(m_Text[m_Position++]);
					if (c == '"')
					{
						outValue = value;
						return true;
					}
					if (c < 0x20)
					{
						SetError("JSON文字列に制御文字が含まれています。");
						return false;
					}
					if (c != '\\')
					{
						value.push_back(static_cast<char>(c));
						continue;
					}

					if (m_Position >= m_Text.size())
					{
						SetError("JSON文字列のエスケープが途中で終了しています。");
						return false;
					}

					const char escaped = m_Text[m_Position++];
					switch (escaped)
					{
					case '"': value.push_back('"'); break;
					case '\\': value.push_back('\\'); break;
					case '/': value.push_back('/'); break;
					case 'b': value.push_back('\b'); break;
					case 'f': value.push_back('\f'); break;
					case 'n': value.push_back('\n'); break;
					case 'r': value.push_back('\r'); break;
					case 't': value.push_back('\t'); break;
					case 'u':
					{
						unsigned int codePoint = 0;
						if (!ParseUnicodeEscape(codePoint))
						{
							return false;
						}
						AppendUtf8(value, codePoint);
						break;
					}
					default:
						SetError("JSON文字列のエスケープ文字が不正です。");
						return false;
					}
				}

				SetError("JSON文字列が閉じられていません。");
				return false;
			}

			bool ParseUnicodeEscape(unsigned int& outCodePoint)
			{
				unsigned int first = 0;
				if (!ParseHex4(first))
				{
					return false;
				}

				if (first >= 0xD800 && first <= 0xDBFF)
				{
					if (m_Position + 2 > m_Text.size() ||
						m_Text[m_Position] != '\\' ||
						m_Text[m_Position + 1] != 'u')
					{
						SetError("UTF-16上位サロゲートの後に下位サロゲートがありません。");
						return false;
					}
					m_Position += 2;

					unsigned int second = 0;
					if (!ParseHex4(second) || second < 0xDC00 || second > 0xDFFF)
					{
						SetError("UTF-16下位サロゲートが不正です。");
						return false;
					}
					outCodePoint = 0x10000 +
						(((first - 0xD800) << 10) | (second - 0xDC00));
					return true;
				}

				if (first >= 0xDC00 && first <= 0xDFFF)
				{
					SetError("UTF-16下位サロゲートだけが指定されています。");
					return false;
				}

				outCodePoint = first;
				return true;
			}

			bool ParseHex4(unsigned int& outValue)
			{
				if (m_Position + 4 > m_Text.size())
				{
					SetError("Unicodeエスケープが途中で終了しています。");
					return false;
				}

				unsigned int value = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char c = m_Text[m_Position++];
					value <<= 4;
					if (c >= '0' && c <= '9')
					{
						value += static_cast<unsigned int>(c - '0');
					}
					else if (c >= 'a' && c <= 'f')
					{
						value += static_cast<unsigned int>(c - 'a' + 10);
					}
					else if (c >= 'A' && c <= 'F')
					{
						value += static_cast<unsigned int>(c - 'A' + 10);
					}
					else
					{
						SetError("Unicodeエスケープに16進数以外が含まれています。");
						return false;
					}
				}
				outValue = value;
				return true;
			}

			void AppendUtf8(std::string& value, unsigned int codePoint)
			{
				if (codePoint <= 0x7F)
				{
					value.push_back(static_cast<char>(codePoint));
				}
				else if (codePoint <= 0x7FF)
				{
					value.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
					value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
				}
				else if (codePoint <= 0xFFFF)
				{
					value.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
					value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
					value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
				}
				else
				{
					value.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
					value.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
					value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
					value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
				}
			}

			bool ParseNumber(JsonValue& outValue)
			{
				const size_t start = m_Position;
				if (m_Text[m_Position] == '-')
				{
					++m_Position;
				}

				if (m_Position >= m_Text.size())
				{
					SetError("JSON数値が途中で終了しています。");
					return false;
				}

				if (m_Text[m_Position] == '0')
				{
					++m_Position;
				}
				else if (m_Text[m_Position] >= '1' && m_Text[m_Position] <= '9')
				{
					while (m_Position < m_Text.size() &&
						m_Text[m_Position] >= '0' && m_Text[m_Position] <= '9')
					{
						++m_Position;
					}
				}
				else
				{
					SetError("JSON数値の整数部が不正です。");
					return false;
				}

				if (m_Position < m_Text.size() && m_Text[m_Position] == '.')
				{
					++m_Position;
					const size_t decimalStart = m_Position;
					while (m_Position < m_Text.size() &&
						m_Text[m_Position] >= '0' && m_Text[m_Position] <= '9')
					{
						++m_Position;
					}
					if (decimalStart == m_Position)
					{
						SetError("JSON数値の小数部が不正です。");
						return false;
					}
				}

				if (m_Position < m_Text.size() &&
					(m_Text[m_Position] == 'e' || m_Text[m_Position] == 'E'))
				{
					++m_Position;
					if (m_Position < m_Text.size() &&
						(m_Text[m_Position] == '+' || m_Text[m_Position] == '-'))
					{
						++m_Position;
					}
					const size_t exponentStart = m_Position;
					while (m_Position < m_Text.size() &&
						m_Text[m_Position] >= '0' && m_Text[m_Position] <= '9')
					{
						++m_Position;
					}
					if (exponentStart == m_Position)
					{
						SetError("JSON数値の指数部が不正です。");
						return false;
					}
				}

				const std::string numberText = m_Text.substr(start, m_Position - start);
				char* end = nullptr;
				errno = 0;
				const double number = std::strtod(numberText.c_str(), &end);
				if (errno == ERANGE || !end || *end != '\0' || !std::isfinite(number))
				{
					SetError("JSON数値が表現可能な範囲を超えています。");
					return false;
				}

				outValue = JsonValue::CreateNumber(number);
				return true;
			}

			bool ParseLiteral(
				const char* literal,
				const JsonValue& value,
				JsonValue& outValue)
			{
				const size_t length = std::strlen(literal);
				if (m_Text.compare(m_Position, length, literal) != 0)
				{
					SetError("JSONリテラルが不正です。");
					return false;
				}
				m_Position += length;
				outValue = value;
				return true;
			}

			bool Consume(char expected)
			{
				if (m_Position >= m_Text.size() || m_Text[m_Position] != expected)
				{
					return false;
				}
				++m_Position;
				return true;
			}
		};

	}

	JsonValue JsonValue::CreateBoolean(bool value)
	{
		JsonValue result;
		result.m_Type = Type::Boolean;
		result.m_Boolean = value;
		return result;
	}

	JsonValue JsonValue::CreateNumber(double value)
	{
		JsonValue result;
		result.m_Type = Type::Number;
		result.m_Number = value;
		return result;
	}

	JsonValue JsonValue::CreateString(const std::string& value)
	{
		JsonValue result;
		result.m_Type = Type::String;
		result.m_String = value;
		return result;
	}

	JsonValue JsonValue::CreateObject(const Object& value)
	{
		JsonValue result;
		result.m_Type = Type::Object;
		result.m_Object = value;
		return result;
	}

	JsonValue JsonValue::CreateArray(const Array& value)
	{
		JsonValue result;
		result.m_Type = Type::Array;
		result.m_Array = value;
		return result;
	}

	const JsonValue* JsonValue::Find(const std::string& key) const
	{
		if (!IsObject())
		{
			return nullptr;
		}

		auto it = m_Object.find(key);
		return it != m_Object.end() ? &it->second : nullptr;
	}

	bool JsonLoader::Parse(
		const std::string& text,
		JsonValue& outValue,
		std::string& outError)
	{
		std::string source = text;

		// UTF-8 BOM付きでも、JSON先頭の構文文字として誤認しないように除去する。
		if (source.size() >= 3 &&
			static_cast<unsigned char>(source[0]) == 0xEF &&
			static_cast<unsigned char>(source[1]) == 0xBB &&
			static_cast<unsigned char>(source[2]) == 0xBF)
		{
			source.erase(0, 3);
		}

		JsonValue value;
		JsonParser parser(source);
		if (!parser.Parse(value, outError))
		{
			return false;
		}

		outValue = value;
		outError.clear();
		return true;
	}

	bool JsonLoader::LoadFile(
		const std::wstring& path,
		JsonValue& outValue,
		std::string& outError)
	{
		std::ifstream file{ std::filesystem::path(path), std::ios::binary };
		if (!file)
		{
			outError = "JSONファイルを開けません。";
			return false;
		}

		const std::string text(
			(std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (!file.good() && !file.eof())
		{
			outError = "JSONファイルの読み込み中にエラーが発生しました。";
			return false;
		}

		return Parse(text, outValue, outError);
	}

}
