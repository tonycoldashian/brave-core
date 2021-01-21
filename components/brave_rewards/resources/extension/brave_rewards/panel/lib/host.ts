/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createStateManager } from '../../../../shared/lib/state_manager'

import { Host, HostState } from './interfaces'

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

export function createHost (): Host {
  const stateManager = createStateManager<HostState>({})

  return {

    get state () {
      return stateManager.getState()
    },

    getString (key: string) {
      return getMessage(key)
    }

  }
}
