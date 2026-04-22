#include "../../include/services/desktop_entry.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <sys/types.h>

namespace kos::services {

// ============================================================================
// DesktopEntry::Position
// ============================================================================

DesktopEntry::Position::Value DesktopEntry::Position::FromString(const std::string& str) {
    if (str == "TopLeft") return Value::TopLeft;
    if (str == "TopCenter") return Value::TopCenter;
    if (str == "TopRight") return Value::TopRight;
    if (str == "CenterLeft") return Value::CenterLeft;
    if (str == "Center") return Value::Center;
    if (str == "CenterRight") return Value::CenterRight;
    if (str == "BottomLeft") return Value::BottomLeft;
    if (str == "BottomCenter") return Value::BottomCenter;
    if (str == "BottomRight") return Value::BottomRight;
    if (str == "Fullscreen") return Value::Fullscreen;
    return Value::Unknown;
}

std::string DesktopEntry::Position::ToString(Value v) {
    switch (v) {
        case Value::TopLeft: return "TopLeft";
        case Value::TopCenter: return "TopCenter";
        case Value::TopRight: return "TopRight";
        case Value::CenterLeft: return "CenterLeft";
        case Value::Center: return "Center";
        case Value::CenterRight: return "CenterRight";
        case Value::BottomLeft: return "BottomLeft";
        case Value::BottomCenter: return "BottomCenter";
        case Value::BottomRight: return "BottomRight";
        case Value::Fullscreen: return "Fullscreen";
        default: return "Unknown";
    }
}

bool DesktopEntry::is_valid() const {
    return !name.empty() && !exec.empty();
}

std::string DesktopEntry::to_string() const {
    std::stringstream ss;
    ss << "[Desktop Entry]\n";
    ss << "Name=" << name << "\n";
    ss << "Exec=" << exec << "\n";
    if (!icon.empty()) ss << "Icon=" << icon << "\n";
    if (!comment.empty()) ss << "Comment=" << comment << "\n";
    if (!categories.empty()) ss << "Categories=" << categories << "\n";
    ss << "X-KOS-Position=" << Position::ToString(position) << "\n";
    ss << "X-KOS-Autostart=" << (autostart ? "true" : "false") << "\n";
    if (!kos_category.empty()) ss << "X-KOS-Category=" << kos_category << "\n";
    ss << "X-KOS-Priority=" << priority << "\n";
    return ss.str();
}

// ============================================================================
// DesktopEntryParser
// ============================================================================

std::string DesktopEntryParser::Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string DesktopEntryParser::UnescapeValue(const std::string& str) {
    std::string result = str;
    // Handle basic escape sequences
    size_t pos = 0;
    while ((pos = result.find("\\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    return result;
}

DesktopEntry DesktopEntryParser::Parse(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return DesktopEntry();
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseFromString(buffer.str(), file_path);
}

DesktopEntry DesktopEntryParser::ParseFromString(const std::string& content, 
                                                 const std::string& filename) {
    DesktopEntry entry;
    std::istringstream stream(content);
    std::string line;
    bool in_desktop_section = false;
    
    while (std::getline(stream, line)) {
        line = Trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Check for section header
        if (line == "[Desktop Entry]") {
            in_desktop_section = true;
            continue;
        } else if (line[0] == '[') {
            in_desktop_section = false;
            continue;
        }
        
        if (!in_desktop_section) continue;
        
        // Parse key=value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = Trim(line.substr(0, eq_pos));
        std::string value = Trim(line.substr(eq_pos + 1));
        value = UnescapeValue(value);
        
        // Standard fields
        if (key == "Name") entry.name = value;
        else if (key == "Exec") entry.exec = value;
        else if (key == "Icon") entry.icon = value;
        else if (key == "Comment") entry.comment = value;
        else if (key == "Categories") entry.categories = value;
        
        // KOS-specific fields
        else if (key == "X-KOS-Position") {
            entry.position = DesktopEntry::Position::FromString(value);
        }
        else if (key == "X-KOS-Autostart") {
            entry.autostart = (value == "true" || value == "1" || value == "yes");
        }
        else if (key == "X-KOS-Category") {
            entry.kos_category = value;
        }
        else if (key == "X-KOS-Priority") {
            try {
                entry.priority = std::stoul(value);
            } catch (...) {
                entry.priority = 100;
            }
        }
        else {
            // Store custom fields
            entry.custom_fields[key] = value;
        }
    }
    
    return entry;
}

std::vector<DesktopEntry> DesktopEntryParser::LoadDirectory(const std::string& directory_path) {
    std::vector<DesktopEntry> entries;
    
    DIR* dir = opendir(directory_path.c_str());
    if (!dir) return entries;
    
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // Only load .desktop files
        if (filename.size() < 9 || 
            filename.substr(filename.size() - 8) != ".desktop") {
            continue;
        }
        
        std::string full_path = directory_path + "/" + filename;
        DesktopEntry parsed = Parse(full_path);
        
        if (parsed.is_valid()) {
            entries.push_back(parsed);
        }
    }
    
    closedir(dir);
    return entries;
}

// ============================================================================
// DesktopEntryRegistry
// ============================================================================

DesktopEntryRegistry& DesktopEntryRegistry::Instance() {
    static DesktopEntryRegistry instance;
    return instance;
}

void DesktopEntryRegistry::Discover() {
    entries_.clear();
    
    // Scan standard desktop directories
    std::vector<std::string> desktop_dirs = {
        "/usr/share/desktop",
        "/usr/local/share/desktop",
        "/etc/desktop",
    };
    
    for (const auto& dir : desktop_dirs) {
        auto found = DesktopEntryParser::LoadDirectory(dir);
        entries_.insert(entries_.end(), found.begin(), found.end());
    }
}

const DesktopEntry* DesktopEntryRegistry::GetByName(const std::string& name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<DesktopEntry*> DesktopEntryRegistry::GetByCategory(const std::string& category) {
    std::vector<DesktopEntry*> result;
    for (auto& entry : entries_) {
        if (entry.kos_category == category) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<DesktopEntry*> DesktopEntryRegistry::GetAutostart() {
    std::vector<DesktopEntry*> result;
    for (auto& entry : entries_) {
        if (entry.autostart) {
            result.push_back(&entry);
        }
    }
    
    // Sort by priority (lower number = higher priority = runs first)
    std::sort(result.begin(), result.end(),
              [](DesktopEntry* a, DesktopEntry* b) {
                  return a->priority < b->priority;
              });
    
    return result;
}

} // namespace kos::services
