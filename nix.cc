#include "nix.hh"
#include "boost/algorithm/string.hpp"

static nix::ref<nix::Store> store()
{
  static std::shared_ptr<nix::Store> s;
    if (!s) {
      nix::loadConfFile();
      nix::settings.lockCPU = false;
      s = nix::openStore();
    }
    return nix::ref<nix::Store>(s);
}

std::string queryPathFromHashPart(std::string hashPart){
  try{
    auto path = store()->queryPathFromHashPart(hashPart);
    return path.has_value() && path.value().to_string().empty()
      ? ""
      : store()->printStorePath(*path);
  }catch(nix::Error e){
    printf("%s",e.what());
    return "";
  }
}

// => ($deriver, $narHash, $time, $narSize, $refs, $sigs)
std::optional<nix::ref<const nix::ValidPathInfo>>  queryPathInfo(std::string storePath){
  try{
    return store()->queryPathInfo(store()->parseStorePath(storePath));
  }catch(nix::Error e){
    printf("%s",e.what());
    return std::nullopt;
  }
}

std::string signString(const char * secretKey, const char * msg){
  return nix::SecretKey(secretKey).signDetached(msg);
}

// Return a fingerprint of a store path to be used in binary cache
// signatures. It contains the store path, the base-32 SHA-256 hash of
// the contents of the path, and the references.
// sub fingerprintPath {
std::string fingerprintPath(std::string storePath, std::string narHash, std::string narSize, std::vector<std::string> refs){
  std::vector<std::string> xs;
  xs.push_back("1");
  xs.push_back(storePath);
  xs.push_back(narHash);
  xs.push_back(narSize);
  xs.push_back(
    boost::algorithm::join(refs,",")
  );
  return boost::algorithm::join(xs,";");
}
