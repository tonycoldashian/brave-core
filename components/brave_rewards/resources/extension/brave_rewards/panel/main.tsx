/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'
import * as ReactDOM from 'react-dom'

import { LocaleContext } from '../../../shared/lib/locale_context'

import { createHost } from './lib/host'
import { HostContext } from './lib/host_context'
import { App } from './components/app'

const host = createHost()

ReactDOM.render(
  <HostContext.Provider value={host}>
    <LocaleContext.Provider value={host}>
      <App />
    </LocaleContext.Provider>
  </HostContext.Provider>,
  document.getElementById('root'))
