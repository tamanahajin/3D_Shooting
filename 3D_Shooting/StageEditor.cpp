#include "stdafx.h"
#include "Project.h"
#include "StageEditor.h"

#if defined(_DEBUG)
#include "imgui.h"
#endif

#include <algorithm>
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

		bool LoadCsvGrid(
			const std::wstring& path,
			std::vector<int>& outValues,
			int& outRows,
			int& outColumns)
		{
			std::ifstream file(NarrowPath(path));
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

			// 改行変換をランタイムへ任せず、保存形式を常にCRLFへ固定する。
			std::ofstream file(
				NarrowPath(path),
				std::ios::binary | std::ios::trunc);
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
		int objectRows = 0;
		int objectColumns = 0;
		int heightRows = 0;
		int heightColumns = 0;

		const std::wstring assets = App::GetRelativeAssetsDir();
		if (!LoadCsvGrid(assets + kObjectCsvPath, objects, objectRows, objectColumns) ||
			!LoadCsvGrid(assets + kHeightCsvPath, heights, heightRows, heightColumns))
		{
			m_statusText = "CSV load failed.";
			return false;
		}
		if (objectRows != heightRows || objectColumns != heightColumns)
		{
			m_statusText = "CSV grid sizes do not match.";
			return false;
		}

		m_objects = std::move(objects);
		m_heights = std::move(heights);
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
		if (!SaveCsvGrid(
				assets + kObjectCsvPath,
				m_objects,
				m_rowCount,
				m_columnCount) ||
			!SaveCsvGrid(
				assets + kHeightCsvPath,
				m_heights,
				m_rowCount,
				m_columnCount))
		{
			m_statusText = "CSV save failed.";
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
		ImGui::SetNextWindowSize(ImVec2(330.0f, 260.0f), ImGuiCond_FirstUseEver);
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

	bool StageEditor::IsValidCell(int row, int column) const
	{
		return row >= 0 &&
			row < m_rowCount &&
			column >= 0 &&
			column < m_columnCount;
	}

}
