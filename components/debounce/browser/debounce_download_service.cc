/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/debounce/browser/debounce_download_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "brave/components/brave_component_updater/browser/dat_file_util.h"
#include "brave/components/brave_component_updater/browser/local_data_files_service.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/url_util.h"

using brave_component_updater::LocalDataFilesObserver;
using brave_component_updater::LocalDataFilesService;

namespace debounce {

const char kDebounceConfigFile[] = "debounce.json";
const char kDebounceConfigFileVersion[] = "1";
// debounce.json keys
const char kInclude[] = "include";
const char kExclude[] = "exclude";
const char kAction[] = "action";
const char kParam[] = "param";

namespace {

// Adapted from net::AppendOrReplaceQueryParameter
bool RemoveQueryParameter(const GURL& url,
                          const std::string& name,
                          GURL* final_url) {
  std::string param_name = net::EscapeQueryParamValue(name, true);
  const std::string input = url.query();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  bool found = false;
  while (url::ExtractQueryKeyValue(input.data(), &cursor, &key_range,
                                   &value_range)) {
    const base::StringPiece key(input.data() + key_range.begin, key_range.len);
    if (key == param_name) {
      found = true;
      continue;
    }
    std::string key_value_pair;
    key_value_pair.assign(input, key_range.begin,
                          value_range.end() - key_range.begin);
    if (!output.empty())
      output += "&";
    output += key_value_pair;
  }

  // If we didn't find the named query parameter, fail now and skip the
  // expensive ReplaceComponents call.
  if (!found)
    return false;

  GURL::Replacements replacements;
  if (output.empty())
    replacements.ClearQuery();
  else
    replacements.SetQueryStr(output);
  *final_url = url.ReplaceComponents(replacements);

  return true;
}

}  // namespace

DebounceRule::DebounceRule() = default;

DebounceRule::~DebounceRule() = default;

void DebounceRule::clear() {
  include_pattern_set_.ClearPatterns();
  exclude_pattern_set_.ClearPatterns();
  action_ = kDebounceNoAction;
  param_ = "";
}

void DebounceRule::Parse(base::ListValue* include_value,
                         base::ListValue* exclude_value,
                         const std::string& action,
                         const std::string& param) {
  clear();
  std::string error;

  // Debouncing only affects HTTP or HTTPS URLs, regardless of how the rules are
  // written. (Also, don't write rules for other URL schemes, because they won't
  // work and you're just wasting everyone's time.)
  int valid_schemes = URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;
  if (!include_pattern_set_.Populate(*include_value, valid_schemes, false,
                                     &error) ||
      !exclude_pattern_set_.Populate(*exclude_value, valid_schemes, false,
                                     &error)) {
    LOG(ERROR) << error;
    clear();
    return;
  }
  if (action == "redirect")
    action_ = kDebounceRedirectToParam;
  else if (action == "base64,redirect")
    action_ = kDebounceBase64DecodeAndRedirectToParam;
  else if (action == "remove")
    action_ = kDebounceRemoveParam;
  param_ = param;
}

bool DebounceRule::Apply(const GURL& original_url,
                         const net::SiteForCookies& original_site_for_cookies,
                         GURL* final_url) {
  // If URL matches an explicitly excluded pattern, this rule does not apply.
  if (exclude_pattern_set_.MatchesURL(original_url))
    return false;

  // If URL does not match an explicitly included pattern, this rule does not
  // apply.
  if (!include_pattern_set_.MatchesURL(original_url))
    return false;

  if (action_ == kDebounceRemoveParam) {
    // RemoveQueryParameter is guaranteed to return a valid URL in
    // |final_url|, so no need for an is_valid check here. It also
    // returns false if the given parameter was not found, which we
    // pass to the caller to say that the rule was not applied.
    return RemoveQueryParameter(original_url, param_, final_url);
  }

  if (action_ == kDebounceRedirectToParam ||
      action_ == kDebounceBase64DecodeAndRedirectToParam) {
    std::string unescaped_value;
    if (!net::GetValueForKeyInQuery(original_url, param_, &unescaped_value))
      return false;
    GURL new_url;
    if (action_ == kDebounceBase64DecodeAndRedirectToParam) {
      std::string base64_decoded_value;
      if (!base::Base64Decode(unescaped_value, &base64_decoded_value))
        return false;
      new_url = GURL(base64_decoded_value);
    } else {
      new_url = GURL(unescaped_value);
    }

    // Failsafe: ensure we got a valid URL out of the param.
    if (!new_url.is_valid())
      return false;

    // Failsafe: never redirect to the same site.
    if (original_site_for_cookies.IsEquivalent(
            net::SiteForCookies::FromUrl(new_url)))
      return false;

    *final_url = new_url;
    return true;
  }

  // Unknown actions always return false, to allow for future updates to the
  // rules file which may be pushed to users before a new version of the code
  // that parses it.
  return false;
}

DebounceDownloadService::DebounceDownloadService(
    LocalDataFilesService* local_data_files_service)
    : LocalDataFilesObserver(local_data_files_service), weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DebounceDownloadService::~DebounceDownloadService() {}

void DebounceDownloadService::LoadDirectlyFromResourcePath() {
  base::FilePath dat_file_path = resource_dir_.AppendASCII(kDebounceConfigFile);
  base::PostTaskAndReplyWithResult(
      GetTaskRunner().get(), FROM_HERE,
      base::BindOnce(&brave_component_updater::GetDATFileAsString,
                     dat_file_path),
      base::BindOnce(&DebounceDownloadService::OnDATFileDataReady,
                     weak_factory_.GetWeakPtr()));
}

void DebounceDownloadService::OnDATFileDataReady(std::string contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rules_.clear();
  if (contents.empty()) {
    LOG(ERROR) << "Could not obtain debounce configuration";
    return;
  }
  base::Optional<base::Value> root = base::JSONReader::Read(contents);
  if (!root) {
    LOG(ERROR) << "Failed to parse debounce configuration";
    return;
  }
  base::ListValue* root_list = nullptr;
  root->GetAsList(&root_list);
  for (base::Value& rule_it : root_list->GetList()) {
    base::DictionaryValue* rule_dict = nullptr;
    rule_it.GetAsDictionary(&rule_dict);
    base::ListValue* include_value = nullptr;
    rule_dict->GetList(kInclude, &include_value);
    base::ListValue* exclude_value = nullptr;
    rule_dict->GetList(kExclude, &exclude_value);
    const std::string* action_ptr = rule_it.FindStringPath(kAction);
    const std::string action_value = action_ptr ? *action_ptr : "";
    const std::string* param_ptr = rule_it.FindStringPath(kParam);
    const std::string param_value = param_ptr ? *param_ptr : "";

    std::unique_ptr<DebounceRule> rule = std::make_unique<DebounceRule>();
    rule->Parse(include_value, exclude_value, action_value, param_value);
    rules_.push_back(std::move(rule));
  }
  for (Observer& observer : observers_)
    observer.OnRulesReady(this);
}

void DebounceDownloadService::OnComponentReady(
    const std::string& component_id,
    const base::FilePath& install_dir,
    const std::string& manifest) {
  resource_dir_ = install_dir.AppendASCII(kDebounceConfigFileVersion);
  LoadDirectlyFromResourcePath();
}

std::vector<std::unique_ptr<DebounceRule>>* DebounceDownloadService::rules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &rules_;
}

scoped_refptr<base::SequencedTaskRunner>
DebounceDownloadService::GetTaskRunner() {
  return local_data_files_service()->GetTaskRunner();
}

///////////////////////////////////////////////////////////////////////////////

// The factory
std::unique_ptr<DebounceDownloadService> DebounceDownloadServiceFactory(
    LocalDataFilesService* local_data_files_service) {
  return std::make_unique<DebounceDownloadService>(local_data_files_service);
}

}  // namespace debounce
