/*!
@file StageEditorObjectPlacement.cpp
@brief ステージエディタで置いた配置物をCSVとして読み書きする
*/
#include "stdafx.h"
#include "StageEditorObjectPlacement.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace shooting {

	namespace
	{
		const wchar_t* kStageEditorObjectPlacementCsv = L"Stage/stage_props.csv";

		std::wstring WidenUtf8(const std::string& value)
		{
			if (value.empty())
			{
				return std::wstring();
			}

			const int requiredSize = MultiByteToWideChar(
				CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
			if (requiredSize <= 0)
			{
				return std::wstring();
			}

			std::wstring result(static_cast<size_t>(requiredSize), L'\0');
			MultiByteToWideChar(
				CP_UTF8,
				0,
				value.c_str(),
				static_cast<int>(value.size()),
				&result[0],
				requiredSize);
			return result;
		}

		std::string NarrowUtf8(const std::wstring& value)
		{
			if (value.empty())
			{
				return std::string();
			}

			const int requiredSize = WideCharToMultiByte(
				CP_UTF8,
				0,
				value.c_str(),
				static_cast<int>(value.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (requiredSize <= 0)
			{
				return std::string();
			}

			std::string result(static_cast<size_t>(requiredSize), '\0');
			WideCharToMultiByte(
				CP_UTF8,
				0,
				value.c_str(),
				static_cast<int>(value.size()),
				&result[0],
				requiredSize,
				nullptr,
				nullptr);
			return result;
		}

		float NormalizeDegrees(float degrees)
		{
			float normalized = std::fmod(degrees, 360.0f);
			if (normalized < 0.0f)
			{
				normalized += 360.0f;
			}
			return normalized;
		}

		bool IsValidSubcell(int subRow, int subColumn)
		{
			return subRow >= 0 &&
				subRow < kStageEditorObjectSubcellCount &&
				subColumn >= 0 &&
				subColumn < kStageEditorObjectSubcellCount;
		}

		std::vector<StageEditorObjectPlacement>::iterator FindPlacementAtCell(
			std::vector<StageEditorObjectPlacement>& placements,
			int row,
			int column)
		{
			return std::find_if(
				placements.begin(),
				placements.end(),
				[row, column](const StageEditorObjectPlacement& placement)
				{
					return placement.row == row && placement.column == column;
				});
		}
	}

	const wchar_t* StageEditorObjectPlacementFile::GetRelativePath()
	{
		return kStageEditorObjectPlacementCsv;
	}

	Vec3 CalculateStageEditorObjectSubcellOffset(
		int subRow,
		int subColumn,
		float parentCellSize)
	{
		if (!IsValidSubcell(subRow, subColumn) || parentCellSize <= 0.0f)
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const float subcellSize =
			parentCellSize / static_cast<float>(kStageEditorObjectSubcellCount);
		const float x =
			(static_cast<float>(subColumn) - 1.0f) * subcellSize;
		const float z =
			(1.0f - static_cast<float>(subRow)) * subcellSize;
		return Vec3(x, 0.0f, z);
	}

	bool StageEditorObjectPlacementFile::Load(
		const std::wstring& path,
		std::vector<StageEditorObjectPlacement>& outPlacements,
		std::string& outErrorMessage)
	{
		outPlacements.clear();
		outErrorMessage.clear();

		std::error_code errorCode;
		if (!std::filesystem::exists(std::filesystem::path(path), errorCode))
		{
			// 配置物CSVがまだ無いステージは、配置物0件として編集を開始できるようにする。
			return !errorCode;
		}

		std::ifstream file{ std::filesystem::path(path), std::ios::binary };
		if (!file.is_open())
		{
			outErrorMessage = "Stage editor object CSV open failed.";
			return false;
		}

		std::string line;
		int lineNumber = 0;
		while (std::getline(file, line))
		{
			++lineNumber;
			const auto commentPosition = line.find('#');
			if (commentPosition != std::string::npos)
			{
				line = line.substr(0, commentPosition);
			}

			std::replace(line.begin(), line.end(), ',', ' ');
			std::replace(line.begin(), line.end(), ';', ' ');
			std::replace(line.begin(), line.end(), '\t', ' ');

			std::stringstream stream(line);
			StageEditorObjectPlacement placement;
			std::string modelName;
			if (!(stream >> placement.row >> placement.column >> modelName >> placement.yRotationDegrees))
			{
				stream.clear();
				stream.str(line);
				std::string firstToken;
				if (!(stream >> firstToken))
				{
					continue;
				}

				outErrorMessage =
					"Invalid stage editor object CSV line: " + std::to_string(lineNumber);
				return false;
			}

			// 旧形式の4列CSVは中央配置として読み込む。5列だけ存在する不完全な行はエラーにする。
			std::string subRowToken;
			std::string subColumnToken;
			if (stream >> subRowToken)
			{
				if (!(stream >> subColumnToken))
				{
					outErrorMessage =
						"Invalid stage editor object subcell at line: " + std::to_string(lineNumber);
					return false;
				}

				std::stringstream subRowStream(subRowToken);
				std::stringstream subColumnStream(subColumnToken);
				std::string invalidToken;
				if (!(subRowStream >> placement.subRow) ||
					(subRowStream >> invalidToken) ||
					!(subColumnStream >> placement.subColumn) ||
					(subColumnStream >> invalidToken))
				{
					outErrorMessage =
						"Invalid stage editor object subcell at line: " + std::to_string(lineNumber);
					return false;
				}
			}

			std::string extraToken;
			if (stream >> extraToken)
			{
				outErrorMessage =
					"Too many stage editor object values at line: " + std::to_string(lineNumber);
				return false;
			}

			if (placement.row < 0 ||
				placement.column < 0 ||
				modelName.empty() ||
				!std::isfinite(placement.yRotationDegrees) ||
				!IsValidSubcell(placement.subRow, placement.subColumn))
			{
				outErrorMessage =
					"Invalid stage editor object value at line: " + std::to_string(lineNumber);
				return false;
			}

			placement.modelName = WidenUtf8(modelName);
			if (placement.modelName.empty())
			{
				outErrorMessage =
					"Invalid stage editor object model name at line: " + std::to_string(lineNumber);
				return false;
			}
			placement.yRotationDegrees = NormalizeDegrees(placement.yRotationDegrees);

			// 同じセルが複数行に書かれていた場合は、後に書かれた設定を採用する。
			auto existing = FindPlacementAtCell(
				outPlacements,
				placement.row,
				placement.column);
			if (existing != outPlacements.end())
			{
				*existing = placement;
			}
			else
			{
				outPlacements.push_back(placement);
			}
		}

		return true;
	}

	bool StageEditorObjectPlacementFile::Save(
		const std::wstring& path,
		const std::vector<StageEditorObjectPlacement>& placements,
		std::string& outErrorMessage)
	{
		outErrorMessage.clear();

		std::vector<StageEditorObjectPlacement> sortedPlacements = placements;
		std::sort(
			sortedPlacements.begin(),
			sortedPlacements.end(),
			[](const StageEditorObjectPlacement& left, const StageEditorObjectPlacement& right)
			{
				if (left.row != right.row)
				{
					return left.row < right.row;
				}
				if (left.column != right.column)
				{
					return left.column < right.column;
				}
				return left.modelName < right.modelName;
			});

		const std::filesystem::path outputPath(path);
		const std::filesystem::path parentPath = outputPath.parent_path();
		if (!parentPath.empty())
		{
			std::error_code errorCode;
			std::filesystem::create_directories(parentPath, errorCode);
			if (errorCode)
			{
				outErrorMessage = "Stage editor object CSV save directory create failed.";
				return false;
			}
		}

		// 改行をCRLFへ固定し、Visual Studioで行末不整合の警告が出ないようにする。
		std::ofstream file{
			outputPath,
			std::ios::binary | std::ios::trunc };
		if (!file.is_open())
		{
			outErrorMessage = "Stage editor object CSV save failed.";
			return false;
		}

		file << "# row,column,model_name,y_rotation_degrees,sub_row,sub_column\r\n";
		file << std::fixed << std::setprecision(1);
		for (const auto& placement : sortedPlacements)
		{
			const std::string modelName = NarrowUtf8(placement.modelName);
			if (placement.row < 0 ||
				placement.column < 0 ||
				modelName.empty() ||
				!std::isfinite(placement.yRotationDegrees) ||
				!IsValidSubcell(placement.subRow, placement.subColumn))
			{
				outErrorMessage = "Invalid stage editor object data.";
				return false;
			}

			file
				<< placement.row << ','
				<< placement.column << ','
				<< modelName << ','
				<< NormalizeDegrees(placement.yRotationDegrees) << ','
				<< placement.subRow << ','
				<< placement.subColumn
				<< "\r\n";
		}

		if (!file.good())
		{
			outErrorMessage = "Stage editor object CSV write failed.";
			return false;
		}
		return true;
	}

}
