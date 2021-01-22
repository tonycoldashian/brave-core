/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface BalanceInfo {
  total: number
  legacyUserFunds: number
  virtualTokens: number
  externalWallet: number
}

type PromotionKind = 'ads' | 'ugp'

export type PromotionStatus =
  'active' |
  'attested' |
  'finished' |
  'expired' |
  'corrupted'

interface Promotion {
  id: string
  kind: PromotionKind
  status: PromotionStatus
  value: number
  expiresAt: number
}

type PromotionCaptchaStatus =
  'start' |
  'wrong-position' |
  'complete' |
  'error'

export interface PromotionCaptcha {
  id: string
  promotionId: string
  status: PromotionCaptchaStatus
  image: string
  hint: string
}

export interface ActivitySummary {
  ads: number
  grants: number
  autoContribution: number
  monthlyTips: number
  oneTimeTips: number
}

interface PublisherInfo {
  key: string
}

export interface HostState {
  balanceInfo?: BalanceInfo
  activitySummary?: ActivitySummary
  promotions?: Promotion[]
  promotionCaptcha?: PromotionCaptcha
  exchangeRate?: number
  autoContributeChoices?: number[]
  prefs?: chrome.braveRewards.RewardsPrefs
  onlyAnonWallet?: boolean
  currentTab?: number
  showOnboarding?: boolean
  pendingContributionAmount?: number
  publisherInfo?: PublisherInfo
}

export type HostListener = (state: HostState) => void

type ExternalWalletLinkKind = 'account' | 'desposit'

export interface Host {
  state: HostState
  getString (key: string): string
  saveOnboardingResult (result: 'opted-in' | 'dismissed'): void
  updatePrefs (prefs: Partial<chrome.braveRewards.RewardsPrefs>): void
  openRewardsSettings (): void
  openExternalWalletLink (kind: ExternalWalletLinkKind): void
  claimPromotion (): void
  attestPromotion (): void
  openTipDialog (entryPoint: chrome.braveRewards.TipDialogEntryPoint): void
  addListener (callback: HostListener): void
}
