/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/ipfs/keys/ipns_keys_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "brave/components/ipfs/ipfs_constants.h"
#include "brave/components/ipfs/ipfs_json_parser.h"
#include "brave/components/ipfs/ipfs_network_utils.h"
#include "brave/components/ipfs/ipfs_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

bool WaitUntilExportFinished(base::Process process, base::FilePath key_path) {
  bool exited = false;
  int exit_code = 0;
  exited = process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(10),
                                          &exit_code);
  if (!exited)
    process.Terminate(0, true);
  return exited && !exit_code && base::PathExists(key_path);
}

}

namespace ipfs {

IpnsKeysManager::IpnsKeysManager(content::BrowserContext* context,
                                 const GURL& server_endpoint,
                                 IpfsService* service)
    : context_(context), server_endpoint_(server_endpoint),
      ipfs_service_(service) {
  DCHECK(context_);
  DCHECK(ipfs_service_);
  ipfs_service_->AddObserver(this);
  url_loader_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
}

IpnsKeysManager::~IpnsKeysManager() {
  ipfs_service_->RemoveObserver(this);
}

bool IpnsKeysManager::KeyExists(const std::string& name) const {
  return keys_.count(name);
}

void IpnsKeysManager::RemoveKey(const std::string& name,
                                RemoveKeyCallback callback) {
  if (!KeyExists(name)) {
    VLOG(1) << "Key " << name << " doesn't exist";
    if (callback)
      std::move(callback).Run(name, false);
    return;
  }

  auto generate_endpoint = server_endpoint_.Resolve(kAPIKeyRemoveEndpoint);
  GURL gurl =
      net::AppendQueryParameter(generate_endpoint, kArgQueryParam, name);

  auto url_loader = CreateURLLoader(gurl, "POST");

  auto iter = url_loaders_.insert(url_loaders_.begin(), std::move(url_loader));
  iter->get()->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&IpnsKeysManager::OnKeyRemoved, base::Unretained(this),
                     iter, name, std::move(callback)));
}

void IpnsKeysManager::OnKeyRemoved(SimpleURLLoaderList::iterator iter,
                                   const std::string& key_to_remove,
                                   RemoveKeyCallback callback,
                                   std::unique_ptr<std::string> response_body) {
  auto* url_loader = iter->get();
  int error_code = url_loader->NetError();
  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  url_loaders_.erase(iter);

  std::string name;
  std::string value;
  bool success = (error_code == net::OK && response_code == net::HTTP_OK);
  std::unordered_map<std::string, std::string> removed_keys;
  success = success &&
            IPFSJSONParser::GetParseKeysFromJSON(*response_body, &removed_keys);
  if (success) {
    if (removed_keys.count(key_to_remove))
      keys_.erase(key_to_remove);
  } else {
    VLOG(1) << "Fail to generate new key, error_code = " << error_code
            << " response_code = " << response_code;
  }
  if (callback)
    std::move(callback).Run(key_to_remove, success);
}

void IpnsKeysManager::GenerateNewKey(const std::string& name,
                                     GenerateKeyCallback callback) {
  if (KeyExists(name)) {
    VLOG(1) << "Key " << name << " already exists";
    if (callback)
      std::move(callback).Run(true, name, keys_[name]);
    return;
  }

  auto generate_endpoint = server_endpoint_.Resolve(kAPIKeyGenerateEndpoint);
  GURL gurl =
      net::AppendQueryParameter(generate_endpoint, kArgQueryParam, name);

  auto url_loader = CreateURLLoader(gurl, "POST");

  auto iter = url_loaders_.insert(url_loaders_.begin(), std::move(url_loader));
  iter->get()->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&IpnsKeysManager::OnKeyCreated, base::Unretained(this),
                     iter, std::move(callback)));
}

void IpnsKeysManager::OnKeyCreated(SimpleURLLoaderList::iterator iter,
                                   GenerateKeyCallback callback,
                                   std::unique_ptr<std::string> response_body) {
  auto* url_loader = iter->get();
  int error_code = url_loader->NetError();
  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  url_loaders_.erase(iter);

  std::string name;
  std::string value;
  bool success = (error_code == net::OK && response_code == net::HTTP_OK);
  success = success && IPFSJSONParser::GetParseSingleKeyFromJSON(*response_body,
                                                                 &name, &value);
  if (success) {
    keys_[name] = value;
  } else {
    VLOG(1) << "Fail to generate new key, error_code = " << error_code
            << " response_code = " << response_code;
  }
  if (callback)
    std::move(callback).Run(success, name, value);
}

void IpnsKeysManager::LoadKeys(LoadKeysCallback callback) {
  if (!pending_load_callbacks_.empty()) {
    if (callback)
      pending_load_callbacks_.push(std::move(callback));
    return;
  }
  if (callback)
    pending_load_callbacks_.push(std::move(callback));

  auto url_loader =
      CreateURLLoader(server_endpoint_.Resolve(kAPIKeyListEndpoint), "POST");
  auto iter = url_loaders_.insert(url_loaders_.begin(), std::move(url_loader));

  iter->get()->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(), base::BindOnce(&IpnsKeysManager::OnKeysLoaded,
                                                base::Unretained(this), iter));
}

void IpnsKeysManager::OnIpfsLaunched(bool result, int64_t pid) {
  bool success = result && pid > 0;
  if (!success)
    return;
  LoadKeys(base::NullCallback());
}

void IpnsKeysManager::OnIpfsShutdown() {
  keys_.clear();
}

void IpnsKeysManager::OnKeysLoaded(SimpleURLLoaderList::iterator iter,
                                   std::unique_ptr<std::string> response_body) {
  auto* url_loader = iter->get();
  int error_code = url_loader->NetError();
  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  url_loaders_.erase(iter);

  bool success = (error_code == net::OK && response_code == net::HTTP_OK);
  std::unordered_map<std::string, std::string> new_keys;
  success = success && response_body &&
            IPFSJSONParser::GetParseKeysFromJSON(*response_body, &new_keys);
  if (success) {
    keys_.swap(new_keys);
  } else {
    VLOG(1) << "Fail to load keys, error_code = " << error_code
            << " response_code = " << response_code;
  }
  NotifyKeysLoaded(success);
}

void IpnsKeysManager::SetLoadCallbackForTest(LoadKeysCallback callback) {
  if (callback)
    pending_load_callbacks_.push(std::move(callback));
}

void IpnsKeysManager::NotifyKeysLoaded(bool result) {
  while (!pending_load_callbacks_.empty()) {
    if (pending_load_callbacks_.front())
      std::move(pending_load_callbacks_.front()).Run(result);
    pending_load_callbacks_.pop();
  }
}

void IpnsKeysManager::SetServerEndpointForTest(const GURL& gurl) {
  server_endpoint_ = gurl;
}

const std::string IpnsKeysManager::FindKey(const std::string& name) const {
  auto it = keys_.find(name);
  if (it == keys_.end())
    return std::string();
  return it->second;
}

void IpnsKeysManager::ExportKey(const std::string& key,
                                const base::FilePath& target_path) {
  base::FilePath path = ipfs_service_->GetIpfsExecutablePath();
  if (path.empty())
    return;

  base::CommandLine cmdline(path);
  cmdline.AppendArg("key");
  cmdline.AppendArg("export");
  cmdline.AppendArg("-o=" + target_path.MaybeAsASCII());
  cmdline.AppendArg(key);

  base::FilePath data_path = ipfs_service_->GetDataPath();
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.environment[L"IPFS_PATH"] = data_path.value();
#else
  options.environment["IPFS_PATH"] = data_path.value();
#endif
#if defined(OS_LINUX)
  options.kill_on_parent_death = true;
#endif
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  DLOG(INFO) << cmdline.GetCommandLineString();
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives(), 
      base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WaitUntilExportFinished, std::move(process), target_path),
      base::BindOnce(&IpnsKeysManager::OnKeyExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IpnsKeysManager::OnKeyExported(bool success) {
  if (!success) {
    VLOG(1) << "Fail to export a key";
  }
}

}  // namespace ipfs
