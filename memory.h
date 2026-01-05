#pragma once

#include <chrono>
#include <Windows.h>
#include <vector>
#include <tlhelp32.h>
#include <unordered_map>
#include <winternl.h>
#include <Psapi.h>
#include <mutex>
#include <future>
#include "websocket_server.h"

struct processSnapshot {
    std::wstring name;
    DWORD pid;
};

struct moduleSection {
    uintptr_t base;
    DWORD size;
    char name[8];
};

struct moduleInfo {
    uintptr_t base;
    DWORD size;
    std::vector<moduleSection> sections;
    std::string name;
};

struct pointerInfo {
    char section[8] = { 0 };
    std::string moduleName;
};

struct RTTICompleteObjectLocator {
    int signature;
    int offset;
    int cdOffset;
    DWORD typeDescriptorOffset;
    DWORD hierarchyDescriptorOffset;
    DWORD selfOffset;
};

struct RTTIClassHierarchyDescriptor {
    DWORD signature;
    DWORD attributes;
    DWORD numBaseClasses;
    DWORD pBaseClassArray;
};

struct TypeDescriptor {
    uintptr_t pVFTable;
    uintptr_t spare;
    char name[60];
};

struct funcExport
{
    std::string name;
    uintptr_t address;
};

namespace mem {
    inline std::vector<processSnapshot> processes;
    inline HANDLE memHandle;
    inline DWORD g_pid;
    inline std::vector<moduleInfo> moduleList;
    inline std::unordered_map<uintptr_t, std::string> g_ExportMap;
    inline bool x32 = false;

    // Cache system
    inline std::unordered_map<uintptr_t, std::pair<std::vector<uint8_t>, std::chrono::steady_clock::time_point>> g_ReadCache;
    inline std::mutex g_CacheMutex;
    inline constexpr std::chrono::milliseconds CACHE_DURATION{ 500 };

    inline bool g_NeedsModuleRefresh = false;

    bool getProcessList();
    void getModules();
    void getSections(const moduleInfo& info, std::vector<moduleSection>& dest);
    bool isPointer(uintptr_t address, pointerInfo* info);
    bool rttiInfo(uintptr_t address, std::string& out);
    std::vector<funcExport> gatherRemoteExports(uintptr_t moduleBase);
    void gatherExports();
    uintptr_t getExport(const std::string& moduleName, const std::string& exportName);

    uintptr_t findPattern(uintptr_t start, uintptr_t size, const std::string& pattern);
    bool read(uintptr_t address, void* buf, uintptr_t size);
    bool write(uintptr_t address, const void* buf, uintptr_t size);
    bool initProcess(DWORD pid);
    bool initProcessByName(const std::string& process_name);
    bool isX32(HANDLE handle);
    void clearCache();



    inline bool activeProcess = false;
    inline std::chrono::steady_clock::time_point lastCheck = std::chrono::steady_clock::now();
    inline constexpr std::chrono::milliseconds PROCESS_CHECK_INTERVAL{ 1000 };

    bool isProcessAlive();
    void cleanDeadProcess();
}

inline bool mem::isX32(HANDLE handle) {
    BOOL wow64 = FALSE;
    if (!IsWow64Process(handle, &wow64)) {
        return false;
    }

    return wow64;
}

template <typename T>
T Read(uintptr_t address);
inline bool mem::rttiInfo(uintptr_t address, std::string& out) {
    uintptr_t objectLocatorPtr = Read<uintptr_t>(address - sizeof(void*));
    if (!objectLocatorPtr) {
        return false;
    }

    auto objectLocator = Read<RTTICompleteObjectLocator>(objectLocatorPtr);
    auto baseModule = objectLocatorPtr - objectLocator.selfOffset;

    auto hierarchy = Read<RTTIClassHierarchyDescriptor>(baseModule + objectLocator.hierarchyDescriptorOffset);
    uintptr_t classArray = baseModule + hierarchy.pBaseClassArray;

    for (DWORD i = 0; i < hierarchy.numBaseClasses; i++) {
        uintptr_t classDescriptor = baseModule + Read<DWORD>(classArray + i * sizeof(DWORD));
        DWORD typeDescriptorOffset = Read<DWORD>(classDescriptor);
        auto typeDescriptor = Read<TypeDescriptor>(baseModule + typeDescriptorOffset);

        std::string name = typeDescriptor.name;
        if (!name.ends_with("@@")) {
            return false;
        }

        name = name.substr(4);
        name = name.substr(0, name.size() - 2);

        out = out + " : " + name;
    }

    return true;
}

DECLSPEC_NOINLINE bool mem::isPointer(uintptr_t address, pointerInfo* info) {
    for (auto& module : moduleList) {
        if (module.base <= address && address <= module.base + module.size) {
            info->moduleName = module.name;

            // default to unknown as pointers to places like the pe header don't get caught by any of these cases
            strcpy_s(info->section, 8, "UNK");

            for (auto& section : module.sections) {
                if (section.base <= address && address < section.base + section.size) {
                    memcpy(info->section, section.name, 8);
                    break;
                }
            }
            return true;
        }
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(memHandle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) {
        return (mbi.Type == MEM_PRIVATE && mbi.State == MEM_COMMIT);
    }

    return false;
}

inline bool mem::getProcessList() {
    processes.clear();

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &processEntry)) {
        do {
            if (processEntry.th32ProcessID == 0) {
                continue;
            }

            if (!wcscmp(processEntry.szExeFile, L"System") || !wcscmp(processEntry.szExeFile, L"Registry")) {
                continue;
            }

            processSnapshot snapshot{ processEntry.szExeFile, processEntry.th32ProcessID };
            processes.push_back(snapshot);
        } while (Process32Next(hSnapshot, &processEntry));
    }

    CloseHandle(hSnapshot);
    return true;
}

inline void mem::getModules() {
    if (!g_WebSocketServer.is_connected() || !activeProcess) {
        logger::addLog("[Memory] Cannot get modules - not connected or no process");
        return;
    }

    logger::addLog("[Memory] Requesting module list");

    json data;

    g_WebSocketServer.send_request("get_modules", data,
        [](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    std::string modules_data = j["modules"].get<std::string>();
                    int count = std::stoi(j["count"].get<std::string>());

                    moduleList.clear();

                    // Parse modules_data: "name,base,size|name,base,size|..."
                    size_t pos = 0;
                    while (pos < modules_data.size()) {
                        size_t pipe_pos = modules_data.find('|', pos);
                        std::string module_str = modules_data.substr(pos,
                            pipe_pos == std::string::npos ? std::string::npos : pipe_pos - pos);

                        // Parse "name,base,size"
                        size_t comma1 = module_str.find(',');
                        size_t comma2 = module_str.find(',', comma1 + 1);

                        if (comma1 != std::string::npos && comma2 != std::string::npos) {
                            moduleInfo info;
                            info.name = module_str.substr(0, comma1);

                            std::string base_str = module_str.substr(comma1 + 1, comma2 - comma1 - 1);
                            std::string size_str = module_str.substr(comma2 + 1);

                            info.base = std::stoull(base_str, nullptr, 16);
                            info.size = std::stoul(size_str, nullptr, 16);

                            moduleList.push_back(info);

                            logger::addLog("[Memory] Module: " + info.name +
                                " @ 0x" + base_str +
                                " (Size: 0x" + size_str + ")");
                        }

                        if (pipe_pos == std::string::npos) break;
                        pos = pipe_pos + 1;
                    }

                    logger::addLog("[Memory] Loaded " + std::to_string(moduleList.size()) + " modules");
                }
                else {
                    std::string error = j.value("error", "Unknown error");
                    logger::addLog("[Memory] Failed to get modules: " + error);
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] Error parsing get_modules response: " + std::string(e.what()));
            }
        });
}

inline void mem::getSections(const moduleInfo& info, std::vector<moduleSection>& dest) {
    BYTE buf[4096];
    read(info.base, buf, sizeof(buf));

    auto dosHeader = (IMAGE_DOS_HEADER*)buf;
    auto ntHeader = (IMAGE_NT_HEADERS*)(buf + dosHeader->e_lfanew);
    auto sectionHeader = IMAGE_FIRST_SECTION(ntHeader);

    for (WORD i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
        auto section = sectionHeader[i];
        moduleSection sectionInfo;
        sectionInfo.base = info.base + section.VirtualAddress;
        sectionInfo.size = section.Misc.VirtualSize;
        memcpy(sectionInfo.name, section.Name, 8);
        dest.push_back(sectionInfo);
    }
}

typedef NTSTATUS(*_NtQueryInformationProcess)(IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);




inline std::vector<funcExport> mem::gatherRemoteExports(uintptr_t moduleBase)
{
    std::vector<funcExport> exports;
    IMAGE_DOS_HEADER dosHeader;

    if (!read(moduleBase, &dosHeader, sizeof(IMAGE_DOS_HEADER)) || dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        return exports;
    }

    IMAGE_NT_HEADERS ntHeaders;

    if (!read(moduleBase + dosHeader.e_lfanew, &ntHeaders, sizeof(IMAGE_NT_HEADERS)) ||
        ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
        return exports;
    }

    DWORD exportDirRVA = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportDirSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

    if (!exportDirRVA || !exportDirSize) {
        return exports;
    }

    IMAGE_EXPORT_DIRECTORY exportDir;

    if (!read(moduleBase + exportDirRVA, &exportDir, sizeof(IMAGE_EXPORT_DIRECTORY))) {
        return exports;
    }

    std::vector<DWORD> functionRVAs(exportDir.NumberOfFunctions);

    if (!read(moduleBase + exportDir.AddressOfFunctions, functionRVAs.data(),
        exportDir.NumberOfFunctions * sizeof(DWORD))) {
        return exports;
    }

    std::vector<DWORD> nameRVAs(exportDir.NumberOfNames);

    if (!read(moduleBase + exportDir.AddressOfNames, nameRVAs.data(),
        exportDir.NumberOfNames * sizeof(DWORD))) {
        return exports;
    }

    std::vector<WORD> ordinals(exportDir.NumberOfNames);

    if (!read(moduleBase + exportDir.AddressOfNameOrdinals, ordinals.data(),
        exportDir.NumberOfNames * sizeof(WORD))) {
        return exports;
    }


    exports.reserve(exportDir.NumberOfNames);

    // TODO: refactor this to use less individual read calls (most names end up in the same pages anyway...)
    for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {
        char nameBuffer[256] = { 0 };
        DWORD nameRVA = nameRVAs[i];

        DWORD readSize = 255;

        if (i + 1 < exportDir.NumberOfNames) {
            DWORD nextNameRVA = nameRVAs[i + 1];
            DWORD NameDelta = nextNameRVA - nameRVA;
            if (NameDelta > 0 && NameDelta < 255)
                readSize = NameDelta; // don't need to read more than this delta
        }


        if (!read(moduleBase + nameRVA, nameBuffer, readSize)) {
            continue;
        }

        std::string exportName = nameBuffer;
        if (exportName.empty()) {
            continue;
        }

        WORD ordinal = ordinals[i];
        if (ordinal >= exportDir.NumberOfFunctions) {
            continue;
        }

        DWORD functionRVA = functionRVAs[ordinal];
        if (functionRVA >= exportDirRVA && functionRVA < (exportDirRVA + exportDirSize)) {
            continue;
        }

        uintptr_t functionAddress = moduleBase + functionRVA;
        funcExport info;
        info.name = exportName;
        info.address = functionAddress;
        exports.push_back(std::move(info));
    }

    return exports;
}

inline void mem::gatherExports()
{
    g_ExportMap.clear();

    for (auto& module : moduleList) {
        char modulePath[MAX_PATH] = { 0 };
        if (K32GetModuleFileNameExA(memHandle, reinterpret_cast<HMODULE>(module.base), modulePath, MAX_PATH)) {
            auto exports = gatherRemoteExports(module.base);

            for (const auto& exp : exports) {
                g_ExportMap[exp.address] = module.name + "!" + exp.name;
            }
        }
    }
}

inline uintptr_t mem::getExport(const std::string& moduleName, const std::string& exportName)
{
    for (auto& module : moduleList) {
        if (_stricmp(module.name.c_str(), moduleName.c_str()) == 0) {
            for (auto& pair : g_ExportMap) {
                std::string fullExport = module.name + "!" + exportName;
                if (pair.second == fullExport) {
                    return pair.first;
                }
            }
            return 0;
        }
    }
    return 0;
}

inline bool mem::isProcessAlive()
{
    if (!memHandle || memHandle == INVALID_HANDLE_VALUE)
        return false;

    auto curTime = std::chrono::steady_clock::now();

    if (curTime - lastCheck < PROCESS_CHECK_INTERVAL)
        return activeProcess;

    lastCheck = curTime;

    DWORD exitCode;
    if (!GetExitCodeProcess(memHandle, &exitCode) || exitCode != STILL_ACTIVE) {
        activeProcess = false;
        return false;
    }

    activeProcess = true;
    return true;
}

// used internally by ui::cleanDeadProcess
inline void mem::cleanDeadProcess() {
    if (memHandle && memHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(memHandle);
        memHandle = nullptr;
    }

    moduleList.clear();
    g_ExportMap.clear();
    g_pid = 0;
    activeProcess = false;
    clearCache();
}

extern void initClasses(bool);

inline bool mem::initProcessByName(const std::string& process_name) {
    if (!g_WebSocketServer.is_connected()) {
        logger::addLog("[Memory] WebSocket not connected to Perception!");
        return false;
    }

    logger::addLog("[Memory] Requesting process attachment for: " + process_name);

    json data;
    data["process_name"] = process_name;

    g_WebSocketServer.send_request("ref_process", data,
        [process_name](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    std::string base_str = j["base_address"].get<std::string>();
                    std::string peb_str = j["peb"].get<std::string>();
                    std::string pid_str = j["pid"].get<std::string>();
                    std::string is_x32_str = j["is_x32"].get<std::string>();

                    uint64_t base = std::stoull(base_str, nullptr, 16);
                    uint64_t peb = std::stoull(peb_str, nullptr, 16);
                    uint64_t pid = std::stoull(pid_str, nullptr, 10);
                    bool is_x32 = (is_x32_str == "true");

                    mem::g_pid = static_cast<DWORD>(pid);
                    mem::activeProcess = true;

                    char base_hex[32], peb_hex[32];
                    sprintf_s(base_hex, "0x%llX", base);
                    sprintf_s(peb_hex, "0x%llX", peb);

                    logger::addLog("[Memory] Process attached successfully: " + process_name);
                    logger::addLog("[Memory] PID: " + std::to_string(pid));
                    logger::addLog(std::string("[Memory] Base: ") + base_hex);
                    logger::addLog(std::string("[Memory] PEB: ") + peb_hex);
                    logger::addLog("[Memory] Is x32: " + std::string(is_x32 ? "true" : "false"));

                    mem::x32 = is_x32;
                    initClasses(is_x32);

                    // Set flag to refresh modules on next frame
                    mem::g_NeedsModuleRefresh = true;

                }
                else {
                    std::string error = j.value("error", "Unknown error");
                    logger::addLog("[Memory] Failed to attach to " + process_name + ": " + error);
                    mem::activeProcess = false;
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] Error parsing ref_process response: " + std::string(e.what()));
                mem::activeProcess = false;
            }
        });

    return true;
}

inline bool mem::initProcess(DWORD pid) {
    if (!g_WebSocketServer.is_connected()) {
        logger::addLog("[Memory] WebSocket not connected to Perception!");
        return false;
    }

    logger::addLog("[Memory] Requesting process attachment for PID: " + std::to_string(pid));

    json data;
    data["pid"] = pid;

    g_WebSocketServer.send_request("ref_process", data,
        [pid](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    mem::g_pid = pid;
                    mem::activeProcess = true;

                    uint64_t base = j["base_address"].get<uint64_t>();
                    uint64_t peb = j["peb"].get<uint64_t>();
                    bool is_x32 = j["is_x32"].get<bool>();

                    char base_str[32], peb_str[32];
                    sprintf_s(base_str, "0x%llX", base);
                    sprintf_s(peb_str, "0x%llX", peb);

                    logger::addLog("[Memory] Process attached successfully");
                    logger::addLog(std::string("[Memory] Base: ") + base_str);
                    logger::addLog(std::string("[Memory] PEB: ") + peb_str);
                    logger::addLog("[Memory] Is x32: " + std::string(is_x32 ? "true" : "false"));

                    mem::x32 = is_x32;
                    initClasses(is_x32);

                }
                else {
                    std::string error = j.value("error", "Unknown error");
                    logger::addLog("[Memory] Failed to attach: " + error);
                    mem::activeProcess = false;
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] Error parsing ref_process response: " + std::string(e.what()));
                mem::activeProcess = false;
            }
        });

    return true;
}

inline bool mem::read(uintptr_t address, void* buf, uintptr_t size) {
    if (!g_WebSocketServer.is_connected() || !activeProcess) {
        return false;
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        auto now = std::chrono::steady_clock::now();
        auto cache_key = address;

        auto it = g_ReadCache.find(cache_key);
        if (it != g_ReadCache.end()) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.second);
            if (age < CACHE_DURATION && it->second.first.size() >= size) {
                memcpy(buf, it->second.first.data(), size);
                return true;
            }
        }
    }

    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    json data;
    data["address"] = std::to_string(address);  // String
    data["size"] = std::to_string(size);        // String

    g_WebSocketServer.send_request("rvm", data,
        [buf, size, address, &promise](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    std::string hex_data = j["data"].get<std::string>();
                    std::vector<uint8_t> buffer(size);

                    for (size_t i = 0; i < size && i * 2 < hex_data.size(); i++) {
                        std::string byte_str = hex_data.substr(i * 2, 2);
                        buffer[i] = (uint8_t)strtoul(byte_str.c_str(), nullptr, 16);
                    }

                    memcpy(buf, buffer.data(), size);

                    {
                        std::lock_guard<std::mutex> lock(g_CacheMutex);
                        g_ReadCache[address] = { buffer, std::chrono::steady_clock::now() };
                    }

                    promise.set_value(true);
                }
                else {
                    promise.set_value(false);
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] rvm error: " + std::string(e.what()));
                promise.set_value(false);
            }
        });

    if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready) {
        return future.get();
    }

    return false;
}

inline bool mem::write(uintptr_t address, const void* buf, uintptr_t size) {
    if (!g_WebSocketServer.is_connected() || !activeProcess) {
        return false;
    }

    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    // Convert bytes to hex string
    std::string hex_data;
    const uint8_t* bytes = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < size; i++) {
        char hex[3];
        sprintf_s(hex, "%02X", bytes[i]);
        hex_data += hex;
    }

    json data;
    data["address"] = std::to_string(address);
    data["data"] = hex_data;

    g_WebSocketServer.send_request("wvm", data,
        [&promise](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    promise.set_value(true);
                }
                else {
                    promise.set_value(false);
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] wvm error: " + std::string(e.what()));
                promise.set_value(false);
            }
        });

    if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready) {
        return future.get();
    }

    return false;
}

inline uintptr_t mem::findPattern(uintptr_t start, uintptr_t size, const std::string& pattern) {
    if (!g_WebSocketServer.is_connected() || !activeProcess) {
        logger::addLog("[Memory] Cannot scan - not connected or no process");
        return 0;
    }

    logger::addLog("[Memory] Scanning for pattern: " + pattern);

    std::promise<uintptr_t> promise;
    std::future<uintptr_t> future = promise.get_future();

    json data;
    data["start"] = std::to_string(start);
    data["size"] = std::to_string(size);
    data["pattern"] = pattern;

    g_WebSocketServer.send_request("find_pattern", data,
        [&promise](const std::string& response) {
            try {
                auto j = json::parse(response);

                if (j.contains("success") && j["success"].get<bool>()) {
                    std::string addr_str = j["address"].get<std::string>();
                    uintptr_t result = std::stoull(addr_str, nullptr, 16);
                    promise.set_value(result);
                }
                else {
                    promise.set_value(0);
                }
            }
            catch (const std::exception& e) {
                logger::addLog("[Memory] find_pattern error: " + std::string(e.what()));
                promise.set_value(0);
            }
        });

    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        return future.get();
    }

    logger::addLog("[Memory] Pattern scan timeout");
    return 0;
}

inline void mem::clearCache() {
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    g_ReadCache.clear();
}

template <typename T>
T Read(uintptr_t address) {
    T response{};
    if (!address) {
        return response;
    }

    mem::read(address, &response, sizeof(T));

    return response;
}

template <typename T>
bool Write(uintptr_t address, const T& value) {
    return mem::write(address, &value, sizeof(T));
}

template <int n>
struct readBuf {
    BYTE data[n] = {};

    readBuf() {
        memset(data, 0, n);
    }
};