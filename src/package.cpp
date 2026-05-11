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
  
  return "";
}

} // namespace epp
