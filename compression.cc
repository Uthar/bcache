

#include <nix/compression.hh>
#include <nix/ref.hh>
#include <zstd.h>

#include "compression.hh"

#include <iostream>

struct ChunkedCompressionSink : nix::CompressionSink
{
    uint8_t outbuf[32 * 1024];

    void write(std::string_view data) override
    {
        const size_t CHUNK_SIZE = sizeof(outbuf) << 2;
        while (!data.empty()) {
            size_t n = std::min(CHUNK_SIZE, data.size());
            writeInternal(data.substr(0, n));
            data.remove_prefix(n);
        }
    }

    virtual void writeInternal(std::string_view data) = 0;
};

struct ZstdCompressionSink : nix::CompressionSink
{
    Sink & nextSink;
    ZSTD_CCtx_s * ctx ;

    ZstdCompressionSink(Sink & nextSink) : nextSink(nextSink)
    {
        ctx = ZSTD_createCStream();
        if (!ctx)
            throw nix::CompressionError("unable to initialise zstd encoder");
    }

    ~ZstdCompressionSink()
    {
        ZSTD_freeCCtx(ctx);
    }

    void finish() override
    {
        CompressionSink::flush();
        write({});
    }

    void write(std::string_view data) override
    {
        int offset = 0;
        size_t bufsz = 32 * 4096;
        uint8_t inbuf[32 * 4096];
        uint8_t outbuf[32 * 4096];
        ZSTD_inBuffer in = {inbuf,bufsz,0};
        ZSTD_outBuffer out = {outbuf,bufsz,0};
        int nin = 0;
        int done = 0;
        while(!done){

          nin = std::min((data.size()-offset),bufsz);
          memcpy(inbuf, (data.data()+offset), nin);
          offset += nin;

          ZSTD_EndDirective flag = ZSTD_e_continue;
          if (in.pos == in.size) {
            flag = ZSTD_e_flush;
            in.pos = 0;
          }
          if (!nin) {
            flag = ZSTD_e_end;
          }
          in.size = nin;
          int ret = ZSTD_compressStream2(ctx, &out, &in, flag);
          if (ZSTD_isError(ret))
                throw nix::CompressionError("error %d while compressing zstd file", ret);
          done = !nin && ret == 0;
          if (out.pos == out.size || done) {
            nextSink({(char *)outbuf, out.pos});
            out.pos = 0;
          }
        }
    }
};

nix::ref<nix::CompressionSink> mkCompressionSink(const std::string & method, nix::Sink & nextSink){
    if (method == "zstd")
        return nix::make_ref<ZstdCompressionSink>(nextSink);
    else
        return nix::makeCompressionSink(method, nextSink);
}
