import os

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class AedstDesktopConan(ConanFile):
    """Third-party C++ dependencies for apps/desktop.

    Qt is deliberately NOT listed here. The ConanCenter `qt` recipe builds
    Qt from source (hours, and fragile across compilers), so Qt 6.8+ is
    expected to be installed via the official Qt online installer or
    aqtinstall, with CMAKE_PREFIX_PATH pointing at it (CMakePresets.json
    reads $QT_ROOT_DIR, which jurplel/install-qt-action also sets in CI).
    Conan owns everything else.
    """

    name = "aedst-desktop"
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_asio": [True, False]}
    default_options = {
        "with_asio": False,
        # Asio is header-only; don't build the compiled Boost libraries.
        "boost/*:header_only": True,
        "spdlog/*:header_only": False,
    }

    def requirements(self):
        self.requires("openssl/3.3.2")
        self.requires("sqlite3/3.53.4")
        self.requires("spdlog/1.14.1")
        # Pinned to the fmt version spdlog/1.14.1 itself requires, so the
        # graph has one fmt.
        self.requires("fmt/10.2.1")
        # Always resolvable so switching with_asio doesn't change the lockfile
        # shape; only linked when AEDST_ENABLE_ASIO=ON.
        self.requires("boost/1.86.0")

    def build_requirements(self):
        self.test_requires("gtest/1.15.0")

    def layout(self):
        # cmake_layout's shape, but with lowercase per-build-type folders so
        # `--output-folder=build` lands generators at build/<preset>/generators,
        # which is exactly where CMakePresets.json's toolchainFile points.
        cmake_layout(self)
        build_type = str(self.settings.build_type).lower()
        self.folders.build = build_type
        self.folders.generators = os.path.join(build_type, "generators")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self, generator="Ninja")
        # CMakePresets.json is hand-maintained; don't let Conan write a
        # competing CMakeUserPresets.json next to it.
        tc.user_presets_path = False
        tc.cache_variables["AEDST_ENABLE_ASIO"] = bool(self.options.with_asio)
        tc.generate()
