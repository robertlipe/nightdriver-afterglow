import sys

with open("src/webserver.cpp", "r") as f:
    content = f.read()

content = content.replace("ApiRoute{\"/effects\"", "{\"/effects\"")

with open("src/webserver.cpp", "w") as f:
    f.write(content)
