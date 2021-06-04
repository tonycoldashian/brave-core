/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_ETH_TX_STATE_MANAGER_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_ETH_TX_STATE_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "brave/components/brave_wallet/browser/brave_wallet_types.h"
#include "brave/components/brave_wallet/browser/eth_address.h"
#include "brave/components/brave_wallet/browser/eth_transaction.h"

namespace brave_wallet {

class EthTxStateManager {
 public:
  enum class TransactionStatus {
    UNAPPROVED,
    APPROVED,
    REJECTED,
    SIGNED,
    SUBMITTED,
    FAILED,
    DROPPED,
    CONFIRMED
  };

  struct TxMeta {
    TxMeta();
    explicit TxMeta(const EthTransaction& tx);
    TxMeta(const TxMeta&);
    ~TxMeta();

    std::string id;
    TransactionStatus status;
    EthAddress from;
    uint256_t last_gas_price = 0;
    // created time
    // submitted time
    // confirmed time
    TransactionReceipt tx_receipt;
    std::string tx_hash;
    EthTransaction tx;
  };

  EthTxStateManager();
  ~EthTxStateManager();
  EthTxStateManager(const EthTxStateManager&) = delete;
  EthTxStateManager operator=(const EthTxStateManager&) = delete;

  static std::string GenerateMetaID();

  void AddOrUpdateTx(const TxMeta& meta);
  bool GetTx(const std::string& id, TxMeta* meta);
  void DeleteTx(const std::string& id);
  void WipeTxs();

  std::vector<TxMeta> GetTransactionsByStatus(TransactionStatus status,
                                              base::Optional<EthAddress> from);

  // TODO(darkdh): persit TxMeta map to disk
 private:
  base::flat_map<std::string, TxMeta> tx_meta_map_;
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_ETH_TX_STATE_MANAGER_H_
