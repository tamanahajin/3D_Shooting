#include "stdafx.h"
#include "Project.h"
#include "StageEditor.h"

#if defined(_DEBUG)
#include "imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace shooting {

	namespace
	{
		const int kStageObjectEmpty = 0;
		const int kStageObjectSlopeRight = 5;
		const int kStageHeightMax = 3;
		const float kStageCellSize = 5.0f;
		const float kStageHeightStep = 5.0f;
		const Vec3 kStageOrigin(0.0f, 0.0f, 0.0f);
		const wchar_t* kObjectCsvPath = L"Stage/stage_objects.csv";
		const wchar_t* kHeightCsvPath = L"Stage/stage_heights.csv";

		bool IsEditablePropCategory(StageObjectCategory category)
		{
			return category == StageObjectCategory::Tree ||
				category == StageObjectCategory::Log ||
				category == StageObjectCategory::Rock ||
				category == StageObjectCategory::Stone ||
				category == StageObjectCategory::Plant ||
				category == StageObjectCategory::Mushroom;
		}

		const char* GetPropCategoryName(StageObjectCategory category)
		{
			switch (category)
			{
			case StageObjectCategory::Tree:
				return "Tree";
			case StageObjectCategory::Log:
				return "Log";
			case StageObjectCategory::Rock:
				return "Rock";
			case StageObjectCategory::Stone:
				return "Stone";
			case StageObjectCategory::Plant:
				return "Plant";
			case StageObjectCategory::Mushroom:
				return "Mushroom";
			default:
				return "Other";
			}
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

		bool LoadCsvGrid(
			const std::wstring& path,
			std::vector<int>& outValues,
			int& outRows,
			int& outColumns)
		{
			std::ifstream file{ std::filesystem::path(path) };
			if (!file.is_open())
			{
				return false;
			}

			std::vector<int> values;
			int rowCount = 0;
			int columnCount = 0;
			std::string line;
			while (std::getline(file, line))
			{
				const auto commentPos = line.find('#');
				if (commentPos != std::string::npos)
				{
					line = line.substr(0, commentPos);
				}

				std::replace(line.begin(), line.end(), ',', ' ');
				std::replace(line.begin(), line.end(), ';', ' ');
				std::replace(line.begin(), line.end(), '\t', ' ');

				std::stringstream stream(line);
				std::vector<int> row;
				int value = 0;
				while (stream >> value)
				{
					row.push_back(value);
				}
				if (row.empty())
				{
					continue;
				}

				if (columnCount == 0)
				{
					columnCount = static_cast<int>(row.size());
				}
				else if (columnCount != static_cast<int>(row.size()))
				{
					return false;
				}

				values.insert(values.end(), row.begin(), row.end());
				++rowCount;
			}

			if (rowCount <= 0 || columnCount <= 0)
			{
				return false;
			}

			outValues = std::move(values);
			outRows = rowCount;
			outColumns = columnCount;
			return true;
		}

		bool SaveCsvGrid(
			const std::wstring& path,
			const std::vector<int>& values,
			int rowCount,
			int columnCount)
		{
			if (rowCount <= 0 ||
				columnCount <= 0 ||
				values.size() != static_cast<size_t>(rowCount * columnCount))
			{
				return false;
			}

			const std::filesystem::path outputPath(path);
			const std::filesystem::path parentPath = outputPath.parent_path();
			if (!parentPath.empty())
			{
				std::error_code errorCode;
				std::filesystem::create_directories(parentPath, errorCode);
				if (errorCode)
				{
					return false;
				}
			}

			// 改行変換をランタイムへ任せず、保存形式を常にCRLFへ固定する。
			std::ofstream file{
				outputPath,
				std::ios::binary | std::ios::trunc };
			if (!file.is_open())
			{
				return false;
			}

			for (int row = 0; row < rowCount; ++row)
			{
				for (int column = 0; column < columnCount; ++column)
				{
					if (column > 0)
					{
						file << ' ';
					}
					file << values[static_cast<size_t>((row * columnCount) + column)];
				}
				file << "\r\n";
			}
			return file.good();
		}

#if defined(_DEBUG)
		bool ProjectToScreen(
			const std::shared_ptr<Camera>& camera,
			const Vec3& worldPosition,
			float screenWidth,
			float screenHeight,
			ImVec2& outScreen)
		{
			if (!camera || screenWidth <= 0.0f || screenHeight <= 0.0f)
			{
				return false;
			}

			const XMMATRIX view = static_cast<XMMATRIX>(camera->GetViewMatrix());
			const XMMATRIX projection = static_cast<XMMATRIX>(camera->GetProjMatrix());
			const XMVECTOR projected = XMVector3Project(
				static_cast<XMVECTOR>(worldPosition),
				0.0f,
				0.0f,
				screenWidth,
				screenHeight,
				0.0f,
				1.0f,
				projection,
				view,
				XMMatrixIdentity());

			XMFLOAT3 screen{};
			XMStoreFloat3(&screen, projected);
			if (screen.z < 0.0f || screen.z > 1.0f)
			{
				return false;
			}

			outScreen = ImVec2(screen.x, screen.y);
			return true;
		}
#endif
	}

	bool StageEditor::Enter(GameStage& stage)
	{
#if !defined(_DEBUG)
		(void)stage;
		return false;
#else
		if (!m_loaded && !Load())
		{
			return false;
		}

		m_active = true;
		AttachEditorCamera(stage);
		return true;
#endif
	}

	void StageEditor::Exit(GameStage& stage)
	{
		RestoreCamera(stage);
		m_active = false;
	}

	void StageEditor::OnStageReloaded(GameStage& stage)
	{
		m_cameraAttached = false;
		if (m_active)
		{
			AttachEditorCamera(stage);
		}
	}

	bool StageEditor::Load()
	{
		std::vector<int> objects;
		std::vector<int> heights;
		std::vector<StagePropPlacement> propPlacements;
		int objectRows = 0;
		int objectColumns = 0;
		int heightRows = 0;
		int heightColumns = 0;
		std::string propError;

		const std::wstring assets = App::GetRelativeAssetsDir();
		if (!LoadCsvGrid(assets + kObjectCsvPath, objects, objectRows, objectColumns) ||
			!LoadCsvGrid(assets + kHeightCsvPath, heights, heightRows, heightColumns) ||
			!StagePropPlacementFile::Load(
				assets + StagePropPlacementFile::GetRelativePath(),
				propPlacements,
				propError))
		{
			m_statusText = propError.empty() ? "CSV load failed." : propError;
			return false;
		}
		if (objectRows != heightRows || objectColumns != heightColumns)
		{
			m_statusText = "CSV grid sizes do not match.";
			return false;
		}
		for (const auto& placement : propPlacements)
		{
			if (placement.row >= objectRows || placement.column >= objectColumns)
			{
				m_statusText = "Stage prop is outside the grid.";
				return false;
			}
		}

		m_objects = std::move(objects);
		m_heights = std::move(heights);
		m_propPlacements = std::move(propPlacements);
		m_rowCount = objectRows;
		m_columnCount = objectColumns;
		m_loaded = true;
		m_dirty = false;
		m_statusText = "Loaded.";

		if (!IsValidCell(m_selectedRow, m_selectedColumn))
		{
			m_selectedRow = m_rowCount / 2;
			m_selectedColumn = m_columnCount / 2;
		}
		return true;
	}

	bool StageEditor::Save()
	{
		const std::wstring assets = App::GetRelativeAssetsDir();
		std::string propError;
		if (!SaveCsvGrid(
				assets + kObjectCsvPath,
				m_objects,
				m_rowCount,
				m_columnCount) ||
			!SaveCsvGrid(
				assets + kHeightCsvPath,
				m_heights,
				m_rowCount,
				m_columnCount) ||
			!StagePropPlacementFile::Save(
				assets + StagePropPlacementFile::GetRelativePath(),
				m_propPlacements,
				propError))
		{
			m_statusText = propError.empty() ? "CSV save failed." : propError;
			return false;
		}

		m_dirty = false;
		m_statusText = "Saved.";
		return true;
	}

	void StageEditor::AttachEditorCamera(GameStage& stage)
	{
		auto camera = stage.GetCamera();
		if (!camera)
		{
			return;
		}

		m_savedCameraEye = camera->GetEye();
		m_savedCameraAt = camera->GetAt();
		m_savedCameraUp = camera->GetUp();
		m_cameraAttached = true;

		// 真上から全セルを確認する。Zを少しずらし、視線とUpベクトルの平行を避ける。
		camera->SetUp(Vec3(0.0f, 0.0f, 1.0f));
		camera->SetAt(kStageOrigin);
		camera->SetEye(Vec3(0.0f, 90.0f, -0.01f));
	}

	void StageEditor::RestoreCamera(GameStage& stage)
	{
		if (!m_cameraAttached)
		{
			return;
		}

		auto camera = stage.GetCamera();
		if (camera)
		{
			camera->SetUp(m_savedCameraUp);
			camera->SetAt(m_savedCameraAt);
			camera->SetEye(m_savedCameraEye);
		}
		m_cameraAttached = false;
	}

	void StageEditor::PickCell(
		const std::shared_ptr<Camera>& camera,
		float screenWidth,
		float screenHeight)
	{
#if defined(_DEBUG)
		if (!camera || screenWidth <= 0.0f || screenHeight <= 0.0f)
		{
			return;
		}

		const auto& mouse = App::GetInputDevice().GetMouseState();
		const XMVECTOR screenNear = XMVectorSet(
			static_cast<float>(mouse.now.x),
			static_cast<float>(mouse.now.y),
			0.0f,
			1.0f);
		const XMVECTOR screenFar = XMVectorSet(
			static_cast<float>(mouse.now.x),
			static_cast<float>(mouse.now.y),
			1.0f,
			1.0f);
		const XMMATRIX view = static_cast<XMMATRIX>(camera->GetViewMatrix());
		const XMMATRIX projection = static_cast<XMMATRIX>(camera->GetProjMatrix());

		const XMVECTOR nearWorld = XMVector3Unproject(
			screenNear,
			0.0f,
			0.0f,
			screenWidth,
			screenHeight,
			0.0f,
			1.0f,
			projection,
			view,
			XMMatrixIdentity());
		const XMVECTOR farWorld = XMVector3Unproject(
			screenFar,
			0.0f,
			0.0f,
			screenWidth,
			screenHeight,
			0.0f,
			1.0f,
			projection,
			view,
			XMMatrixIdentity());

		XMFLOAT3 nearPoint{};
		XMFLOAT3 farPoint{};
		XMStoreFloat3(&nearPoint, nearWorld);
		XMStoreFloat3(&farPoint, farWorld);
		const Vec3 origin(nearPoint.x, nearPoint.y, nearPoint.z);
		const Vec3 direction(
			farPoint.x - nearPoint.x,
			farPoint.y - nearPoint.y,
			farPoint.z - nearPoint.z);
		if (std::fabs(direction.y) <= 1e-6f)
		{
			return;
		}

		const float distance = (kStageOrigin.y - origin.y) / direction.y;
		if (distance < 0.0f)
		{
			return;
		}

		const Vec3 hit = origin + (direction * distance);
		const float rowCenter = static_cast<float>(m_rowCount - 1) * 0.5f;
		const float columnCenter = static_cast<float>(m_columnCount - 1) * 0.5f;
		const int row = static_cast<int>(std::floor(
			((hit.z - kStageOrigin.z) / kStageCellSize) + rowCenter + 0.5f));
		const int column = static_cast<int>(std::floor(
			columnCenter - ((hit.x - kStageOrigin.x) / kStageCellSize) + 0.5f));

		if (IsValidCell(row, column))
		{
			m_selectedRow = row;
			m_selectedColumn = column;
		}
#else
		(void)camera;
		(void)screenWidth;
		(void)screenHeight;
#endif
	}

	void StageEditor::DrawGridOverlay(
		const std::shared_ptr<Camera>& camera,
		float screenWidth,
		float screenHeight) const
	{
#if defined(_DEBUG)
		if (!camera || m_rowCount <= 0 || m_columnCount <= 0)
		{
			return;
		}

		// 背景DrawListへ描き、編集パネルの文字や操作部品より手前に線が重ならないようにする。
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		const ImU32 gridColor = IM_COL32(110, 180, 210, 105);
		const float halfRows = static_cast<float>(m_rowCount) * 0.5f;
		const float halfColumns = static_cast<float>(m_columnCount) * 0.5f;
		const float minX = kStageOrigin.x - (halfColumns * kStageCellSize);
		const float maxX = kStageOrigin.x + (halfColumns * kStageCellSize);
		const float minZ = kStageOrigin.z - (halfRows * kStageCellSize);
		const float maxZ = kStageOrigin.z + (halfRows * kStageCellSize);
		const float gridY = kStageOrigin.y + 0.15f;

		for (int line = 0; line <= m_columnCount; ++line)
		{
			const float x = minX + (static_cast<float>(line) * kStageCellSize);
			ImVec2 start{};
			ImVec2 end{};
			if (ProjectToScreen(camera, Vec3(x, gridY, minZ), screenWidth, screenHeight, start) &&
				ProjectToScreen(camera, Vec3(x, gridY, maxZ), screenWidth, screenHeight, end))
			{
				drawList->AddLine(start, end, gridColor, 1.0f);
			}
		}
		for (int line = 0; line <= m_rowCount; ++line)
		{
			const float z = minZ + (static_cast<float>(line) * kStageCellSize);
			ImVec2 start{};
			ImVec2 end{};
			if (ProjectToScreen(camera, Vec3(minX, gridY, z), screenWidth, screenHeight, start) &&
				ProjectToScreen(camera, Vec3(maxX, gridY, z), screenWidth, screenHeight, end))
			{
				drawList->AddLine(start, end, gridColor, 1.0f);
			}
		}

		if (!IsValidCell(m_selectedRow, m_selectedColumn))
		{
			return;
		}

		const float centerX =
			kStageOrigin.x +
			((static_cast<float>(m_columnCount - 1) * 0.5f - static_cast<float>(m_selectedColumn)) *
				kStageCellSize);
		const float centerZ =
			kStageOrigin.z +
			((static_cast<float>(m_selectedRow) - static_cast<float>(m_rowCount - 1) * 0.5f) *
				kStageCellSize);
		const float halfCell = kStageCellSize * 0.5f;
		const float selectedY =
			kStageOrigin.y +
			(static_cast<float>(m_heights[GetIndex(m_selectedRow, m_selectedColumn)]) * kStageHeightStep) +
			0.25f;
		const Vec3 corners[] =
		{
			Vec3(centerX - halfCell, selectedY, centerZ - halfCell),
			Vec3(centerX + halfCell, selectedY, centerZ - halfCell),
			Vec3(centerX + halfCell, selectedY, centerZ + halfCell),
			Vec3(centerX - halfCell, selectedY, centerZ + halfCell),
		};

		ImVec2 screenCorners[4]{};
		for (int i = 0; i < 4; ++i)
		{
			if (!ProjectToScreen(camera, corners[i], screenWidth, screenHeight, screenCorners[i]))
			{
				return;
			}
		}

		drawList->AddQuadFilled(
			screenCorners[0],
			screenCorners[1],
			screenCorners[2],
			screenCorners[3],
			IM_COL32(255, 215, 40, 55));
		drawList->AddQuad(
			screenCorners[0],
			screenCorners[1],
			screenCorners[2],
			screenCorners[3],
			IM_COL32(255, 215, 40, 255),
			3.0f);

		const auto* placement = FindPropAtCell(m_selectedRow, m_selectedColumn);
		if (placement)
		{
			const float subcellSize =
				kStageCellSize / static_cast<float>(kStagePropSubcellCount);
			const float minX = centerX - halfCell;
			const float minZ = centerZ - halfCell;
			const ImU32 subgridColor = IM_COL32(80, 220, 235, 210);

			// 選択中の親セルだけ3x3へ分割し、配置位置とステージ上の向きを対応させる。
			for (int line = 1; line < kStagePropSubcellCount; ++line)
			{
				const float x = minX + (static_cast<float>(line) * subcellSize);
				ImVec2 start{};
				ImVec2 end{};
				if (ProjectToScreen(
						camera,
						Vec3(x, selectedY, centerZ - halfCell),
						screenWidth,
						screenHeight,
						start) &&
					ProjectToScreen(
						camera,
						Vec3(x, selectedY, centerZ + halfCell),
						screenWidth,
						screenHeight,
						end))
				{
					drawList->AddLine(start, end, subgridColor, 1.5f);
				}

				const float z = minZ + (static_cast<float>(line) * subcellSize);
				if (ProjectToScreen(
						camera,
						Vec3(centerX - halfCell, selectedY, z),
						screenWidth,
						screenHeight,
						start) &&
					ProjectToScreen(
						camera,
						Vec3(centerX + halfCell, selectedY, z),
						screenWidth,
						screenHeight,
						end))
				{
					drawList->AddLine(start, end, subgridColor, 1.5f);
				}
			}

			const Vec3 offset = CalculateStagePropSubcellOffset(
				placement->subRow,
				placement->subColumn,
				kStageCellSize);
			const float subcellCenterX = centerX + offset.x;
			const float subcellCenterZ = centerZ + offset.z;
			const float halfSubcell = subcellSize * 0.5f;
			const Vec3 selectedSubcellCorners[] =
			{
				Vec3(subcellCenterX - halfSubcell, selectedY, subcellCenterZ - halfSubcell),
				Vec3(subcellCenterX + halfSubcell, selectedY, subcellCenterZ - halfSubcell),
				Vec3(subcellCenterX + halfSubcell, selectedY, subcellCenterZ + halfSubcell),
				Vec3(subcellCenterX - halfSubcell, selectedY, subcellCenterZ + halfSubcell),
			};

			ImVec2 selectedSubcellScreenCorners[4]{};
			bool canDrawSelectedSubcell = true;
			for (int i = 0; i < 4; ++i)
			{
				if (!ProjectToScreen(
						camera,
						selectedSubcellCorners[i],
						screenWidth,
						screenHeight,
						selectedSubcellScreenCorners[i]))
				{
					canDrawSelectedSubcell = false;
					break;
				}
			}
			if (canDrawSelectedSubcell)
			{
				drawList->AddQuadFilled(
					selectedSubcellScreenCorners[0],
					selectedSubcellScreenCorners[1],
					selectedSubcellScreenCorners[2],
					selectedSubcellScreenCorners[3],
					IM_COL32(80, 220, 235, 90));
				drawList->AddQuad(
					selectedSubcellScreenCorners[0],
					selectedSubcellScreenCorners[1],
					selectedSubcellScreenCorners[2],
					selectedSubcellScreenCorners[3],
					IM_COL32(80, 220, 235, 255),
					2.5f);
			}
		}
#else
		(void)camera;
		(void)screenWidth;
		(void)screenHeight;
#endif
	}

	bool StageEditor::DrawImGui(GameStage& stage, float screenWidth, float screenHeight)
	{
#if !defined(_DEBUG)
		(void)stage;
		(void)screenWidth;
		(void)screenHeight;
		return false;
#else
		if (!m_active || !m_loaded)
		{
			return false;
		}

		bool reloadStage = false;
		ImGui::SetNextWindowSize(ImVec2(380.0f, 490.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Stage Editor"))
		{
			ImGui::Text("Cell: row %d / column %d", m_selectedRow, m_selectedColumn);
			ImGui::Separator();

			if (IsValidCell(m_selectedRow, m_selectedColumn))
			{
				const int index = GetIndex(m_selectedRow, m_selectedColumn);
				int objectCode = m_objects[index];
				const char* objectNames[] =
				{
					"Empty",
					"Block",
					"Slope Up",
					"Slope Down",
					"Slope Left",
					"Slope Right",
				};

				if (ImGui::Combo(
						"Object",
						&objectCode,
						objectNames,
						static_cast<int>(_countof(objectNames))))
				{
					m_objects[index] = bsmUtil::Clamp(
						objectCode,
						kStageObjectEmpty,
						kStageObjectSlopeRight);
					m_dirty = true;
					m_statusText = "Modified.";
				}

				int height = m_heights[index];
				if (ImGui::SliderInt("Height", &height, 0, kStageHeightMax))
				{
					m_heights[index] = bsmUtil::Clamp(height, 0, kStageHeightMax);
					m_dirty = true;
					m_statusText = "Modified.";
				}

				ImGui::Separator();
				const StagePropPlacement* selectedPlacement =
					FindPropAtCell(m_selectedRow, m_selectedColumn);
				const std::string currentModelName = selectedPlacement
					? NarrowUtf8(selectedPlacement->modelName)
					: "None";
				std::wstring selectedModelName;
				bool removePlacement = false;

				if (ImGui::BeginCombo("Placement", currentModelName.c_str()))
				{
					if (ImGui::Selectable("None", selectedPlacement == nullptr))
					{
						removePlacement = true;
					}

					const StageObjectCategory categories[] =
					{
						StageObjectCategory::Tree,
						StageObjectCategory::Log,
						StageObjectCategory::Rock,
						StageObjectCategory::Stone,
						StageObjectCategory::Plant,
						StageObjectCategory::Mushroom,
					};
					for (const auto category : categories)
					{
						const auto defs = StageObjectCatalog::GetByCategory(category);
						if (defs.empty())
						{
							continue;
						}

						ImGui::Separator();
						ImGui::TextDisabled("%s", GetPropCategoryName(category));
						for (const auto* def : defs)
						{
							if (!def || !IsEditablePropCategory(def->category))
							{
								continue;
							}

							const std::string displayName = NarrowUtf8(def->name);
							const std::string selectableLabel =
								displayName + "##" + NarrowUtf8(def->key);
							const bool isSelected =
								selectedPlacement &&
								selectedPlacement->modelName == def->name;
							if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
							{
								selectedModelName = def->name;
							}
						}
					}
					ImGui::EndCombo();
				}

				if (removePlacement)
				{
					RemovePropAtCell(m_selectedRow, m_selectedColumn);
					m_dirty = true;
					m_statusText = "Modified.";
				}
				else if (!selectedModelName.empty())
				{
					auto* placement = FindPropAtCell(m_selectedRow, m_selectedColumn);
					if (!placement)
					{
						StagePropPlacement newPlacement;
						newPlacement.row = m_selectedRow;
						newPlacement.column = m_selectedColumn;
						newPlacement.modelName = selectedModelName;
						m_propPlacements.push_back(newPlacement);
					}
					else
					{
						placement->modelName = selectedModelName;
					}
					m_dirty = true;
					m_statusText = "Modified.";
				}

				auto* editablePlacement =
					FindPropAtCell(m_selectedRow, m_selectedColumn);
				if (editablePlacement)
				{
					ImGui::TextUnformatted("Position in Cell");
					const char* subcellLabels[kStagePropSubcellCount][kStagePropSubcellCount] =
					{
						{ "NW##PropPos00", "N##PropPos01", "NE##PropPos02" },
						{ "W##PropPos10", "C##PropPos11", "E##PropPos12" },
						{ "SW##PropPos20", "S##PropPos21", "SE##PropPos22" },
					};
					for (int subRow = 0; subRow < kStagePropSubcellCount; ++subRow)
					{
						for (int subColumn = 0; subColumn < kStagePropSubcellCount; ++subColumn)
						{
							const bool isSelected =
								editablePlacement->subRow == subRow &&
								editablePlacement->subColumn == subColumn;
							if (isSelected)
							{
								ImGui::PushStyleColor(
									ImGuiCol_Button,
									ImVec4(0.90f, 0.57f, 0.10f, 1.0f));
								ImGui::PushStyleColor(
									ImGuiCol_ButtonHovered,
									ImVec4(1.0f, 0.68f, 0.18f, 1.0f));
								ImGui::PushStyleColor(
									ImGuiCol_ButtonActive,
									ImVec4(0.78f, 0.45f, 0.05f, 1.0f));
							}

							if (ImGui::Button(
									subcellLabels[subRow][subColumn],
									ImVec2(52.0f, 28.0f)))
							{
								editablePlacement->subRow = subRow;
								editablePlacement->subColumn = subColumn;
								m_dirty = true;
								m_statusText = "Modified.";
							}

							if (isSelected)
							{
								ImGui::PopStyleColor(3);
							}
							if (subColumn + 1 < kStagePropSubcellCount)
							{
								ImGui::SameLine();
							}
						}
					}

					float rotation = editablePlacement->yRotationDegrees;
					if (ImGui::SliderFloat(
							"Y Rotation",
							&rotation,
							0.0f,
							359.0f,
							"%.0f deg"))
					{
						editablePlacement->yRotationDegrees = NormalizeDegrees(rotation);
						m_dirty = true;
						m_statusText = "Modified.";
					}

					if (ImGui::Button("-90 deg", ImVec2(110.0f, 28.0f)))
					{
						editablePlacement->yRotationDegrees =
							NormalizeDegrees(editablePlacement->yRotationDegrees - 90.0f);
						m_dirty = true;
						m_statusText = "Modified.";
					}
					ImGui::SameLine();
					if (ImGui::Button("+90 deg", ImVec2(110.0f, 28.0f)))
					{
						editablePlacement->yRotationDegrees =
							NormalizeDegrees(editablePlacement->yRotationDegrees + 90.0f);
						m_dirty = true;
						m_statusText = "Modified.";
					}
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Save & Apply", ImVec2(142.0f, 34.0f)))
			{
				reloadStage = Save();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload", ImVec2(142.0f, 34.0f)))
			{
				if (Load())
				{
					reloadStage = true;
				}
			}

			if (!m_statusText.empty())
			{
				ImGui::TextUnformatted(m_statusText.c_str());
			}
			if (m_dirty)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f), "*");
			}
		}
		ImGui::End();

		if (!ImGui::GetIO().WantCaptureMouse &&
			App::GetInputDevice().MousePressed(VK_LBUTTON))
		{
			PickCell(stage.GetCamera(), screenWidth, screenHeight);
		}
		DrawGridOverlay(stage.GetCamera(), screenWidth, screenHeight);
		return reloadStage;
#endif
	}

	int StageEditor::GetIndex(int row, int column) const
	{
		return (row * m_columnCount) + column;
	}

	StagePropPlacement* StageEditor::FindPropAtCell(int row, int column)
	{
		const auto found = std::find_if(
			m_propPlacements.begin(),
			m_propPlacements.end(),
			[row, column](const StagePropPlacement& placement)
			{
				return placement.row == row && placement.column == column;
			});
		return found != m_propPlacements.end() ? &(*found) : nullptr;
	}

	const StagePropPlacement* StageEditor::FindPropAtCell(int row, int column) const
	{
		const auto found = std::find_if(
			m_propPlacements.begin(),
			m_propPlacements.end(),
			[row, column](const StagePropPlacement& placement)
			{
				return placement.row == row && placement.column == column;
			});
		return found != m_propPlacements.end() ? &(*found) : nullptr;
	}

	void StageEditor::RemovePropAtCell(int row, int column)
	{
		m_propPlacements.erase(
			std::remove_if(
				m_propPlacements.begin(),
				m_propPlacements.end(),
				[row, column](const StagePropPlacement& placement)
				{
					return placement.row == row && placement.column == column;
				}),
			m_propPlacements.end());
	}

	bool StageEditor::IsValidCell(int row, int column) const
	{
		return row >= 0 &&
			row < m_rowCount &&
			column >= 0 &&
			column < m_columnCount;
	}

}
