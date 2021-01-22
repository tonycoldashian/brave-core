/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { OnboardingCompletedStore } from '../../../../shared/lib/onboarding_completed_store'
import { createStateManager } from '../../../../shared/lib/state_manager'

import { Host, HostState, PromotionStatus } from './interfaces'

const braveExtensionId = 'mnojpmjdmbbfmejpflffifhffcmidifd'

function getMessage (key: string) {
  // In order to normalize messages across extensions and WebUI, replace all
  // chrome.i18n message placeholders with $N marker patterns. UI components
  // are responsible for replacing these markers with appropriate text or
  // using the markers to build HTML.
  const substitutions = ['$1', '$2', '$3', '$4', '$5', '$6', '$7', '$8', '$9']
  if (!chrome.i18n) {
    return `i18n missing: ${key}`
  }
  return chrome.i18n.getMessage(key, substitutions) || `i18n: ${key}`
}

function whenReady () {
  return new Promise<void>((resolve) => {
    chrome.braveRewards.isInitialized((initialized) => {
      if (initialized) {
        resolve()
      }
    })
    chrome.braveRewards.initialized.addListener(() => {
      resolve()
    })
  })
}

function isGreaselionPublisherURL (url: string) {
  let hostname = ''
  try {
    hostname = new URL(url).hostname
    if (!hostname) {
      return false
    }
  } catch {
    return false
  }

  const domains = [
    'github.com',
    'reddit.com',
    'twitch.tv',
    'twitter.com',
    'vimeo.com',
    'youtube.com'
  ]

  return domains.some((domain) => (
    hostname === domain || hostname.endsWith(`.${ domain }`)
  ))
}

export function createHost (): Host {
  const onboardingCompletedStore = new OnboardingCompletedStore()
  const stateManager = createStateManager<HostState>({})

  function loadPublisherInfo () {
    // TODO(zenparsing): Introduce a locale-storage based cache, keyed on tab
    // URL, with expiration and a max size.

    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
      const [tab] = tabs || []

      // Don't retreive publisher info for incognito tabs
      if (!tab || !tab.id || !tab.url || !tab.active || tab.incognito) {
        stateManager.update({ currentTab: undefined })
        return
      }

      stateManager.update({ currentTab: tab.id })

      if (isGreaselionPublisherURL(tab.url)) {
        // Send a message to the brave extension requesting publisher info
        // for the current tab. The brave extension will dispatch the request
        // to the appropriate content script loaded by Greaselion.
        chrome.runtime.sendMessage(braveExtensionId, {
          type: 'GetPublisherPanelInfo',
          tabId: tab.id
        })
      } else {
        chrome.braveRewards.getPublisherData(
          tab.id, tab.url, tab.favIconUrl || '', '')
      }

      // TODO(zenparsing): How do we get the updated publisher data?
    })
  }

  function handleActionURL () {
    const { hash } = window.location
    if (!hash) {
      return
    }
    const match = /^#grant_(.)+/.exec(hash)
    if (match) {
      const promotion = match[1]
      chrome.braveRewards.claimPromotion(promotion, () => {
        // TODO: this.actions.onClaimPromotion(properties)
      })
    }
  }

  function initialize () {
    chrome.braveRewards.fetchBalance((balance) => {
      const wallets = balance.wallets || {}
      stateManager.update({
        balanceInfo: {
          total: balance.total,
          legacyUserFunds: Number(wallets['anonymous']) || 0,
          virtualTokens: Number(wallets['blinded']) || 0,
          externalWallet: Number(wallets['uphold']) || 0
        }
      })
    })

    chrome.braveRewards.onlyAnonWallet((onlyAnonWallet) => {
      stateManager.update({ onlyAnonWallet })
    })

    chrome.braveRewards.getPrefs((prefs) => {
      stateManager.update({ prefs })
    })

    chrome.braveRewards.getRewardsParameters((parameters) => {
      stateManager.update({
        exchangeRate: parameters.rate,
        autoContributeChoices: parameters.autoContributeChoices
      })
    })

    chrome.braveRewards.shouldShowOnboarding((showOnboarding) => {
      stateManager.update({ showOnboarding })
    })

    chrome.braveRewards.getPendingContributionsTotal(((amount) => {
      stateManager.update({ pendingContributionAmount: amount })
    }))

    chrome.braveRewards.getBalanceReport(
      new Date().getMonth() + 1,
      new Date().getFullYear(),
      (report) => {
        stateManager.update({
          activitySummary: {
            ads: report.ads,
            grants: report.grant,
            autoContribution: report.contribute,
            monthlyTips: report.monthly,
            oneTimeTips: report.tips
          }
        })
      })

    chrome.braveRewards.onPromotions.addListener((result, promotions) => {
      if (result === 1 || !promotions) { // LEDGER_ERROR
        return
      }

      const mapPromotionStatus = (status: number): PromotionStatus => {
        switch (status) {
          case 0: return 'active'
          case 1: return 'attested'
          case 4: return 'finished'
          case 5: return 'expired'
          default: return 'active'
        }
      }

      stateManager.update({
        promotions: promotions.map((promotion) => {
          return {
            id: promotion.promotionId,
            kind: promotion.type === 0 ? 'ads' : 'ugp',
            status: mapPromotionStatus(promotion.status),
            value: promotion.amount,
            expiresAt: promotion.expiresAt * 1000 // Convert seconds to ms
          }
        })
      })
    })

    chrome.braveRewards.fetchPromotions()

    loadPublisherInfo()

    handleActionURL()
  }

  whenReady().then(() => {
    initialize()
  }).catch((err) => {
    console.error(err)
  })

  stateManager.addListener(console.log)

  return {

    get state () {
      return stateManager.getState()
    },

    getString: getMessage,

    saveOnboardingResult (result) {
      chrome.braveRewards.saveOnboardingResult(result)
      stateManager.update({ showOnboarding: false })
      onboardingCompletedStore.save()
    },

    updatePrefs (prefs) {
      chrome.braveRewards.updatePrefs(prefs)
      const currentPrefs = stateManager.getState().prefs
      if (currentPrefs) {
        stateManager.update({
          prefs: { ...currentPrefs, ...prefs }
        })
      }
    },

    openRewardsSettings () {
      chrome.tabs.create({ url: 'brave://rewards' })
      /* TODO
      if (notificationId) {
        this.onCloseNotification(notificationId)
      */
    },

    openExternalWalletLink (kind) {
      if (kind === 'desposit') {
        /* TODO
        if (notificationId) {
          this.actions.deleteNotification(notificationId)
        }

        if (!externalWallet) {
          return
        }

        if (externalWallet.addUrl) {
          chrome.tabs.create({
            url: externalWallet.addUrl
          })
          return
        }

        utils.handleUpholdLink(balance, externalWallet)
        */
        return
      }

      if (kind === 'account') {
        /*
        chrome.tabs.create({ url: externalWallet.accountUrl })
        */
      }
    },

    claimPromotion () {

    },

    attestPromotion () {

    },

    openTipDialog (entryPoint) {
      const { currentTab, publisherInfo } = stateManager.getState()
      if (typeof currentTab !== 'number' || !publisherInfo) {
        return
      }
      chrome.braveRewards.tipSite(currentTab, publisherInfo.key, entryPoint)
      window.close()
    },

    addListener: stateManager.addListener

  }
}
