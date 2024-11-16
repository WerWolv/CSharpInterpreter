#include <ili/assembly.hpp>
#include <ili/runtime.hpp>

int main() {
    ili::Runtime runtime;

    static const std::filesystem::path BasePath = "test/example/bin/Debug/net8.0/win-x64";
    runtime.addAssemblyLoader([](const std::string &assemblyName) -> std::optional<ili::Assembly> {
        for (const auto &entry : std::filesystem::directory_iterator(BasePath)) {
            if (entry.path().stem() == assemblyName) {
                return ili::Assembly(entry.path());
            }
        }

        return std::nullopt;
    });

    runtime.run(ili::Assembly(BasePath / "example.dll"));

    return EXIT_SUCCESS;
}
