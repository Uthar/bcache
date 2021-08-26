#include "unistd.h"

#include <string>
#include <vector>
#include <regex>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <nix/config.h>
#include <nix/globals.hh>
#include <nix/hash.hh>
#include <nix/compression.hh>

#include "nix.hh"
#include "sink.hh"
#include "config.hh"
#include "compression.hh"

void handle(int conn){
  uint8_t buf[1000] = {0};
  read(conn, buf, 1000-1);

  std::vector<std::string> splits;
  boost::split(splits,buf,boost::is_any_of(" "));
  std::string path;
  if(splits.size() >= 2)
    path = splits.at(1);

  if(path=="/nix-cache-info"){
    std::string http =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n";
    http.append("StoreDir: "+nix::settings.nixStore+"\n");
    http.append("WantMassQuery: 1\n");
    http.append("Priority: 25\n");

    write(conn,http.c_str(),http.size());
    close(conn);
  }

  else if(std::regex_match(path,std::regex("^[/]([0-9a-z]+)[.]narinfo$"))){
    std::string hashPart = path.substr(1,32);

    auto storePath = queryPathFromHashPart(hashPart);
    auto maybeInfo = queryPathInfo(storePath);
    if(!maybeInfo.has_value()){
      std::string http =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";
      http.append("No such path.");
      write(conn,http.c_str(),http.size());
      close(conn);
      return;
    }
    auto info = maybeInfo.value();

    std::string res;

    res.append("StorePath: "+storePath+"\n");
    auto narHash=info->narHash.to_string(nix::Base32,true);
    auto narHash2=narHash.substr(std::string("sha256:").length());
    res.append("URL: nar/"+hashPart+"-"+narHash2+".nar");
    if(compressionType()!="none"){
        res.append("."+compressionType());
    }
    res.append("\n");
    // FIXME: add FileHash, FileSize
    res.append("Compression: "+compressionType()+"\n");
    res.append("NarHash: "+narHash+"\n");
    auto narSize=std::to_string(info->narSize);
    res.append("NarSize: "+narSize+"\n");

    std::vector<std::string> references;
    std::vector<std::string> refs_long;
    for (auto const &r : info->references){
      references.push_back(std::string{r.to_string()});
      refs_long.push_back(nix::settings.nixStore+"/"+std::string{r.to_string()});
    }
    if(references.size()>0){
      res.append("References: ");
      res.append(boost::algorithm::join(references," "));
      res.append("\n");
    }

    if (info->deriver) {
      res.append("Deriver: ");
      res.append(info->deriver->to_string());
      res.append("\n");
    }

    std::string secretKey;
    char *keyPath = getenv("NIX_SECRET_KEY_FILE");
    if (keyPath) {
      try{
        boost::filesystem::load_string_file(keyPath,secretKey);
      }catch(const std::exception &ex){
        std::cerr << "Cannot read secret key file: "+std::string(keyPath) << std::endl;
      }
    }


    if (!secretKey.empty()){
      // FIXME just use info->fingerprint(*store())
      auto fingerprint = fingerprintPath(storePath,narHash,narSize,refs_long);
      // FIXME just nix::SecretKey
      std::string sig = signString(secretKey.c_str(),fingerprint.c_str());
      res.append("Sig: "+sig+"\n");
    }

    std::string http =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/x-nix-narinfo\r\n"
      "Content-Length: "+std::to_string(res.size())+"\r\n"
      "\r\n";
    http.append(res);
    write(conn,http.c_str(),http.size());
    close(conn);
  }

  else if(std::regex_match(path,std::regex("^[/]nar[/]([0-9a-z]+)-([0-9a-z]+)[.]nar.*"))){
    std::string hashPart = path.substr(5,32);
    std::string expectedNarHash = path.substr(38,52);

    auto storePath = queryPathFromHashPart(hashPart);
    auto maybeInfo = queryPathInfo(storePath);
    if(!maybeInfo.has_value()){
      std::string http =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";
      http.append("No such path.");
      write(conn,http.c_str(),http.size());
      close(conn);
      return;
    }
    auto info = maybeInfo.value();

    auto narHash=info->narHash.to_string(nix::Base32,true);
    auto narSize=std::to_string(info->narSize);
    if(narHash!="sha256:"+expectedNarHash){
      std::string http =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";
      http.append("Incorrect NAR hash. Maybe the path has been recreated.\n");
      write(conn,http.c_str(),http.size());
      close(conn);
      return;
    }

    std::string http =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/x-nix-nar\r\n";
      // "Content-Length: "+narSize+"\r\n"; FIXME: FileSize
    http.append("\r\n");
    write(conn,http.c_str(),http.size());
    auto fdsink = nix::FdSink(conn);
    auto nullsink = nix::NullSink();
    auto altsink = AltSink(fdsink,nullsink);
    auto zsink = mkCompressionSink(compressionType(),altsink);
    auto pth = store()->queryPathFromHashPart(hashPart);
    if(pth.has_value()){
      try{
        store()->narFromPath(pth.value(),*zsink);
        zsink->finish();
        // VERY important, otherwise small files, like 5k, aren't written!
        fdsink.flush();
      }catch(const nix::Error & ex){
        std::cerr<< "error when writing nar: " << ex.what() <<std::endl;
      };
    }
    close(conn);
  }

  else {
    std::string http =
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n";
    http.append("No such path.");
    write(conn,http.c_str(),http.size());
    close(conn);
  }
}
