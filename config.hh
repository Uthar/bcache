#pragma once

#include <string>

void setCompressionType(std::string type);
void setSocketPath(std::string path);
std::string compressionType();
const char * socketPath();
