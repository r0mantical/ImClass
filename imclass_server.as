ws_t g_ws;
int g_callback_id = 0;
proc_t g_proc;
string g_attached_process_name = "";

void handle_ref_process(dictionary &in request)
{
    string request_id;
    request.get("request_id", request_id);
    
    dictionary response;
    response.set("request_id", request_id);
    
    double pid_double = 0;
    string process_name;
    bool has_pid = request.get("pid", pid_double);
    bool has_name = request.get("process_name", process_name);
    
    if (!has_pid && !has_name) {
        log("[Bridge] ref_process missing pid and process_name");
        response.set("success", false);
        response.set("error", "Missing pid or process_name");
        
        string json, err;
        if (json_stringify(response, json, err)) {
            g_ws.send_json(json);
        }
        return;
    }
    
    if (g_proc.alive()) {
        g_proc.deref();
    }
    
    if (has_name) {
        log("[Bridge] ref_process for process: " + process_name);
        g_proc = ref_process(process_name);
        g_attached_process_name = process_name;
    } else {
        uint pid = uint(pid_double);
        log("[Bridge] ref_process for PID: " + pid);
        g_proc = ref_process(pid);
        g_attached_process_name = "";
    }
    
    if (!g_proc.alive()) {
        string target = has_name ? process_name : formatInt(uint(pid_double), "", 0);
        log("[Bridge] Failed to attach to: " + target);
        response.set("success", false);
        response.set("error", "Failed to open process");
    }
    else {
        string target = has_name ? process_name : formatInt(uint(pid_double), "", 0);
        log("[Bridge] Successfully attached to: " + target);
        
        uint64 base = g_proc.base_address();
        uint64 peb = g_proc.peb();
        uint pid = g_proc.pid();
        
        array<uint8> test_buffer;
        g_proc.rvm(base, 16, test_buffer);
        string test_hex = "Base read test: ";
        for (uint i = 0; i < test_buffer.length(); i++) {
            test_hex += formatUInt(test_buffer[i], "0H", 2) + " ";
        }
        log("[Bridge] " + test_hex);
        
        string base_str = formatUInt(base, "0H", 16);
        string peb_str = formatUInt(peb, "0H", 16);
        string pid_response = formatUInt(pid, "", 10);
        
        response.set("success", true);
        response.set("base_address", base_str);
        response.set("peb", peb_str);
        response.set("pid", pid_response);
        response.set("is_x32", "false");
        
        log("[Bridge] PID: " + pid_response);
        log("[Bridge] Base: 0x" + base_str);
        log("[Bridge] PEB: 0x" + peb_str);
    }
    
    string json, err;
    if (json_stringify(response, json, err)) {
        g_ws.send_json(json);
    } else {
        log("[Bridge] Failed to stringify response: " + err);
    }
}

void handle_rvm(dictionary &in request)
{
    string request_id, addr_str, size_str;
    
    request.get("request_id", request_id);
    request.get("address", addr_str);
    request.get("size", size_str);
    
    uint64 addr = parseUInt(addr_str, 10);
    uint size = parseUInt(size_str, 10);
    
    dictionary response;
    response.set("request_id", request_id);
    
    if (!g_proc.alive()) {
        response.set("success", false);
        response.set("error", "No active process");
    }
    else {
        array<uint8> buffer;
        g_proc.rvm(addr, size, buffer);
        
        string hex_data;
        for (uint i = 0; i < buffer.length(); i++) {
            hex_data += formatUInt(buffer[i], "0H", 2);
        }
        
        response.set("success", true);
        response.set("data", hex_data);
    }
    
    string json, err;
    if (json_stringify(response, json, err)) {
        g_ws.send_json(json);
    }
}

void handle_wvm(dictionary &in request)
{
    string request_id, addr_str, data_hex;
    
    request.get("request_id", request_id);
    request.get("address", addr_str);
    request.get("data", data_hex);
    
    uint64 addr = parseUInt(addr_str, 10);
    
    dictionary response;
    response.set("request_id", request_id);
    
    if (!g_proc.alive()) {
        response.set("success", false);
        response.set("error", "No active process");
    }
    else {
        array<uint8> buffer;
        for (uint i = 0; i < data_hex.length(); i += 2) {
            string byte_str = data_hex.substr(i, 2);
            uint8 byte_val = uint8(parseUInt(byte_str, 16));
            buffer.insertLast(byte_val);
        }
        
        bool success = g_proc.wvm(addr, buffer);
        
        response.set("success", success);
        if (!success) {
            response.set("error", "Write failed");
        }
    }
    
    string json, err;
    if (json_stringify(response, json, err)) {
        g_ws.send_json(json);
    }
}

void handle_find_pattern(dictionary &in request)
{
    string request_id, start_str, size_str, pattern;
    
    request.get("request_id", request_id);
    request.get("start", start_str);
    request.get("size", size_str);
    request.get("pattern", pattern);
    
    uint64 start = parseUInt(start_str, 10);
    uint64 size = parseUInt(size_str, 10);
    
    dictionary response;
    response.set("request_id", request_id);
    
    if (!g_proc.alive()) {
        response.set("success", false);
        response.set("error", "No active process");
    }
    else {
        uint64 result = g_proc.find_code_pattern(start, size, pattern);
        
        response.set("success", true);
        response.set("address", formatUInt(result, "0H", 16));
        
        if (result != 0) {
            log("[Bridge] Pattern found at: 0x" + formatUInt(result, "0H", 16));
        } else {
            log("[Bridge] Pattern not found");
        }
    }
    
    string json, err;
    if (json_stringify(response, json, err)) {
        g_ws.send_json(json);
    }
}

void handle_get_modules(dictionary &in request)
{
    string request_id;
    request.get("request_id", request_id);
    
    dictionary response;
    response.set("request_id", request_id);
    
    if (!g_proc.alive()) {
        response.set("success", false);
        response.set("error", "No active process");
        
        string json, err;
        if (json_stringify(response, json, err)) {
            g_ws.send_json(json);
        }
        return;
    }
    
    log("[Bridge] Enumerating modules from PEB");
    
    uint64 peb = g_proc.peb();
    if (peb == 0) {
        log("[Bridge] Failed to get PEB");
        response.set("success", false);
        response.set("error", "Failed to get PEB");
        
        string json, err;
        if (json_stringify(response, json, err)) {
            g_ws.send_json(json);
        }
        return;
    }
    
    uint64 ldr_ptr = g_proc.ru64(peb + 0x18);
    if (ldr_ptr == 0) {
        log("[Bridge] Failed to read PEB.Ldr");
        response.set("success", false);
        response.set("error", "Failed to read PEB.Ldr");
        
        string json, err;
        if (json_stringify(response, json, err)) {
            g_ws.send_json(json);
        }
        return;
    }
    
    uint64 list_head = ldr_ptr + 0x20;
    uint64 current_link = g_proc.ru64(list_head);
    
    string modules_data = "";
    int module_count = 0;
    int max_modules = 500;
    
    while (current_link != list_head && module_count < max_modules) {
        uint64 entry_base = current_link - 0x10;
        uint64 base_dll_name_addr = entry_base + 0x58;
        
        uint16 name_length = g_proc.ru16(base_dll_name_addr);
        uint64 name_buffer_ptr = g_proc.ru64(base_dll_name_addr + 0x8);
        
        if (name_buffer_ptr != 0 && name_length > 0 && name_length < 512) {
            int max_chars = int(name_length / 2);
            string module_name = g_proc.rws(name_buffer_ptr, max_chars);
            
            uint64 dll_base = g_proc.ru64(entry_base + 0x30);
            uint32 size_of_image = g_proc.ru32(entry_base + 0x40);
            
            if (dll_base != 0 && module_name != "") {
                if (module_count > 0) {
                    modules_data += "|";
                }
                
                string base_str = formatUInt(dll_base, "0H", 16);
                string size_str = formatUInt(size_of_image, "0H", 8);
                
                modules_data += module_name + "," + base_str + "," + size_str;
                module_count++;
            }
        }
        
        current_link = g_proc.ru64(current_link);
        
        if (current_link == 0) {
            break;
        }
    }
    
    response.set("success", true);
    response.set("modules", modules_data);
    response.set("count", formatUInt(module_count, "", 10));
    
    log("[Bridge] Enumerated " + module_count + " modules from PEB");
    
    string json, err;
    if (json_stringify(response, json, err)) {
        g_ws.send_json(json);
    }
}

void websocket_callback(int id, int data)
{
    if (!g_ws.is_open()) {
        return;
    }
    
    string msg;
    bool text, closed;
    
    if (g_ws.poll(msg, text, closed)) {
        dictionary d;
        string err;
        
        if (!json_parse(msg, d, err)) {
            log("[Bridge] JSON parse failed: " + err);
            return;
        }
        
        string type;
        d.get("type", type);
        
        if (type == "ref_process") {
            handle_ref_process(d);
        }
        else if (type == "rvm") {
            handle_rvm(d);
        }
        else if (type == "wvm") {
            handle_wvm(d);
        }
        else if (type == "get_modules") {
            handle_get_modules(d);
        }
        else if (type == "find_pattern") {
            handle_find_pattern(d);
        }
    }
    
    if (closed) {
        log("[Bridge] Connection closed by server");
        g_ws.close();
        if (g_callback_id != 0) {
            unregister_callback(g_callback_id);
            g_callback_id = 0;
        }
    }
}

int main()
{
    log("[Bridge] Connecting to ImClass on ws://localhost:9001");
    
    g_ws = ws_connect("ws://localhost:9001", 5000);
    
    if (!g_ws.is_open()) {
        log("[Bridge] Connection FAILED!");
        return 0;
    }
    
    log("[Bridge] Connected successfully!");
    
    g_ws.send_json("{\"type\":\"hello\",\"from\":\"perception.cx\"}");
    log("[Bridge] Sent hello message");
    
    g_callback_id = register_callback(websocket_callback, 1, 0);
    
    return 1;
}

void on_unload()
{
    if (g_proc.alive()) {
        g_proc.deref();
    }
    
    if (g_ws.is_open()) {
        g_ws.close();
    }
    
    log("[Bridge] Unloaded");
}