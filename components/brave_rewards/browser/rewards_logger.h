/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_REWARDS_BROWSER_REWARDS_LOGGER_H_
#define BRAVE_COMPONENTS_BRAVE_REWARDS_BROWSER_REWARDS_LOGGER_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "brave/components/brave_rewards/common/brave_rewards.mojom.h"

namespace brave_rewards {

void CreateRewardsLoggerOnTaskRunner(
    const base::FilePath& file_path,
    mojo::PendingReceiver<mojom::RewardsLogger> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner);

}  // namespace brave_rewards

#endif  // BRAVE_COMPONENTS_BRAVE_REWARDS_BROWSER_REWARDS_LOGGER_H_
