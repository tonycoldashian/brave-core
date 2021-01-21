/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import Theme from 'brave-ui/theme/brave-default'
import { ThemeProvider } from 'brave-ui/theme'
import { initLocale } from 'brave-ui/helpers'

import { getUIMessages } from '../../background/api/locale_api'

interface Props {
  children?: React.ReactNode
}

initLocale(getUIMessages())

export function LegacyThemeProvider (props: Props) {
  return (
    <ThemeProvider theme={Theme}>
      {props.children}
    </ThemeProvider>
  )
}
