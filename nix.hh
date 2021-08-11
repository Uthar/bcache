#pragma once

#include <nix/ref.hh>
#include <string>
#include "nix/config.h"
#include "nix/derivations.hh"
#include "nix/globals.hh"
#include "nix/store-api.hh"
#include "nix/util.hh"
#include "nix/crypto.hh"

nix::ref<nix::Store> store();

std::string queryPathFromHashPart(std::string hashPart);

// => ($deriver, $narHash, $time, $narSize, $refs, $sigs)
std::optional<nix::ref<const nix::ValidPathInfo>> queryPathInfo(std::string storePath);

void *readFile(void *secretKeyFile);

std::string fingerprintPath(std::string storePath, std::string narHash, std::string narSize, std::vector<std::string> refs);

std::string signString(const char * secretKey, const char * fingerprint);
