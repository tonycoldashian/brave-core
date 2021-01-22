/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import styled from 'styled-components'

export const root = styled.div`
  min-width: 373px;
  max-width: 415px;
  min-height: 540px;

  .tour-modal-displayed & {
    /* The rewards tour modal is taller than the current panel contents.
     * When the tour is displayed, extend the height of the panel.
     */
    min-height: 560px;
  }
`
