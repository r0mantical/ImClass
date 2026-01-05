#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <unordered_set>

inline std::chrono::steady_clock::time_point g_LastClassUpdate = std::chrono::steady_clock::now();
inline constexpr std::chrono::milliseconds CLASS_UPDATE_INTERVAL{ 16 };

inline std::thread g_MemoryReadThread;
inline std::atomic<bool> g_MemoryThreadRunning{ false };

namespace ui {
	extern std::string toHexString(uintptr_t address, int width);
}

template <typename T>
T Read(uintptr_t address);

enum nodeType {
	node_hex8,
	node_hex16,
	node_hex32,
	node_hex64,
	node_int8,
	node_int16,
	node_int32,
	node_int64,
	node_uint8,
	node_uint16,
	node_uint32,
	node_uint64,
	node_float,
	node_double,
	node_vector4,
	node_vector3,
	node_vector2,
	node_vector4d,
	node_vector3d,
	node_vector2d,
	node_matrix4x4,
	node_matrix3x4,
	node_matrix3x3,
	node_bool,
	node_utf8,
	node_utf16,
	node_utf32,
	node_max
};

struct Vector4 {
	float x, y, z, w;
};

struct Vector3 {
	float x, y, z;
};

struct Vector2 {
	float x, y;
};

struct Vector4d {
	double x, y, z, w;
};

struct Vector3d {
	double x, y, z;
};

struct Vector2d {
	double x, y;
};

struct Matrix4x4 {
	float m[4][4];
};

struct Matrix3x4 {
	float m[3][4];
};

struct Matrix3x3 {
	float m[3][3];
};

struct nodeTypeInfo {
	nodeType type;
	uint8_t size;
	char name[21];
	ImColor color;
	char codeName[16];
};

inline nodeTypeInfo nodeData[] = {
	{node_hex8, sizeof(uint8_t), "Hex8", ImColor(255, 255, 255)},
	{node_hex16, sizeof(uint16_t), "Hex16", ImColor(255, 255, 255)},
	{node_hex32, sizeof(uint32_t), "Hex32", ImColor(255, 255, 255)},
	{node_hex64, sizeof(uint64_t), "Hex64", ImColor(255, 255, 255)},
	{node_int8, sizeof(int8_t), "Int8", ImColor(255, 200, 0), "int8_t"},
	{node_int16, sizeof(int16_t), "Int16", ImColor(255, 200, 0), "int16_t"},
	{node_int32, sizeof(int32_t), "Int32", ImColor(255, 200, 0), "int"},
	{node_int64, sizeof(int64_t), "Int64", ImColor(255, 200, 0), "int64_t"},
	{node_uint8, sizeof(uint8_t), "UInt8", ImColor(7, 247, 163), "uint8_t"},
	{node_uint16, sizeof(uint16_t), "UInt16", ImColor(7, 247, 163), "uint16_t"},
	{node_uint32, sizeof(uint32_t), "UInt32", ImColor(7, 247, 163), "uint32_t"},
	{node_uint64, sizeof(uint64_t), "UInt64", ImColor(7, 247, 163), "uint64_t"},
	{node_float, sizeof(float), "Float", ImColor(225, 143, 255), "float"},
	{node_double, sizeof(double), "Double", ImColor(187, 0, 255), "double"},
	{node_vector4, sizeof(Vector4), "Vector4", ImColor(115, 255, 124), "Vector4"},
	{node_vector3, sizeof(Vector3), "Vector3", ImColor(115, 255, 124), "Vector3"},
	{node_vector2, sizeof(Vector2), "Vector2", ImColor(115, 255, 124), "Vector2"},
	{node_vector4d, sizeof(Vector4d), "Vector4d", ImColor(95, 235, 104), "Vector4d"},
	{node_vector3d, sizeof(Vector3d), "Vector3d", ImColor(95, 235, 104), "Vector3d"},
	{node_vector2d, sizeof(Vector2d), "Vector2d", ImColor(95, 235, 104), "Vector2d"},
	{node_matrix4x4, sizeof(Matrix4x4), "Matrix4x4", ImColor(3, 252, 144), "matrix4x4_t"},
	{node_matrix3x4, sizeof(Matrix3x4), "Matrix3x4", ImColor(3, 252, 144), "matrix3x4_t"},
	{node_matrix3x3, sizeof(Matrix3x3), "Matrix3x3", ImColor(3, 252, 144), "matrix3x3_t"},
	{node_bool, sizeof(bool), "Bool", ImColor(0, 183, 255), "bool"},
	{node_utf8, 64, "UTF8", ImColor(252, 186, 3), "char[]"},
	{node_utf16, 64, "UTF16", ImColor(252, 186, 3), "wchar_t[]"},
	{node_utf32, 64, "UTF32", ImColor(252, 186, 3), "char32_t[]"}
};

inline bool g_HoveringPointer = false;
inline bool g_InPopup = false;
inline size_t g_SelectedClass = 0;

class nodeBase {
public:
	char name[64];
	nodeType type;
	uint8_t size;
	bool selected = false;
	int absoluteOffset = -1;
	bool isLocked = false;

	nodeBase(const char* aName, nodeType aType, bool aSelected = false, int offset = -1) {
		memset(name, 0, sizeof(name));
		if (aName) {
			memcpy(name, aName, sizeof(name));
		}

		type = aType;
		size = nodeData[aType].size;
		selected = aSelected;
		absoluteOffset = offset;
		isLocked = (offset != -1);
	}
};

inline int g_nameCounter = 0;



class uClass {
public:
	char name[64];
	char addressInput[256];
	std::vector<nodeBase> nodes;
	uintptr_t address = 0;
	int varCounter = 0;
	size_t size;
	BYTE* data = 0;
	float cur_pad = 0;
	std::vector<float> totalHeight;
	size_t lastNodeCount = 0;
	size_t lastTypeHash = 0;

	uClass(int nodeCount, bool incrementCounter = true) {
		size = 0;

		for (int i = 0; i < nodeCount; i++) {
			nodeBase node = { 0, mem::x32 ? node_hex32 : node_hex64 };
			nodes.push_back(node);
			size += nodeData[node.type].size;
		}
		memset(name, 0, sizeof(name));
		memset(addressInput, 0, sizeof(addressInput));
		addressInput[0] = '0';

		std::string newName = "Class_" + std::to_string(g_nameCounter);
		memcpy(name, newName.data(), newName.size());

		data = (BYTE*)malloc(size);

		if (data) {
			memset(data, 0, size);
		}
		else {
			MessageBoxA(0, "Failed to allocate memory!", "ERROR", MB_ICONERROR);
		}

		if (incrementCounter) {
			g_nameCounter++;
		}
	}

	void sizeToNodes();
	void resize(int size);
	void normalizeNodes();
	void drawNodes();
	void drawStringBytes(int i, const BYTE* data, int pos, int size);
	void drawOffset(int i, int pos);
	void drawAddress(int i, int pos) const;
	void drawBytes(int i, BYTE* data, int pos, int size);
	void drawNumber(int i, int64_t num);
	void drawFloat(int i, float num);
	void drawDouble(int i, double num);
	void drawHexNumber(int i, uintptr_t num, uintptr_t* ptrOut = 0);
	void drawControllers(int i, int counter);
	void changeType(int i, nodeType newType, bool selectNew = false, int* newNodes = 0);
	void changeType(nodeType newType);

	int drawVariableName(int i, nodeType type);
	void copyPopup(int i, std::string toCopy, std::string id);
	void drawInteger(int i, int64_t value, nodeType type);
	void drawUInteger(int i, uint64_t value, nodeType type);
	void drawFloatVar(int i, float value);
	void drawDoubleVar(int i, double value);
	void drawVector4(int i, Vector4& value);
	void drawVector3(int i, Vector3& value);
	void drawVector2(int i, Vector2& value);
	void drawVector4d(int i, Vector4d& value);
	void drawVector3d(int i, Vector3d& value);
	void drawVector2d(int i, Vector2d& value);
	void drawUTF8(int i, const char* str);
	void drawUTF16(int i, const wchar_t* str);
	void drawUTF32(int i, const char32_t* str);
	template<typename MatrixType>
	void drawMatrixText(float xPadding, int rows, int columns, const MatrixType& value);
	void drawMatrix4x4(int i, const Matrix4x4& value);
	void drawMatrix3x4(int i, const Matrix3x4& value);
	void drawMatrix3x3(int i, const Matrix3x3& value);
	void drawBool(int i, bool value);

	std::string exportClass();

	void recalculateHeights();
};

inline uClass g_PreviewClass(15);
inline std::vector<uClass> g_Classes = { uClass(50) };


inline void MemoryReadThreadFunc() {
	while (g_MemoryThreadRunning) {
		auto now = std::chrono::steady_clock::now();

		if (mem::activeProcess) {
			// Collect all addresses that need reading
			std::vector<std::pair<uintptr_t, size_t>> read_requests;

			for (auto& uClass : g_Classes) {
				if (uClass.address != 0 && uClass.size > 0) {
					read_requests.push_back({ uClass.address, uClass.size });
					logger::addLog("[MemThread] Class addr: 0x" + std::to_string(uClass.address) + " size: " + std::to_string(uClass.size));
				}
			}

			// Add preview class if it has an address
			if (g_PreviewClass.address != 0 && g_PreviewClass.size > 0) {
				read_requests.push_back({ g_PreviewClass.address, g_PreviewClass.size });
			}

			logger::addLog("[MemThread] Reading " + std::to_string(read_requests.size()) + " classes");

			// Clear old snapshots that are no longer needed
			{
				std::lock_guard<std::mutex> lock(mem::g_MemoryMutex);
				std::unordered_set<uintptr_t> active_addresses;
				for (auto& [addr, size] : read_requests) {
					active_addresses.insert(addr);
				}

				// Remove stale entries
				for (auto it = mem::g_MemorySnapshots.begin(); it != mem::g_MemorySnapshots.end();) {
					if (active_addresses.find(it->first) == active_addresses.end()) {
						logger::addLog("[MemThread] Removing stale cache for: 0x" + std::to_string(it->first));
						it = mem::g_MemorySnapshots.erase(it);
					}
					else {
						++it;
					}
				}
			}

			// Read memory for all classes - USE BLOCKING READ
			for (auto& [addr, size] : read_requests) {
				std::vector<uint8_t> buffer(size);
				if (mem::read_blocking(addr, buffer.data(), size)) {
					std::lock_guard<std::mutex> lock(mem::g_MemoryMutex);
					mem::g_MemorySnapshots[addr] = std::move(buffer);
					logger::addLog("[MemThread] Updated cache for: 0x" + std::to_string(addr) + " (" + std::to_string(size) + " bytes)");
				}
				else {
					logger::addLog("[MemThread] FAILED to read: 0x" + std::to_string(addr));
				}
			}
		}

		g_LastClassUpdate = now;

		// Sleep for the update interval
		std::this_thread::sleep_for(CLASS_UPDATE_INTERVAL);
	}

	logger::addLog("[MemThread] Thread stopped");
}

inline void uClass::normalizeNodes() {
	std::vector<nodeBase> newNodes;
	int currentOffset = 0;

	for (size_t i = 0; i < nodes.size(); i++) {
		auto& node = nodes[i];

		// If this node is locked to an absolute offset and we haven't reached it yet
		if (node.isLocked && node.absoluteOffset > currentOffset) {
			int paddingNeeded = node.absoluteOffset - currentOffset;

			// Insert padding to reach the locked offset
			while (paddingNeeded > 0) {
				if (paddingNeeded >= 8) {
					newNodes.emplace_back(nullptr, node_hex64, false, -1);
					paddingNeeded -= 8;
					currentOffset += 8;
				}
				else if (paddingNeeded >= 4) {
					newNodes.emplace_back(nullptr, node_hex32, false, -1);
					paddingNeeded -= 4;
					currentOffset += 4;
				}
				else if (paddingNeeded >= 2) {
					newNodes.emplace_back(nullptr, node_hex16, false, -1);
					paddingNeeded -= 2;
					currentOffset += 2;
				}
				else {
					newNodes.emplace_back(nullptr, node_hex8, false, -1);
					paddingNeeded -= 1;
					currentOffset += 1;
				}
			}
		}

		newNodes.push_back(node);
		currentOffset += node.size;
	}

	nodes = newNodes;
	sizeToNodes();
}

inline std::string uClass::exportClass() {
	normalizeNodes();

	std::string exportedClass = std::format("class {} {{\npublic:", name);
	int pad = 0;
	for (auto& node : nodes) {
		if (node.type <= node_hex64) {
			pad += node.size;
			continue;
		}
		else {
			if (pad > 0) {
				std::string var = std::format("\tBYTE pad[{}];", pad);
				exportedClass = exportedClass + "\n" + var;
				pad = 0;
			}
		}
		auto& data = nodeData[node.type];
		std::string var = std::format("\t{} {};", data.codeName, node.name);
		exportedClass = exportedClass + "\n" + var;
	}
	exportedClass += "\n};";

	return exportedClass;
}

inline void uClass::sizeToNodes() {
	size_t szNodes = 0;
	for (auto& node : nodes) {
		szNodes += node.size;
	}

	auto newData = (BYTE*)realloc(data, szNodes);
	if (!newData) {
		MessageBoxA(0, "Failed to reallocate memory!", "ERROR", MB_ICONERROR);
		return;
	}

	data = newData;
	size = szNodes;
}

inline void uClass::resize(int mod) {
	assert(mod > 0 || mod == -8);

	int newSize = size + mod;
	if (newSize < 1) {
		return;
	}

	if (mod < 0) {
		nodes.erase(nodes.begin() + nodes.size() - 1);
		normalizeNodes();
	}
	else {
		int remaining = mod;
		while (remaining > 0) {
			if (remaining >= 8) {
				remaining = remaining - 8;
				nodes.emplace_back(nodeBase(nullptr, node_hex64, false));
			}
			else if (remaining >= 4) {
				remaining = remaining - 4;
				nodes.emplace_back(nodeBase(nullptr, node_hex32, false));
			}
			else if (remaining >= 2) {
				remaining = remaining - 2;
				nodes.emplace_back(nodeBase(nullptr, node_hex16, false));
			}
			else if (remaining >= 1) {
				remaining = remaining - 1;
				nodes.emplace_back(nodeBase(nullptr, node_hex8, false));
			}
		}
		normalizeNodes();
		memset(data, 0, size);
	}
}

inline void uClass::copyPopup(int i, std::string toCopy, std::string id) {
	std::string sID = "copyvar_" + id + std::to_string(i);

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
		g_InPopup = true;
		ImGui::OpenPopup(sID.c_str());
	}

	if (ImGui::BeginPopup(sID.c_str())) {
		g_InPopup = true;
		if (ImGui::Selectable("Copy value")) {
			ImGui::SetClipboardText(toCopy.c_str());
		}
		ImGui::EndPopup();
	}
}

inline int uClass::drawVariableName(int i, nodeType type) {
	auto& node = nodes[i];
	ImVec2 nameSize = ImGui::CalcTextSize(node.name);

	auto& lData = nodeData[type];
	auto typeName = lData.name;
	ImVec2 typenameSize = ImGui::CalcTextSize(typeName);

	ImGui::PushStyleColor(ImGuiCol_Text, lData.color.Value);
	ImGui::SetCursorPos(ImVec2(180, 0));
	ImGui::Text("%s", typeName);
	ImGui::PopStyleColor();

	// Show lock icon if node is locked
	if (node.isLocked) {
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
		ImGui::Text("[L]");
		ImGui::PopStyleColor();
	}

	static char renameBuf[64] = { 0 };
	static int renamedNode = -1;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.9f, .9f, .9f, 1.f));
	ImGui::SetCursorPos(ImVec2(180 + typenameSize.x + 15 + (node.isLocked ? 25 : 0), 0));
	if (renamedNode != i) {
		ImGui::Text(node.name);

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			renamedNode = i;
			memcpy(renameBuf, node.name, sizeof(renameBuf));
		}
	}
	else {
		ImGui::SetKeyboardFocusHere();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 1));
		ImGui::SetNextItemWidth(200);
		nameSize.x = 200;
		if (ImGui::InputText("##RenameNode", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
			memcpy(node.name, renameBuf, sizeof(renameBuf));
			renamedNode = -1;
		}
		ImGui::PopStyleVar();

		if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			renamedNode = -1;
		}
	}
	ImGui::PopStyleColor();

	return typenameSize.x + nameSize.x + (node.isLocked ? 25 : 0);
}

inline void uClass::drawBool(int i, bool value) {
	float xPad = static_cast<float>(drawVariableName(i, node_bool));
	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::Text(value ? "=  true" : "=  false");
}

inline void uClass::drawUTF8(int i, const char* str) {
	float xPad = static_cast<float>(drawVariableName(i, node_utf8));

	std::string displayStr(str);
	if (displayStr.length() > 60) {
		displayStr = displayStr.substr(0, 60) + "...";
	}

	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(252, 186, 3).Value);
	ImGui::Text("=  \"%s\"", displayStr.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, str, "utf8");
}

inline void uClass::drawUTF16(int i, const wchar_t* str) {
	float xPad = static_cast<float>(drawVariableName(i, node_utf16));

	std::wstring wstr(str);
	std::string displayStr(wstr.begin(), wstr.end());
	if (displayStr.length() > 60) {
		displayStr = displayStr.substr(0, 60) + "...";
	}

	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(252, 186, 3).Value);
	ImGui::Text("=  \"%s\"", displayStr.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, displayStr, "utf16");
}

inline void uClass::drawUTF32(int i, const char32_t* str) {
	float xPad = static_cast<float>(drawVariableName(i, node_utf32));

	std::u32string u32str(str);
	std::string displayStr;
	for (char32_t c : u32str) {
		if (c < 128) displayStr += static_cast<char>(c);
		else displayStr += '?';
	}
	if (displayStr.length() > 60) {
		displayStr = displayStr.substr(0, 60) + "...";
	}

	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(252, 186, 3).Value);
	ImGui::Text("=  \"%s\"", displayStr.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, displayStr, "utf32");
}

template<typename MatrixType>
inline void uClass::drawMatrixText(float xPadding, int rows, int columns, const MatrixType& value)
{
	float mPad = 0;
	float maxWidth = 0.f;
	float y = 0;

	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			float val = value.m[i][j];
			float currentWidth = ImGui::CalcTextSize(std::format("{:.3f}", val).c_str()).x;
			maxWidth = max(currentWidth, maxWidth);
		}
	}

	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			float val = value.m[i][j];
			ImGui::SetCursorPos(ImVec2(180 + xPadding + 30 + mPad, y));
			ImGui::Text("%.3f", val);
			mPad += maxWidth;
		}

		y += 15;
		mPad = 0;
	}
}

inline void uClass::drawMatrix4x4(int i, const Matrix4x4& value) {
	float xPad = static_cast<float>(drawVariableName(i, node_matrix4x4));
	drawMatrixText(xPad, 4, 4, value);
}

inline void uClass::drawMatrix3x4(int i, const Matrix3x4& value) {
	float xPad = static_cast<float>(drawVariableName(i, node_matrix3x4));
	drawMatrixText(xPad, 3, 4, value);
}

inline void uClass::drawMatrix3x3(int i, const Matrix3x3& value) {
	float xPad = static_cast<float>(drawVariableName(i, node_matrix3x3));
	drawMatrixText(xPad, 3, 3, value);
}

inline void uClass::drawVector4(int i, Vector4& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector4));
	std::string vec = std::format("{:.3f}, {:.3f}, {:.3f}, {:.3f}", value.x, value.y, value.z, value.w);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30.f, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec4");
}

inline void uClass::drawVector3(int i, Vector3& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector3));
	std::string vec = std::format("{:.3f}, {:.3f}, {:.3f}", value.x, value.y, value.z);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec3");
}

inline void uClass::drawVector2(int i, Vector2& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector2));
	std::string vec = std::format("{:.3f}, {:.3f}", value.x, value.y);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec2");
}

inline void uClass::drawVector4d(int i, Vector4d& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector4d));
	std::string vec = std::format("{:.6f}, {:.6f}, {:.6f}, {:.6f}", value.x, value.y, value.z, value.w);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30.f, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec4d");
}

inline void uClass::drawVector3d(int i, Vector3d& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector3d));
	std::string vec = std::format("{:.6f}, {:.6f}, {:.6f}", value.x, value.y, value.z);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec3d");
}

inline void uClass::drawVector2d(int i, Vector2d& value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_vector2d));
	std::string vec = std::format("{:.6f}, {:.6f}", value.x, value.y);
	std::string toDraw = std::format("=  ({})", vec);
	ImGui::SetCursorPos(ImVec2(180 + xPad + 30, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, vec, "vec2d");
}

inline void uClass::drawFloatVar(int i, float value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_float));
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30, 0));
	ImGui::Text("=  %.3f", value);
	copyPopup(i, std::to_string(value), "float");
}

inline void uClass::drawDoubleVar(int i, double value) {
	const float xPad = static_cast<float>(drawVariableName(i, node_double));
	std::string toDraw = std::format("=  {:.6f}", value);
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30, 0));
	ImGui::Text("%s", toDraw.c_str());
	copyPopup(i, std::to_string(value), "double");
}

inline void uClass::drawInteger(int i, int64_t value, nodeType type) {
	const float xPad = static_cast<float>(drawVariableName(i, type));
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30.f, 0));
	ImGui::Text("=  %lld", value);
	copyPopup(i, std::to_string(value), "int");
}

inline void uClass::drawUInteger(int i, uint64_t value, nodeType type) {
	const float xPad = static_cast<float>(drawVariableName(i, type));
	ImGui::SetCursorPos(ImVec2(180.f + xPad + 30.f, 0));
	ImGui::Text("=  %llu", value);
	copyPopup(i, std::to_string(value), "uint");
}

inline void uClass::changeType(nodeType newType) {
	// Calculate current offset for each selected node
	int currentOffset = 0;
	for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
		auto& node = nodes[i];
		if (node.selected) {
			// Lock this node to its current offset
			node.absoluteOffset = currentOffset;
			node.isLocked = true;

			int newNodes;
			changeType(i, newType, true, &newNodes);
			i += newNodes;

			// Update currentOffset after the change
			currentOffset = 0;
			for (int j = 0; j <= i; j++) {
				currentOffset += nodes[j].size;
			}
		}
		else {
			currentOffset += node.size;
		}
	}
	normalizeNodes();
}

inline void uClass::changeType(int i, nodeType newType, bool selectNew, int* newNodes) {
	auto node = this->nodes[i];
	auto oldSize = node.size;
	auto typeSize = nodeData[newType].size;

	// Calculate current offset for this node
	int currentOffset = 0;
	for (int j = 0; j < i; j++) {
		currentOffset += nodes[j].size;
	}

	nodes.erase(nodes.begin() + i);

	// Create new node with locked offset if this node had a name
	bool shouldLock = (strlen(node.name) > 0 && node.name[0] != 0) || node.isLocked;
	nodeBase newNode = {
		(newType > node_hex64) ? ("Var_" + std::to_string(varCounter++)).c_str() : nullptr,
		newType,
		selectNew,
		shouldLock ? (node.isLocked ? node.absoluteOffset : currentOffset) : -1
	};
	newNode.isLocked = shouldLock;

	// Copy name if it was a named variable
	if (strlen(node.name) > 0 && newType > node_hex64) {
		memcpy(newNode.name, node.name, sizeof(node.name));
	}

	nodes.insert(nodes.begin() + i, newNode);
	int inserted = 1;

	int sizeDiff = oldSize - typeSize;
	while (sizeDiff > 0) {
		if (sizeDiff >= 4) {
			sizeDiff = sizeDiff - 4;
			nodes.insert(nodes.begin() + i + inserted++, { nullptr, node_hex32, selectNew, -1 });
		}
		else if (sizeDiff >= 2) {
			sizeDiff = sizeDiff - 2;
			nodes.insert(nodes.begin() + i + inserted++, { nullptr, node_hex16, selectNew, -1 });
		}
		else if (sizeDiff >= 1) {
			sizeDiff = sizeDiff - 1;
			nodes.insert(nodes.begin() + i + inserted++, { nullptr, node_hex8, selectNew, -1 });
		}
	}

	lastTypeHash = 0;

	if (newNodes) {
		*newNodes = inserted - 1;
	}
}

// ... (continue with drawHexNumber, drawDouble, drawFloat, drawNumber, drawOffset, drawAddress, drawBytes, drawStringBytes - these stay the same)

inline void uClass::drawHexNumber(int i, uintptr_t num, uintptr_t* ptrOut) {
	cur_pad += 15;

	ImColor color = ImColor(255, 162, 0);

	std::string numText = ui::toHexString(num, 0);

	std::string targetAddress = ("0x" + numText);
	std::string toDraw;

	pointerInfo info;
	bool isPointer = mem::isPointer(num, &info);
	if (isPointer) {
		color = ImColor(255, 0, 0);

		if (info.moduleName == "") {
			toDraw = "[heap] " + targetAddress;
		}
		else {
			auto exportIt = mem::g_ExportMap.find(num);
			if (exportIt != mem::g_ExportMap.end()) {
				color = ImColor(0, 255, 0);
				toDraw = "[EXPORT] " + exportIt->second + " " + targetAddress;
			}
			else {
				toDraw = std::format("[{}] {} {}", info.section, info.moduleName, targetAddress);
			}
		}

		std::string rttiNames;
		if (mem::rttiInfo(num, rttiNames)) {
			toDraw += rttiNames;
		}
	}

	ImVec2 textSize = ImGui::CalcTextSize(toDraw.c_str());

	ImGui::SetCursorPos(ImVec2(455 + cur_pad, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, color.Value);
	ImGui::Text("%s", toDraw.c_str());
	ImGui::PopStyleColor();

	if (isPointer) {
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			bool found = false;
			for (size_t j = 0; j < g_Classes.size(); j++) {
				auto& lClass = g_Classes[j];
				if (lClass.address == num) {
					g_SelectedClass = j;
					found = true;
					break;
				}
			}

			if (!found) {
				if (ptrOut) {
					*ptrOut = num;
				}
			}
		}
	}

	copyPopup(i, numText, "hex");

	if (isPointer) {
		ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
		if (ImGui::IsItemHovered()) {
			g_HoveringPointer = true;
		}

		if (ImGui::BeginItemTooltip()) {
			ImGui::BeginChild("MemPreview_Child", ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);

			float mWheel = ImGui::GetIO().MouseWheel;
			if (mWheel > 0) {
				g_PreviewClass.resize(-8);
			}
			else if (mWheel < 0) {
				g_PreviewClass.resize(8);
			}

			g_PreviewClass.address = num;
			g_PreviewClass.drawNodes();

			ImGui::EndChild();
			ImGui::EndTooltip();
		}
	}

	auto buf = Read<readBuf<64>>(num);
	bool isString = true;
	for (int it = 0; it < 4; it++) {
		if (buf.data[it] < 21 || buf.data[it] > 126) {
			isString = false;
			break;
		}
	}

	if (isString) {
		std::string stringDraw = std::format("'{}'", (char*)buf.data);
		ImGui::SetCursorPos(ImVec2(455.f + cur_pad + textSize.x + 15, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImColor(3, 252, 140).Value);
		ImGui::Text("%s", stringDraw.c_str());
		ImGui::PopStyleColor();
	}
}

inline void uClass::drawDouble(int i, double num) {
	std::string toDraw = std::format("{:.3f}", num);
	if (toDraw.size() > 20) {
		toDraw = "#####";
	}

	cur_pad += ImGui::CalcTextSize(toDraw.c_str()).x;

	ImGui::SetCursorPos(ImVec2(455, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(163, 255, 240).Value);
	ImGui::Text("%s", toDraw.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, toDraw, "dbl");
}

inline void uClass::drawFloat(int i, float num) {
	std::string toDraw = std::format("{:.3f}", num);
	if (toDraw.size() > 20) {
		toDraw = "#####";
	}

	cur_pad += ImGui::CalcTextSize(toDraw.c_str()).x;

	ImGui::SetCursorPos(ImVec2(455, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(163, 255, 240).Value);
	ImGui::Text("%s", toDraw.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, toDraw, "flt");
}

inline void uClass::drawNumber(int i, int64_t num) {
	if (cur_pad > 0) {
		cur_pad += 15;
	}

	std::string toDraw = std::to_string(num);

	ImGui::SetCursorPos(ImVec2(455 + cur_pad, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255, 218, 133).Value);
	ImGui::Text("%s", toDraw.c_str());
	ImGui::PopStyleColor();

	copyPopup(i, toDraw, "num");

	cur_pad += ImGui::CalcTextSize(toDraw.c_str()).x;
}

inline void uClass::drawOffset(int i, int pos) {
	ImGui::SetCursorPos(ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.9f, .9f, .9f, 1.f));
	ImGui::Text(ui::toHexString(pos, 4).c_str());
	ImGui::PopStyleColor();
}

inline void uClass::drawAddress(int i, int pos) const {
	ImGui::SetCursorPos(ImVec2(50, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.7f, .7f, .7f, 1.f));
	ImGui::Text("%s", ui::toHexString(this->address + pos, 16).c_str());
	ImGui::PopStyleColor();
}

inline void uClass::drawBytes(int i, BYTE* data, int pos, int size) {
	for (int j = 0; j < size; j++) {
		BYTE byte = data[pos];
		ImGui::SetCursorPos(ImVec2(285 + j * 20, 0));
		ImGui::Text("%s", ui::toHexString(byte, 2).c_str());
		pos++;
	}
}

inline void uClass::drawStringBytes(int i, const BYTE* lData, int pos, int lSize) {
	for (int j = 0; j < lSize; j++) {
		BYTE byte = lData[pos];
		ImGui::SetCursorPos(ImVec2(180.f + static_cast<float>(j) * 12.f, 0));
		if (byte > 32 && byte < 127) {
			ImGui::Text("%c", byte);
		}
		else if (byte != 0) {
			ImGui::Text(".");
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.3f, .3f, .3f, 1.f));
			ImGui::Text(".");
			ImGui::PopStyleColor();
		}
		pos++;
	}
}

inline bool showModuleMissingPopup = false;

inline void uClass::drawControllers(int i, int counter) {
	auto& node = nodes[i];

	ImVec2 parentSize = ImGui::GetContentRegionAvail();
	float h = ImGui::GetCursorPosY() - 2;

	ImGui::SetCursorPos(ImVec2(0, 0));
	if (ImGui::Selectable(("##Controller_" + std::to_string(i) + std::to_string(h)).c_str(), node.selected, 0, ImVec2(parentSize.x, h))) {
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
			node.selected = !node.selected;
		}
		else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
			int min = INT_MAX;
			for (size_t j = 0; j < nodes.size(); j++) {
				if (nodes[j].selected) {
					min = min(min, static_cast<int>(j));
				}
			}

			if (min > i) {
				for (int j = min; j >= i; j--) {
					nodes[j].selected = true;
				}
			}
			else {
				for (int j = min; j <= i; j++) {
					nodes[j].selected = true;
				}
			}
		}
		else {
			for (auto& lNode : nodes) {
				lNode.selected = false;
			}
			nodes[i].selected = true;
		}
	}

	std::string sID = "##NodePopup_" + std::to_string(i);
	if (!g_InPopup && ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
		ImGui::OpenPopup(sID.c_str());
	}

	if (ImGui::BeginPopup(sID.c_str())) {
		if (!node.selected) {
			for (auto& lNode : nodes) {
				lNode.selected = false;
			}
			nodes[i].selected = true;
		}

		if (ImGui::BeginMenu("Change Type")) {
			if (ImGui::Selectable("Hex8", false, 0, ImVec2(100, 0))) {
				changeType(node_hex8);
			}
			if (ImGui::Selectable("Hex16")) {
				changeType(node_hex16);
			}
			if (ImGui::Selectable("Hex32")) {
				changeType(node_hex32);
			}
			if (ImGui::Selectable("Hex64")) {
				changeType(node_hex64);
			}

			ImGui::Separator();

			if (ImGui::Selectable("Int64")) {
				changeType(node_int64);
			}
			if (ImGui::Selectable("Int32")) {
				changeType(node_int32);
			}
			if (ImGui::Selectable("Int16")) {
				changeType(node_int16);
			}
			if (ImGui::Selectable("Int8")) {
				changeType(node_int8);
			}

			ImGui::Separator();

			if (ImGui::Selectable("UInt64")) {
				changeType(node_uint64);
			}
			if (ImGui::Selectable("UInt32")) {
				changeType(node_uint32);
			}
			if (ImGui::Selectable("UInt16")) {
				changeType(node_uint16);
			}
			if (ImGui::Selectable("UInt8")) {
				changeType(node_uint8);
			}

			ImGui::Separator();

			if (ImGui::Selectable("Float")) {
				changeType(node_float);
			}
			if (ImGui::Selectable("Double")) {
				changeType(node_double);
			}
			if (ImGui::Selectable("Bool")) {
				changeType(node_bool);
			}

			ImGui::Separator();

			if (ImGui::Selectable("Vector4")) {
				changeType(node_vector4);
			}
			if (ImGui::Selectable("Vector3")) {
				changeType(node_vector3);
			}
			if (ImGui::Selectable("Vector2")) {
				changeType(node_vector2);
			}

			ImGui::Separator();

			if (ImGui::Selectable("Vector4d")) {
				changeType(node_vector4d);
			}
			if (ImGui::Selectable("Vector3d")) {
				changeType(node_vector3d);
			}
			if (ImGui::Selectable("Vector2d")) {
				changeType(node_vector2d);
			}

			ImGui::Separator();

			if (ImGui::Selectable("Matrix4x4")) {
				changeType(node_matrix4x4);
			}
			if (ImGui::Selectable("Matrix3x4")) {
				changeType(node_matrix3x4);
			}
			if (ImGui::Selectable("Matrix3x3")) {
				changeType(node_matrix3x3);
			}

			ImGui::Separator();

			if (ImGui::Selectable("UTF8")) {
				changeType(node_utf8);
			}
			if (ImGui::Selectable("UTF16")) {
				changeType(node_utf16);
			}
			if (ImGui::Selectable("UTF32")) {
				changeType(node_utf32);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add Bytes")) {
			if (ImGui::Selectable("4 Bytes")) {
				resize(4);
			}
			if (ImGui::Selectable("8 Bytes")) {
				resize(8);
			}
			if (ImGui::Selectable("64 Bytes")) {
				resize(64);
			}
			if (ImGui::Selectable("512 Bytes")) {
				resize(512);
			}
			if (ImGui::Selectable("1024 Bytes")) {
				resize(1024);
			}
			if (ImGui::Selectable("4096 Bytes")) {
				resize(4096);
			}
			if (ImGui::Selectable("8192 Bytes")) {
				resize(8192);
			}
			ImGui::EndMenu();
		}

		if (ImGui::Selectable("Delete")) {
			for (int j = static_cast<int>(nodes.size()) - 1; j >= 0; j--) {
				if (nodes[j].selected) {
					nodes.erase(nodes.begin() + j);
				}
			}
			normalizeNodes();
		}

		// Add lock/unlock option
		if (node.type > node_hex64) {
			ImGui::Separator();
			if (node.isLocked) {
				if (ImGui::Selectable("Unlock Offset")) {
					node.isLocked = false;
					node.absoluteOffset = -1;
				}
			}
			else {
				if (ImGui::Selectable("Lock Offset")) {
					node.isLocked = true;
					node.absoluteOffset = counter;
					normalizeNodes();
				}
			}
		}

		if (ImGui::BeginMenu("Copy")) {
			uintptr_t fullAddress = this->address + counter;

			if (ImGui::Selectable("Address")) {
				ImGui::SetClipboardText(ui::toHexString(fullAddress, 0).c_str());
			}

			if (ImGui::Selectable("Offset")) {
				ImGui::SetClipboardText(ui::toHexString(counter, 0).c_str());
			}

			if (ImGui::Selectable("RVA")) {
				mem::getModules();
				bool foundIt = false;

				for (auto module : mem::moduleList)
				{
					if (fullAddress >= module.base && fullAddress <= module.base + module.size)
					{
						foundIt = true;
						ImGui::SetClipboardText(ui::toHexString(fullAddress - module.base, 0).c_str());
						break;
					}
				}

				if (!foundIt) {
					showModuleMissingPopup = true;
				}
			}

			if (ImGui::Selectable("Full RVA")) {
				mem::getModules();
				bool foundIt = false;

				for (auto module : mem::moduleList)
				{
					if (fullAddress >= module.base && fullAddress <= module.base + module.size)
					{
						foundIt = true;
						std::string fullName = std::format("{} + 0x{:X}", module.name.c_str(), fullAddress - module.base);
						ImGui::SetClipboardText(fullName.c_str());
						break;
					}
				}

				if (!foundIt) {
					showModuleMissingPopup = true;
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
}

inline void uClass::recalculateHeights() {
	float baseHeight = ImGui::GetTextLineHeightWithSpacing();

	totalHeight.resize(nodes.size() + 1);
	totalHeight[0] = 0.0f;

	for (size_t i = 0; i < nodes.size(); i++) {
		float nodeHeight = baseHeight;

		switch (nodes[i].type) {
		case node_matrix4x4:
			nodeHeight = 15.0f * 4.0f + 8.0f;
			break;
		case node_matrix3x4:
		case node_matrix3x3:
			nodeHeight = 15.0f * 3.0f + 8.0f;
			break;
		default:
			break;
		}

		totalHeight[i + 1] = totalHeight[i] + nodeHeight;
	}
}

inline void uClass::drawNodes() {
	// Copy memory snapshot from background thread
	bool foundCache = false;
	{
		std::lock_guard<std::mutex> lock(mem::g_MemoryMutex);
		auto it = mem::g_MemorySnapshots.find(this->address);
		if (it != mem::g_MemorySnapshots.end() && it->second.size() == this->size) {
			memcpy(this->data, it->second.data(), this->size);
			foundCache = true;
		}
	}

	// ADD THIS - log once every 60 frames
	static int frameCounter = 0;
	if (frameCounter++ % 60 == 0) {
		logger::addLog("[DrawNodes] Cache " + std::string(foundCache ? "FOUND" : "NOT FOUND") +
			" for 0x" + std::to_string(this->address) +
			" size: " + std::to_string(this->size));
	}

	ImVec2 parentSize = ImGui::GetContentRegionAvail();

	uintptr_t clickedPointer = 0;

	size_t typeHash = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		typeHash ^= (static_cast<size_t>(nodes[i].type) << (i % 8));
	}

	if (lastNodeCount != nodes.size() || lastTypeHash != typeHash) {
		recalculateHeights();
		lastNodeCount = nodes.size();
		lastTypeHash = typeHash;
	}

	float scrollY = ImGui::GetScrollY();
	float windowHeight = ImGui::GetWindowHeight();

	int startIdx = 0, endIdx = nodes.size();

	for (int i = 0; i < nodes.size(); i++) {
		if (totalHeight[i + 1] >= scrollY) {
			startIdx = i;
			break;
		}
	}

	for (int i = startIdx; i < nodes.size(); i++) {
		if (totalHeight[i] > scrollY + windowHeight) {
			endIdx = min(i + 10, static_cast<int>(nodes.size()));
			break;
		}
	}

	if (startIdx > 0) {
		ImGui::Dummy(ImVec2(0.0f, totalHeight[startIdx]));
	}

	int counter = 0;
	for (int i = 0; i < startIdx; i++) {
		counter += nodes[i].size;
	}

	for (int i = startIdx; i < endIdx; i++) {
		auto& node = nodes[i];

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::BeginChild(("Node_" + std::to_string(i)).c_str(), ImVec2((this == &g_PreviewClass) ? 1100 : parentSize.x, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_AlwaysUseWindowPadding);
		ImGui::PopStyleVar();

		drawOffset(i, counter);
		drawAddress(i, counter);

		size_t lCounter = 0;
		cur_pad = 0;

		auto dataPos = reinterpret_cast<std::uint8_t*>(data) + counter;

		switch (node.type) {
		case node_hex8:
		{
			drawStringBytes(i, data, counter, 1);
			drawBytes(i, data, counter, 1);
			auto num = *reinterpret_cast<int8_t*>(dataPos);
			drawNumber(i, num);
			drawHexNumber(i, num);
			break;
		}
		case node_hex16:
		{
			drawStringBytes(i, data, counter, 2);
			drawBytes(i, data, counter, 2);
			auto num = *reinterpret_cast<int16_t*>(dataPos);
			drawNumber(i, num);
			drawHexNumber(i, num);
			break;
		}
		case node_hex32:
		{
			drawStringBytes(i, data, counter, 4);
			drawBytes(i, data, counter, 4);
			auto fNum = *reinterpret_cast<float*>(dataPos);
			drawFloat(i, fNum);
			auto num = *reinterpret_cast<int32_t*>(dataPos);
			drawNumber(i, num);
			drawHexNumber(i, num, &clickedPointer);
			break;
		}
		case node_hex64:
		{
			drawStringBytes(i, data, counter, 8);
			drawBytes(i, data, counter, 8);
			auto dNum = *reinterpret_cast<double*>(dataPos);
			drawDouble(i, dNum);
			auto num = *reinterpret_cast<int64_t*>(dataPos);
			drawNumber(i, num);
			drawHexNumber(i, num, &clickedPointer);
			break;
		}
		case node_int64:
			drawInteger(i, *reinterpret_cast<int64_t*>(dataPos), node_int64);
			break;
		case node_int32:
			drawInteger(i, *reinterpret_cast<int32_t*>(dataPos), node_int32);
			break;
		case node_int16:
			drawInteger(i, *reinterpret_cast<int16_t*>(dataPos), node_int16);
			break;
		case node_int8:
			drawInteger(i, *reinterpret_cast<int8_t*>(dataPos), node_int8);
			break;
		case node_uint64:
			drawUInteger(i, *reinterpret_cast<uint64_t*>(dataPos), node_uint64);
			break;
		case node_uint32:
			drawUInteger(i, *reinterpret_cast<uint32_t*>(dataPos), node_uint32);
			break;
		case node_uint16:
			drawUInteger(i, *reinterpret_cast<uint16_t*>(dataPos), node_uint16);
			break;
		case node_uint8:
			drawUInteger(i, *reinterpret_cast<uint8_t*>(dataPos), node_uint8);
			break;
		case node_float:
			drawFloatVar(i, *reinterpret_cast<float*>(dataPos));
			break;
		case node_double:
			drawDoubleVar(i, *reinterpret_cast<double*>(dataPos));
			break;
		case node_vector4:
			drawVector4(i, *reinterpret_cast<Vector4*>(dataPos));
			break;
		case node_vector3:
			drawVector3(i, *reinterpret_cast<Vector3*>(dataPos));
			break;
		case node_vector2:
			drawVector2(i, *reinterpret_cast<Vector2*>(dataPos));
			break;
		case node_vector4d:
			drawVector4d(i, *reinterpret_cast<Vector4d*>(dataPos));
			break;
		case node_vector3d:
			drawVector3d(i, *reinterpret_cast<Vector3d*>(dataPos));
			break;
		case node_vector2d:
			drawVector2d(i, *reinterpret_cast<Vector2d*>(dataPos));
			break;
		case node_matrix4x4:
			drawMatrix4x4(i, *reinterpret_cast<Matrix4x4*>(dataPos));
			break;
		case node_matrix3x4:
			drawMatrix3x4(i, *reinterpret_cast<Matrix3x4*>(dataPos));
			break;
		case node_matrix3x3:
			drawMatrix3x3(i, *reinterpret_cast<Matrix3x3*>(dataPos));
			break;
		case node_bool:
			drawBool(i, *reinterpret_cast<bool*>(dataPos));
			break;
		case node_utf8:
			drawUTF8(i, reinterpret_cast<const char*>(dataPos));
			break;
		case node_utf16:
			drawUTF16(i, reinterpret_cast<const wchar_t*>(dataPos));
			break;
		case node_utf32:
			drawUTF32(i, reinterpret_cast<const char32_t*>(dataPos));
			break;
		}

		drawControllers(i, counter);

		ImGui::EndChild();

		if (node.type < node_max) {
			counter += node.size;
		}
	}

	if (endIdx < static_cast<int>(nodes.size())) {
		float remainingHeight = totalHeight[nodes.size()] - totalHeight[endIdx];
		ImGui::Dummy(ImVec2(0.0f, remainingHeight));
	}

	static bool oHoveringPointer = false;
	if (oHoveringPointer && !g_HoveringPointer) {
		free(g_PreviewClass.data);
		g_PreviewClass = uClass(15, false);
	}
	oHoveringPointer = g_HoveringPointer;

	if (clickedPointer) {
		uClass newClass(50);
		newClass.address = clickedPointer;
		g_Classes.push_back(newClass);
		g_SelectedClass = g_Classes.size() - 1;
	}
}

inline void initClasses(bool isX32) {
	if (g_Classes.empty() || isX32 != mem::x32) {
		mem::x32 = isX32;
		g_Classes = { uClass(50) };
	}
}