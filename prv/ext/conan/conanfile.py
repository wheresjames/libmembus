
#
#   Install dependencies
# $ conan install .
#
#   Conan build dependencies
# $ conan install . --build
#
#   Conan build
# $ conan build .
#
#   Build from cmake
# $ cmake --build ./bld -j 8
#
#   install / build / package
# $ ./clean.sh && conan install . && conan build . && conan package .
#
#   Install / Uninstall
# $ sudo cmake --install ./bld && sudo ./run.sh uninstall
#

import datetime
import platform
from conans import ConanFile, tools, CMake

# Read from config file
def readConfig(fname):
    cfg = {}
    with open(fname) as f:
        lines = f.readlines()
        for line in lines:
            parts = line.strip().replace("\t", " ").split(" ")
            k = parts.pop(0).strip()
            cfg[k] = " ".join(parts).strip()
    return cfg


class Library_libmembus(ConanFile):

    # Read config file
    cfg = readConfig('PROJECT.txt')
    for k in ['name', 'version', 'description', 'url', 'license', 'company', 'author']:
        if k not in cfg:
            raise Exception("Variable '%s' not specified in 'PROJECT.txt'" % k)
        locals()[k] = cfg[k]

    # Create a build number
    buildno = datetime.datetime.now().strftime("%y.%m.%d.%H%M").replace(".0", ".")

    # requires = ()
    settings = "os", "compiler", "arch", "build_type"
    options = {}
    default_options = ""
    exports = "*"
    generators = "cmake"
    build_policy = "missing"
    build_output = "./bld"

    if platform.system() == "Windows":
        install_path = "C:/Program Files/%s" % cfg['name']
    else:
        install_path = "/usr/local"

    def conan_info(self):
        self.info.settings.clear()

    def configure(self):
        if self.settings.compiler == "gcc":
            self.settings.compiler.libcxx = "libstdc++11"

    def build(self):

        print("COMPILER: %s %s %s" 
                % (self.settings.compiler, self.settings.compiler.version, self.settings.compiler.libcxx))

        # https://docs.conan.io/en/latest/reference/build_helpers/cmake.html#definitions
        cmake = CMake(self, parallel=True)
        # cmake.verbose = True
        cmake.definitions["APPBUILD"] = self.buildno
        cmake.definitions["CMAKE_INSTALL_PREFIX"] = self.install_path
        cmake.configure(build_dir=self.build_output)

        print("cmake.command_line : %s" % cmake.command_line)
        print("cmake.build_config : %s" % cmake.build_config)

        cmake.build()
        # cmake.install()

    def package(self):
        self.run('cpack -B ./pck -G DEB -C Release --config %s/CPackConfig.cmake' % self.build_output)

    def package_info(self):
        print(self.package_folder)

