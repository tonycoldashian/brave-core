/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface BalanceInfo {
  total: number
  legacyUserFunds: number
  virtualTokens: number
  externalWallet: number
}

export interface HostState {
  balanceInfo?: BalanceInfo
  exchangeRate?: number
  autoContributeChoices?: number[]
  prefs?: chrome.braveRewards.RewardsPrefs
  onlyAnonWallet?: boolean
  currentTab?: number
  showOnboarding?: boolean
}

export type HostListener = (state: HostState) => void

export interface Host {
  state: HostState
  getString (key: string): string
  saveOnboardingResult (result: 'opted-in' | 'dismissed'): void
  updatePrefs (prefs: Partial<chrome.braveRewards.RewardsPrefs>): void
  addListener (callback: HostListener): void
}
