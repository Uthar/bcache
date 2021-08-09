#include <string>

#include "config.hh"

std::string _compressionType = "none";
std::string _socketPath;

void setCompressionType(std::string type) {
    _compressionType=type;
}
void setSocketPath(std::string path) {
    _socketPath=path;
}
std::string compressionType() {
    return _compressionType;
}
const char * socketPath() {
    return _socketPath.c_str();
}
