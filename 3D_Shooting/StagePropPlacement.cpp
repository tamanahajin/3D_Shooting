#include "stdafx.h"
#include "StagePropPlacement.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace shooting {

	namespace
	{
		const wchar_t* kStagePropPlacementCsv = L"Stage/stage_props.csv";

		std::string NarrowPath(const std::wstring& path)
		{
			const int requiredSize = WideCharToMultiByte(
				CP_ACP, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
			if (requiredSize <= 1)
			{
				return std::string();
			}

			std::string result(static_cast<size_t>(requiredSize), '\0');
			WideCharToMultiByte(
				CP_ACP, 0, path.c_str(), -1, &result[0], requiredSize, nullptr, nullptr);
			result.pop_back();
			return result;
		}

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
				subRow < kStagePropSubcellCount &&
				subColumn >= 0 &&
				subColumn < kStagePropSubcellCount;
		}

		std::vector<StagePropPlacement>::iterator FindPlacementAtCell(
			std::vector<StagePropPlacement>& placements,
			int row,
			int column)
		{
			return std::find_if(
				placements.begin(),
				placements.end(),
				[row, column](const StagePropPlacement& placement)
				{
					return placement.row == row && placement.column == column;
				});
		}
	}

	const wchar_t* StagePropPlacementFile::GetRelativePath()
	{
		return kStagePropPlacementCsv;
	}

	Vec3 CalculateStagePropSubcellOffset(
		int subRow,
		int subColumn,
		float parentCellSize)
	{
		if (!IsValidSubcell(subRow, subColumn) || parentCellSize <= 0.0f)
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const float subcellSize =
			parentCellSize / static_cast<float>(kStagePropSubcellCount);
		const float x =
			(static_cast<float>(subColumn) - 1.0f) * subcellSize;
		const float z =
			(1.0f - static_cast<float>(subRow)) * subcellSize;
		return Vec3(x, 0.0f, z);
	}

	bool StagePropPlacementFile::Load(
		const std::wstring& path,
		std::vector<StagePropPlacement>& outPlacements,
		std::string& outErrorMessage)
	{
		outPlacements.clear();
		outErrorMessage.clear();

		const DWORD attributes = GetFileAttributesW(path.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			const DWORD error = GetLastError();
			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			{
				// 配置物CSVがまだ無いステージは、配置物0件として編集を開始できるようにする。
				return true;
			}
		}

		std::ifstream file(NarrowPath(path), std::ios::binary);
		if (!file.is_open())
		{
			outErrorMessage = "Stage prop CSV open failed.";
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
			StagePropPlacement placement;
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
					"Invalid stage prop CSV line: " + std::to_string(lineNumber);
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
						"Invalid stage prop subcell at line: " + std::to_string(lineNumber);
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
						"Invalid stage prop subcell at line: " + std::to_string(lineNumber);
					return false;
				}
			}

			std::string extraToken;
			if (stream >> extraToken)
			{
				outErrorMessage =
					"Too many stage prop values at line: " + std::to_string(lineNumber);
				return false;
			}

			if (placement.row < 0 ||
				placement.column < 0 ||
				modelName.empty() ||
				!std::isfinite(placement.yRotationDegrees) ||
				!IsValidSubcell(placement.subRow, placement.subColumn))
			{
				outErrorMessage =
					"Invalid stage prop value at line: " + std::to_string(lineNumber);
				return false;
			}

			placement.modelName = WidenUtf8(modelName);
			if (placement.modelName.empty())
			{
				outErrorMessage =
					"Invalid stage prop model name at line: " + std::to_string(lineNumber);
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

	bool StagePropPlacementFile::Save(
		const std::wstring& path,
		const std::vector<StagePropPlacement>& placements,
		std::string& outErrorMessage)
	{
		outErrorMessage.clear();

		std::vector<StagePropPlacement> sortedPlacements = placements;
		std::sort(
			sortedPlacements.begin(),
			sortedPlacements.end(),
			[](const StagePropPlacement& left, const StagePropPlacement& right)
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

		// 改行をCRLFへ固定し、Visual Studioで行末不整合の警告が出ないようにする。
		std::ofstream file(
			NarrowPath(path),
			std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			outErrorMessage = "Stage prop CSV save failed.";
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
				outErrorMessage = "Invalid stage prop data.";
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
			outErrorMessage = "Stage prop CSV write failed.";
			return false;
		}
		return true;
	}

}
