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

@IBDesignable
class SliderWithValue: UISlider {
  let kValueFieldWidth: CGFloat = 60.0
  let kPadding: CGFloat = 16.0
  var valueFormatter = { (value: Float) in return "\(Int(value))" }
  private var valueField = UITextField()

  override var value: Float {
    didSet {
      updateValueField()
    }
  }

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    initialize()
  }

  override init(frame: CGRect) {
    super.init(frame: frame)
    initialize()
  }

  private func initialize() {
    valueField.borderStyle = .roundedRect
    valueField.font = UIFont.systemFont(ofSize: 15.0)
    valueField.textAlignment = .center
    valueField.isUserInteractionEnabled = false
    valueField.translatesAutoresizingMaskIntoConstraints = false
    addSubview(valueField)
    updateValueField()

    let valueFieldWidthConstraint = NSLayoutConstraint(
      item: valueField, attribute: .width, relatedBy: .equal, toItem: nil,
      attribute: .notAnAttribute, multiplier: 1.0, constant: kValueFieldWidth)
    let valueFieldConstraint = NSLayoutConstraint(
      item: valueField, attribute: .trailing, relatedBy: .equal, toItem: self,
      attribute: .trailing, multiplier: 1.0, constant: 0)
    NSLayoutConstraint.activate([valueFieldWidthConstraint, valueFieldConstraint])

    self.addTarget(self, action: #selector(onValueChange), for: .valueChanged)
  }

  override func trackRect(forBounds bounds: CGRect) -> CGRect {
    var rect = super.trackRect(forBounds: bounds)
    rect.size.width = rect.width - kValueFieldWidth - kPadding
    return rect
  }

  @objc private func onValueChange() {
    updateValueField()
  }

  private func updateValueField() {
    valueField.text = valueFormatter(value)
  }
}
