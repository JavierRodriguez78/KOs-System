#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace kos::services {

/**
 * @brief Represents a parsed .desktop file entry
 * 
 * Desktop Entry Format (freedesktop.org):
 * [Desktop Entry]
 * Name=Application Name
 * Exec=/usr/bin/app
 * Icon=icon.png
 * X-KOS-Position=TopLeft
 * X-KOS-Autostart=true
 * X-KOS-Category=System
 */
struct DesktopEntry {
    // Standard fields
    std::string name;           // Name= (display name)
    std::string exec;           // Exec= (executable path)
    std::string icon;           // Icon= (icon filename or path)
    std::string comment;        // Comment= (description)
    std::string categories;     // Categories= (comma-separated)
    
    // KOS-specific fields
    struct Position {
        enum class Value {
            TopLeft,
            TopCenter,
            TopRight,
            CenterLeft,
            Center,
            CenterRight,
            BottomLeft,
            BottomCenter,
            BottomRight,
            Fullscreen,
            Unknown
        };
        static Value FromString(const std::string& str);
        static std::string ToString(Value v);
    };
    
    Position::Value position = Position::Value::Unknown;
    bool autostart = false;     // X-KOS-Autostart=
    std::string kos_category;   // X-KOS-Category= (System, User, Game, etc)
    uint32_t priority = 100;    // X-KOS-Priority= (for autostart order, 0-255)
    
    // Custom key-value pairs (for extensibility)
    std::unordered_map<std::string, std::string> custom_fields;
    
    bool is_valid() const;
    std::string to_string() const;
};

/**
 * @brief Parser for .desktop files
 */
class DesktopEntryParser {
public:
    /**
     * @brief Parse a single .desktop file
     * @param file_path Path to .desktop file
     * @return Parsed DesktopEntry, or invalid entry on error
     */
    static DesktopEntry Parse(const std::string& file_path);
    
    /**
     * @brief Parse .desktop content from string
     * @param content File content
     * @param filename Optional filename for error reporting
     * @return Parsed DesktopEntry
     */
    static DesktopEntry ParseFromString(const std::string& content, 
                                        const std::string& filename = "");
    
    /**
     * @brief Load all .desktop files from a directory
     * @param directory_path Path to scan (e.g., "/usr/share/desktop")
     * @return Vector of parsed entries
     */
    static std::vector<DesktopEntry> LoadDirectory(const std::string& directory_path);

private:
    static std::string Trim(const std::string& str);
    static std::string UnescapeValue(const std::string& str);
};

/**
 * @brief Registry of discovered desktop applications
 */
class DesktopEntryRegistry {
public:
    static DesktopEntryRegistry& Instance();
    
    /**
     * @brief Scan and register all .desktop files
     */
    void Discover();
    
    /**
     * @brief Get all registered entries
     */
    const std::vector<DesktopEntry>& GetEntries() const { return entries_; }
    
    /**
     * @brief Get entry by name
     */
    const DesktopEntry* GetByName(const std::string& name) const;
    
    /**
     * @brief Get entries by category
     */
    std::vector<DesktopEntry*> GetByCategory(const std::string& category);
    
    /**
     * @brief Get autostart entries sorted by priority
     */
    std::vector<DesktopEntry*> GetAutostart();

private:
    DesktopEntryRegistry() = default;
    std::vector<DesktopEntry> entries_;
};

} // namespace kos::services
