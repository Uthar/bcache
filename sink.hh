#pragma once

#include <nix/serialise.hh>

/* Sinks data to sink, or if it's not good(), falls back to alt */
struct AltSink : nix::Sink
{
    nix::Sink & sink;
    nix::Sink & alt;

    AltSink(nix::Sink & sink, nix::Sink & alt) : sink(sink), alt(alt) { };

    void operator()(std::string_view data) override {
      if (sink.good())
        sink(data);
      else
        alt(data);
    }

    bool good() override { return sink.good(); };
};
