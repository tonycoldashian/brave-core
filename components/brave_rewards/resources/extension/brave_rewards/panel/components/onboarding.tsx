/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { HostContext } from '../lib/host_context'
import { RewardsOptInModal, RewardsTourModal } from '../../../../shared/components/onboarding'

export function Onboarding () {
  const host = React.useContext(HostContext)

  const [firstTimeSetup, setFirstTimeSetup] = React.useState(false)
  const [showTour, setShowTour] = React.useState(false)
  const [prefs, setPrefs] = React.useState(host.state.prefs)
  const [onlyAnonWallet, setOnlyAnonWallet] = React.useState(
    Boolean(host.state.onlyAnonWallet))
  const [autoContributeChoices, setAutoContributeChoices] = React.useState(
    host.state.autoContributeChoices
  )
  const [showOnboarding, setShowOnboarding] = React.useState(
    Boolean(host.state.showOnboarding)
  )

  host.addListener((state) => {
    setOnlyAnonWallet(Boolean(state.onlyAnonWallet))
    setPrefs(state.prefs)
    setAutoContributeChoices(state.autoContributeChoices)
    setShowOnboarding(Boolean(state.showOnboarding))
  })

  if (showTour) {
    if (!prefs || !autoContributeChoices) {
      // TOOD(zenparsing): Loading or something?
      return null
    }

    const onDone = () => {
      setShowTour(false)
      setFirstTimeSetup(false)
    }

    const onClose = () => {
      onDone()
      if (showOnboarding) {
        host.saveOnboardingResult('dismissed')
      }
    }

    const onAdsPerHourChanged = (adsPerHour: number) => {
      host.updatePrefs({ adsPerHour })
    }

    const onAcAmountChanged = (autoContributeAmount: number) => {
      host.updatePrefs({ autoContributeAmount })
    }

    const onMount = (element: HTMLElement | null) => {
      const className = 'tour-modal-displayed'
      if (element) {
        document.body.classList.add(className)
      } else {
        document.body.classList.remove(className)
      }
    }

    return (
      <div ref={onMount}>
        <RewardsTourModal
          firstTimeSetup={firstTimeSetup}
          onlyAnonWallet={onlyAnonWallet}
          adsPerHour={prefs.adsPerHour}
          autoContributeAmount={prefs.autoContributeAmount}
          autoContributeAmountOptions={autoContributeChoices}
          onAdsPerHourChanged={onAdsPerHourChanged}
          onAutoContributeAmountChanged={onAcAmountChanged}
          onDone={onDone}
          onClose={onClose}
        />
      </div>
    )
  }

  if (!showOnboarding) {
    return null
  }

  const onTakeTour = () => {
    setShowTour(true)
  }

  const onEnable = () => {
    host.saveOnboardingResult('opted-in')
    setFirstTimeSetup(true)
    setShowTour(true)
  }

  const onClose = () => {
    host.saveOnboardingResult('dismissed')
  }

  return (
    <RewardsOptInModal
      onTakeTour={onTakeTour}
      onEnable={onEnable}
      onClose={onClose}
    />
  )
}
