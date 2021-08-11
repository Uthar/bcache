// simple Nix binary cache through HTTP

#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "handle.hh"
#include "config.hh"


#define LISTEN_BACKLOG 50

void panic(std::string msg){
  if(errno)
    perror(msg.c_str());
  else
    std::cout<<msg<<std::endl;
  exit(errno);
}

void ignore_handler(int sig){
}

int
main(int argc, char *argv[]){
  struct sigaction ignore;
  ignore.sa_handler = ignore_handler;
  sigemptyset(&ignore.sa_mask);
  ignore.sa_flags=0;
  sigaction(SIGPIPE, &ignore, NULL);

  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("compression", po::value<std::string>(), "set compression type (bzip2,xz,none,lz)")
    ("socket", po::value<std::string>(), "set path of unix socket");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if(vm.count("help")){
    std::cout << desc << "\n";
    return 0;
  }

  if(vm.count("compression")){
    setCompressionType(vm["compression"].as<std::string>());
  }

  if(vm.count("socket")){
    setSocketPath(vm["socket"].as<std::string>());
  }else{
    panic("--socket is required");
  }

  int sfd, cfd;
  struct sockaddr_un my_addr, peer_addr;
  socklen_t peer_addr_size;

  // FIXME ugly
  unlink(socketPath());

  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd == -1)
    panic("socket()");

  memset(&my_addr, 0, sizeof(struct sockaddr_un));
  my_addr.sun_family = AF_UNIX;
  strncpy(my_addr.sun_path, socketPath(), sizeof(my_addr.sun_path)-1);

  if(bind(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_un)) == -1)
    panic("bind()");

  if(listen(sfd, LISTEN_BACKLOG) == -1)
    panic("listen()");

  chmod(socketPath(),0765);

  boost::asio::io_service ioService;
  boost::thread_group threadpool;
  boost::asio::io_service::work work(ioService);
  int nthreads = 10;
  for(int i=0;i<nthreads;i++){
    threadpool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
    );
  }

  peer_addr_size = sizeof(struct sockaddr_un);

  while(1){
    cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if(cfd != -1){
      ioService.post(boost::bind(handle,cfd));
    }
  }
}
