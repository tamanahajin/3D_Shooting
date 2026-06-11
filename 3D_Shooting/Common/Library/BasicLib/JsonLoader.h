/*!
@file JsonLoader.h
@brief 汎用JSONファイルローダー
*/

#pragma once
#include <map>
#include <string>
#include <vector>

namespace shooting {

	/*!
	@brief JSON内の値を表す汎用データ

	ゲーム固有の型には変換せず、JSONが持つ値の種類をそのまま保持する。
	各設定ローダーはこの値を参照して、必要なゲーム用構造体へ変換する。
	*/
	class JsonValue
	{
	public:
		enum class Type
		{
			Null,
			Boolean,
			Number,
			String,
			Object,
			Array
		};

		typedef std::map<std::string, JsonValue> Object;
		typedef std::vector<JsonValue> Array;

		JsonValue() {}

		static JsonValue CreateBoolean(bool value);
		static JsonValue CreateNumber(double value);
		static JsonValue CreateString(const std::string& value);
		static JsonValue CreateObject(const Object& value);
		static JsonValue CreateArray(const Array& value);

		Type GetType() const { return m_Type; }
		bool IsNull() const { return m_Type == Type::Null; }
		bool IsBoolean() const { return m_Type == Type::Boolean; }
		bool IsNumber() const { return m_Type == Type::Number; }
		bool IsString() const { return m_Type == Type::String; }
		bool IsObject() const { return m_Type == Type::Object; }
		bool IsArray() const { return m_Type == Type::Array; }

		bool GetBoolean() const { return m_Boolean; }
		double GetNumber() const { return m_Number; }
		const std::string& GetString() const { return m_String; }
		const Object& GetObject() const { return m_Object; }
		const Array& GetArray() const { return m_Array; }

		/*!
		@brief オブジェクトから指定キーの値を取得する
		@param key 検索するキー
		@return 値が存在する場合はそのポインタ。オブジェクトでない場合や未定義の場合はnullptr
		*/
		const JsonValue* Find(const std::string& key) const;

	private:
		Type m_Type = Type::Null;
		bool m_Boolean = false;
		double m_Number = 0.0;
		std::string m_String;
		Object m_Object;
		Array m_Array;
	};

	/*!
	@brief JSONテキストとJSONファイルをJsonValueへ変換する汎用ローダー

	ファイル形式とJSON構文だけを扱い、ゲーム固有の設定名や値の範囲は解釈しない。
	*/
	class JsonLoader
	{
	public:
		/*!
		@brief UTF-8のJSONテキストを解析する
		@param text JSONテキスト
		@param outValue 解析結果
		@param outError 解析失敗時の理由
		@return 解析に成功した場合はtrue
		*/
		static bool Parse(
			const std::string& text,
			JsonValue& outValue,
			std::string& outError);

		/*!
		@brief UTF-8のJSONファイルを読み込んで解析する
		@param path JSONファイルのパス
		@param outValue 解析結果
		@param outError 読み込みまたは解析失敗時の理由
		@return 読み込みと解析に成功した場合はtrue
		*/
		static bool LoadFile(
			const std::wstring& path,
			JsonValue& outValue,
			std::string& outError);
	};

}
