/*
 * Copyright (c) 2019 Ableton AG, Berlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

import UIKit

class VisualizationsOnSwitch: UIBarButtonItem {
  var isOn = true

  static private let offImage = UIImage(named: "EnableVisualizations")!
  static private let onImage = UIImage(named: "EnableVisualizationsFilled")!

  private let button = UIButton(type: .system)

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    button.widthAnchor.constraint(equalToConstant: 60).isActive = true
    // Expand the tappable area
    button.heightAnchor.constraint(equalToConstant: 44).isActive = true
    button.setImage(VisualizationsOnSwitch.onImage, for: .normal)
    button.addTarget(self, action: #selector(onPressed), for: .touchUpInside)
    customView = button
  }

  @objc private func onPressed() {
    isOn = !isOn
    button.setImage(
      isOn ? VisualizationsOnSwitch.onImage : VisualizationsOnSwitch.offImage,
      for: .normal)
    if let action = action {
      _ = target?.perform(action, with: self)
    }
  }
}
