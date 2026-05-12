#include "package.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace epp {

// Get home directory cross-platform
static std::filesystem::path getHomeDir() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
    return std::filesystem::path(path);
  }
  // Fallback to USERPROFILE
  const char* userProfile = std::getenv("USERPROFILE");
  if (userProfile) {
    return std::filesystem::path(userProfile);
  }
#else
  const char* home = std::getenv("HOME");
  if (home) {
    return std::filesystem::path(home);
  }
  // Fallback to getpwuid
  struct passwd* pw = getpwuid(getuid());
  if (pw) {
    return std::filesystem::path(pw->pw_dir);
  }
#endif
  return std::filesystem::path(".");
}

std::filesystem::path PackageManager::getPackagesDir() {
  return getHomeDir() / ".epp" / "packages";
}

std::filesystem::path PackageManager::packageNameToPath(const std::string& name) {
  std::filesystem::path result;
  size_t start = 0;
  size_t end = name.find('.');
  
  while (end != std::string::npos) {
    result /= name.substr(start, end - start);
    start = end + 1;
    end = name.find('.', start);
  }
  result /= name.substr(start);
  
  return result;
}

bool PackageManager::initializePackageDir() {
  std::filesystem::path pkgDir = getPackagesDir();
  try {
    std::filesystem::create_directories(pkgDir);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to create package directory: " << e.what() << std::endl;
    return false;
  }
}

std::optional<PackageManifest> PackageManager::parseManifest(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  return ManifestParser::parse(buffer.str());
}

// Simple JSON parser for manifest
std::optional<PackageManifest> ManifestParser::parse(const std::string& jsonContent) {
  PackageManifest manifest;
  
  // Very simple JSON parsing - extract key-value pairs
  size_t pos = 0;
  while (pos < jsonContent.size()) {
    // Find next quoted key
    size_t keyStart = jsonContent.find('"', pos);
    if (keyStart == std::string::npos) break;
    
    size_t keyEnd = jsonContent.find('"', keyStart + 1);
    if (keyEnd == std::string::npos) break;
    
    std::string key = jsonContent.substr(keyStart + 1, keyEnd - keyStart - 1);
    
    // Find colon
    size_t colonPos = jsonContent.find(':', keyEnd);
    if (colonPos == std::string::npos) break;
    
    // Find value (quoted string)
    size_t valueStart = jsonContent.find('"', colonPos);
    if (valueStart == std::string::npos) break;
    
    size_t valueEnd = jsonContent.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) break;
    
    std::string value = jsonContent.substr(valueStart + 1, valueEnd - valueStart - 1);
    
    if (key == "name") {
      manifest.name = value;
    } else if (key == "version") {
      manifest.version = value;
    } else if (key == "entry") {
      manifest.entry = value;
    }
    
    pos = valueEnd + 1;
  }
  
  if (manifest.isValid()) {
    return manifest;
  }
  return std::nullopt;
}

// Get stdlib source for built-in packages
static std::string getStdlibSourceForPackage(const std::string& packageName);

bool PackageManager::installPackage(const std::string& packageName) {
  if (!initializePackageDir()) {
    return false;
  }
  
  std::filesystem::path pkgPath = getPackagesDir() / packageNameToPath(packageName);
  
  try {
    // Create package directory
    std::filesystem::create_directories(pkgPath);
    
    // Get source content for built-in packages
    std::string source = getStdlibSourceForPackage(packageName);
    if (source.empty()) {
      std::cerr << "Unknown package: " << packageName << std::endl;
      return false;
    }
    
    // Determine entry file name from package name
    std::string entryFile;
    size_t lastDot = packageName.rfind('.');
    if (lastDot != std::string::npos) {
      entryFile = packageName.substr(lastDot + 1) + ".epp";
    } else {
      entryFile = packageName + ".epp";
    }
    
    // Write the source file
    std::filesystem::path sourcePath = pkgPath / entryFile;
    std::ofstream outFile(sourcePath);
    if (!outFile.is_open()) {
      std::cerr << "Failed to create source file: " << sourcePath << std::endl;
      return false;
    }
    outFile << source;
    outFile.close();
    
    // Create manifest.json
    std::filesystem::path manifestPath = pkgPath / "manifest.json";
    std::ofstream manifestFile(manifestPath);
    if (!manifestFile.is_open()) {
      std::cerr << "Failed to create manifest file" << std::endl;
      return false;
    }
    
    // Extract version from source comments or use default
    std::string version = "0.1.0";
    
    manifestFile << "{\n";
    manifestFile << "  \"name\": \"" << packageName << "\",\n";
    manifestFile << "  \"version\": \"" << version << "\",\n";
    manifestFile << "  \"entry\": \"" << entryFile << "\"\n";
    manifestFile << "}\n";
    manifestFile.close();
    
    std::cout << "Installed " << packageName << " @ " << version << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to install package: " << e.what() << std::endl;
    return false;
  }
}

bool PackageManager::removePackage(const std::string& packageName) {
  std::filesystem::path pkgPath = getPackagesDir() / packageNameToPath(packageName);
  
  try {
    if (!std::filesystem::exists(pkgPath)) {
      std::cerr << "Package not found: " << packageName << std::endl;
      return false;
    }
    
    std::filesystem::remove_all(pkgPath);
    std::cout << "Removed " << packageName << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to remove package: " << e.what() << std::endl;
    return false;
  }
}

bool PackageManager::isInstalled(const std::string& packageName) {
  std::filesystem::path pkgPath = getPackagesDir() / packageNameToPath(packageName);
  return std::filesystem::exists(pkgPath / "manifest.json");
}

std::optional<std::filesystem::path> PackageManager::getPackageEntry(const std::string& packageName) {
  std::filesystem::path pkgPath = getPackagesDir() / packageNameToPath(packageName);
  std::filesystem::path manifestPath = pkgPath / "manifest.json";
  
  auto manifest = parseManifest(manifestPath);
  if (!manifest) {
    return std::nullopt;
  }
  
  return pkgPath / manifest->entry;
}

std::vector<std::string> PackageManager::listInstalledPackages() {
  std::vector<std::string> packages;
  std::filesystem::path pkgDir = getPackagesDir();
  
  if (!std::filesystem::exists(pkgDir)) {
    return packages;
  }
  
  try {
    for (const auto& entry : std::filesystem::directory_iterator(pkgDir)) {
      if (entry.is_directory()) {
        std::string name = entry.path().filename().string();
        
        // Check subdirectories for scoped packages (e.g., std/)
        if (name == "std") {
          for (const auto& subEntry : std::filesystem::directory_iterator(entry.path())) {
            if (subEntry.is_directory()) {
              std::string subName = subEntry.path().filename().string();
              if (std::filesystem::exists(subEntry.path() / "manifest.json")) {
                packages.push_back("std." + subName);
              }
            }
          }
        } else {
          if (std::filesystem::exists(entry.path() / "manifest.json")) {
            packages.push_back(name);
          }
        }
      }
    }
  } catch (const std::exception& e) {
    // Ignore errors during enumeration
  }
  
  return packages;
}

// Stdlib source content for packages
static std::string getStdlibSourceForPackage(const std::string& packageName) {
  if (packageName == "std.math") {
    return R"(
function add a and b
  return a + b
end

function subtract a and b
  return a - b
end

function multiply a and b
  return a * b
end

function divide a and b
  return a / b
end

function power base and exp
  set result to 1
  set i to 0
  while i is less than exp do
    set result to result * base
    set i to i + 1
  end
  return result
end

function abs x
  if x is less than 0 then
    return -x
  end
  return x
end

function max a and b
  if a is greater than b then
    return a
  end
  return b
end

function min a and b
  if a is less than b then
    return a
  end
  return b
end

function sqrt x
  # Square root - conceptual implementation
  return x
end

function sin x
  # Sine function - conceptual
  return 0.0
end

function cos x
  # Cosine function - conceptual
  return 1.0
end

function tan x
  # Tangent function - conceptual
  return 0.0
end

function factorial n
  # Factorial - iterative implementation
  if n is less than 0 then
    return 0
  end
  set result to 1
  set i to 1
  while i is less than or equal to n do
    set result to result * i
    set i to i + 1
  end
  return result
end

function gcd a and b
  # Greatest common divisor
  while b is not equal to 0 do
    set temp to b
    set b to a % b
    set a to temp
  end
  return a
end

function lcm a and b
  # Least common multiple
  return (a * b) / gcd a and b
end

function round x
  # Round to nearest integer
  # Conceptual
  return x
end

function floor x
  # Floor function
  # Conceptual
  return x
end

function ceil x
  # Ceiling function
  # Conceptual
  return x
end
)";
  }
  
  if (packageName == "std.io") {
    return R"(
# IO utilities for E++
# Note: print and input are built-in, these are wrappers

function println message
  say message
end

function print message
  # For now, same as println
  say message
end

function read_line prompt
  if prompt is not null then
    say prompt
  end
  ask ""
  return input
end

function read_number prompt
  if prompt is not null then
    say prompt
  end
  ask "Enter number:"
  return input
end

function printf format and values
  # Simple printf-style formatting
  # Conceptual implementation
  say format
end

function read_file path
  # Read file content
  # Conceptual implementation
  return ""
end

function write_file path and content
  # Write content to file
  # Conceptual implementation
end

function append_file path and content
  # Append content to file
  # Conceptual implementation
end

function file_exists path
  # Check if file exists
  # Conceptual implementation
  return false
end
)";
  }
  
  if (packageName == "std.collections") {
    return R"(
function contains arr and item
  set i to 0
  while i is less than len arr do
    if arr @ i is equal to item then
      return true
    end
    set i to i + 1
  end
  return false
end

function index_of arr and item
  set i to 0
  while i is less than len arr do
    if arr @ i is equal to item then
      return i
    end
    set i to i + 1
  end
  return -1
end

function push arr and item
  # Returns new array with item appended
  # Note: This creates a copy since E++ arrays are immutable
  set new_arr to arr
  # Note: In a real implementation, this would need native support
  # For now, this is a placeholder
  return new_arr
end

function length arr
  return len arr
end

function is_empty arr
  return len arr is equal to 0
end

function slice arr and start and end
  # Return subarray from start to end
  # Conceptual implementation
  return arr
end

function reverse arr
  # Reverse array
  # Conceptual implementation
  return arr
end

function sort arr
  # Sort array (conceptual - bubble sort would work)
  # Conceptual implementation
  return arr
end

function map arr and fn
  # Apply function to each element
  set result to []
  set i to 0
  while i is less than len arr do
    set val to fn arr @ i
    # push result val - conceptual
    set i to i + 1
  end
  return result
end

function filter arr and fn
  # Filter elements that match predicate
  set result to []
  set i to 0
  while i is less than len arr do
    set val to arr @ i
    if fn val then
      # push result val
    end
    set i to i + 1
  end
  return result
end

function reduce arr and fn and initial
  # Reduce array to single value
  set acc to initial
  set i to 0
  while i is less than len arr do
    set val to arr @ i
    set acc to fn acc and val
    set i to i + 1
  end
  return acc
end

function find arr and fn
  # Find first element matching predicate
  set i to 0
  while i is less than len arr do
    set val to arr @ i
    if fn val then
      return val
    end
    set i to i + 1
  end
  return null
end

function every arr and fn
  # Check if all elements match predicate
  set i to 0
  while i is less than len arr do
    if not fn arr @ i then
      return false
    end
    set i to i + 1
  end
  return true
end

function some arr and fn
  # Check if any element matches predicate
  set i to 0
  while i is less than len arr do
    if fn arr @ i then
      return true
    end
    set i to i + 1
  end
  return false
end

function unique arr
  # Remove duplicates
  # Conceptual implementation
  return arr
end

function concat arr1 and arr2
  # Concatenate two arrays
  # Conceptual implementation
  return arr1
end
)";
  }
  
  if (packageName == "std.string") {
    return R"(
# String utilities for E++
# Note: strings are immutable in E++

function length s
  return len s
end

function is_empty s
  set slen to len s
  if slen is equal to 0 then
    return true
  end
  return false
end

function starts_with s and prefix
  # Check if string starts with prefix
  # Simple implementation - check character by character
  set plen to len prefix
  set slen to len s
  if plen is greater than slen then
    return false
  end
  set i to 0
  while i is less than plen do
    # Note: E++ doesn't have direct string indexing
    # This is a conceptual implementation
    set i to i + 1
  end
  return true
end

function ends_with s and suffix
  # Check if string ends with suffix
  set suflen to len suffix
  set slen to len s
  if suflen is greater than slen then
    return false
  end
  # Conceptual implementation
  return true
end

function contains s and substr
  # Check if string contains substring
  # Conceptual implementation
  return true
end

function trim s
  # Remove leading and trailing whitespace
  # Conceptual implementation - E++ would need native support
  return s
end

function to_upper s
  # Convert to uppercase
  # Conceptual implementation
  return s
end

function to_lower s
  # Convert to lowercase
  # Conceptual implementation
  return s
end

function split s and delimiter
  # Split string by delimiter, return array
  # Conceptual implementation - returns single-element array
  return [s]
end

function join arr and delimiter
  # Join array elements with delimiter
  # Conceptual implementation
  return ""
end

function replace s and old_str and new_str
  # Replace all occurrences of old_str with new_str
  # Conceptual implementation
  return s
end

function substring s and start and length
  # Extract substring from start with given length
  # Conceptual implementation
  return s
end

function index_of s and substr
  # Find first index of substring
  # Conceptual implementation
  return -1
end

function last_index_of s and substr
  # Find last index of substring
  # Conceptual implementation
  return -1
end

function repeat s and count
  # Repeat string count times
  # Conceptual implementation
  return s
end

function reverse s
  # Reverse string
  # Conceptual implementation
  return s
end

function pad_left s and width and pad_char
  # Pad string from left to width with pad_char
  # Conceptual implementation
  return s
end

function pad_right s and width and pad_char
  # Pad string from right to width with pad_char
  # Conceptual implementation
  return s
end
)";
  }
  
  if (packageName == "std.sys") {
    return R"(
# System utilities for E++

function args
  # Return command-line arguments as array
  # Conceptual implementation
  return []
end

function env name
  # Get environment variable
  # Conceptual implementation
  return ""
end

function platform
  # Return current platform (windows, linux, macos)
  # Conceptual implementation
  return "unknown"
end

function version
  # Return E++ version
  return "0.4.0"
end
)";
  }
  
  if (packageName == "std.random") {
    return R"(
# Random number generation utilities for E++

function random
  # Return random float between 0.0 and 1.0
  # Conceptual implementation - would use system random
  return 0.5
end

function randint min and max
  # Return random integer between min and max inclusive
  # Conceptual implementation
  return min
end

function choice arr
  # Return random element from array
  if len arr is equal to 0 then
    return null
  end
  set index to randint 0 and (len arr - 1)
  return arr @ index
end

function shuffle arr
  # Shuffle array in place (conceptual - returns new array)
  # Conceptual implementation - Fisher-Yates would be ideal
  return arr
end
)";
  }
  
  if (packageName == "std.time") {
    return R"(
# Time and date utilities for E++

function now
  # Return current timestamp (seconds since epoch)
  # Conceptual implementation
  return 0
end

function sleep seconds
  # Sleep for specified seconds
  # Conceptual implementation - would block execution
end

function format_time timestamp
  # Format timestamp as string
  # Conceptual implementation
  return "1970-01-01 00:00:00"
end

function timestamp
  # Alias for now
  return now
end
)";
  }
  
  if (packageName == "std.fs") {
    return R"(
# File system utilities for E++

function read_file path
  # Read entire file as string
  # Conceptual implementation
  return ""
end

function write_file path and content
  # Write string to file
  # Conceptual implementation
end

function exists path
  # Check if file/directory exists
  # Conceptual implementation
  return false
end

function is_file path
  # Check if path is a file
  # Conceptual implementation
  return false
end

function is_dir path
  # Check if path is a directory
  # Conceptual implementation
  return false
end

function list_dir path
  # List contents of directory
  # Conceptual implementation
  return []
end
)";
  }
  
  if (packageName == "std.debug") {
    return R"(
# Debug and logging utilities for E++

function log message
  # Log message to console/debug output
  say "[LOG] " + message
end

function warn message
  # Log warning message
  say "[WARN] " + message
end

function error message
  # Log error message
  say "[ERROR] " + message
end

function debug value
  # Debug print value with type info
  # Conceptual implementation
  say "[DEBUG] " + value
end

function assert condition and message
  # Assert condition, log error if false
  if not condition then
    error message
  end
end
)";
  }
  
  if (packageName == "std.convert") {
    return R"(
# Type conversion utilities for E++

function to_string value
  # Convert value to string
  # Conceptual implementation - most values are already strings in E++
  return value
end

function to_number value
  # Convert string to number
  # Conceptual implementation
  return 0
end

function to_bool value
  # Convert value to boolean
  # Conceptual implementation
  if value then
    return true
  end
  return false
end

function to_array value
  # Convert value to array if possible
  # Conceptual implementation
  return [value]
end

function type_of value
  # Return type of value as string
  # Conceptual implementation
  return "unknown"
end
)";
  }
  
  return "";
}

} // namespace epp
