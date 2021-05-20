/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/ipfs/brave_ipfs_client_updater.h"

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "components/component_updater/component_updater_service.h"
#include "third_party/re2/src/re2/re2.h"

namespace ipfs {

std::string BraveIpfsClientUpdater::g_ipfs_client_component_id_(
    kIpfsClientComponentId);
std::string BraveIpfsClientUpdater::g_ipfs_client_component_base64_public_key_(
    kIpfsClientComponentBase64PublicKey);

BraveIpfsClientUpdater::BraveIpfsClientUpdater(
    BraveComponent::Delegate* delegate,
    const base::FilePath& user_data_dir)
    : BraveComponent(delegate),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      registered_(false),
      user_data_dir_(user_data_dir),
      weak_ptr_factory_(this) {}

BraveIpfsClientUpdater::~BraveIpfsClientUpdater() {}

void BraveIpfsClientUpdater::Register() {
  if (registered_)
    return;

  BraveComponent::Register(kIpfsClientComponentName,
                           g_ipfs_client_component_id_,
                           g_ipfs_client_component_base64_public_key_);
  if (!updater_observer_.IsObservingSource(this))
    updater_observer_.Observe(this);
  registered_ = true;
}

namespace {

const char kExecutableRegEx[] = "go-ipfs_v(\\d+\\.\\d+\\.\\d+)(-rc1)?\\_\\w+-amd64";

base::Version ExtractExecutableVersion(const std::string& filename) {
  std::string version;
  if (!RE2::FullMatch(filename, kExecutableRegEx, &version))
    VLOG(1) << "Filename doesn't match regex:" << filename;
  return base::Version(version);
}

bool MigrateTo090Version(const base::FilePath& target,
    const base::FilePath& data_path) {
  base::CommandLine cmdline(target);
  cmdline.AppendArg("daemon");
  cmdline.AppendArg("--migrate");

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
    return false;
  }
 
  int exit_code = 0;
  return process.WaitForExit(&exit_code) && !exit_code;
}

bool MigrateVersionsOnFileThread(const base::FilePath& source,
                                 const base::FilePath& target,
                                 const base::FilePath& data_path) {
  base::Version from = ExtractExecutableVersion(
      source.BaseName().MaybeAsASCII());
  base::Version to = ExtractExecutableVersion(
      target.BaseName().MaybeAsASCII());
  if (!from.IsValid() || !to.IsValid())
    return false;

  if ((from.CompareTo(to) == -1) &&
     !to.CompareTo(base::Version("0.9.0"))) {
    if (!MigrateTo090Version(target, data_path)) {
      VLOG(1) << "IPFS node migration from:" << from << " to:" << to << " failed";
      return false;
    }
  }
  return true;
}

base::FilePath InitExecutablePath(const base::FilePath& install_dir) {
  base::FilePath executable_path;
  base::FileEnumerator traversal(install_dir, false,
                                 base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("go-ipfs_v*"));
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    base::FileEnumerator::FileInfo file_info = traversal.GetInfo();
    if (!RE2::FullMatch(file_info.GetName().MaybeAsASCII(),
                        kExecutableRegEx))
      continue;
    executable_path = current;
    break;
  }

  if (executable_path.empty()) {
    LOG(ERROR) << "Failed to locate Ipfs client executable in "
               << install_dir.value().c_str();
    return base::FilePath();
  }

#if defined(OS_POSIX)
  // Ensure that Ipfs client executable has appropriate file
  // permissions, as CRX unzipping does not preserve them.
  // See https://crbug.com/555011
  if (!base::SetPosixFilePermissions(executable_path, 0755)) {
    LOG(ERROR) << "Failed to set executable permission on "
               << executable_path.value().c_str();
    return base::FilePath();
  }
#endif  // defined(OS_POSIX)

  return executable_path;
}

void DeleteDir(const base::FilePath& path) {
  base::DeletePathRecursively(path);
}

}  // namespace

void BraveIpfsClientUpdater::SetExecutablePath(const base::FilePath& path) {
  executable_path_ = path;
  for (Observer& observer : observers_)
    observer.OnExecutableReady(path);
}

base::FilePath BraveIpfsClientUpdater::GetExecutablePath() const {
  return executable_path_;
}

void BraveIpfsClientUpdater::OnEvent(Events event, const std::string& id) {
  if (id != kIpfsClientComponentId)
    return;
  if (event == Events::COMPONENT_UPDATE_ERROR) {
    registered_ = false;
  }
  for (Observer& observer : observers_)
    observer.OnInstallationEvent(event);
}

void BraveIpfsClientUpdater::OnComponentReady(const std::string& component_id,
                                              const base::FilePath& install_dir,
                                              const std::string& manifest) {
  base::PostTaskAndReplyWithResult(
      GetTaskRunner().get(), FROM_HERE,
      base::BindOnce(&InitExecutablePath, install_dir),
      base::BindOnce(&BraveIpfsClientUpdater::SetExecutablePath,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BraveIpfsClientUpdater::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BraveIpfsClientUpdater::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BraveIpfsClientUpdater::Cleanup() {
  DCHECK(!user_data_dir_.empty());
  base::FilePath ipfs_component_dir =
      user_data_dir_.AppendASCII(kIpfsClientComponentId);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeleteDir, ipfs_component_dir));
}

void BraveIpfsClientUpdater::MigrationCompleted(
    LaunchExecutableCallback callback, const base::FilePath& source,
    const base::FilePath& target, bool success) {
  if (callback)
    std::move(callback).Run(success ? target : source);
}

void BraveIpfsClientUpdater::MigrateVersions(const base::FilePath& source,
    const base::FilePath& target, LaunchExecutableCallback callback) {
  auto data_path = user_data_dir_.Append(FILE_PATH_LITERAL("brave_ipfs"));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives(), 
      base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&MigrateVersionsOnFileThread, source, target, data_path),
      base::BindOnce(&BraveIpfsClientUpdater::MigrationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     source, target));
}

// static
void BraveIpfsClientUpdater::SetComponentIdAndBase64PublicKeyForTest(
    const std::string& component_id,
    const std::string& component_base64_public_key) {
  g_ipfs_client_component_id_ = component_id;
  g_ipfs_client_component_base64_public_key_ = component_base64_public_key;
}

///////////////////////////////////////////////////////////////////////////////

// The Brave Ipfs client extension factory.
std::unique_ptr<BraveIpfsClientUpdater> BraveIpfsClientUpdaterFactory(
    BraveComponent::Delegate* delegate,
    const base::FilePath& user_data_dir) {
  return std::make_unique<BraveIpfsClientUpdater>(delegate, user_data_dir);
}

}  // namespace ipfs
