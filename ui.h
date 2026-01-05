#pragma once

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <regex>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/imgui_internal.h>

#include "patterns.h"
#include <logging.h>

namespace ui {
    bool open = true;
    bool processWindow = false;
    bool signaturesWindow = false;
    bool stringSearchWindow = false;
    bool sigScanWindow = false;
    bool exportWindow = false;
    bool consoleWindow = true;  // Console visible by default
    bool moduleListWindow = false;

    std::string exportedClass;
    inline std::optional<PatternScanResult> patternResults;
    char addressInput[256] = "0";
    char module[512] = { 0 };
    char signature[512] = { 0 };
    char searchString[512] = { 0 };

    ImVec2 mainPos;
    ImVec2 signaturePos = { 0, 0 };
    ImVec2 stringSearchPos = { 0, 0 };

    void init(HWND hwnd);
    void renderProcessWindow();
    void renderMain();
    void renderExportWindow();
    void render();
    bool searchMatches(std::string str, std::string term);
    uintptr_t toAddress(std::string address);
    std::string toHexString(uintptr_t address, int width = 0);
    bool isValidHex(std::string& str);
    void updateAddress(uintptr_t newAddress, uintptr_t* dest = 0);
    void renderSignatureResults();
    void renderSignatureScan();
    void renderStringScan();
    void updateAddressBox(char* dest, char* src);
    void cleanDeadProcess();
    void renderModals();
    void renderConsoleWindow();
    void renderModuleListWindow();
}

// reused for small tool windows
constexpr float headerHeight = 30.0f;
constexpr float footerHeight = 30.0f;
constexpr float minWidth = 300.0f;
constexpr float padding = 20.0f;

void ui::updateAddressBox(char* dest, char* src) {
    memset(dest, 0, sizeof(addressInput));
    memcpy(dest, src, strlen(src));
}

void ui::cleanDeadProcess()
{
    mem::cleanDeadProcess();

    for (auto& cClass : g_Classes)
    {
        memset(cClass.data, 0, cClass.size);
    }
}


inline void ui::renderSignatureScan()
{
    static bool oSigScanWindow = false;
    if (!sigScanWindow) {
        oSigScanWindow = sigScanWindow;
        return;
    }

    const float entryHeight = ImGui::GetTextLineHeightWithSpacing();

    constexpr int numElements = 3;
    const float contentHeight = (entryHeight * numElements) + padding;
    const float windowHeight = min(headerHeight + contentHeight + footerHeight, 300.0f);
    static bool hasSetPos = false;

    if (sigScanWindow != oSigScanWindow) {
        if (hasSetPos) {
            ImGui::SetNextWindowPos(signaturePos, ImGuiCond_Always);
        }
        else
        {
            ImVec2 defaultPos = ImVec2(mainPos.x + 50, mainPos.y + 50);
            ImGui::SetNextWindowPos(defaultPos, ImGuiCond_Always);
            signaturePos = ImVec2(defaultPos);
            hasSetPos = true;
        }
        ImGui::SetNextWindowSize(ImVec2(minWidth, windowHeight), ImGuiCond_Always);
    }
    oSigScanWindow = sigScanWindow;

    ImGui::Begin("Nigga Scan", &sigScanWindow);
    ImGui::InputText("Module", module, sizeof(module));
    ImGui::InputText("Signature", signature, sizeof(signature));
    if (ImGui::Button("Scan")) {
        logger::addLog("[Signature] Scanning for: " + std::string(signature));
        logger::addLog("[Signature] In module: " + std::string(module));

        PatternInfo pattern;
        pattern.pattern = signature;
        patternResults = pattern::scanPattern(pattern, module);

        if (patternResults.has_value() && !patternResults.value().matches.empty()) {
            logger::addLog("[Signature] Found " + std::to_string(patternResults.value().matches.size()) + " matches");
            signaturesWindow = true;
        }
        else {
            logger::addLog("[Signature] No matches found");
        }

        sigScanWindow = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        sigScanWindow = false;
    }

    signaturePos = ImGui::GetWindowPos();

    ImGui::End();
}

inline void ui::renderStringScan()
{
    static bool oStringSearchWindow = false;
    if (!stringSearchWindow) {
        oStringSearchWindow = stringSearchWindow;
        return;
    }

    const float entryHeight = ImGui::GetTextLineHeightWithSpacing();

    constexpr int numElements = 3;
    const float contentHeight = (entryHeight * numElements) + padding;
    const float windowHeight = min(headerHeight + contentHeight + footerHeight, 300.0f);
    static bool hasSetPos = false;

    if (stringSearchWindow != oStringSearchWindow) {
        if (hasSetPos) {
            ImGui::SetNextWindowPos(stringSearchPos, ImGuiCond_Always);
        }
        else
        {
            ImVec2 defaultPos = ImVec2(mainPos.x + 50, mainPos.y + 50);
            ImGui::SetNextWindowPos(defaultPos, ImGuiCond_Always);
            stringSearchPos = ImVec2(defaultPos);
            hasSetPos = true;
        }
        ImGui::SetNextWindowSize(ImVec2(minWidth, windowHeight), ImGuiCond_Always);
    }
    oStringSearchWindow = stringSearchWindow;

    ImGui::Begin("Stwing Scan", &stringSearchWindow);
    ImGui::InputText("Module", module, sizeof(module));
    ImGui::InputText("String", searchString, sizeof(searchString));
    if (ImGui::Button("Scan")) {
        logger::addLog("[String] Scanning for: " + std::string(searchString));
        logger::addLog("[String] In module: " + std::string(module));

        PatternInfo pattern;
        std::string patternString = pattern::stringToSignature(searchString);

        logger::addLog("[String] Converted to pattern: " + patternString);

        pattern.pattern = patternString;
        pattern.type = PatternType::BYTE_PATTERN;

        patternResults = pattern::scanPattern(pattern, module, std::nullopt);

        if (patternResults.has_value() && !patternResults.value().matches.empty()) {
            logger::addLog("[String] Found " + std::to_string(patternResults.value().matches.size()) + " matches");
            signaturesWindow = true;
        }
        else {
            logger::addLog("[String] No matches found");
        }

        stringSearchWindow = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        stringSearchWindow = false;
    }

    stringSearchPos = ImGui::GetWindowPos();

    ImGui::End();
}


void ui::renderSignatureResults() {
    static bool oSignaturesWindow = false;
    if (!signaturesWindow) {
        oSignaturesWindow = signaturesWindow;
        return;
    }

    const float entryHeight = ImGui::GetTextLineHeightWithSpacing();
    constexpr float headerHeight = 30.0f;
    constexpr float footerHeight = 30.0f;
    constexpr float minWidth = 200.0f;

    const size_t numEntries = patternResults.has_value() && !patternResults.value().matches.empty()
        ? patternResults.value().matches.size()
        : 1;

    const float windowHeight = min(headerHeight + (entryHeight * numEntries) + footerHeight, 500.0f);

    if (signaturesWindow != oSignaturesWindow) {
        ImGui::SetNextWindowPos(signaturePos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(minWidth, windowHeight), ImGuiCond_Always);
    }
    oSignaturesWindow = signaturesWindow;

    ImGui::Begin("Signatures", &signaturesWindow);
    const ImVec2 wndSize = ImGui::GetWindowSize();
    ImGui::BeginChild("##SignaturesList", ImVec2(0, wndSize.y - 30));

    if (patternResults.has_value() && !patternResults.value().matches.empty()) {

        PatternScanResult& results = patternResults.value();

        for (uintptr_t match : results.matches) {
            const std::string address = toHexString(match);
            const char* cAddr = address.c_str();
            if (ImGui::Selectable(cAddr)) {
                if (g_Classes.size() >= g_SelectedClass) {
                    uClass& cClass = g_Classes[g_SelectedClass];
                    updateAddressBox(addressInput, (char*)(cAddr));
                    updateAddressBox(cClass.addressInput, (char*)(cAddr));
                    updateAddress(match, &cClass.address);
                    signaturesWindow = false;
                }
            }
        }
    }
    else {
        ImGui::Text("No signatures found.");
    }

    ImGui::EndChild();
    ImGui::End();
}

void ui::updateAddress(uintptr_t newAddress, uintptr_t* dest) {
    if (newAddress != 0 && strcmp(addressInput, "0")) {
        if (dest) {
            *dest = newAddress;
        }
    }
}

void ui::renderModals()
{
    if (showModuleMissingPopup) {
        ImGui::OpenPopup("Module Missing");
        showModuleMissingPopup = false;
    }

    if (ImGui::BeginPopup("Module Missing"))
    {
        ImGui::Text("The selected address is not inside a module!");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ui::renderMain() {
    ImGui::Begin("ImClass", &open, ImGuiWindowFlags_MenuBar);
    mainPos = ImGui::GetWindowPos();
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Attach to process")) {
                processWindow = true;
                mem::getProcessList();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Signature Scanner"))
            {
                sigScanWindow = true;
            }
            if (ImGui::MenuItem("String Scanner"))
            {
                stringSearchWindow = true;
            }
            if (ImGui::MenuItem("Module List"))
            {
                moduleListWindow = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Console", nullptr, consoleWindow)) {
                consoleWindow = !consoleWindow;
            }
            ImGui::EndMenu();
        }

        // Show attached process
        if (mem::activeProcess && mem::g_pid != 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));

            // Find process name from process list
            std::string processName = "Unknown";
            for (const auto& proc : mem::processes) {
                if (proc.pid == mem::g_pid) {
                    std::wstring wname = proc.name;
                    processName = std::string(wname.begin(), wname.end());
                    break;
                }
            }

            ImGui::Text("Process: %s (PID %d)", processName.c_str(), mem::g_pid);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("|");
            ImGui::SameLine();
        }

        // Show perception connection status
        if (g_WebSocketServer.is_connected()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            ImGui::Text("perception.cx connected");
            ImGui::PopStyleColor();
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            ImGui::Text("waiting for perception.cx");
            ImGui::PopStyleColor();
        }

        ImGui::EndMenuBar();

        ImGui::Columns(2);

        ImGui::SetColumnOffset(1, 150);

        float columnOffset = ImGui::GetColumnOffset(1);

        ImVec2 wndSize = ImGui::GetWindowSize();

        ImGui::BeginChild("ClassesChild", ImVec2(columnOffset - 15, wndSize.y - 54), 1, ImGuiWindowFlags_NoScrollbar);

        static char renameBuf[64] = { 0 };
        static int renamedClass = -1;

        for (int i = 0; i < g_Classes.size(); i++) {
            auto& lClass = g_Classes[i];

            if (renamedClass != i) {
                if (ImGui::Selectable(lClass.name, (i == g_SelectedClass))) {
                    g_SelectedClass = i;
                    updateAddress(lClass.address);
                    updateAddressBox(addressInput, lClass.addressInput);
                }

                if (ImGui::BeginPopupContextItem(("##ClassContext" + std::to_string(i)).c_str())) {
                    if (ImGui::MenuItem("Export Class")) {
                        uClass& sClass = g_Classes[i];
                        std::string exported = sClass.exportClass();
                        exportedClass = exported;
                        exportWindow = true;
                    }
                    if (ImGui::MenuItem("New Class")) {
                        g_Classes.push_back({ uClass(50) });
                    }
                    if (ImGui::MenuItem("Delete")) {
                        if (g_Classes.size() > 0) {
                            uClass& sClass = g_Classes[i];
                            free(sClass.data);
                            g_Classes.erase(g_Classes.begin() + i);
                            if (g_SelectedClass > 0 && g_SelectedClass > g_Classes.size() - 1) {
                                g_SelectedClass--;
                            }
                        }
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    renamedClass = i;
                    memset(renameBuf, 0, sizeof(renameBuf));
                    memcpy(renameBuf, lClass.name, sizeof(renameBuf));
                }
            }
            else {
                ImGui::SetKeyboardFocusHere();

                if (ImGui::InputText("##RenameClass", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    memcpy(lClass.name, renameBuf, sizeof(renameBuf));
                    renamedClass = -1;
                }

                if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    renamedClass = -1;
                }
            }
        }

        ImGui::EndChild();

        if (g_Classes.empty()) {
            if (ImGui::BeginPopupContextItem("##ClassesContext")) {
                if (ImGui::MenuItem("New Class")) {
                    g_Classes.push_back({ uClass(50) });
                }
                ImGui::EndPopup();
            }
            ImGui::End();
            return;
        }

        uClass& sClass = g_Classes[g_SelectedClass];

        ImGui::NextColumn();

        ImGui::BeginChild("MemoryViewChild", ImVec2(wndSize.x - columnOffset - 16, wndSize.y - 54), 1);
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("Address", addressInput, sizeof(addressInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
            uintptr_t newAddress = addressParser::parseInput(addressInput);
            updateAddress(newAddress, &sClass.address);
            updateAddressBox(sClass.addressInput, addressInput);
        }

        static bool oInputFocused = false;

        // ImGui::IsItemFocused() doesn't really work for losing focus from the inputtext
        // Idk why, so I'm taking the logic from InputText itself, it works...
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID("Address");

        bool inputFocused = (GImGui->ActiveId == id);
        if (oInputFocused && !inputFocused) {
            updateAddress(sClass.address);
            updateAddressBox(addressInput, sClass.addressInput);
        }

        oInputFocused = inputFocused;

        ImGui::BeginChild("MemView", ImVec2(0, 0), 0, g_HoveringPointer ? ImGuiWindowFlags_NoScrollWithMouse : 0);
        g_HoveringPointer = false;
        g_InPopup = false;
        //auto buf = Read<readBuf<4096>>(sClass.address);
        sClass.drawNodes();
        ImGui::EndChild();

        ImGui::EndChild();

        bool processActive = mem::isProcessAlive() && mem::activeProcess;
        static bool processWasActive = false;

        if (processWasActive && !processActive)
            cleanDeadProcess();

        processWasActive = processActive;

    }
    ImGui::End();
}

void ui::renderProcessWindow() {
    static bool oProcessWindow = false;
    if (!processWindow) {
        oProcessWindow = processWindow;
        return;
    }
    if (processWindow != oProcessWindow) {
        ImGui::SetNextWindowPos(ImVec2(mainPos.x + 50, mainPos.y + 50), ImGuiCond_Always);
    }
    oProcessWindow = processWindow;

    ImGui::Begin("Attach to process", &processWindow);

    static char processNameInput[256] = { 0 };

    ImGui::Text("Process Name:");
    ImGui::InputTextWithHint("##ProcessName", "e.g. notepad.exe", processNameInput, sizeof(processNameInput));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Attach", ImVec2(120, 0))) {
        if (strlen(processNameInput) > 0) {
            processWindow = false;
            mem::initProcessByName(processNameInput);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        processWindow = false;
    }

    ImGui::End();
}

void ui::renderExportWindow() {
    if (!exportWindow) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(437, 305));
    ImGui::Begin("Exported class", &exportWindow, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::InputTextMultiline("##exportedclass", exportedClass.data(), exportedClass.size(), ImVec2(420, 250));
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(exportedClass.c_str());
    }
    ImGui::End();
}

void ui::renderConsoleWindow() {
    if (!consoleWindow) return;

    ImGui::Begin("Console >.<", &consoleWindow);

    // Add clear button at the top
    if (ImGui::Button("Clear Logs")) {
        logger::clearLogs();
    }

    ImGui::Separator();

    // Iterate through the log entries
    for (const auto& entry : logger::getLogs()) {
        ImGui::PushID(&entry);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("[%s]", entry.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Selectable(entry.message.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
            std::string fullLog = "[" + entry.timestamp + "] " + entry.message;
            ImGui::SetClipboardText(fullLog.c_str());
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::End();
}

void ui::renderModuleListWindow() {
    if (!moduleListWindow) return;

    ImGui::Begin("Epstein's List", &moduleListWindow);

    if (ImGui::Button("Refresh Modules")) {
        mem::getModules();
    }

    ImGui::Separator();

    ImGui::Text("Total Modules: %zu", mem::moduleList.size());

    ImGui::Separator();

    ImGui::BeginChild("ModuleListChild");

    if (ImGui::BeginTable("ModulesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Base Address", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (const auto& mod : mem::moduleList) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (ImGui::Selectable(mod.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                // Copy module name to clipboard
                ImGui::SetClipboardText(mod.name.c_str());
            }

            ImGui::TableNextColumn();
            std::string baseStr = toHexString(mod.base, 16);
            ImGui::Text("0x%s", baseStr.c_str());
            if (ImGui::IsItemClicked()) {
                ImGui::SetClipboardText(baseStr.c_str());
            }

            ImGui::TableNextColumn();
            std::string sizeStr = toHexString(mod.size, 8);
            ImGui::Text("0x%s", sizeStr.c_str());
            if (ImGui::IsItemClicked()) {
                ImGui::SetClipboardText(sizeStr.c_str());
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
}

bool ui::searchMatches(std::string str, std::string term) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
    std::transform(term.begin(), term.end(), term.begin(), tolower);
    return str.find(term) != std::string::npos;
}

void ui::render() {
    renderMain();
    renderProcessWindow();
    renderExportWindow();
    renderSignatureScan();
    renderSignatureResults();
    renderStringScan();
    renderModals();
    renderConsoleWindow();
    renderModuleListWindow();
}

void ui::init(HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoDefaultParent = true;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.Colors[ImGuiCol_Header] = ImColor(66, 135, 245, 75);
    style.Colors[ImGuiCol_HeaderActive] = ImColor(66, 135, 245, 75);
    style.Colors[ImGuiCol_HeaderHovered] = ImColor(66, 135, 245, 50);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
}

uintptr_t ui::toAddress(std::string address) {
    address.erase(std::remove(address.begin(), address.end(), ' '), address.end());

    if (!isValidHex(address)) {
        return 0;
    }

    if (address.size() > 2 && (address[0] == '0') && (address[1] == 'x' || address[1] == 'X')) {
        address = address.substr(2);
    }

    std::uintptr_t result = 0;
    std::istringstream(address) >> std::hex >> result;

    return result;
}

std::string ui::toHexString(uintptr_t address, int width) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setw(width) << std::setfill('0') << address;
    return ss.str();
}

bool ui::isValidHex(std::string& str) {
    static std::regex hexRegex("^(0x|0X)?[0-9a-fA-F]+$");
    return std::regex_match(str, hexRegex);
}