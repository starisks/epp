#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace epp {

// Package manifest structure
struct PackageManifest {
  std::string name;
  std::string version;
  std::string entry;
  std::vector<std::string> dependencies;

  bool isValid() const {
    return !name.empty() && !version.empty() && !entry.empty();
  }
};

// Package system for local package management
class PackageManager {
public:
  // Get the default packages directory (~/.epp/packages)
  static std::filesystem::path getPackagesDir();

  // Parse manifest.json file
  static std::optional<PackageManifest> parseManifest(const std::filesystem::path& path);

  // Install a package from a source directory to the local cache
  // For stdlib: copies from embedded source to ~/.epp/packages/
  static bool installPackage(const std::string& packageName);

  // Remove a package from the local cache
  static bool removePackage(const std::string& packageName);

  // Check if a package is installed
  static bool isInstalled(const std::string& packageName);

  // Get the entry point path for an installed package
  static std::optional<std::filesystem::path> getPackageEntry(const std::string& packageName);

  // List all installed packages
  static std::vector<std::string> listInstalledPackages();

  // Initialize the package directory structure
  static bool initializePackageDir();

private:
  // Convert package name to directory name (std.math -> std/math)
  static std::filesystem::path packageNameToPath(const std::string& name);
};

// Package manifest JSON parsing
class ManifestParser {
public:
  static std::optional<PackageManifest> parse(const std::string& jsonContent);
};

} // namespace epp
