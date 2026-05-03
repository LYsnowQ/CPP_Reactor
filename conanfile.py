from conan import ConanFile
from conan.tools.cmake import cmake_layout


class CPPReactorConan(ConanFile):
    name = "cppreactor"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"

    requires = (
        "nlohmann_json/3.11.3",
        "spdlog/1.14.1",
    )

    generators = (
        "CMakeDeps",
        "CMakeToolchain",
    )

    def layout(self):
        cmake_layout(self)
