import sys

with open("src/webserver.cpp", "r") as f:
    content = f.read()

content = content.replace("    }};\n\n    const std::array<ApiRoute, 19> routes {{", "    };\n\n    const std::array<ApiRoute, 19> routes {{")

with open("src/webserver.cpp", "w") as f:
    f.write(content)
