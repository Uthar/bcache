// FIXME cleanup imports
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <regex>
#include "nix.hh"
#include "boost/filesystem/string_file.hpp"
#include "boost/algorithm/string.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define MY_SOCK_PATH "/tmp/mysock"
#define LISTEN_BACKLOG 50

#define handle_error(msg)                               \
  do { perror(msg); goto restart; } while (0)

int running=1;

// FIXME just remove when starting when it already exists
void cleanup(int sig){
  /* When no longer required, the socket pathname, MY_SOCK_PATH
     should be deleted using unlink(2) or remove(3) */
  running=0;
  unlink(MY_SOCK_PATH);
  exit(0);
}

void ignore_handler(int sig){
}

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

    int wrote = write(conn,http.c_str(),http.size());
    if (wrote < 0) {
      perror("ERROR writing to socket");
    }
    close(conn);
    return;
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
    auto narHash2=narHash.substr(7);
    // res.append("URL: nar/"+hashPart+"-"+narHash2+".nar\n");
    res.append("URL: nar/"+hashPart+"-"+narHash2+".nar\n");
    res.append("Compression: none\n");
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
      }catch(const std::exception &ex){}
    }

    // FIXME just use info->fingerprint(*store())
    auto fingerprint =
      fingerprintPath(storePath,narHash,narSize,refs_long);
    std::cout << fingerprint << std::endl;

    if (!secretKey.empty()){
      // FIXME just nix::SecretKey
      std::string sig = signString(secretKey.c_str(),fingerprint.c_str());
      res.append("Sig: "+sig+"\n");
    }

    std::string http =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/x-nix-narinfo\r\n";
    http.append("Content-Length: "+std::to_string(res.size()));
    http.append("\r\n");
    http.append("\r\n");
    http.append(res);
    write(conn,http.c_str(),http.size());
    close(conn);
  }

  else if(std::regex_match(path,std::regex("^[/]nar[/]([0-9a-z]+)-([0-9a-z]+)[.]nar$"))){
    std::string hashPart = path.substr(5,32);
    std::string expectedNarHash = path.substr(38,52);
    std::cout << hashPart << std::endl;
    std::cout << expectedNarHash << std::endl;

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
      "Content-Type: text/plain\r\n"
      "Content-Length: "+narSize+"\r\n";
    http.append("\r\n");
    write(conn,http.c_str(),http.size());
    std::string cmd="nix dump-path "+storePath;
    // FIXME use narFromPath with FdSink(conn)
    // FIXME implement XzSink/GzippedSink/BzippedSink
    // See serialise.cc, dump-path.cc
    try{
      FILE *dump=popen(cmd.c_str(),"r");
      if(dump){
        int fd = fileno(dump);
        constexpr std::size_t nbuf = 1024;
        uint8_t buf[nbuf] ;
        int nread=0;
        while(nread = read(fd,buf,nbuf)){
          if(conn){
            write(conn,buf,nread);
            memset(buf,0,nbuf);
          }
        }
        pclose(dump);
      }
      close(conn);
    }catch(const std::exception &ex){
      perror("writing to socket\n");
    }
  }

  else if(std::regex_match(path,std::regex("^[/]nar[/]([0-9a-z]+)[.]nar$"))){
    // FIXME use regex groups
    std::string hashPart = path.substr(5,32);
    std::cout << hashPart << std::endl;

    auto storePath = queryPathFromHashPart(hashPart);
    auto maybeInfo = queryPathInfo(storePath);
    // FIXME abstract http away
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
    std::string http =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: "+narSize+"\r\n";
    http.append("\r\n");
    write(conn,http.c_str(),http.size());
    std::cout << storePath << std::endl;
    std::string cmd="nix dump-path "+storePath;
    try{
      FILE *dump=popen(cmd.c_str(),"r");
      if(dump){
        int fd = fileno(dump);
        constexpr std::size_t nbuf = 1024;
        uint8_t buf[nbuf] ;
        int nread =0;
        // important, otherwise writin zeros
        while(nread = read(fd,buf,nbuf)){
          if(conn){
            write(conn,buf,nread);
            memset(buf,0,nbuf);
          }
        }
        pclose(dump);
      }
      close(conn);
    }catch(const std::exception &ex){
      perror("writing to socket\n");
    }
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


int
main(int argc, char *argv[]){
  // FIXME move to init
  signal(SIGINT, &cleanup);
  struct sigaction ignore;
  ignore.sa_handler = ignore_handler;
  sigemptyset(&ignore.sa_mask);
  ignore.sa_flags=0;
  sigaction(SIGPIPE, &ignore, NULL);

  int sfd, cfd;
  struct sockaddr_un my_addr, peer_addr;
  socklen_t peer_addr_size;

  // FIXME remove, possible infinite loop
  // will be restarted by systemd
restart:
  if(sfd){
    close(sfd);
  }
  unlink(MY_SOCK_PATH);

  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd == -1)
    handle_error("socket");

  memset(&my_addr, 0, sizeof(struct sockaddr_un));
  my_addr.sun_family = AF_UNIX;
  strncpy(my_addr.sun_path, MY_SOCK_PATH, sizeof(my_addr.sun_path)-1);

  if(bind(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_un)) == -1)
    handle_error("bind");

  if(listen(sfd, LISTEN_BACKLOG) == -1)
    handle_error("listen");

  chmod(MY_SOCK_PATH,0765);

  boost::asio::io_service ioService;
  boost::thread_group threadpool;
  boost::asio::io_service::work work(ioService);
  int nthreads = 10;
  for(int i=0;i<nthreads;i++){
    threadpool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
    );
  }

  while(running){
    peer_addr_size = sizeof(struct sockaddr_un);
    // FIXME add destructor for cfd
    cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if(cfd == -1)
      handle_error("accept");
    ioService.post(boost::bind(handle,cfd));
  }
}
