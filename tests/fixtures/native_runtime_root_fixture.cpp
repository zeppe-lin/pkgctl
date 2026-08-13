// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <elf.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

namespace {

std::string shell_quote(const std::string& value)
{
  std::string result("'");
  for (const char byte : value) {
    if (byte == '\'') {
      result += "'\\''";
    } else {
      result.push_back(byte);
    }
  }
  result.push_back('\'');
  return result;
}

std::filesystem::path program_interpreter(const fs::path& executable)
{
  std::ifstream stream(executable, std::ios::binary);
  Elf64_Ehdr header{};
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!stream || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB ||
      header.e_machine != EM_X86_64 ||
      header.e_phentsize != sizeof(Elf64_Phdr) || header.e_phnum == PN_XNUM) {
    throw std::runtime_error(
        "runtime fixture is not a supported x86-64 ELF executable");
  }

  stream.seekg(static_cast<std::streamoff>(header.e_phoff));
  if (!stream) {
    throw std::runtime_error("cannot inspect runtime fixture program headers");
  }

  for (Elf64_Half index = 0; index < header.e_phnum; ++index) {
    Elf64_Phdr program_header{};
    stream.read(reinterpret_cast<char*>(&program_header), sizeof(program_header));
    if (!stream) {
      throw std::runtime_error("cannot inspect runtime fixture program headers");
    }
    if (program_header.p_type != PT_INTERP) {
      continue;
    }
    if (program_header.p_filesz < 2U || program_header.p_filesz > 4096U) {
      throw std::runtime_error("invalid runtime fixture ELF interpreter");
    }

    const auto resume = stream.tellg();
    stream.seekg(static_cast<std::streamoff>(program_header.p_offset));
    std::vector<char> value(static_cast<std::size_t>(program_header.p_filesz));
    stream.read(value.data(), static_cast<std::streamsize>(value.size()));
    if (!stream || value.back() != '\0') {
      throw std::runtime_error("cannot read runtime fixture ELF interpreter");
    }
    const std::string path(value.data(), value.size() - 1U);
    if (path.empty() || path.front() != '/' ||
        path.find('\0') != std::string::npos) {
      throw std::runtime_error("runtime fixture ELF interpreter is not absolute");
    }
    stream.clear();
    stream.seekg(resume);
    return fs::path(path);
  }
  return {};
}

void copy_one(const fs::path& root,
              const fs::path& source,
              const fs::path& logical_path)
{
  const auto destination = root / logical_path.relative_path();
  fs::create_directories(destination.parent_path());
  fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
  fs::permissions(destination, fs::status(source).permissions());
}

void copy_runtime(const fs::path& root, const fs::path& executable)
{
  copy_one(root, executable, executable);
  const auto interpreter = program_interpreter(executable);
  if (interpreter.empty()) {
    return;
  }
  copy_one(root, interpreter, interpreter);

  const std::string command = "ldd " + shell_quote(executable.string());
  FILE* stream = ::popen(command.c_str(), "r");
  if (stream == nullptr) {
    throw std::runtime_error("cannot inspect runtime fixture closure");
  }

  std::array<char, 4096> line{};
  std::vector<fs::path> dependencies;
  while (::fgets(line.data(), static_cast<int>(line.size()), stream) != nullptr) {
    const std::string value(line.data());
    const auto start = value.find('/');
    if (start == std::string::npos) {
      continue;
    }
    const auto end = value.find_first_of(" \t\n", start);
    const auto path = value.substr(start, end - start);
    if (!path.empty() && path.front() == '/') {
      dependencies.emplace_back(path);
    }
  }

  const int status = ::pclose(stream);
  if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error("ldd failed for the runtime fixture");
  }

  std::sort(dependencies.begin(), dependencies.end());
  dependencies.erase(
      std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
  for (const auto& dependency : dependencies) {
    copy_one(root, dependency, dependency);
  }
}

int run(int argc, char** argv)
{
  if (argc != 3) {
    std::cerr << "usage: native-runtime-root-fixture ROOT INTERPRETER\n";
    return 2;
  }

  fs::path root(argv[1]);
  const fs::path requested(argv[2]);
  if (!root.is_absolute() || !requested.is_absolute()) {
    throw std::runtime_error("root and interpreter paths must be absolute");
  }
  if (!fs::is_directory(root)) {
    throw std::runtime_error("runtime root is not an existing directory");
  }

  std::error_code ec;
  const auto interpreter = fs::canonical(requested, ec);
  if (ec || !fs::is_regular_file(interpreter)) {
    throw std::runtime_error("cannot resolve real interpreter runtime");
  }

  copy_runtime(root, interpreter);
  std::cout << interpreter.string() << '\n';
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  try {
    return run(argc, argv);
  } catch (const std::exception& problem) {
    std::cerr << "native-runtime-root-fixture: " << problem.what() << '\n';
    return 1;
  }
}
