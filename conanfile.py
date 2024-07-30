from conan import ConanFile
from conan.tools.cmake import (
    CMakeToolchain,
    CMake,
    cmake_layout,
    CMakeDeps,
)
from conan.tools.build import check_min_cppstd, can_run


class CanopennodeRecipe(ConanFile):
    name = "canopennode"
    version = "0.1.0"

    # Optional metadata
    license = "Apache-2.0"
    url = ""
    author = ""
    description = "CANopen protocol stack"
    topics = "CANopen"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_examples": [True, False],
    }
    default_options = {"shared": False, "fPIC": True, "build_examples": False}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = (
        "CMakeLists.txt",
        "301/*",
        "303/*",
        "303/*",
        "304/*",
        "305/*",
        "309/*",
        "extra/*",
        "storage/*",
        "*.h",
        "*.c",
    )

    def requirements(self):
        self.tool_requires("cmake/[>=3.23 <=3.30]")

    def validate(self):
        pass

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = self.options.build_examples
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if (not self.conf.get("tools.build:skip_test", default=False)) and can_run(
            self
        ):
            pass
            # cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.components["canopennode"].set_property(
            "cmake_target_name", "canopennode::canopennode"
        )

        self.cpp_info.components["co_storage"].set_property(
            "cmake_target_name", "canopennode::co_storage"
        )
        self.cpp_info.components["co_storage"].requires = ["canopennode"]

        self.cpp_info.components["co_storage_eeprom"].set_property(
            "cmake_target_name", "canopennode::co_storage_eeprom"
        )
        self.cpp_info.components["co_storage_eeprom"].requires = ["co_storage"]

    def package_id(self):
        self.info.clear()
