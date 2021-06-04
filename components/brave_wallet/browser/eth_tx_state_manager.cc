/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/eth_tx_state_manager.h"

#include "base/guid.h"

namespace brave_wallet {

EthTxStateManager::EthTxStateManager() = default;
EthTxStateManager::~EthTxStateManager() = default;

EthTxStateManager::TxMeta::TxMeta() = default;
EthTxStateManager::TxMeta::TxMeta(const EthTransaction& tx) : tx(tx) {}
EthTxStateManager::TxMeta::TxMeta(const TxMeta& meta) = default;
EthTxStateManager::TxMeta::~TxMeta() = default;

std::string EthTxStateManager::GenerateMetaID() {
  return base::GenerateGUID();
}

void EthTxStateManager::AddOrUpdateTx(const TxMeta& meta) {
  tx_meta_map_[meta.id] = meta;
}

bool EthTxStateManager::GetTx(const std::string& id, TxMeta* meta) {
  if (!meta)
    return false;
  auto iter = tx_meta_map_.find(id);
  if (iter == tx_meta_map_.end())
    return false;

  *meta = iter->second;

  return true;
}

void EthTxStateManager::DeleteTx(const std::string& id) {
  tx_meta_map_.erase(id);
}

void EthTxStateManager::WipeTxs() {
  tx_meta_map_.clear();
}

std::vector<EthTxStateManager::TxMeta>
EthTxStateManager::GetTransactionsByStatus(TransactionStatus status,
                                           base::Optional<EthAddress> from) {
  std::vector<EthTxStateManager::TxMeta> result;
  for (auto& tx_meta : tx_meta_map_) {
    if (tx_meta.second.status == status) {
      if (from.has_value() && tx_meta.second.from != *from)
        continue;
      result.push_back(tx_meta.second);
    }
  }
  return result;
}

}  // namespace brave_wallet
