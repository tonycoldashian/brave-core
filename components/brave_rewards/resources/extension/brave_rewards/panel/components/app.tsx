/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { LocaleContext } from '../../../../shared/lib/locale_context'
import { ActivitySummary } from '../lib/interfaces'
import { HostContext } from '../lib/host_context'

import { BatIcon } from '../../../../shared/components/icons/bat_icon'
import { WithThemeVariables } from '../../../../shared/components/with_theme_variables'
import { LegacyThemeProvider } from './legacy_theme_provider'
import { Onboarding } from './onboarding'

import {
  WalletWrapper,
  WalletSummary,
  WalletSummarySlider,
  WalletPanel
} from '../../../../ui/components'

import * as style from './app.style'

function formatTokenAmount (amount: number) {
  return amount.toFixed(3)
}

function formatExchange (amount: number, rate: number) {
  return (amount * rate).toFixed(2)
}

function getSummaryReportData (activity: ActivitySummary, rate: number) {
  const entries: Record<string, { tokens: string, converted: string }> = {}

  function add (key: string, amount: number) {
    if (amount) {
      entries[key] = {
        tokens: formatTokenAmount(amount),
        converted: formatExchange(amount, rate)
      }
    }
  }

  add('ads', activity.ads)
  add('contribute', activity.autoContribution)
  add('monthly', activity.monthlyTips)
  add('grant', activity.grants)
  add('tips', activity.oneTimeTips)

  return entries
}

export function App () {
  const host = React.useContext(HostContext)
  const { getString } = React.useContext(LocaleContext)

  // TODO: showPublisher should be true when we're showing publisher data
  const [showPublisher, setShowPublisher] = React.useState(false)
  const [balanceInfo, setBalanceInfo] = React.useState(host.state.balanceInfo)
  const [onlyAnonWallet, setOnlyAnonWallet] = React.useState(
    Boolean(host.state.onlyAnonWallet))
  const [pendingContributionAmount, setPendingContributionAmount] =
    React.useState(host.state.pendingContributionAmount || 0)
  const [activitySummary, setActivitySummary] = React.useState(
    host.state.activitySummary)
  const [exchangeRate, setExchangeRate] = React.useState(
    host.state.exchangeRate || 0)

  host.addListener((state) => {
    setBalanceInfo(state.balanceInfo)
    setOnlyAnonWallet(Boolean(state.onlyAnonWallet))
    setPendingContributionAmount(state.pendingContributionAmount || 0)
    setActivitySummary(activitySummary)
    setExchangeRate(state.exchangeRate || 0)
  })

  if (!balanceInfo) {
    // TODO(zenparsing): Loading, etc.
    return null
  }

  function getWalletActions () {
    const actions = [
      {
        name:  getString('rewardsSettings'),
        icon: <BatIcon />,
        externalWallet: false,
        action: () => { host.openRewardsSettings() }
      }
    ]
    if (!onlyAnonWallet) {
      actions.unshift({
        name: getString('addFunds'),
        icon: <WalletAddIcon />,
        externalWallet: true,
        action: () => { host.openExternalWalletLink('desposit') },
      })
    }
    return actions
  }

  function onToggleSlider () {
    setShowPublisher(!showPublisher)
  }

  function showWalletPanel (): React.ReactNode {
    if (!publisher && !publisher.publisherKey) {
      return null
    }

    const setMonthlyAction = () => {
      host.openTipDialog('set-monthly')
    }

    const cancelMonthlyAction = () => {
      host.openTipDialog('clear-monthly')
    }

    const onToggleTips = () => {
      // Noop
    }

    return (
      <WalletPanel
        id={'wallet-panel'}
        platform={publisher.provider as Provider}
        publisherName={publisher.name}
        publisherImg={faviconUrl}
        monthlyAmount={defaultContribution}
        isVerified={checkmark}
        tipsEnabled={true}
        includeInAuto={!publisher.excluded}
        attentionScore={(publisher.percentage || 0).toString()}
        onToggleTips={onToggleTips}
        donationAction={this.showTipSiteDetail.bind(this, 'one-time')}
        onIncludeInAuto={this.switchAutoContribute}
        showUnVerified={this.shouldShowConnectedMessage()}
        acEnabled={enabledAC}
        moreLink={'https://brave.com/faq/#unclaimed-funds'}
        onRefreshPublisher={this.refreshPublisher}
        refreshingPublisher={this.state.refreshingPublisher}
        publisherRefreshed={this.state.publisherRefreshed}
        setMonthlyAction={setMonthlyAction}
        cancelMonthlyAction={cancelMonthlyAction}
        onlyAnonWallet={onlyAnonWallet}
      />
    )
  }

  return (
    <WithThemeVariables>
      <LegacyThemeProvider>
        <style.root>
          <Onboarding />
          <WalletWrapper
            id={'rewards-panel'}
            compact={true}
            contentPadding={false}
            gradientTop={showPublisher ? '249,251,252' : '233,235,255'}
            balance={formatTokenAmount(balanceInfo.total)}
            converted={`${formatExchange(balanceInfo.total, exchangeRate)} USD`}
            actions={getWalletActions()}
            showCopy={false}
            showSecActions={false}
            grant={currentPromotion}
            onGrantHide={this.onPromotionHide}
            onNotificationClick={notificationClick}
            onSolution={this.onSolution}
            onFinish={this.onFinish}
            walletState={walletStatus}
            onVerifyClick={onVerifyClick}
            onDisconnectClick={this.onDisconnectClick}
            goToUphold={this.goToUphold}
            greetings={utils.getGreetings(externalWallet)}
            onlyAnonWallet={onlyAnonWallet}
            showLoginMessage={this.showLoginMessage()}
            {...notification}
          >
            <WalletSummarySlider id={'panel-slider'} onToggle={onToggleSlider}>
              { showWalletPanel() }
              <WalletSummary
                compact={true}
                reservedAmount={pendingContributionAmount}
                onlyAnonWallet={onlyAnonWallet}
                reservedMoreLink={'https://brave.com/faq/#unclaimed-funds'}
                report={getSummaryReportData(activitySummary, exchangeRate)}
              />
            </WalletSummarySlider>
          </WalletWrapper>
        </style.root>
      </LegacyThemeProvider>
    </WithThemeVariables>
  )
}
