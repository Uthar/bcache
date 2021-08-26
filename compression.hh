#pragma once

#include <nix/compression.hh>

nix::ref<nix::CompressionSink> mkCompressionSink(const std::string & method, nix::Sink & nextSink);
